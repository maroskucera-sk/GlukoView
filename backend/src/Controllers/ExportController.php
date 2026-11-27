<?php
declare(strict_types=1);

final class ExportController
{
    /**
     * GET /api/v1/export?from=ISO&to=ISO&format=csv
     * PDF tabuľku rieši Android klient z rovnakých dát; server poskytuje CSV.
     */
    public static function export(): void
    {
        $family = Auth::requireFamily();
        $fid = $family['id'];

        $from = isset($_GET['from']) ? parse_iso_datetime((string) $_GET['from']) : null;
        $to   = isset($_GET['to']) ? parse_iso_datetime((string) $_GET['to']) : null;
        if ($from === null || $to === null) {
            Response::error('Parametre from a to (ISO8601) sú povinné.', 422);
        }

        $format = strtolower((string) ($_GET['format'] ?? 'csv'));
        if ($format !== 'csv') {
            Response::error('Podporovaný formát: csv (PDF generuje aplikácia).', 422);
        }

        /** @var array $config */
        global $config;
        $tzName = $config['export_timezone'] ?? 'Europe/Bratislava';
        try {
            $tz = new DateTimeZone($tzName);
        } catch (Exception) {
            $tz = new DateTimeZone('UTC');
        }

        $rows = Db::fetchAll(
            'SELECT d.*, p.name AS profile_name, i.name AS insulin_name, i.category AS insulin_category,
                    s.name AS scene_name
             FROM dose_records d
             JOIN profiles p ON p.id = d.profile_id
             JOIN insulins i ON i.id = d.insulin_id
             LEFT JOIN scenes s ON s.id = d.scene_id
             WHERE d.family_id = ?
               AND d.deleted_at IS NULL
               AND d.administered_at >= ?
               AND d.administered_at <= ?
             ORDER BY d.administered_at ASC',
            [$fid, $from, $to]
        );

        $siteMap = [
            'abdomen' => 'Brucho',
            'thigh'   => 'Stehno',
            'arm'     => 'Rameno',
            'buttock' => 'Zadok',
        ];

        $out = fopen('php://temp', 'r+');
        fputcsv($out, [
            'Datum_cas',
            'Profil',
            'Inzulin',
            'Kategoria',
            'Jednotky',
            'Scena',
            'Glykemia',
            'Jednotka_glykemie',
            'Trend',
            'SJ',
            'Sacharidy_g',
            'Miesto_vpichu',
            'Poznamka',
        ], ';');

        foreach ($rows as $r) {
            $local = (new DateTimeImmutable($r['administered_at'], new DateTimeZone('UTC')))
                ->setTimezone($tz)
                ->format('Y-m-d H:i');

            fputcsv($out, [
                $local,
                $r['profile_name'],
                $r['insulin_name'],
                $r['insulin_category'] === 'bolus' ? 'rychly' : 'bazalny',
                $r['units'],
                $r['scene_name'] ?? '',
                $r['glucose_value'] ?? '',
                $r['glucose_unit'] ?? '',
                $r['glucose_trend'] ?? '',
                $r['carbs_sj'] ?? '',
                $r['carbs_grams'] ?? '',
                $siteMap[$r['injection_site'] ?? ''] ?? ($r['injection_site'] ?? ''),
                $r['note'] ?? '',
            ], ';');
        }

        rewind($out);
        $csv = stream_get_contents($out);
        fclose($out);

        $filename = sprintf(
            'glukoview-inzulin-%s-%s.csv',
            (new DateTimeImmutable($from, new DateTimeZone('UTC')))->format('Ymd'),
            (new DateTimeImmutable($to, new DateTimeZone('UTC')))->format('Ymd')
        );

        Response::csv($filename, $csv !== false ? $csv : '');
    }
}
