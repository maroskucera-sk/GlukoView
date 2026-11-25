<?php
/**
 * Skopírujte ako config.php a upravte hodnoty.
 * config.php NIKDY necommitujte (je v .gitignore).
 */
declare(strict_types=1);

return [
    'db' => [
        'host'    => '127.0.0.1',
        'port'    => 3306,
        'name'    => 'glukoview_insulin',
        'user'    => 'glukoview',
        'pass'    => 'ZMENTE_HESLO',
        'charset' => 'utf8mb4',
    ],

    // true = CORS * (užitočné pri testovaní z prehliadača)
    'cors' => true,

    // Voliteľné: obmedziť API na jednu rodinu (ID z tabuľky families). null = overí kľúč voči všetkým.
    'family_id' => null,

    // Timezone pre CSV export (DB ukladá UTC)
    'export_timezone' => 'Europe/Bratislava',
];
