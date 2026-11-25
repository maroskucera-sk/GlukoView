# Android — GlukoView Inzulín

Kotlin · Jetpack Compose · Material Design 3 · MVVM · Room · Retrofit  
Jazyk UI: **slovenčina** · Dark Mode (systém / svetlá / tmavá)

> Toto nie je zdravotnícka pomôcka.

## Stav

| Krok | Obsah | Stav |
|------|-------|------|
| 3–4 | Compose app + obrazovky | ✅ |
| UX | Time picker, kombinovaný export pre lekára (CSV/PDF) | ✅ |

## Nightscout — čo appka robí s glykémiami?

| Režim | Správanie |
|-------|-----------|
| **Dashboard** | Stiahne len aktuálne 1–2 merania. CGM históriu **neukladá**. |
| **Zápis dávky** | Predvyplní aktuálnu G (+ trend); uloží sa len pri dávke. |
| **Export pre lekára** | On-demand stiahne CGM za obdobie z Nightscoutu a **zlúči s inzulínom** do jedného CSV/PDF. |

Zdroj pravdy pre CGM ostáva Nightscout (žiadna duplicitná archívna DB v telefóne).

## Obrazovky

- **Prehľad:** NS glykémia + trend/delta, IOB, posledná dávka, dnešný zoznam, pripomienka bazálu
- **Záznam:** inzulín, ± j, scéna, dátum/čas picker, glykémia, SJ/g, miesto vpichu, poznámka, varovanie duplicity
- **História:** dni + súhrn rýchly/bazál
- **Nastavenia:** profil, téma, Nightscout, inzulíny/scény, sync, **export CSV/PDF pre diabetológa**

## Build

```bash
./gradlew :app:assembleDebug
./gradlew :app:testDebugUnitTest
```
