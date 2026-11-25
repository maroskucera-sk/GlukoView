<?php
declare(strict_types=1);

final class Auth
{
    /**
     * Overí Bearer token voči hashom v families.
     * @return array{id: int|string, name: string}
     */
    public static function requireFamily(): array
    {
        $header = $_SERVER['HTTP_AUTHORIZATION']
            ?? $_SERVER['REDIRECT_HTTP_AUTHORIZATION']
            ?? '';

        if (!preg_match('/^Bearer\s+(\S+)$/i', $header, $m)) {
            Response::error('Chýba Authorization: Bearer <rodinný_kľúč>.', 401);
        }
        $key = $m[1];

        /** @var array $config */
        global $config;
        $forcedId = $config['family_id'] ?? null;

        if ($forcedId !== null) {
            $family = Db::fetchOne(
                'SELECT id, name, access_key_hash FROM families WHERE id = ?',
                [(int) $forcedId]
            );
            if ($family === null || !password_verify($key, $family['access_key_hash'])) {
                Response::error('Neplatný prístupový kľúč.', 401);
            }
            return ['id' => (int) $family['id'], 'name' => $family['name']];
        }

        // Typicky 1 rodina na hosting — overíme postupne (bcrypt).
        $families = Db::fetchAll('SELECT id, name, access_key_hash FROM families LIMIT 50');
        foreach ($families as $family) {
            if (password_verify($key, $family['access_key_hash'])) {
                return ['id' => (int) $family['id'], 'name' => $family['name']];
            }
        }

        Response::error('Neplatný prístupový kľúč.', 401);
    }

    /** Overí, že profile_id patrí do rodiny. */
    public static function assertProfile(int $familyId, int $profileId): void
    {
        $row = Db::fetchOne(
            'SELECT id FROM profiles WHERE id = ? AND family_id = ? AND is_active = 1',
            [$profileId, $familyId]
        );
        if ($row === null) {
            Response::error('Profil neexistuje alebo nepatrí do tejto rodiny.', 422);
        }
    }

    public static function assertInsulin(int $familyId, int $insulinId, bool $allowArchived = false): void
    {
        $sql = 'SELECT id FROM insulins WHERE id = ? AND family_id = ?';
        if (!$allowArchived) {
            $sql .= ' AND is_archived = 0';
        }
        $row = Db::fetchOne($sql, [$insulinId, $familyId]);
        if ($row === null) {
            Response::error('Inzulín neexistuje alebo je archivovaný.', 422);
        }
    }

    public static function assertScene(int $familyId, ?int $sceneId, bool $allowArchived = false): void
    {
        if ($sceneId === null) {
            return;
        }
        $sql = 'SELECT id FROM scenes WHERE id = ? AND family_id = ?';
        if (!$allowArchived) {
            $sql .= ' AND is_archived = 0';
        }
        $row = Db::fetchOne($sql, [$sceneId, $familyId]);
        if ($row === null) {
            Response::error('Scéna neexistuje alebo je archivovaná.', 422);
        }
    }
}
