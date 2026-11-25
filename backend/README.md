# Backend — GlukoView Insulin Log

Čisté **PHP 8+** REST API + **MySQL 8**. Bez frameworku (žiadny Composer).

> Nie je to zdravotnícka pomôcka. Údaje sú na evidenciu v rodine, nie na automatické dávkovanie.

## Požiadavky hostingu

- PHP 8.0+ s rozšíreniami: `pdo_mysql`, `json`, `mbstring` (odporúčané `openssl`)
- MySQL 8+ / MariaDB 10.5+ (utf8mb4)
- HTTPS (povinné — v API putuje Nightscout token a rodinný kľúč)
- Apache s `mod_rewrite` **alebo** Nginx + PHP-FPM

## Rýchle nasadenie

### 1. Nahrajte súbory

Na server skopírujte celý priečinok `backend/`.  
**Document root** nastavte na `backend/public/` (nie na rodiča `backend/`).

```
backend/
├── public/             ← webroot (index.php, install.php, .htaccess)
├── src/                ← mimo webu
├── config/             ← mimo webu (tu bude config.php)
├── schema.sql          ← demo + seed
├── schema_empty.sql    ← len tabuľky (+ CREATE DATABASE)
├── schema_webhost.sql  ← len tabuľky pre shared hosting (Websupport)
└── tools/
```

Príklady vhostov: `deploy/apache-vhost.example.conf`, `deploy/nginx.example.conf`.

### 2. Databáza

V phpMyAdmin / MySQL konzole:

```bash
# Produkcia (prázdna schéma):
mysql -u USER -p < schema_empty.sql

# Alebo demo so seedom (rodina + Maroš/Veronika/Melinka):
mysql -u USER -p < schema.sql
```

Demo prístupový kľúč (len seed): `change-me-family-key` — **ihneď zmeňte**.

### 3. Konfigurácia

```bash
cp config/config.example.php config/config.php
# upravte db host/user/pass/name
```

### 4. Vytvorenie rodiny (produkcia)

Na serveri (SSH) alebo lokálne s prístupom k DB:

```bash
php tools/create_family.php "Naša rodina" "VAS-SILNY-KLUC-ASPOŇ-12" --with-defaults
```

`--with-defaults` vloží profily Maroš/Veronika/Melinka, základné inzulíny a scény.

### 5. Overenie

```bash
curl -s https://VAS-DOMAIN/api/v1/health
# → {"ok":true,"name":"GlukoView Insulin Log API","version":"1.0.0"}

curl -s -X POST https://VAS-DOMAIN/api/v1/auth/verify \
  -H "Authorization: Bearer VAS-SILNY-KLUC-ASPOŇ-12"
```

V Android aplikácii zadáte: **URL** = `https://VAS-DOMAIN` (bez `/api/v1`) + **rodinný kľúč**.

---

## Autentifikácia

| Hlavička | Význam |
|----------|--------|
| `Authorization: Bearer <kľúč>` | Rodinný prístupový kľúč (plaintext; na serveri je hash) |
| `Content-Type: application/json` | Pri POST/PUT/PATCH |

Profil (kto zapisuje) je v tele požiadavky ako `profile_id` — nie v auth.

---

## Endpointy

| Metóda | Cesta | Popis |
|--------|-------|-------|
| GET | `/api/v1/health` | Healthcheck (bez auth) |
| POST | `/api/v1/auth/verify` | Overenie kľúča → rodina, profily, nastavenia |
| GET | `/api/v1/sync?since=` | Delta sync (profily, inzulíny, scény, dávky, settings) |
| GET | `/api/v1/doses?from=&to=&limit=` | Zoznam dávok |
| POST | `/api/v1/doses` | Vytvorenie / upsert podľa `uuid` |
| GET/PATCH/DELETE | `/api/v1/doses/{uuid}` | Detail / úprava / soft delete |
| GET/POST | `/api/v1/insulins` | Zoznam / nový typ |
| PATCH | `/api/v1/insulins/{id}` | Úprava / archivácia (`is_archived`) |
| GET/POST | `/api/v1/scenes` | Zoznam / nová scéna |
| PATCH | `/api/v1/scenes/{id}` | Úprava / archivácia |
| GET/POST | `/api/v1/profiles` | Zoznam / nový profil |
| PATCH | `/api/v1/profiles/{id}` | Úprava / deaktivácia |
| GET/PUT | `/api/v1/settings` | Nightscout, bazál pripomienka, jednotky |
| GET | `/api/v1/export?from=&to=&format=csv` | CSV export pre diabetológa |

### Príklad: nová dávka

```http
POST /api/v1/doses
Authorization: Bearer ...
Content-Type: application/json

{
  "uuid": "550e8400-e29b-41d4-a716-446655440000",
  "profile_id": 1,
  "insulin_id": 1,
  "scene_id": 3,
  "units": 3.5,
  "administered_at": "2026-11-25T07:30:00.000Z",
  "glucose_value": 7.2,
  "glucose_unit": "mmol",
  "glucose_trend": "FortyFiveUp",
  "carbs_sj": 4.0,
  "injection_site": "abdomen",
  "note": "pizza"
}
```

Ak existuje nedávny bolus (&lt; `duplicate_bolus_minutes`), odpoveď obsahuje `warnings[]` — záznam sa aj tak uloží (varovanie potvrdí klient pred odoslaním).

### Sync

```http
GET /api/v1/sync
GET /api/v1/sync?since=2026-11-25T06:00:00.000Z
```

Bez `since` = snapshot (posledných 500 dávok). S `since` = zmeny `updated_at` (vrátane soft-deleted dávok).

Časy sú vždy **UTC** vo formáte ISO-8601 (`…Z`).

---

## Bezpečnosť na hostingu

1. Webroot = len `public/`.
2. `config/config.php` mimo webu + silné DB heslo.
3. Len HTTPS.
4. Rodinný kľúč majte ako heslo (dlhý, náhodný); zdieľajte len v rodine.
5. Nightscout token v `family_settings` — kto má rodinný kľúč, vidí aj NS token (zámerné zdieľanie v rodine).

---

## Shared hosting tipy

- **Subdoména** `insulin.vasadomena.sk` → root na `…/public`.
- Ak nemôžete zmeniť document root: nahrajte obsah `public/` do `public_html/insulin/` a zvyšok (`src`, `config`) o úroveň vyššie; upravte `require` cesty v `index.php` len ak je to nevyhnutné. Odporúčané je vždy oddelený vhost/subdoména.
- MariaDB: `DATETIME(3)` a `CHECK` sú OK na 10.5+; pri staršej verzii odstráňte `CHECK` constraint zo schémy.
- **Websupport:** na subdoméne je webroot = celý `sub/nazov/` — použi **flat layout** (obsah `public/` + `src/` + `config/` do jedného priečinka; `src/.htaccess` a `config/.htaccess` zakážu HTTP). Návod: [`docs/insulin-log/NASADENIE-WEBSUPPORT.md`](../docs/insulin-log/NASADENIE-WEBSUPPORT.md).
- **Bez SSH:** `https://VAS-DOMAIN/install.php` → potom **zmaž** `install.php`. Schéma: `schema_webhost.sql`.
