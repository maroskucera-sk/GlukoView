<?php
declare(strict_types=1);

$configPath = dirname(__DIR__) . '/config/config.php';
if (!is_file($configPath)) {
    http_response_code(500);
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode([
        'error' => 'Chýba config/config.php. Skopírujte config.example.php a upravte údaje.',
    ], JSON_UNESCAPED_UNICODE);
    exit;
}

/** @var array $config */
$config = require $configPath;

require_once __DIR__ . '/helpers.php';
require_once __DIR__ . '/Response.php';
require_once __DIR__ . '/Db.php';
require_once __DIR__ . '/Auth.php';
require_once __DIR__ . '/Router.php';
require_once __DIR__ . '/Mapper.php';
require_once __DIR__ . '/SchemaMigrator.php';
require_once __DIR__ . '/Controllers/AuthController.php';
require_once __DIR__ . '/Controllers/SyncController.php';
require_once __DIR__ . '/Controllers/DoseController.php';
require_once __DIR__ . '/Controllers/InsulinController.php';
require_once __DIR__ . '/Controllers/SceneController.php';
require_once __DIR__ . '/Controllers/InjectionSiteController.php';
require_once __DIR__ . '/Controllers/ProfileController.php';
require_once __DIR__ . '/Controllers/SettingsController.php';
require_once __DIR__ . '/Controllers/ExportController.php';

try {
    SchemaMigrator::run(Db::pdo());
} catch (Throwable $e) {
    error_log('SchemaMigrator: ' . $e->getMessage());
}

if (!empty($config['cors'])) {
    header('Access-Control-Allow-Origin: *');
    header('Access-Control-Allow-Headers: Authorization, Content-Type, X-Profile-Id');
    header('Access-Control-Allow-Methods: GET, POST, PUT, PATCH, DELETE, OPTIONS');
    if (($_SERVER['REQUEST_METHOD'] ?? 'GET') === 'OPTIONS') {
        http_response_code(204);
        exit;
    }
}
