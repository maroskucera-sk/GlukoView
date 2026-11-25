# Android — GlukoView Inzulín

Kotlin · Jetpack Compose · Material Design 3 · MVVM · Room · Retrofit  
Jazyk UI: **slovenčina** · Dark Mode (systém / svetlá / tmavá)

> Toto nie je zdravotnícka pomôcka.

## Stav

| Krok | Obsah | Stav |
|------|-------|------|
| 3 | Gradle, navigácia, M3 téma, modely, Room, API + Nightscout, onboarding | ✅ |
| 4 | Dashboard, Záznam, História, Nastavenia (plné UI) | ⏳ po schválení |

## Otvorenie v Android Studio

1. Otvorte priečinok `android-app/` (nie celý GlukoView root).
2. Sync Gradle (AGP 8.7 / JDK 17).
3. Spustite na emulátore alebo zariadení (minSdk 26).

Pri prvom spustení zadáte:
- URL vášho PHP API (napr. `https://insulin.vasadomena.sk`)
- rodinný prístupový kľúč
- výber profilu (Maroš / Veronika / …)

## Balíky

```
sk.glukoview.insulinlog/
├── di/                 # AppContainer (manuálne DI)
├── domain/             # modely, IOB, duplicita, NS trend
├── data/
│   ├── local/          # Room + DataStore preferencie
│   ├── remote/         # Retrofit REST klient
│   ├── nightscout/     # entries.json klient
│   └── repository/
└── ui/
    ├── theme/          # Material 3 (teal, dark mode)
    ├── navigation/     # bottom bar + FAB
    ├── onboarding/     # prihlásenie + výber profilu
    ├── dashboard/      # placeholder → krok 4
    ├── dose/
    ├── history/
    └── settings/
```

## Testy (domain)

```bash
./gradlew :app:testDebugUnitTest
```

## Poznámky

- Nightscout volá telefón priamo (URL + token zo `family_settings`).
- Offline: záznamy majú `syncPending`; `SyncRepository.syncNow()` pushne frontu a pullne deltu.
- Cleartext HTTP je povolený len pre `localhost` / `10.0.2.2` (emulátor).
