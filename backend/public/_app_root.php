<?php
/**
 * Nájde koreň aplikácie pre dva layouty:
 *  A) klasický:  …/public/index.php  +  …/src  …/config
 *  B) flat (Websupport sub/):  index.php vedľa src/ a config/
 */
declare(strict_types=1);

$webDir = __DIR__;

if (is_file($webDir . '/src/bootstrap.php') && is_dir($webDir . '/config')) {
    return $webDir;
}

$parent = dirname($webDir);
if (is_file($parent . '/src/bootstrap.php') && is_dir($parent . '/config')) {
    return $parent;
}

http_response_code(500);
header('Content-Type: text/plain; charset=utf-8');
echo "GlukoView: chýba src/ alebo config/. Nahrajte súbory podľa návodu NASADENIE-WEBSUPPORT.md.\n";
exit(1);
