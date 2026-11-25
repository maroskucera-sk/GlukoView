# Nasadenie GlukoView API na Websupport

Áno — **MySQL je povinné**. API ukladá dávky, inzulíny, scény a nastavenia do MySQL
(na Websupport to je MariaDB v paneli **Databázy → MySQL**). Bez DB aplikácia nefunguje.

Tento návod je pre **WebAdmin** (Websupport shared hosting), bez SSH.

Balík na stiahnutie:
[glukoview_insulin_backend.tar.gz](https://raw.githubusercontent.com/maroskucera-sk/glukoview/cursor/insulin-log-schema-ec2f/docs/insulin-log/downloads/glukoview_insulin_backend.tar.gz)

---

## Čo budete potrebovať

| Vec | Kde vo WebAdmin |
|-----|-----------------|
| MySQL databáza + používateľ + heslo | **Databázy → MySQL** |
| Subdoména (odporúčané) + HTTPS | **Domény → Subdomény** / SSL |
| FTP alebo File Manager | **FTP účty** / **Správca súborov** |
| Backend balík `.tar.gz` | odkaz vyššie |

Odporúčaná adresa API: `https://insulin.vasadomena.sk`  
V Android appke potom zadáte túto URL (bez `/api/v1`) + rodinný kľúč.

---

## Krok 1 — MySQL databáza

1. Prihláste sa do **WebAdmin**.
2. Otvorte **Databázy → MySQL → Vytvoriť databázu**.
3. Zvoľte názov (napr. `gluko`) — Websupport doplní prefix, výsledok typu `w12345_gluko`.
4. Vytvorte / priraďte **používateľa** a **heslo**. Heslo si bezpečne uložte.
5. Poznačte si:
   - **Host** — väčšinou `localhost` (ak panel ukazuje iný hostname, použite ten)
   - **Názov DB** — napr. `w12345_gluko`
   - **Používateľ** — napr. `w12345_gluko`
   - **Heslo**
   - **Port** — `3306`

Charset/collation: **utf8mb4** (ak sa pýta panel).

> Tabuľky vytvorí webový inštalátor v kroku 4. phpMyAdmin import nie je potrebný, ak použijete `install.php`.

---

## Krok 2 — Subdoména a document root

1. **Domény → Subdomény → Vytvoriť** (napr. `insulin`).
2. Nastavte **document root** na priečinok, kde bude obsah `public/`:
   - Ideálne: `…/insulin/public`  
     (celý backend je v `…/insulin/`, webroot len `public/`)
3. Počkajte na DNS / SSL (Let’s Encrypt vo WebAdmin — **HTTPS musí byť zapnuté**).

### Odporúčaná štruktúra na disku

```
insulin.vasadomena.sk/          ← priečinok subdomény
├── public/                     ← document root (index.php, install.php, .htaccess)
├── src/
├── config/                     ← tu vznikne config.php (mimo webu)
├── schema_webhost.sql
└── …
```

Ak panel **neumožní** root na podpriečinok `public/`:
1. Nahrajte obsah `public/` priamo do document rootu subdomény.
2. Nahrajte `src/`, `config/`, `schema_webhost.sql` **o úroveň vyššie** (súrodenci document rootu).
3. Štandardné cesty v `index.php` (`dirname(__DIR__) . '/src/…'`) potom sedia.

---

## Krok 3 — Nahrajte súbory (FTP)

1. Rozbaľte `glukoview_insulin_backend.tar.gz` na PC.
2. Cez **FTP** (FileZilla) alebo **Správca súborov** nahrajte celý obsah podľa štruktúry vyššie.
3. Overte, že existujú:
   - `public/index.php`
   - `public/install.php`
   - `public/.htaccess`
   - `src/` (súbory PHP)
   - `config/config.example.php`
   - `schema_webhost.sql`

PHP na Websupport: zvoľte **PHP 8.0+** (najlepšie 8.2) pre danú doménu v **WebAdmin → Domény / PHP**.

---

## Krok 4 — Webový inštalátor

1. Otvorte v prehliadači:  
   `https://insulin.vasadomena.sk/install.php`
2. Vyplňte MySQL údaje z kroku 1.
3. Zadajte **názov rodiny**, **prístupový kľúč** (≥ 12 znakov) a mená profilov.
4. Nechajte zaškrtnuté „predvolené inzulíny a scény“, ak chcete hotový základ.
5. Kliknite **Nainštalovať**.

Inštalátor:
- vytvorí tabuľky,
- zapíše `config/config.php`,
- vytvorí rodinu + profily,
- vytvorí `config/install.lock`.

### Po úspechu (povinné)

1. **Zmažte** `public/install.php` cez FTP / File Manager.
2. Uložte si **API URL** a **prístupový kľúč** (server ukladá len hash).
3. Overte: `https://insulin.vasadomena.sk/api/v1/health`  
   → má vrátiť `{"ok":true,...}`

---

## Krok 5 — Android aplikácia

1. Nainštalujte APK:  
   [glukoview_insulin_log_debug.apk](https://raw.githubusercontent.com/maroskucera-sk/glukoview/cursor/insulin-log-schema-ec2f/docs/insulin-log/downloads/glukoview_insulin_log_debug.apk)
2. V nastavení zadajte:
   - **URL servera:** `https://insulin.vasadomena.sk` (bez lomítka na konci, bez `/api/v1`)
   - **Rodinný kľúč:** ten istý, čo ste zadali v inštalátore
3. Overte prihlásenie / sync.

Nightscout URL a token nastavíte neskôr v appke (nastavenia rodiny) — nie pri inštalácii API.

---

## Riešenie problémov

| Problém | Čo skontrolovať |
|---------|-----------------|
| `install.php` → MySQL error | Host (`localhost`), prefix názvu DB/usera, heslo, PHP `pdo_mysql` |
| 404 na `/api/v1/health` | Document root = `public/`, súbor `.htaccess`, `mod_rewrite` |
| 500 / chýba config | Inštalácia nedobehla; `config/` musí byť zapisovateľný |
| HTTP namiesto HTTPS | Zapnite SSL vo WebAdmin; appka vyžaduje bezpečné spojenie |
| Inštalátor sa otvorí znova | Chýba `install.lock` alebo ste nezmazali `install.php` — po úspechu vždy zmažte `install.php` |

### Manuálny import (ak inštalátor nejde)

1. phpMyAdmin → vyberte DB → Import → súbor `schema_webhost.sql`.
2. Skopírujte `config/config.example.php` → `config/config.php` a vyplňte DB údaje.
3. Rodinu vytvorte cez SSH (`php tools/create_family.php …`) alebo dočasne nahrajte `install.php` znova (len na prázdnu DB bez locku).

---

## Bezpečnosť (stručne)

1. Document root = len `public/`.
2. Po inštalácii **zmazať `install.php`**.
3. Len **HTTPS**.
4. Rodinný kľúč ako silné heslo; zdieľať len v rodine.
5. `config/config.php` necommitovať a nedávať do webrootu.
