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
| **Graf** | On-demand CGM + markery bolusu/bazálu (3–24 h), TIR, IOB, súhrny. |
| **Zápis dávky** | Pri čase „teraz“ predvyplní aktuálnu G; pri spätnom čase najbližšiu G z NS k tomu momentu. Tlačidlo **NS** = vždy aktuálna. |
| **Export pre lekára** | On-demand stiahne CGM za obdobie z Nightscoutu a **zlúči s inzulínom** do jedného CSV/PDF. |

Zdroj pravdy pre CGM ostáva Nightscout (žiadna duplicitná archívna DB v telefóne).

## Obrazovky

- **Prehľad:** NS glykémia + trend/delta, IOB, dnešný bolus/bazál, odkaz na graf, posledná dávka, pripomienka bazálu
- **Graf:** CGM krivka + inzulínové markery, cieľové pásmo, TIR/min/max, zoznam dávok v období
- **Záznam:** inzulín, ± j, scéna, dátum/čas picker, glykémia, SJ/g, miesto vpichu, poznámka, varovanie duplicity
- **História:** dni + súhrn rýchly/bazál
- **Nastavenia:** profil, téma, Nightscout, katalóg inzulínov, sync, **export CSV/PDF pre diabetológa**

## Build

```bash
./gradlew :app:assembleDebug
./gradlew :app:testDebugUnitTest
```
