<?php
declare(strict_types=1);

/**
 * Jednorazové / idempotentné úpravy schémy pre shared hosting (Websupport).
 * Volá sa pri štarte API — bezpečné opakovať.
 */
final class SchemaMigrator
{
    public static function run(PDO $pdo): void
    {
        self::ensureInjectionSitesTable($pdo);
        self::widenDoseInjectionSite($pdo);
        self::ensureCurrentInjectionSiteColumn($pdo);
        self::seedDefaultSitesForFamilies($pdo);
    }

    private static function ensureInjectionSitesTable(PDO $pdo): void
    {
        $pdo->exec(
            "CREATE TABLE IF NOT EXISTS injection_sites (
              id            BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
              family_id     BIGINT UNSIGNED NOT NULL,
              name          VARCHAR(80)     NOT NULL,
              sort_order    SMALLINT        NOT NULL DEFAULT 0,
              is_archived   TINYINT(1)      NOT NULL DEFAULT 0,
              created_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3),
              updated_at    DATETIME(3)     NOT NULL DEFAULT CURRENT_TIMESTAMP(3)
                              ON UPDATE CURRENT_TIMESTAMP(3),
              PRIMARY KEY (id),
              KEY idx_injection_sites_family (family_id, is_archived, sort_order),
              CONSTRAINT fk_injection_sites_family
                FOREIGN KEY (family_id) REFERENCES families (id)
                ON DELETE CASCADE
            ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci"
        );
    }

    private static function widenDoseInjectionSite(PDO $pdo): void
    {
        $col = self::columnType($pdo, 'dose_records', 'injection_site');
        if ($col === null) {
            return;
        }
        if (stripos($col, 'enum') !== false || stripos($col, 'varchar') === false) {
            $pdo->exec('ALTER TABLE dose_records MODIFY injection_site VARCHAR(80) NULL');
        }
    }

    private static function ensureCurrentInjectionSiteColumn(PDO $pdo): void
    {
        if (self::columnType($pdo, 'family_settings', 'current_injection_site') !== null) {
            return;
        }
        $pdo->exec(
            'ALTER TABLE family_settings
             ADD COLUMN current_injection_site VARCHAR(80) NULL AFTER duplicate_bolus_minutes'
        );
    }

    private static function seedDefaultSitesForFamilies(PDO $pdo): void
    {
        $families = $pdo->query('SELECT id FROM families')->fetchAll(PDO::FETCH_COLUMN);
        if ($families === false) {
            return;
        }
        $defaults = ['Brucho', 'Stehno', 'Rameno', 'Zadok', 'iPort'];
        $countStmt = $pdo->prepare('SELECT COUNT(*) FROM injection_sites WHERE family_id = ?');
        $ins = $pdo->prepare(
            'INSERT INTO injection_sites (family_id, name, sort_order) VALUES (?, ?, ?)'
        );
        foreach ($families as $fid) {
            $countStmt->execute([(int) $fid]);
            if ((int) $countStmt->fetchColumn() > 0) {
                continue;
            }
            foreach ($defaults as $i => $name) {
                $ins->execute([(int) $fid, $name, $i + 1]);
            }
        }
    }

    private static function columnType(PDO $pdo, string $table, string $column): ?string
    {
        $stmt = $pdo->prepare(
            'SELECT COLUMN_TYPE FROM information_schema.COLUMNS
             WHERE TABLE_SCHEMA = DATABASE() AND TABLE_NAME = ? AND COLUMN_NAME = ?'
        );
        $stmt->execute([$table, $column]);
        $type = $stmt->fetchColumn();
        return $type === false ? null : (string) $type;
    }
}
