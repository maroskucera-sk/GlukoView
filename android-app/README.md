# Android — GlukoView Inzulín

Kotlin · Jetpack Compose · Material Design 3 · MVVM · Room · Retrofit  
Jazyk UI: **slovenčina** · Dark Mode (systém / svetlá / tmavá)

> Toto nie je zdravotnícka pomôcka.

## Stav

| Krok | Obsah | Stav |
|------|-------|------|
| 3 | Gradle, navigácia, M3 téma, modely, Room, API + Nightscout, onboarding | ✅ |
| 4 | Dashboard, Záznam, História, Nastavenia | ✅ |

## Otvorenie v Android Studio

1. Otvorte priečinok `android-app/` (nie celý GlukoView root).
2. Sync Gradle (AGP 8.7 / JDK 17).
3. Spustite na emulátore alebo zariadení (minSdk 26).

Pri prvom spustení zadáte URL PHP API + rodinný kľúč + profil.

## Obrazovky (krok 4)

- **Prehľad:** Nightscout glykémia + trend + delta, IOB, posledná dávka, dnešný zoznam, pripomienka bazálu, pull-to-refresh sync
- **Pridať/upraviť záznam:** inzulín, ± jednotky, scéna, čas, glykémia (predvyplnená z NS), SJ/g, miesto vpichu, poznámka, varovanie duplicitného bolusu
- **História:** dni + súhrn rýchly/bazál, tap = úprava
- **Nastavenia:** profil, téma, Nightscout, inzulíny/scény, sync, CSV export, odhlásenie

## Build

```bash
./gradlew :app:assembleDebug
./gradlew :app:testDebugUnitTest
```
