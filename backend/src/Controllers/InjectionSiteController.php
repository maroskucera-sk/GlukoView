<?php
declare(strict_types=1);

final class InjectionSiteController
{
    public static function list(): void
    {
        $family = Auth::requireFamily();
        $includeArchived = filter_var($_GET['include_archived'] ?? false, FILTER_VALIDATE_BOOLEAN);
        $sql = 'SELECT * FROM injection_sites WHERE family_id = ?';
        if (!$includeArchived) {
            $sql .= ' AND is_archived = 0';
        }
        $sql .= ' ORDER BY sort_order, id';
        $rows = Db::fetchAll($sql, [$family['id']]);
        Response::json(['injection_sites' => array_map([Mapper::class, 'injectionSite'], $rows)]);
    }

    public static function create(): void
    {
        $family = Auth::requireFamily();
        $data = json_body();
        require_fields($data, ['name']);

        $name = trim((string) $data['name']);
        if ($name === '' || mb_strlen($name) > 80) {
            Response::error('Názov miesta musí mať 1–80 znakov.', 422);
        }
        $sort = (int) ($data['sort_order'] ?? 0);

        Db::exec(
            'INSERT INTO injection_sites (family_id, name, sort_order) VALUES (?, ?, ?)',
            [$family['id'], $name, $sort]
        );

        $row = Db::fetchOne('SELECT * FROM injection_sites WHERE id = ?', [Db::lastId()]);
        Response::json(['injection_site' => Mapper::injectionSite($row)], 201);
    }

    public static function update(array $params): void
    {
        $family = Auth::requireFamily();
        $id = (int) $params['id'];
        $row = Db::fetchOne(
            'SELECT * FROM injection_sites WHERE id = ? AND family_id = ?',
            [$id, $family['id']]
        );
        if ($row === null) {
            Response::error('Miesto vpichu neexistuje.', 404);
        }

        $data = json_body();
        $name = array_key_exists('name', $data) ? trim((string) $data['name']) : $row['name'];
        if ($name === '' || mb_strlen($name) > 80) {
            Response::error('Názov miesta musí mať 1–80 znakov.', 422);
        }
        $sort = array_key_exists('sort_order', $data) ? (int) $data['sort_order'] : (int) $row['sort_order'];
        $archived = array_key_exists('is_archived', $data)
            ? bool_int($data['is_archived'])
            : (int) $row['is_archived'];

        Db::exec(
            'UPDATE injection_sites SET name = ?, sort_order = ?, is_archived = ?
             WHERE id = ? AND family_id = ?',
            [$name, $sort, $archived, $id, $family['id']]
        );

        $updated = Db::fetchOne('SELECT * FROM injection_sites WHERE id = ?', [$id]);
        Response::json(['injection_site' => Mapper::injectionSite($updated)]);
    }
}
