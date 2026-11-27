# GlukoView Insulin Log — architektúra (krok 1)

> **Upozornenie:** Toto nie je zdravotnícka pomôcka. Záznamy a výpočty (IOB, glykémia) sú orientačné
> a neslúžia ako jediný podklad na dávkovanie inzulínu. Vždy overte hodnoty na svojom senzore / glukomere.

## Cieľ

Rodinné zaznamenávanie podaného inzulínu: 1 rodinné konto na serveri, viac profilov
(napr. Maroš, Veronika, Melinka), synchronizácia cez REST API, lokálny offline cache v Android aplikácii,
voliteľné čítanie glykémie z Nightscoutu.

## Štruktúra projektu

```
GlukoView/
├── backend/                      # PHP 8+ REST API (vlastný webhosting)
│   ├── public/                   # Document root (index.php = front controller)
│   ├── src/                      # Router, auth, repositories, services
│   ├── config/                   # config.example.php (bez tajomstiev)
│   ├── schema.sql                # MySQL schéma + seed dát
│   └── README.md                 # Nasadenie na hosting (krok 2)
│
├── android-app/                  # Kotlin + Jetpack Compose (MVVM) — krok 3 ✅
│   ├── app/src/main/java/.../    # domain, data (Room/Retrofit/NS), ui
│   ├── gradle/
│   └── README.md
│
└── docs/insulin-log/
    └── ARCHITECTURE.md           # Tento dokument
```

Existujúci ESP32 firmware (`src/`, `platformio.ini`, …) ostáva nedotknutý.

## Autentifikácia

1. Rodina má jeden **prístupový kľúč** (token). Na serveri sa ukladá len **hash** (Argon2id / bcrypt).
2. Klient posiela hlavičku `Authorization: Bearer <family_access_key>`.
3. Profil (kto práve zapisuje) je zvolený v aplikácii a posiela sa ako `X-Profile-Id` / pole `profile_id` v tele.
4. Nightscout URL + token sú **rodinné nastavenia** (nie per-zariadenie) — stačí nastaviť raz.

## Synchronizácia a offline

| Entita        | Sync model                                      |
|---------------|-------------------------------------------------|
| Profily       | Pull (CRUD na serveri, zriedka)                 |
| Inzulíny      | Pull + push zmien, soft-archive                 |
| Scény         | Pull + push zmien, soft-archive                 |
| Nastavenia    | Pull + push (Nightscout, pripomienka bazálu)    |
| Záznamy dávok | Offline-first: Room → fronta → POST/PATCH/DELETE |

Každý záznam má klientský `uuid` (UUID v4), aby sa rovnaký offline zápis nezduplikoval pri opätovnom odoslaní.

`GET /api/v1/sync?since=<ISO8601>` vráti zmeny od poslednej synchronizácie (záznamy, inzulíny, scény, nastavenia, soft-delete).

## Nightscout

- Aplikácia volá Nightscout **priamo z telefónu** (backend ho neproxyuje).
- **Bežná práca (dashboard / predvyplnenie):** stiahne len aktuálne 1–2 merania (`entries.json?count=2`). CGM históriu do Room **neukladá**.
- **Export pre diabetológa:** pri stlačení exportu on-demand stiahne entries za zvolené obdobie a **v jednom CSV/PDF** zlúči časovú os glykémií s inzulínovými dávkami. Zdroj pravdy pre CGM ostáva Nightscout.
- Pri novom zázname sa predvyplní aktuálna glykémia + trend; používateľ môže upraviť (glukomer / výpadok senzora).

## IOB (orientačné)

Výpočet len na klientovi z lokálnych / syncovaných bolusových záznamov:

```
zostávajúce = units * max(0, 1 - elapsed_hours / duration_hours)
```

`duration_hours` berie z typu inzulínu (predvolene 3–4 h). Nie je to medicínsky model (žiadny peak/DIA krivka).

## Ochrany v praxi

| Funkcia                    | Kde                          |
|----------------------------|------------------------------|
| Duplicitný bolus < 20 min  | Klient (kontrola histórie) + voliteľne server |
| Pripomienka bazálu         | Klient podľa `basal_reminder_time` |
| Soft delete záznamov       | Server `deleted_at` pre sync |

## API (krok 2 — implementované)

Podrobnosti a nasadenie: `backend/README.md`.

| Metóda | Cesta                         | Účel                          |
|--------|-------------------------------|-------------------------------|
| GET    | `/api/v1/health`              | Healthcheck                   |
| POST   | `/api/v1/auth/verify`         | Overenie kľúča + zoznam profilov |
| GET    | `/api/v1/sync`                | Delta sync                    |
| GET/POST/PATCH/DELETE | `/api/v1/doses`  | Záznamy inzulínu              |
| GET/POST/PATCH        | `/api/v1/insulins` | Typy inzulínov                |
| GET/POST/PATCH        | `/api/v1/scenes`   | Scény                         |
| GET/POST/PATCH        | `/api/v1/profiles` | Profily rodiny                |
| GET/PUT               | `/api/v1/settings` | Nightscout, bazál pripomienka |
| GET                   | `/api/v1/export`   | CSV export obdobia            |

## Databázové entity (prehľad)

Pozri `backend/schema.sql`.

```
families 1──* profiles
         1──* insulins
         1──* scenes
         1──1 family_settings
         1──* dose_records
dose_records *──1 profiles (podal)
             *──1 insulins
             *──0..1 scenes
```
