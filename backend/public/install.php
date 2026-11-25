<?php
/**
 * Jednorazový webový inštalátor (Websupport / shared hosting, bez SSH).
 *
 * Po úspešnej inštalácii:
 *  1. Zmažte tento súbor (install.php) cez FTP.
 *  2. Súbor config/install.lock zabráni opätovnému spusteniu.
 */
declare(strict_types=1);

header('Content-Type: text/html; charset=utf-8');
header('X-Robots-Tag: noindex, nofollow');

/** @var string $rootDir */
$rootDir    = require __DIR__ . '/_app_root.php';
$configDir  = $rootDir . '/config';
$lockFile   = $configDir . '/install.lock';
$configFile = $configDir . '/config.php';
$schemaFile = $rootDir . '/schema_webhost.sql';

$errors = [];
$success = null;
$apiBaseHint = guessApiBase();

if (is_file($lockFile) || (is_file($configFile) && !isset($_GET['force']))) {
    renderPage(
        'Inštalácia je už hotová',
        '<p>API je nainštalované. <strong>Zmažte súbor <code>install.php</code></strong> cez FTP.</p>'
        . '<p>Overenie: <a href="' . h($apiBaseHint) . '/api/v1/health">' . h($apiBaseHint) . '/api/v1/health</a></p>',
        [],
        null,
        $apiBaseHint
    );
    exit;
}

if ($_SERVER['REQUEST_METHOD'] === 'POST') {
    $dbHost = trim((string) ($_POST['db_host'] ?? 'localhost'));
    $dbPort = (int) ($_POST['db_port'] ?? 3306);
    $dbName = trim((string) ($_POST['db_name'] ?? ''));
    $dbUser = trim((string) ($_POST['db_user'] ?? ''));
    $dbPass = (string) ($_POST['db_pass'] ?? '');
    $familyName = trim((string) ($_POST['family_name'] ?? ''));
    $accessKey = (string) ($_POST['access_key'] ?? '');
    $profilesRaw = trim((string) ($_POST['profiles'] ?? "Maroš\nVeronika\nMelinka"));
    $withDefaults = isset($_POST['with_defaults']);

    if ($dbHost === '' || $dbName === '' || $dbUser === '') {
        $errors[] = 'Vyplňte host, názov DB a používateľa MySQL.';
    }
    if ($familyName === '') {
        $errors[] = 'Zadajte názov rodiny.';
    }
    if (strlen($accessKey) < 12) {
        $errors[] = 'Prístupový kľúč musí mať aspoň 12 znakov.';
    }
    if (!is_file($schemaFile)) {
        $errors[] = 'Chýba schema_webhost.sql — nahrajte celý balík backend/.';
    }
    if (!is_dir($configDir) || !is_writable($configDir)) {
        $errors[] = 'Priečinok config/ musí existovať a byť zapisovateľný (chmod 755 alebo 775).';
    }

    $profileNames = [];
    foreach (preg_split('/\R+/', $profilesRaw) ?: [] as $line) {
        $n = trim($line);
        if ($n !== '') {
            $profileNames[] = mb_substr($n, 0, 80);
        }
    }
    if ($profileNames === []) {
        $errors[] = 'Zadajte aspoň jeden profil (meno).';
    }

    if ($errors === []) {
        try {
            $dsn = sprintf(
                'mysql:host=%s;port=%d;dbname=%s;charset=utf8mb4',
                $dbHost,
                $dbPort > 0 ? $dbPort : 3306,
                $dbName
            );
            $pdo = new PDO($dsn, $dbUser, $dbPass, [
                PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
            ]);
            $pdo->exec("SET NAMES utf8mb4");
            $pdo->exec("SET time_zone = '+00:00'");

            runSqlFile($pdo, $schemaFile);

            $configPhp = buildConfigPhp($dbHost, $dbPort > 0 ? $dbPort : 3306, $dbName, $dbUser, $dbPass);
            if (file_put_contents($configFile, $configPhp) === false) {
                throw new RuntimeException('Nepodarilo sa zapísať config/config.php');
            }
            @chmod($configFile, 0640);

            $hash = password_hash($accessKey, PASSWORD_ARGON2ID);
            if ($hash === false) {
                $hash = password_hash($accessKey, PASSWORD_BCRYPT);
            }
            if ($hash === false) {
                throw new RuntimeException('password_hash zlyhal — skontrolujte PHP.');
            }

            $pdo->beginTransaction();
            try {
                $stmt = $pdo->prepare('INSERT INTO families (name, access_key_hash) VALUES (?, ?)');
                $stmt->execute([$familyName, $hash]);
                $familyId = (int) $pdo->lastInsertId();

                $pdo->prepare(
                    'INSERT INTO family_settings (family_id, basal_reminder_time, glucose_unit, duplicate_bolus_minutes)
                     VALUES (?, ?, ?, ?)'
                )->execute([$familyId, '21:30', 'mmol', 20]);

                $colors = ['#1976D2', '#C2185B', '#7B1FA2', '#00897B', '#F57C00', '#5D4037'];
                $ps = $pdo->prepare(
                    'INSERT INTO profiles (family_id, name, color_hex, sort_order) VALUES (?, ?, ?, ?)'
                );
                foreach ($profileNames as $i => $name) {
                    $ps->execute([$familyId, $name, $colors[$i % count($colors)], $i + 1]);
                }

                if ($withDefaults) {
                    seedDefaults($pdo, $familyId);
                }

                $pdo->commit();
            } catch (Throwable $e) {
                $pdo->rollBack();
                throw $e;
            }

            file_put_contents(
                $lockFile,
                'installed_at=' . gmdate('c') . "\nfamily_id={$familyId}\n"
            );

            $success = [
                'family_id' => $familyId,
                'family_name' => $familyName,
                'api_base' => $apiBaseHint,
                'access_key' => $accessKey,
            ];
        } catch (PDOException $e) {
            $errors[] = 'MySQL: ' . $e->getMessage();
            if (is_file($configFile) && !is_file($lockFile)) {
                @unlink($configFile);
            }
        } catch (Throwable $e) {
            $errors[] = $e->getMessage();
            if (is_file($configFile) && !is_file($lockFile)) {
                @unlink($configFile);
            }
        }
    }
}

$formHtml = null;
if ($success === null) {
    $formHtml = buildForm($_POST ?? []);
}

renderPage(
    $success ? 'Inštalácia úspešná' : 'Inštalátor GlukoView API',
    $formHtml,
    $errors,
    $success,
    $apiBaseHint
);

// -----------------------------------------------------------------------------

function guessApiBase(): string
{
    $https = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off')
        || (isset($_SERVER['HTTP_X_FORWARDED_PROTO']) && $_SERVER['HTTP_X_FORWARDED_PROTO'] === 'https');
    $scheme = $https ? 'https' : 'http';
    $host = $_SERVER['HTTP_HOST'] ?? 'localhost';
    $script = $_SERVER['SCRIPT_NAME'] ?? '/install.php';
    $basePath = rtrim(str_replace('\\', '/', dirname($script)), '/');
    if ($basePath === '/' || $basePath === '\\' || $basePath === '.') {
        $basePath = '';
    }
    return $scheme . '://' . $host . $basePath;
}

function h(string $s): string
{
    return htmlspecialchars($s, ENT_QUOTES | ENT_SUBSTITUTE, 'UTF-8');
}

function buildConfigPhp(string $host, int $port, string $name, string $user, string $pass): string
{
    $export = static function (string $v): string {
        return var_export($v, true);
    };
    return <<<PHP
<?php
declare(strict_types=1);

return [
    'db' => [
        'host'    => {$export($host)},
        'port'    => {$port},
        'name'    => {$export($name)},
        'user'    => {$export($user)},
        'pass'    => {$export($pass)},
        'charset' => 'utf8mb4',
    ],
    'cors' => true,
    'family_id' => null,
    'export_timezone' => 'Europe/Bratislava',
];

PHP;
}

function runSqlFile(PDO $pdo, string $path): void
{
    $sql = file_get_contents($path);
    if ($sql === false || trim($sql) === '') {
        throw new RuntimeException('Prázdny alebo nečitateľný SQL súbor.');
    }
    // Odstráň komentáre riadkov -- ...
    $lines = preg_split('/\R/', $sql) ?: [];
    $buf = '';
    foreach ($lines as $line) {
        if (preg_match('/^\s*--/', $line)) {
            continue;
        }
        $buf .= $line . "\n";
    }
    $statements = array_filter(array_map('trim', explode(';', $buf)));
    foreach ($statements as $stmt) {
        if ($stmt === '') {
            continue;
        }
        $pdo->exec($stmt);
    }
}

function seedDefaults(PDO $pdo, int $familyId): void
{
    $insulins = [
        ['NovoRapid', 'bolus', '#E53935', 4.00, 1],
        ['Fiasp', 'bolus', '#FB8C00', 3.50, 2],
        ['Tresiba', 'basal', '#1E88E5', 24.00, 3],
        ['Lantus', 'basal', '#43A047', 24.00, 4],
    ];
    $is = $pdo->prepare(
        'INSERT INTO insulins (family_id, name, category, color_hex, duration_hours, sort_order)
         VALUES (?, ?, ?, ?, ?, ?)'
    );
    foreach ($insulins as $i) {
        $is->execute([$familyId, $i[0], $i[1], $i[2], $i[3], $i[4]]);
    }

    $scenes = [
        ['Raňajky', 'breakfast', 1],
        ['Desiata', 'snack', 2],
        ['Obed', 'lunch', 3],
        ['Olovrant', 'snack', 4],
        ['Večera', 'dinner', 5],
        ['Korekcia', 'correction', 6],
        ['Pred spaním', 'bedtime', 7],
    ];
    $ss = $pdo->prepare(
        'INSERT INTO scenes (family_id, name, icon_key, sort_order) VALUES (?, ?, ?, ?)'
    );
    foreach ($scenes as $s) {
        $ss->execute([$familyId, $s[0], $s[1], $s[2]]);
    }

    $sites = ['Brucho', 'Stehno', 'Rameno', 'Zadok', 'iPort'];
    $siteIns = $pdo->prepare(
        'INSERT INTO injection_sites (family_id, name, sort_order) VALUES (?, ?, ?)'
    );
    foreach ($sites as $i => $name) {
        $siteIns->execute([$familyId, $name, $i + 1]);
    }
}

/** @param array<string, mixed> $old */
function buildForm(array $old): string
{
    $v = static function (string $key, string $default = '') use ($old): string {
        return h((string) ($old[$key] ?? $default));
    };
    $checked = !isset($old['access_key']) || isset($old['with_defaults']) ? ' checked' : '';

    return <<<HTML
<form method="post" autocomplete="off">
  <fieldset>
    <legend>MySQL (z WebAdmin → Databázy)</legend>
    <label>Host <input name="db_host" value="{$v('db_host', 'localhost')}" required></label>
    <label>Port <input name="db_port" type="number" value="{$v('db_port', '3306')}"></label>
    <label>Názov DB <input name="db_name" value="{$v('db_name')}" placeholder="w12345_gluko" required></label>
    <label>Používateľ <input name="db_user" value="{$v('db_user')}" required></label>
    <label>Heslo <input name="db_pass" type="password" value="{$v('db_pass')}"></label>
  </fieldset>
  <fieldset>
    <legend>Rodina</legend>
    <label>Názov rodiny <input name="family_name" value="{$v('family_name', 'Naša rodina')}" required></label>
    <label>Prístupový kľúč (≥12 znakov)
      <input name="access_key" type="text" value="{$v('access_key')}" minlength="12" required
             placeholder="dlhy-nahodny-kluc-pre-rodinu">
    </label>
    <label>Profily (jeden riadok = jedno meno)
      <textarea name="profiles" rows="4">{$v('profiles', "Maroš\nVeronika\nMelinka")}</textarea>
    </label>
    <label class="check"><input type="checkbox" name="with_defaults" value="1"{$checked}>
      Vložiť predvolené inzulíny a scény (NovoRapid, Tresiba, Raňajky…)</label>
  </fieldset>
  <button type="submit">Nainštalovať</button>
</form>
HTML;
}

/**
 * @param list<string> $errors
 * @param array{family_id:int,family_name:string,api_base:string,access_key:string}|null $success
 */
function renderPage(string $title, ?string $body, array $errors, ?array $success, string $apiBase): void
{
    echo '<!DOCTYPE html><html lang="sk"><head><meta charset="utf-8">';
    echo '<meta name="viewport" content="width=device-width,initial-scale=1">';
    echo '<title>' . h($title) . '</title>';
    echo '<style>
      :root{--bg:#0f1419;--card:#1a2332;--text:#e8eef6;--muted:#9aabbc;--acc:#3d8bfd;--err:#ff6b6b;--ok:#3dd68c;}
      *{box-sizing:border-box}
      body{margin:0;font-family:system-ui,sans-serif;background:linear-gradient(160deg,#0f1419,#1a2838);color:var(--text);min-height:100vh;padding:1.5rem}
      main{max-width:560px;margin:0 auto;background:var(--card);padding:1.5rem 1.75rem;border-radius:12px;border:1px solid #2a3a4f}
      h1{font-size:1.35rem;margin:0 0 .75rem}
      p,li{color:var(--muted);line-height:1.5}
      code{background:#0d1218;padding:.1em .35em;border-radius:4px;color:#c8e0ff;font-size:.9em}
      .err{background:#3a1a1a;color:var(--err);padding:.75rem 1rem;border-radius:8px;margin:1rem 0}
      .ok{background:#143028;color:var(--ok);padding:.75rem 1rem;border-radius:8px;margin:1rem 0}
      fieldset{border:1px solid #2a3a4f;border-radius:8px;margin:1rem 0;padding:1rem}
      legend{padding:0 .4rem;color:#c8e0ff}
      label{display:block;margin:.6rem 0;font-size:.9rem;color:var(--muted)}
      input,textarea{display:block;width:100%;margin-top:.3rem;padding:.55rem .65rem;border-radius:6px;border:1px solid #334155;background:#0d1218;color:var(--text);font:inherit}
      label.check{display:flex;gap:.5rem;align-items:flex-start}
      label.check input{width:auto;margin-top:.2rem}
      button{margin-top:.5rem;background:var(--acc);color:#fff;border:0;padding:.7rem 1.2rem;border-radius:8px;font:inherit;font-weight:600;cursor:pointer;width:100%}
      button:hover{filter:brightness(1.08)}
      .warn{margin-top:1.25rem;padding:.75rem;border:1px dashed #f59e0b;border-radius:8px;color:#fbbf24}
      a{color:var(--acc)}
    </style></head><body><main>';
    echo '<h1>' . h($title) . '</h1>';

    foreach ($errors as $e) {
        echo '<div class="err">' . h($e) . '</div>';
    }

    if ($success !== null) {
        echo '<div class="ok"><strong>Hotovo.</strong> Rodina #' . (int) $success['family_id']
            . ' „' . h($success['family_name']) . '“ je pripravená.</div>';
        echo '<ol>';
        echo '<li><strong>Ihneď zmažte</strong> súbor <code>install.php</code> cez FTP / WebAdmin File Manager.</li>';
        echo '<li>V Android aplikácii nastavte:<br>URL: <code>' . h($success['api_base'])
            . '</code><br>Kľúč: <code>' . h($success['access_key']) . '</code></li>';
        echo '<li>Overenie API: <a href="' . h($success['api_base']) . '/api/v1/health">'
            . h($success['api_base']) . '/api/v1/health</a></li>';
        echo '</ol>';
        echo '<div class="warn">Kľúč si uložte — server ukladá len hash, plaintext neskôr neuvidíte.</div>';
    } else {
        echo '<p>Jednorazová inštalácia pre Websupport (a podobný shared hosting). '
            . 'Najprv vytvorte MySQL databázu v WebAdmin — tento formulár vytvorí tabuľky, '
            . '<code>config.php</code> a rodinu.</p>';
        if ($body !== null) {
            echo $body;
        }
        echo '<p style="margin-top:1.25rem;font-size:.85rem">Očakávaná API URL: <code>'
            . h($apiBase) . '</code></p>';
    }

    echo '</main></body></html>';
}
