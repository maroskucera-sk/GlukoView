<?php
declare(strict_types=1);

final class Mapper
{
    public static function profile(array $r): array
    {
        return [
            'id'         => (int) $r['id'],
            'name'       => $r['name'],
            'color_hex'  => $r['color_hex'],
            'sort_order' => (int) $r['sort_order'],
            'is_active'  => (bool) $r['is_active'],
            'created_at' => format_dt($r['created_at']),
            'updated_at' => format_dt($r['updated_at']),
        ];
    }

    public static function insulin(array $r): array
    {
        return [
            'id'             => (int) $r['id'],
            'name'           => $r['name'],
            'category'       => $r['category'],
            'color_hex'      => $r['color_hex'],
            'duration_hours' => (float) $r['duration_hours'],
            'sort_order'     => (int) $r['sort_order'],
            'is_archived'    => (bool) $r['is_archived'],
            'created_at'     => format_dt($r['created_at']),
            'updated_at'     => format_dt($r['updated_at']),
        ];
    }

    public static function scene(array $r): array
    {
        return [
            'id'          => (int) $r['id'],
            'name'        => $r['name'],
            'icon_key'    => $r['icon_key'],
            'sort_order'  => (int) $r['sort_order'],
            'is_archived' => (bool) $r['is_archived'],
            'created_at'  => format_dt($r['created_at']),
            'updated_at'  => format_dt($r['updated_at']),
        ];
    }

    public static function settings(array $r): array
    {
        return [
            'nightscout_url'          => $r['nightscout_url'],
            'nightscout_token'        => $r['nightscout_token'],
            'basal_reminder_time'     => $r['basal_reminder_time'],
            'glucose_unit'            => $r['glucose_unit'],
            'duplicate_bolus_minutes' => (int) $r['duplicate_bolus_minutes'],
            'updated_at'              => format_dt($r['updated_at']),
        ];
    }

    public static function dose(array $r): array
    {
        return [
            'id'              => (int) $r['id'],
            'uuid'            => $r['uuid'],
            'profile_id'      => (int) $r['profile_id'],
            'insulin_id'      => (int) $r['insulin_id'],
            'scene_id'        => $r['scene_id'] !== null ? (int) $r['scene_id'] : null,
            'units'           => (float) $r['units'],
            'administered_at' => format_dt($r['administered_at']),
            'glucose_value'   => $r['glucose_value'] !== null ? (float) $r['glucose_value'] : null,
            'glucose_unit'    => $r['glucose_unit'],
            'glucose_trend'   => $r['glucose_trend'],
            'carbs_sj'        => $r['carbs_sj'] !== null ? (float) $r['carbs_sj'] : null,
            'carbs_grams'     => $r['carbs_grams'] !== null ? (float) $r['carbs_grams'] : null,
            'injection_site'  => $r['injection_site'],
            'note'            => $r['note'],
            'created_at'      => format_dt($r['created_at']),
            'updated_at'      => format_dt($r['updated_at']),
            'deleted_at'      => format_dt($r['deleted_at'] ?? null),
        ];
    }
}
