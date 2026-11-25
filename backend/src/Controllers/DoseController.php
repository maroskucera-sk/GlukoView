<?php
declare(strict_types=1);

final class DoseController
{
    private const SITES = ['abdomen', 'thigh', 'arm', 'buttock'];

    /** GET /api/v1/doses?from=&to=&limit= */
    public static function list(): void
    {
        $family = Auth::requireFamily();
        $fid = $family['id'];

        $from = isset($_GET['from']) ? parse_iso_datetime((string) $_GET['from']) : null;
        $to   = isset($_GET['to']) ? parse_iso_datetime((string) $_GET['to']) : null;
        $limit = min(500, max(1, (int) ($_GET['limit'] ?? 100)));

        $sql = 'SELECT * FROM dose_records WHERE family_id = ? AND deleted_at IS NULL';
        $params = [$fid];
        if ($from !== null) {
            $sql .= ' AND administered_at >= ?';
            $params[] = $from;
        }
        if ($to !== null) {
            $sql .= ' AND administered_at <= ?';
            $params[] = $to;
        }
        $sql .= ' ORDER BY administered_at DESC LIMIT ' . $limit;

        $rows = Db::fetchAll($sql, $params);
        Response::json(['doses' => array_map([Mapper::class, 'dose'], $rows)]);
    }

    /** GET /api/v1/doses/{uuid} */
    public static function get(array $params): void
    {
        $family = Auth::requireFamily();
        $row = self::find($family['id'], $params['uuid'], includeDeleted: true);
        if ($row === null) {
            Response::error('Záznam neexistuje.', 404);
        }
        Response::json(['dose' => Mapper::dose($row)]);
    }

    /**
     * POST /api/v1/doses — vytvorenie / upsert podľa uuid (offline sync)
     * Body: uuid, profile_id, insulin_id, units, administered_at, …
     */
    public static function create(): void
    {
        $family = Auth::requireFamily();
        $fid = $family['id'];
        $data = json_body();
        require_fields($data, ['uuid', 'profile_id', 'insulin_id', 'units', 'administered_at']);

        $uuid = (string) $data['uuid'];
        if (!valid_uuid($uuid)) {
            Response::error('Neplatné uuid.', 422);
        }

        $profileId = (int) $data['profile_id'];
        $insulinId = (int) $data['insulin_id'];
        $sceneId = isset($data['scene_id']) && $data['scene_id'] !== null ? (int) $data['scene_id'] : null;
        $units = optional_float($data['units']);
        if ($units === null || $units <= 0) {
            Response::error('units musí byť > 0.', 422);
        }
        $administeredAt = parse_iso_datetime((string) $data['administered_at']);

        Auth::assertProfile($fid, $profileId);
        Auth::assertInsulin($fid, $insulinId);
        Auth::assertScene($fid, $sceneId);

        $site = optional_string($data['injection_site'] ?? null, 20);
        if ($site !== null && !in_array($site, self::SITES, true)) {
            Response::error('Neplatné injection_site.', 422);
        }

        $fields = self::optionalDoseFields($data);
        $existing = self::find($fid, $uuid, includeDeleted: true);

        $warnings = self::duplicateWarnings($fid, $insulinId, $administeredAt, $existing['uuid'] ?? null);

        if ($existing !== null) {
            // Idempotentný upsert — aktualizácia existujúceho (vrátane undelete)
            Db::exec(
                'UPDATE dose_records SET
                    profile_id = ?, insulin_id = ?, scene_id = ?, units = ?, administered_at = ?,
                    glucose_value = ?, glucose_unit = ?, glucose_trend = ?,
                    carbs_sj = ?, carbs_grams = ?, injection_site = ?, note = ?,
                    deleted_at = NULL
                 WHERE family_id = ? AND uuid = ?',
                [
                    $profileId, $insulinId, $sceneId, $units, $administeredAt,
                    $fields['glucose_value'], $fields['glucose_unit'], $fields['glucose_trend'],
                    $fields['carbs_sj'], $fields['carbs_grams'], $site, $fields['note'],
                    $fid, $uuid,
                ]
            );
            $row = self::find($fid, $uuid);
            Response::json(['dose' => Mapper::dose($row), 'warnings' => $warnings, 'upserted' => true]);
        }

        Db::exec(
            'INSERT INTO dose_records (
                family_id, uuid, profile_id, insulin_id, scene_id, units, administered_at,
                glucose_value, glucose_unit, glucose_trend, carbs_sj, carbs_grams,
                injection_site, note
             ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)',
            [
                $fid, $uuid, $profileId, $insulinId, $sceneId, $units, $administeredAt,
                $fields['glucose_value'], $fields['glucose_unit'], $fields['glucose_trend'],
                $fields['carbs_sj'], $fields['carbs_grams'], $site, $fields['note'],
            ]
        );

        $row = self::find($fid, $uuid);
        Response::json(['dose' => Mapper::dose($row), 'warnings' => $warnings], 201);
    }

    /** PATCH /api/v1/doses/{uuid} */
    public static function update(array $params): void
    {
        $family = Auth::requireFamily();
        $fid = $family['id'];
        $uuid = $params['uuid'];
        $row = self::find($fid, $uuid);
        if ($row === null) {
            Response::error('Záznam neexistuje.', 404);
        }

        $data = json_body();
        $profileId = isset($data['profile_id']) ? (int) $data['profile_id'] : (int) $row['profile_id'];
        $insulinId = isset($data['insulin_id']) ? (int) $data['insulin_id'] : (int) $row['insulin_id'];
        $sceneId = array_key_exists('scene_id', $data)
            ? ($data['scene_id'] !== null ? (int) $data['scene_id'] : null)
            : ($row['scene_id'] !== null ? (int) $row['scene_id'] : null);

        $units = array_key_exists('units', $data) ? optional_float($data['units']) : (float) $row['units'];
        if ($units === null || $units <= 0) {
            Response::error('units musí byť > 0.', 422);
        }

        $administeredAt = array_key_exists('administered_at', $data)
            ? parse_iso_datetime((string) $data['administered_at'])
            : $row['administered_at'];

        Auth::assertProfile($fid, $profileId);
        Auth::assertInsulin($fid, $insulinId, allowArchived: true);
        Auth::assertScene($fid, $sceneId, allowArchived: true);

        $site = array_key_exists('injection_site', $data)
            ? optional_string($data['injection_site'], 20)
            : $row['injection_site'];
        if ($site !== null && !in_array($site, self::SITES, true)) {
            Response::error('Neplatné injection_site.', 422);
        }

        $merged = array_merge($row, $data);
        $fields = self::optionalDoseFields($merged);

        Db::exec(
            'UPDATE dose_records SET
                profile_id = ?, insulin_id = ?, scene_id = ?, units = ?, administered_at = ?,
                glucose_value = ?, glucose_unit = ?, glucose_trend = ?,
                carbs_sj = ?, carbs_grams = ?, injection_site = ?, note = ?
             WHERE family_id = ? AND uuid = ? AND deleted_at IS NULL',
            [
                $profileId, $insulinId, $sceneId, $units, $administeredAt,
                $fields['glucose_value'], $fields['glucose_unit'], $fields['glucose_trend'],
                $fields['carbs_sj'], $fields['carbs_grams'], $site, $fields['note'],
                $fid, $uuid,
            ]
        );

        $updated = self::find($fid, $uuid);
        Response::json(['dose' => Mapper::dose($updated)]);
    }

    /** DELETE /api/v1/doses/{uuid} — soft delete */
    public static function delete(array $params): void
    {
        $family = Auth::requireFamily();
        $fid = $family['id'];
        $uuid = $params['uuid'];
        $row = self::find($fid, $uuid);
        if ($row === null) {
            Response::error('Záznam neexistuje.', 404);
        }

        Db::exec(
            'UPDATE dose_records SET deleted_at = UTC_TIMESTAMP(3) WHERE family_id = ? AND uuid = ?',
            [$fid, $uuid]
        );
        Response::noContent();
    }

    private static function find(int $familyId, string $uuid, bool $includeDeleted = false): ?array
    {
        $sql = 'SELECT * FROM dose_records WHERE family_id = ? AND uuid = ?';
        if (!$includeDeleted) {
            $sql .= ' AND deleted_at IS NULL';
        }
        return Db::fetchOne($sql, [$familyId, $uuid]);
    }

    /** @return array{glucose_value: ?float, glucose_unit: ?string, glucose_trend: ?string, carbs_sj: ?float, carbs_grams: ?float, note: ?string} */
    private static function optionalDoseFields(array $data): array
    {
        $gUnit = optional_string($data['glucose_unit'] ?? null, 8);
        if ($gUnit !== null && !in_array($gUnit, ['mmol', 'mgdl'], true)) {
            Response::error('glucose_unit musí byť mmol alebo mgdl.', 422);
        }

        return [
            'glucose_value' => optional_float($data['glucose_value'] ?? null),
            'glucose_unit'  => $gUnit,
            'glucose_trend' => optional_string($data['glucose_trend'] ?? null, 16),
            'carbs_sj'      => optional_float($data['carbs_sj'] ?? null),
            'carbs_grams'   => optional_float($data['carbs_grams'] ?? null),
            'note'          => optional_string($data['note'] ?? null, 280),
        ];
    }

    /** @return list<array{code: string, message: string, recent_dose?: array}> */
    private static function duplicateWarnings(
        int $familyId,
        int $insulinId,
        string $administeredAt,
        ?string $excludeUuid
    ): array {
        $insulin = Db::fetchOne(
            'SELECT category FROM insulins WHERE id = ? AND family_id = ?',
            [$insulinId, $familyId]
        );
        if ($insulin === null || $insulin['category'] !== 'bolus') {
            return [];
        }

        $settings = Db::fetchOne(
            'SELECT duplicate_bolus_minutes FROM family_settings WHERE family_id = ?',
            [$familyId]
        );
        $minutes = (int) ($settings['duplicate_bolus_minutes'] ?? 20);
        if ($minutes <= 0) {
            return [];
        }

        $sql = 'SELECT d.*, i.name AS insulin_name, p.name AS profile_name
                FROM dose_records d
                JOIN insulins i ON i.id = d.insulin_id
                JOIN profiles p ON p.id = d.profile_id
                WHERE d.family_id = ?
                  AND d.deleted_at IS NULL
                  AND i.category = \'bolus\'
                  AND ABS(TIMESTAMPDIFF(SECOND, d.administered_at, ?)) < ?
                  AND d.uuid <> COALESCE(?, \'\')
                ORDER BY d.administered_at DESC
                LIMIT 1';
        $recent = Db::fetchOne($sql, [$familyId, $administeredAt, $minutes * 60, $excludeUuid ?? '']);
        if ($recent === null) {
            return [];
        }

        return [[
            'code' => 'duplicate_bolus',
            'message' => sprintf(
                'Nedávny bolus: %sj %s pred %s (podal: %s).',
                rtrim(rtrim(number_format((float) $recent['units'], 2, '.', ''), '0'), '.'),
                $recent['insulin_name'],
                format_dt($recent['administered_at']),
                $recent['profile_name']
            ),
            'recent_dose' => Mapper::dose($recent),
        ]];
    }
}
