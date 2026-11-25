<?php
declare(strict_types=1);

final class SceneController
{
    public static function list(): void
    {
        $family = Auth::requireFamily();
        $includeArchived = filter_var($_GET['include_archived'] ?? false, FILTER_VALIDATE_BOOLEAN);
        $sql = 'SELECT * FROM scenes WHERE family_id = ?';
        if (!$includeArchived) {
            $sql .= ' AND is_archived = 0';
        }
        $sql .= ' ORDER BY sort_order, id';
        $rows = Db::fetchAll($sql, [$family['id']]);
        Response::json(['scenes' => array_map([Mapper::class, 'scene'], $rows)]);
    }

    public static function create(): void
    {
        $family = Auth::requireFamily();
        $data = json_body();
        require_fields($data, ['name']);

        $name = trim((string) $data['name']);
        $icon = optional_string($data['icon_key'] ?? 'meal', 40) ?? 'meal';
        $sort = (int) ($data['sort_order'] ?? 0);

        Db::exec(
            'INSERT INTO scenes (family_id, name, icon_key, sort_order) VALUES (?, ?, ?, ?)',
            [$family['id'], $name, $icon, $sort]
        );

        $row = Db::fetchOne('SELECT * FROM scenes WHERE id = ?', [Db::lastId()]);
        Response::json(['scene' => Mapper::scene($row)], 201);
    }

    public static function update(array $params): void
    {
        $family = Auth::requireFamily();
        $id = (int) $params['id'];
        $row = Db::fetchOne(
            'SELECT * FROM scenes WHERE id = ? AND family_id = ?',
            [$id, $family['id']]
        );
        if ($row === null) {
            Response::error('Scéna neexistuje.', 404);
        }

        $data = json_body();
        $name = array_key_exists('name', $data) ? trim((string) $data['name']) : $row['name'];
        $icon = array_key_exists('icon_key', $data)
            ? (optional_string($data['icon_key'], 40) ?? $row['icon_key'])
            : $row['icon_key'];
        $sort = array_key_exists('sort_order', $data) ? (int) $data['sort_order'] : (int) $row['sort_order'];
        $archived = array_key_exists('is_archived', $data)
            ? bool_int($data['is_archived'])
            : (int) $row['is_archived'];

        Db::exec(
            'UPDATE scenes SET name = ?, icon_key = ?, sort_order = ?, is_archived = ?
             WHERE id = ? AND family_id = ?',
            [$name, $icon, $sort, $archived, $id, $family['id']]
        );

        $updated = Db::fetchOne('SELECT * FROM scenes WHERE id = ?', [$id]);
        Response::json(['scene' => Mapper::scene($updated)]);
    }
}
