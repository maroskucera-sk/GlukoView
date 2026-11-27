<?php
declare(strict_types=1);

final class SyncController
{
    /**
     * GET /api/v1/sync?since=ISO8601
     * Bez since → plný snapshot (bez soft-deleted dávok starších… vrátime aj deleted ak since).
     */
    public static function sync(): void
    {
        $family = Auth::requireFamily();
        $fid = $family['id'];
        $sinceRaw = $_GET['since'] ?? null;
        $since = $sinceRaw ? parse_iso_datetime((string) $sinceRaw) : null;

        if ($since === null) {
            $profiles = Db::fetchAll(
                'SELECT * FROM profiles WHERE family_id = ? ORDER BY sort_order, id',
                [$fid]
            );
            $insulins = Db::fetchAll(
                'SELECT * FROM insulins WHERE family_id = ? ORDER BY sort_order, id',
                [$fid]
            );
            $scenes = Db::fetchAll(
                'SELECT * FROM scenes WHERE family_id = ? ORDER BY sort_order, id',
                [$fid]
            );
            $injectionSites = Db::fetchAll(
                'SELECT * FROM injection_sites WHERE family_id = ? ORDER BY sort_order, id',
                [$fid]
            );
            $doses = Db::fetchAll(
                'SELECT * FROM dose_records WHERE family_id = ? AND deleted_at IS NULL
                 ORDER BY administered_at DESC LIMIT 500',
                [$fid]
            );
        } else {
            $profiles = Db::fetchAll(
                'SELECT * FROM profiles WHERE family_id = ? AND updated_at > ? ORDER BY updated_at',
                [$fid, $since]
            );
            $insulins = Db::fetchAll(
                'SELECT * FROM insulins WHERE family_id = ? AND updated_at > ? ORDER BY updated_at',
                [$fid, $since]
            );
            $scenes = Db::fetchAll(
                'SELECT * FROM scenes WHERE family_id = ? AND updated_at > ? ORDER BY updated_at',
                [$fid, $since]
            );
            $injectionSites = Db::fetchAll(
                'SELECT * FROM injection_sites WHERE family_id = ? AND updated_at > ? ORDER BY updated_at',
                [$fid, $since]
            );
            $doses = Db::fetchAll(
                'SELECT * FROM dose_records WHERE family_id = ? AND updated_at > ?
                 ORDER BY updated_at LIMIT 1000',
                [$fid, $since]
            );
        }

        $settingsRow = Db::fetchOne('SELECT * FROM family_settings WHERE family_id = ?', [$fid]);
        if ($since !== null && $settingsRow !== null) {
            $settingsUpdated = $settingsRow['updated_at'];
            if ($settingsUpdated <= $since) {
                $settingsRow = null; // bez zmeny
            }
        }

        Response::json([
            'server_time' => (new DateTimeImmutable('now', new DateTimeZone('UTC')))->format('Y-m-d\TH:i:s.v\Z'),
            'since'       => $sinceRaw,
            'profiles'    => array_map([Mapper::class, 'profile'], $profiles),
            'insulins'    => array_map([Mapper::class, 'insulin'], $insulins),
            'scenes'           => array_map([Mapper::class, 'scene'], $scenes),
            'injection_sites'  => array_map([Mapper::class, 'injectionSite'], $injectionSites),
            'doses'            => array_map([Mapper::class, 'dose'], $doses),
            'settings'         => $settingsRow ? Mapper::settings($settingsRow) : null,
        ]);
    }
}
