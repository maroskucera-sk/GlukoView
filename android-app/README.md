# Android — GlukoView Insulin Log

Kotlin · Jetpack Compose · Material Design 3 · MVVM · Room · Retrofit/Ktor  
Jazyk UI: **slovenčina** · Dark Mode podľa systému / manuálne

## Stav

| Krok | Obsah                                                         | Stav        |
|------|---------------------------------------------------------------|-------------|
| 1    | Štruktúra projektu                                            | ✅ hotovo   |
| 3    | Gradle, navigácia, téma, modely, API + Nightscout klient      | ⏳ po schválení |
| 4    | Dashboard, Záznam, História, Nastavenia                       | ⏳ po schválení |

## Plánovaná štruktúra (krok 3+)

```
android-app/
├── app/
│   ├── build.gradle.kts
│   └── src/main/
│       ├── AndroidManifest.xml
│       ├── java/sk/glukoview/insulinlog/
│       │   ├── GlukoViewApp.kt
│       │   ├── MainActivity.kt
│       │   ├── ui/
│       │   │   ├── theme/          # Material 3, dark/light
│       │   │   ├── navigation/
│       │   │   ├── dashboard/
│       │   │   ├── dose/           # pridanie / úprava
│       │   │   ├── history/
│       │   │   └── settings/
│       │   ├── data/
│       │   │   ├── local/          # Room (cache + sync fronta)
│       │   │   ├── remote/         # Retrofit API
│       │   │   ├── nightscout/     # entries.json klient
│       │   │   └── repository/
│       │   └── domain/             # IOB, duplicita, modely
│       └── res/
├── gradle/
├── build.gradle.kts
├── settings.gradle.kts
└── README.md
```
