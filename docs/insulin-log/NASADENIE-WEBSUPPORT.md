# Nasadenie GlukoView API na Websupport

Áno — **MySQL je povinné**. Bez DB API nefunguje.

Balík:  
[glukoview_insulin_backend.tar.gz](https://raw.githubusercontent.com/maroskucera-sk/glukoview/cursor/insulin-log-schema-ec2f/docs/insulin-log/downloads/glukoview_insulin_backend.tar.gz)

---

## Ako to na Websupport funguje

Na FTP uvidíš:

```
logs/
web/          ← hlavný web (TU NIČ Z BALÍKA NEDÁVAJ)
sub/          ← subdomény
```

Subdoména `insulin.tvojadomena.sk` = priečinok **`sub/insulin/`**.  
**Celý ten priečinok je webroot** — nie je oddelený „mimo web“ priečinok.  
Preto sa všetko nahrá **priamo do `sub/insulin/`** a citlivé priečinky `src/` + `config/` sú zakázané cez `.htaccess`.

---

## Krok 1 — MySQL (ak ešte nie)

WebAdmin → **Databázy → MySQL** → databáza (utf8mb4).  
Poznač: host (`localhost`), celý názov DB, user, heslo.

---

## Krok 2 — Subdoména

1. Vytvor priečinok **`sub/insulin`** (alebo WebAdmin → Web → Služby → nová subdoména `insulin`).
2. Zapni **HTTPS** (Let’s Encrypt).
3. PHP **8.0+** (ideálne 8.2) pre túto službu.

Hlavný web v `web/` ostáva nedotknutý.

---

## Krok 3 — Presne čo kam nahrať

Rozbaľ `glukoview_insulin_backend.tar.gz`. Vo vnútri je `backend/`.

**Celý obsah daj do `sub/insulin/` takto** (flat layout — všetko v jednom webroote):

| Zo balíka | Kam na FTP |
|-----------|------------|
| **obsah** `backend/public/` → `index.php`, `install.php`, `.htaccess`, `_app_root.php` | → priamo do `sub/insulin/` |
| celý `backend/src/` | → `sub/insulin/src/` |
| celý `backend/config/` | → `sub/insulin/config/` |
| `backend/schema_webhost.sql` | → `sub/insulin/schema_webhost.sql` |
| `README.md`, `schema.sql`, `deploy/`, `tools/` | **nenahrávaj** |

### Výsledok na disku

```
sub/insulin/                 ← = https://insulin.tvojadomena.sk/
├── index.php
├── install.php
├── .htaccess
├── _app_root.php
├── schema_webhost.sql       ← HTTP zakázané (.htaccess)
├── src/                     ← HTTP zakázané (src/.htaccess)
│   ├── .htaccess
│   ├── bootstrap.php
│   └── …
└── config/                  ← HTTP zakázané (config/.htaccess)
    ├── .htaccess
    └── config.example.php
```

**Overenie ochrany:** po nahratí skús v prehliadači  
`https://insulin.tvojadomena.sk/config/config.example.php`  
→ musí byť **403 Forbidden** (nie obsah súboru).

---

## Krok 4 — Inštalátor

1. Otvor `https://insulin.tvojadomena.sk/install.php`
2. Vyplň MySQL + názov rodiny + kľúč (≥12 znakov) + profily
3. **Nainštalovať**
4. **Zmaž** `sub/insulin/install.php` cez FTP
5. Over: `https://insulin.tvojadomena.sk/api/v1/health` → `{"ok":true,...}`

---

## Krok 5 — Android

- URL: `https://insulin.tvojadomena.sk` (bez `/api/v1`)
- Rodinný kľúč: ten z inštalátora

APK:  
[glukoview_insulin_log_debug.apk](https://raw.githubusercontent.com/maroskucera-sk/glukoview/cursor/insulin-log-schema-ec2f/docs/insulin-log/downloads/glukoview_insulin_log_debug.apk)

---

## Časté chyby

| Problém | Riešenie |
|---------|----------|
| Nahral si do `web/` | Zmaž z hlavného webu, daj do `sub/insulin/` |
| Vytvoril si vnútri ešte `public/` | Na Websupport **nepoužívaj** podpriečinok `public/` — obsah `public/` daj priamo do `sub/insulin/` |
| 404 na `/api/v1/health` | Chýba `.htaccess` alebo `mod_rewrite` |
| 403 na `config/` nefunguje (vidíš PHP kód) | Nahraj aj `config/.htaccess` a `src/.htaccess` |
| MySQL error v install | Skontroluj **celý** názov DB s prefixom z WebAdminu |

---

## Alternatíva (voliteľné): vlastný koreňový adresár

Ak vieš v **Web → Služby** nastaviť vlastný koreň na `sub/insulin/public`, môžeš použiť klasickú štruktúru (`public/` + `src/` mimo webu). Flat layout vyššie je však priamy a overený postup pre Websupport bez tejto voľby.
