<?php
declare(strict_types=1);

final class AuthController
{
    /** POST /api/v1/auth/verify — overí kľúč, vráti rodinu + profily + nastavenia */
    public static function verify(): void
    {
        $family = Auth::requireFamily();
        $fid = $family['id'];

        $profiles = array_map(
            [Mapper::class, 'profile'],
            Db::fetchAll(
                'SELECT * FROM profiles WHERE family_id = ? AND is_active = 1 ORDER BY sort_order, id',
                [$fid]
            )
        );

        $settingsRow = Db::fetchOne('SELECT * FROM family_settings WHERE family_id = ?', [$fid]);
        $settings = $settingsRow ? Mapper::settings($settingsRow) : null;

        Response::json([
            'family' => [
                'id'   => $fid,
                'name' => $family['name'],
            ],
            'profiles' => $profiles,
            'settings' => $settings,
            'server_time' => (new DateTimeImmutable('now', new DateTimeZone('UTC')))->format('Y-m-d\TH:i:s.v\Z'),
        ]);
    }
}
