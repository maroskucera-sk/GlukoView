# Backend — GlukoView Insulin Log

Čisté PHP 8+ REST API + MySQL. Bez ťažkého frameworku.

## Stav

| Krok | Obsah                                      | Stav        |
|------|--------------------------------------------|-------------|
| 1    | Štruktúra + `schema.sql`                   | ✅ hotovo   |
| 2    | REST endpointy + návod na nasadenie        | ⏳ po schválení |
| 3–4  | Android klient                             | neskôr      |

## Plánovaná štruktúra (krok 2)

```
backend/
├── public/
│   ├── index.php          # Front controller
│   └── .htaccess          # Rewrite → index.php
├── src/
│   ├── bootstrap.php
│   ├── Router.php
│   ├── Auth.php
│   ├── Db.php
│   ├── Response.php
│   └── Controllers/
├── config/
│   └── config.example.php
├── schema.sql
└── README.md
```

Document root webhostingu nastavte na `backend/public/`.
