<?php
declare(strict_types=1);

final class InsulinController
{
    public static function list(): void
    {
        $family = Auth::requireFamily();
        $includeArchived = filter_var($_GET['include_archived'] ?? false, FILTER_VALIDATE_BOOLEAN);
        $sql = 'SELECT * FROM insulins WHERE family_id = ?';
        if (!$includeArchived) {
            $sql .= ' AND is_archived = 0';
        }
        $sql .= ' ORDER BY sort_order, id';
        $rows = Db::fetchAll($sql, [$family['id']]);
        Response::json(['insulins' => array_map([Mapper::class, 'insulin'], $rows)]);
    }

    public static function create(): void
    {
        $family = Auth::requireFamily();
        $data = json_body();
        require_fields($data, ['name', 'category']);

        $name = trim((string) $data['name']);
        $category = (string) $data['category'];
        if (!in_array($category, ['bolus', 'basal'], true)) {
            Response::error('category musí byť bolus alebo basal.', 422);
        }

        $color = optional_string($data['color_hex'] ?? '#2196F3', 7) ?? '#2196F3';
        if (!valid_color($color)) {
            Response::error('color_hex musí byť #RRGGBB.', 422);
        }

        $duration = optional_float($data['duration_hours'] ?? ($category === 'bolus' ? 4.0 : 24.0));
        if ($duration === null || $duration <= 0 || $duration > 48) {
            Response::error('duration_hours musí byť v rozsahu 0–48.', 422);
        }

        $sort = (int) ($data['sort_order'] ?? 0);

        Db::exec(
            'INSERT INTO insulins (family_id, name, category, color_hex, duration_hours, sort_order)
             VALUES (?, ?, ?, ?, ?, ?)',
            [$family['id'], $name, $category, $color, $duration, $sort]
        );

        $row = Db::fetchOne('SELECT * FROM insulins WHERE id = ?', [Db::lastId()]);
        Response::json(['insulin' => Mapper::insulin($row)], 201);
    }

    public static function update(array $params): void
    {
        $family = Auth::requireFamily();
        $id = (int) $params['id'];
        $row = Db::fetchOne(
            'SELECT * FROM insulins WHERE id = ? AND family_id = ?',
            [$id, $family['id']]
        );
        if ($row === null) {
            Response::error('Inzulín neexistuje.', 404);
        }

        $data = json_body();
        $name = array_key_exists('name', $data) ? trim((string) $data['name']) : $row['name'];
        $category = array_key_exists('category', $data) ? (string) $data['category'] : $row['category'];
        if (!in_array($category, ['bolus', 'basal'], true)) {
            Response::error('category musí byť bolus alebo basal.', 422);
        }

        $color = array_key_exists('color_hex', $data)
            ? (optional_string($data['color_hex'], 7) ?? $row['color_hex'])
            : $row['color_hex'];
        if (!valid_color($color)) {
            Response::error('color_hex musí byť #RRGGBB.', 422);
        }

        $duration = array_key_exists('duration_hours', $data)
            ? optional_float($data['duration_hours'])
            : (float) $row['duration_hours'];
        if ($duration === null || $duration <= 0 || $duration > 48) {
            Response::error('duration_hours musí byť v rozsahu 0–48.', 422);
        }

        $sort = array_key_exists('sort_order', $data) ? (int) $data['sort_order'] : (int) $row['sort_order'];
        $archived = array_key_exists('is_archived', $data)
            ? bool_int($data['is_archived'])
            : (int) $row['is_archived'];

        Db::exec(
            'UPDATE insulins SET name = ?, category = ?, color_hex = ?, duration_hours = ?,
             sort_order = ?, is_archived = ? WHERE id = ? AND family_id = ?',
            [$name, $category, $color, $duration, $sort, $archived, $id, $family['id']]
        );

        $updated = Db::fetchOne('SELECT * FROM insulins WHERE id = ?', [$id]);
        Response::json(['insulin' => Mapper::insulin($updated)]);
    }
}
