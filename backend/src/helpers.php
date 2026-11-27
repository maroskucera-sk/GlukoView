<?php
declare(strict_types=1);

function json_body(): array
{
    $raw = file_get_contents('php://input');
    if ($raw === false || $raw === '') {
        return [];
    }
    $data = json_decode($raw, true);
    if (!is_array($data)) {
        Response::error('Neplatné JSON telo požiadavky.', 400);
    }
    return $data;
}

function require_fields(array $data, array $fields): void
{
    foreach ($fields as $field) {
        if (!array_key_exists($field, $data) || $data[$field] === null || $data[$field] === '') {
            Response::error("Chýba povinné pole: {$field}", 422);
        }
    }
}

/** @return string|null ISO8601 UTC or null */
function parse_iso_datetime(?string $value): ?string
{
    if ($value === null || $value === '') {
        return null;
    }
    try {
        $dt = new DateTimeImmutable($value);
        return $dt->setTimezone(new DateTimeZone('UTC'))->format('Y-m-d H:i:s.v');
    } catch (Exception) {
        Response::error("Neplatný dátum/čas: {$value}", 422);
    }
}

function format_dt(?string $mysqlDt): ?string
{
    if ($mysqlDt === null || $mysqlDt === '') {
        return null;
    }
    try {
        // MySQL DATETIME(3) bez timezone → berieme ako UTC
        $dt = new DateTimeImmutable($mysqlDt, new DateTimeZone('UTC'));
        return $dt->format('Y-m-d\TH:i:s.v\Z');
    } catch (Exception) {
        return $mysqlDt;
    }
}

function bool_int(mixed $v): int
{
    return filter_var($v, FILTER_VALIDATE_BOOLEAN) ? 1 : 0;
}

function optional_float(mixed $v): ?float
{
    if ($v === null || $v === '') {
        return null;
    }
    if (!is_numeric($v)) {
        Response::error('Očakávané číslo.', 422);
    }
    return (float) $v;
}

function optional_string(mixed $v, int $max = 255): ?string
{
    if ($v === null) {
        return null;
    }
    $s = trim((string) $v);
    if ($s === '') {
        return null;
    }
    if (mb_strlen($s) > $max) {
        Response::error("Text je príliš dlhý (max {$max}).", 422);
    }
    return $s;
}

function valid_uuid(string $uuid): bool
{
    return (bool) preg_match(
        '/^[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i',
        $uuid
    );
}

function valid_color(?string $hex): bool
{
    if ($hex === null) {
        return true;
    }
    return (bool) preg_match('/^#[0-9A-Fa-f]{6}$/', $hex);
}

function valid_hhmm(?string $t): bool
{
    if ($t === null || $t === '') {
        return true;
    }
    return (bool) preg_match('/^([01]\d|2[0-3]):[0-5]\d$/', $t);
}
