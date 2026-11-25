<?php
declare(strict_types=1);

/** @var string $appRoot */
$appRoot = require __DIR__ . '/_app_root.php';
require_once $appRoot . '/src/bootstrap.php';

$method = $_SERVER['REQUEST_METHOD'] ?? 'GET';

// Cesta: podporuje /api/v1/... aj priamy rewrite
$uri = $_SERVER['REQUEST_URI'] ?? '/';
$path = parse_url($uri, PHP_URL_PATH) ?: '/';

// Ak je API v podadresári (napr. /insulin/api/v1/...), nájdi /api/v1
if (preg_match('#(/api/v1(?:/.*)?)$#', $path, $m)) {
    $path = $m[1];
}

$router = new Router();

$router->add('GET',  '/api/v1/health', static function (): void {
    Response::json([
        'ok' => true,
        'name' => 'GlukoView Insulin Log API',
        'version' => '1.0.0',
    ]);
});

$router->add('POST', '/api/v1/auth/verify', [AuthController::class, 'verify']);
$router->add('GET',  '/api/v1/sync', [SyncController::class, 'sync']);

$router->add('GET',    '/api/v1/doses', [DoseController::class, 'list']);
$router->add('POST',   '/api/v1/doses', [DoseController::class, 'create']);
$router->add('GET',    '/api/v1/doses/{uuid}', [DoseController::class, 'get']);
$router->add('PATCH',  '/api/v1/doses/{uuid}', [DoseController::class, 'update']);
$router->add('DELETE', '/api/v1/doses/{uuid}', [DoseController::class, 'delete']);

$router->add('GET',   '/api/v1/insulins', [InsulinController::class, 'list']);
$router->add('POST',  '/api/v1/insulins', [InsulinController::class, 'create']);
$router->add('PATCH', '/api/v1/insulins/{id}', [InsulinController::class, 'update']);

$router->add('GET',   '/api/v1/scenes', [SceneController::class, 'list']);
$router->add('POST',  '/api/v1/scenes', [SceneController::class, 'create']);
$router->add('PATCH', '/api/v1/scenes/{id}', [SceneController::class, 'update']);

$router->add('GET',   '/api/v1/profiles', [ProfileController::class, 'list']);
$router->add('POST',  '/api/v1/profiles', [ProfileController::class, 'create']);
$router->add('PATCH', '/api/v1/profiles/{id}', [ProfileController::class, 'update']);

$router->add('GET', '/api/v1/settings', [SettingsController::class, 'get']);
$router->add('PUT', '/api/v1/settings', [SettingsController::class, 'put']);

$router->add('GET', '/api/v1/export', [ExportController::class, 'export']);

try {
    $router->dispatch($method, $path);
} catch (PDOException $e) {
    error_log('DB error: ' . $e->getMessage());
    Response::error('Chyba databázy.', 500);
} catch (Throwable $e) {
    error_log('API error: ' . $e->getMessage());
    Response::error('Interná chyba servera.', 500);
}
