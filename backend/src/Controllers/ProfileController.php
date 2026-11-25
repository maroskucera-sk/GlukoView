<?php
declare(strict_types=1);

final class ProfileController
{
    public static function list(): void
    {
        $family = Auth::requireFamily();
        $rows = Db::fetchAll(
            'SELECT * FROM profiles WHERE family_id = ? ORDER BY sort_order, id',
            [$family['id']]
        );
        Response::json(['profiles' => array_map([Mapper::class, 'profile'], $rows)]);
    }

    public static function create(): void
    {
        $family = Auth::requireFamily();
        $data = json_body();
        require_fields($data, ['name']);

        $name = trim((string) $data['name']);
        $color = optional_string($data['color_hex'] ?? '#4CAF50', 7) ?? '#4CAF50';
        if (!valid_color($color)) {
            Response::error('color_hex musí byť #RRGGBB.', 422);
        }
        $sort = (int) ($data['sort_order'] ?? 0);

        Db::exec(
            'INSERT INTO profiles (family_id, name, color_hex, sort_order) VALUES (?, ?, ?, ?)',
            [$family['id'], $name, $color, $sort]
        );

        $row = Db::fetchOne('SELECT * FROM profiles WHERE id = ?', [Db::lastId()]);
        Response::json(['profile' => Mapper::profile($row)], 201);
    }

    public static function update(array $params): void
    {
        $family = Auth::requireFamily();
        $id = (int) $params['id'];
        $row = Db::fetchOne(
            'SELECT * FROM profiles WHERE id = ? AND family_id = ?',
            [$id, $family['id']]
        );
        if ($row === null) {
            Response::error('Profil neexistuje.', 404);
        }

        $data = json_body();
        $name = array_key_exists('name', $data) ? trim((string) $data['name']) : $row['name'];
        $color = array_key_exists('color_hex', $data)
            ? (optional_string($data['color_hex'], 7) ?? $row['color_hex'])
            : $row['color_hex'];
        if (!valid_color($color)) {
            Response::error('color_hex musí byť #RRGGBB.', 422);
        }
        $sort = array_key_exists('sort_order', $data) ? (int) $data['sort_order'] : (int) $row['sort_order'];
        $active = array_key_exists('is_active', $data)
            ? bool_int($data['is_active'])
            : (int) $row['is_active'];

        Db::exec(
            'UPDATE profiles SET name = ?, color_hex = ?, sort_order = ?, is_active = ?
             WHERE id = ? AND family_id = ?',
            [$name, $color, $sort, $active, $id, $family['id']]
        );

        $updated = Db::fetchOne('SELECT * FROM profiles WHERE id = ?', [$id]);
        Response::json(['profile' => Mapper::profile($updated)]);
    }
}
