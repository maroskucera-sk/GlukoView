#!/usr/bin/env php
<?php
/**
 * Vytvorí novú rodinu (alebo obnoví hash kľúča) z CLI.
 *
 * Použitie:
 *   php tools/create_family.php "Naša rodina" "silny-tajny-kluc"
 *   php tools/create_family.php "Naša rodina" "silny-tajny-kluc" --with-defaults
 *
 * Vyžaduje: config/config.php a importovanú schému (bez nutnosti seedu).
 */
declare(strict_types=1);

if (PHP_SAPI !== 'cli') {
    fwrite(STDERR, "Spúšťajte len z príkazového riadku.\n");
    exit(1);
}

$configPath = dirname(__DIR__) . '/config/config.php';
if (!is_file($configPath)) {
    fwrite(STDERR, "Chýba config/config.php\n");
    exit(1);
}

/** @var array $config */
$config = require $configPath;

$name = $argv[1] ?? null;
$key  = $argv[2] ?? null;
$withDefaults = in_array('--with-defaults', $argv, true);

if ($name === null || $key === null) {
    fwrite(STDERR, "Použitie: php tools/create_family.php \"Názov rodiny\" \"prístupový-kľúč\" [--with-defaults]\n");
    exit(1);
}

if (strlen($key) < 12) {
    fwrite(STDERR, "Kľúč by mal mať aspoň 12 znakov.\n");
    exit(1);
}

$db = $config['db'];
$dsn = sprintf(
    'mysql:host=%s;port=%d;dbname=%s;charset=%s',
    $db['host'],
    (int) $db['port'],
    $db['name'],
    $db['charset']
);

$pdo = new PDO($dsn, $db['user'], $db['pass'], [
    PDO::ATTR_ERRMODE => PDO::ERRMODE_EXCEPTION,
]);

$hash = password_hash($key, PASSWORD_ARGON2ID);
if ($hash === false) {
    // Fallback na hostingoch bez Argon2
    $hash = password_hash($key, PASSWORD_BCRYPT);
}

$pdo->beginTransaction();
try {
    $stmt = $pdo->prepare('INSERT INTO families (name, access_key_hash) VALUES (?, ?)');
    $stmt->execute([$name, $hash]);
    $familyId = (int) $pdo->lastInsertId();

    $pdo->prepare('INSERT INTO family_settings (family_id, basal_reminder_time, glucose_unit, duplicate_bolus_minutes)
                   VALUES (?, ?, ?, ?)')
        ->execute([$familyId, '21:30', 'mmol', 20]);

    if ($withDefaults) {
        $profiles = [
            ['Maroš', '#1976D2', 1],
            ['Veronika', '#C2185B', 2],
            ['Melinka', '#7B1FA2', 3],
        ];
        $ps = $pdo->prepare('INSERT INTO profiles (family_id, name, color_hex, sort_order) VALUES (?, ?, ?, ?)');
        foreach ($profiles as $p) {
            $ps->execute([$familyId, $p[0], $p[1], $p[2]]);
        }

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

    $pdo->commit();
} catch (Throwable $e) {
    $pdo->rollBack();
    fwrite(STDERR, 'Chyba: ' . $e->getMessage() . "\n");
    exit(1);
}

echo "OK — rodina #{$familyId} „{$name}“ vytvorená.\n";
echo "V aplikácii zadajte URL API a prístupový kľúč (plaintext, ktorý ste práve zadali).\n";
if ($withDefaults) {
    echo "Predvolené profily, inzulíny a scény boli vložené.\n";
}
