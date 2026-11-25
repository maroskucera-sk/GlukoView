<?php
declare(strict_types=1);

final class SettingsController
{
    public static function get(): void
    {
        $family = Auth::requireFamily();
        $row = Db::fetchOne('SELECT * FROM family_settings WHERE family_id = ?', [$family['id']]);
        if ($row === null) {
            // Lazy create
            Db::exec(
                'INSERT INTO family_settings (family_id) VALUES (?)',
                [$family['id']]
            );
            $row = Db::fetchOne('SELECT * FROM family_settings WHERE family_id = ?', [$family['id']]);
        }
        Response::json(['settings' => Mapper::settings($row)]);
    }

    /** PUT /api/v1/settings — čiastočná alebo plná aktualizácia */
    public static function put(): void
    {
        $family = Auth::requireFamily();
        $fid = $family['id'];
        $data = json_body();

        $existing = Db::fetchOne('SELECT * FROM family_settings WHERE family_id = ?', [$fid]);
        if ($existing === null) {
            Db::exec('INSERT INTO family_settings (family_id) VALUES (?)', [$fid]);
            $existing = Db::fetchOne('SELECT * FROM family_settings WHERE family_id = ?', [$fid]);
        }

        $nsUrl = array_key_exists('nightscout_url', $data)
            ? optional_string($data['nightscout_url'], 512)
            : $existing['nightscout_url'];
        if ($nsUrl !== null && !preg_match('#^https?://#i', $nsUrl)) {
            Response::error('nightscout_url musí začínať http:// alebo https://.', 422);
        }

        $nsToken = array_key_exists('nightscout_token', $data)
            ? optional_string($data['nightscout_token'], 255)
            : $existing['nightscout_token'];

        $basal = array_key_exists('basal_reminder_time', $data)
            ? optional_string($data['basal_reminder_time'], 5)
            : $existing['basal_reminder_time'];
        if (!valid_hhmm($basal)) {
            Response::error('basal_reminder_time musí byť HH:MM.', 422);
        }

        $gUnit = array_key_exists('glucose_unit', $data)
            ? (string) $data['glucose_unit']
            : $existing['glucose_unit'];
        if (!in_array($gUnit, ['mmol', 'mgdl'], true)) {
            Response::error('glucose_unit musí byť mmol alebo mgdl.', 422);
        }

        $dup = array_key_exists('duplicate_bolus_minutes', $data)
            ? (int) $data['duplicate_bolus_minutes']
            : (int) $existing['duplicate_bolus_minutes'];
        if ($dup < 0 || $dup > 180) {
            Response::error('duplicate_bolus_minutes musí byť 0–180.', 422);
        }

        $currentSite = array_key_exists('current_injection_site', $data)
            ? optional_string($data['current_injection_site'], 80)
            : ($existing['current_injection_site'] ?? null);

        Db::exec(
            'UPDATE family_settings SET
                nightscout_url = ?, nightscout_token = ?, basal_reminder_time = ?,
                glucose_unit = ?, duplicate_bolus_minutes = ?, current_injection_site = ?
             WHERE family_id = ?',
            [$nsUrl, $nsToken, $basal, $gUnit, $dup, $currentSite, $fid]
        );

        $row = Db::fetchOne('SELECT * FROM family_settings WHERE family_id = ?', [$fid]);
        Response::json(['settings' => Mapper::settings($row)]);
    }
}
