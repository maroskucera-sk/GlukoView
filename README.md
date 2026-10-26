# GlukoView

🌍 *[Read in English](#-english-version) | 🇸🇰 [Čítať po slovensky](#-slovenská-verzia)*

---

## 🇬🇧 English Version

**🚨 IMPORTANT NOTICE / MEDICAL DISCLAIMER**
*   **This is not a medical device.** Do not use the number on this display as the only basis for an insulin dose or a treatment decision. Always verify with your primary sensor or reader that you use with your doctor.
*   *The GlukoView project is independent open-source software. It is not affiliated with, sponsored by, endorsed by, or otherwise approved by Abbott (FreeStyle Libre), Dexcom, or Medtronic (Guardian). All trademarks and logos mentioned are the property of their respective owners.*

### About the Project
GlukoView is an open-source IoT desktop blood glucose monitor (not a smartwatch) designed to provide continuous visibility of blood sugar levels. It runs on Waveshare ESP32-S3 microcontrollers with a touch display. The project uses a single codebase to build two distinct firmwares for different display sizes.

The user interface is fully localized. The default language is Slovak, but English, Czech, German, Spanish, and French are available in the settings.

### Supported Data Sources
GlukoView can fetch data from multiple platforms. Please note that these integrations are **not official public APIs** (manufacturers may change them at any time) and their use is entirely at your own risk.
*   **Nightscout**
*   **Gluroo** uses the same Nightscout setting. It is not a separate source in the firmware. In the Gluroo app open Connections → DIY / Nightscout and copy the base URL (`https://something.ns.gluroo.com`) and the API Secret Token. Paste the base URL only, without `/api/v1` and without `?token=`. Gluroo allows about 5,000 requests per account per day; one display asks about once a minute. That token can also write data, so treat it like a password.
*   **Libre LinkUp**
*   **Dexcom Share**
*   **Guardian (CareLink)**

This code does not bypass the Dexcom follower-account signature.

### Supported Hardware

The firmware is optimized for the following Waveshare development boards. **Important:** Firmware built for one board must not be flashed onto the other.

| Board Model | PlatformIO Env | Display Mode | OTA Folder | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **[Waveshare ESP32-S3 1.28" round](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28)** (GC9A01) | `waveshare_s3_1_28` | **Solo** (1 user) | `update/solo/` | Circular design |
| **[Waveshare ESP32-S3-Touch-LCD-3.5B](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B)** | `waveshare_s3_3_5` | **Duo** (1–2 users) | `update/duo/` | Rectangular display, audio support |

*(Note for the 3.5B board: For manual upload, use the `waveshare_s3_3_5_manual` env and follow this sequence: hold BOOT → press RST → release BOOT).*

### Firmware Installation

#### 1. Web Installation (For regular users)
Users do not need Visual Studio or any developer tools. Installation is done directly via Chrome or Edge browser (requires Web Serial API support).
*   Go to the installation page: `https://maroskucera-sk.github.io/GlukoView/install/`
*   Select your version (Solo / Duo) and follow the on-screen instructions.
*   *Technical background:* The `docs/install/` page is hosted via GitHub Pages from the `main` branch. Binaries are automatically packed using `python tools/pack_web_install.py` from the compiled firmware. Web install is meant for the initial flash or "rescue" mode; regular updates are handled via OTA.

#### 2. For Developers (PlatformIO & GitHub Actions)
This project uses [PlatformIO](https://platformio.org/). Firmware is automatically built via GitHub Actions (`.github/workflows/firmware.yml`) on every push and PR.

CLI compilation and upload:
```bash
pio run -e waveshare_s3_1_28
pio run -e waveshare_s3_3_5

pio run -e waveshare_s3_1_28 -t upload --upload-port COMx
pio run -e waveshare_s3_3_5 -t upload --upload-port COMy
```

### First Setup & Captive Portal
1. **Connection:** Upon first boot (without WiFi), the board creates an open AP named `GlukoView` (no password).
2. **Configuration:** Connect to this AP from your phone or PC. A captive portal will open. Select your home network.
3. **Sources & Limits:** Once the board is on your LAN, visit its assigned IP address. Go to `/param` to configure data sources and glucose limits. For Guardian (Medtronic), go to `/carelink` (OAuth token, patient selection).
4. **Security:** Set up a **PIN code** directly on the touch display to lock the portal. Passwords are never populated into web forms for security reasons (leaving a password field empty keeps the stored password).
   * *Remove PIN:* Clears the portal lock.
   * *Delete WiFi:* Performs a complete factory reset.

#### CareLink Specifics
After a power loss or reboot, the CareLink connection restores automatically (refresh token + discovery). If it fails, log in again via `/carelink`.
* **Recommendation:** Create a separate "Care Partner" account specifically for GlukoView. Auth0 refresh token rotation only allows one active session per account, so you cannot share the same account on your phone and the monitor simultaneously.

### Security
*   **Encrypted storage:** WiFi credentials, Libre/Dexcom passwords, Nightscout tokens, and CareLink tokens are encrypted in the NVS (`dia-config`) using **AES-GCM**. The encryption key is uniquely derived from the chip's MAC address. The serial log only outputs string lengths, never the passwords.
*   More details: [SECURITY.md](SECURITY.md).

### OTA (Over-The-Air) Updates
*   Firmware is downloaded **only via HTTPS** and only if the digital signature matches. (The "TLS without verification" toggle in settings applies only to Nightscout servers, not OTA).
*   **Time restrictions:** To prevent disruptions, automatic background updates and reboots only happen during the day, following the active daytime brightness schedule (default 7:00 to 22:00). Manual updates via the menu work anytime.

**Custom OTA Server Setup:**
Requires `version.txt` (e.g., `1.1`), `firmware.bin`, and a signature `firmware.sig`.
The private key (`tools/ota_private.pem`) must **not** be committed to Git. The public key is in `include/dia_ota_pubkey.h`.
```bash
python tools/sign_ota.py .pio/build/waveshare_s3_1_28/firmware.bin -o firmware.sig
```

### License
MIT License — see [LICENSE](LICENSE).

---

## 🇸🇰 Slovenská verzia

**🚨 DÔLEŽITÉ UPOZORNENIE / MEDICAL DISCLAIMER**
*   **Toto nie je zdravotnícka pomôcka.** Hodnota na displeji je len orientačná. Nesmie byť jediným podkladom pre dávkovanie inzulínu ani pre akékoľvek rozhodnutia o liečbe. Vždy si hodnotu overte na svojom senzore alebo na certifikovanej čítačke, ktorú používate so svojím lekárom.
*   *Projekt GlukoView je nezávislý open-source softvér. Nie je nijako prepojený, sponzorovaný, podporovaný ani inak schválený spoločnosťami Abbott (FreeStyle Libre), Dexcom ani Medtronic (Guardian). Všetky uvedené ochranné známky a logá sú majetkom ich príslušných vlastníkov.*

### O projekte
GlukoView je open-source IoT stolný monitor glykémie (nie sú to hodinky), určený pre neustály a prehľadný dohľad nad hladinou cukru v krvi. Beží na mikrokontroléroch Waveshare ESP32-S3 s dotykovým displejom. Projekt využíva jednu spoločnú kódovú základňu, z ktorej sa generujú dva samostatné firmvéry pre rôzne veľkosti displejov. 

Používateľské rozhranie je plne lokalizované – základom je slovenčina, no v nastaveniach je dostupná aj čeština, angličtina, nemčina, španielčina a francúzština.

### Podporované zdroje dát
GlukoView dokáže sťahovať dáta z viacerých platforiem. Tieto integrácie **nie sú oficiálnymi verejnými API** daných služieb (výrobcovia ich môžu kedykoľvek zmeniť) a ich použitie je výhradne na vlastné riziko.
*   **Nightscout**
*   **Gluroo** ide cez to isté nastavenie Nightscout. Vo firmvéri to nie je samostatný zdroj. V aplikácii Gluroo otvor Connections → DIY / Nightscout a skopíruj základnú adresu (`https://nieco.ns.gluroo.com`) a API Secret Token. Do poľa adresy patrí len tá základná adresa, bez `/api/v1` a bez `?token=`. Gluroo povoľuje asi 5 000 požiadaviek na účet za deň; jeden displej sa pýta zhruba raz za minútu. Token vie údaje aj zapisovať, preto sa s ním zaobchádza ako s heslom.
*   **Libre LinkUp**
*   **Dexcom Share**
*   **Guardian (CareLink)**

Obídenie podpisu Dexcom follower účtu v tomto kóde nie je.

### Podporovaný hardvér

Kód je optimalizovaný pre nasledujúce vývojové dosky od Waveshare. **Dôležité:** Firmvér určený pre jednu dosku sa nesmie nahrať do druhej.

| Model dosky | PlatformIO env | Režim zobrazenia | OTA priečinok | Poznámka |
| :--- | :--- | :--- | :--- | :--- |
| **[Waveshare ESP32-S3 1.28" round](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28)** (GC9A01) | `waveshare_s3_1_28` | **Solo** (1 používateľ) | `update/solo/` | Kruhový dizajn |
| **[Waveshare ESP32-S3-Touch-LCD-3.5B](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B)** | `waveshare_s3_3_5` | **Duo** (1–2 používatelia) | `update/duo/` | Obdĺžnikový displej, zvuk |

*(Poznámka pre 3.5B dosku: V prípade nutnosti manuálneho uploadu použite env `waveshare_s3_3_5_manual` postupom: podržať BOOT → stlačiť RST → pustiť BOOT).*

### Inštalácia firmvéru

#### 1. Webová inštalácia (Pre bežných používateľov)
Pre používateľov, ktorí si GlukoView zakúpili, nie je potrebné inštalovať Visual Studio ani žiadne vývojárske nástroje. Inštalácia prebieha priamo cez prehliadače Chrome alebo Edge s podporou Web Serial API.
*   Prejdite na inštalačnú stránku: `https://maroskucera-sk.github.io/GlukoView/install/`
*   Vyberte si verziu (Solo / Duo) a postupujte podľa pokynov na obrazovke.
*   *Technické pozadie:* Stránka v `docs/install/` je hostovaná cez GitHub Pages z vetvy `main`. Binárne obrazy sa automaticky balia príkazom `python tools/pack_web_install.py` z preloženého firmvéru. Webová inštalácia slúži ako prvotné nahratie alebo "záchrana", inak sa zariadenie aktualizuje automaticky cez OTA.

#### 2. Pre vývojárov (PlatformIO a GitHub Actions)
Projekt využíva [PlatformIO](https://platformio.org/). O preklad firmvéru pri každom pushi alebo Pull Requeste sa stará GitHub Actions (`.github/workflows/firmware.yml`).

Kompilácia a upload z príkazového riadku:
```bash
pio run -e waveshare_s3_1_28
pio run -e waveshare_s3_3_5

pio run -e waveshare_s3_1_28 -t upload --upload-port COMx
pio run -e waveshare_s3_3_5 -t upload --upload-port COMy
```

### Prvé nastavenie a Captive Portal
1. **Pripojenie:** Po prvom zapnutí si doska vytvorí vlastnú otvorenú WiFi sieť s názvom `GlukoView` (bez hesla, čo je štandard pre Captive portály).
2. **Konfigurácia:** Pripojte sa k nej z mobilu alebo PC, otvorí sa webový portál. Zvoľte vašu domácu sieť a pripojte zariadenie na internet.
3. **Zdroje a limity:** Keď je doska vo vašej LAN, prejdite na jej pridelenú IP adresu. Na ceste `/param` nastavíte zdroje dát a limity glykémie. Pre Guardian (Medtronic) použite `/carelink` (OAuth token, výber pacienta).
4. **Zabezpečenie:** Priamo na dotykovom displeji si nastavte **PIN kód** pre prístup k portálu. Zadané heslá sa vo webových formulároch z bezpečnostných dôvodov nezobrazujú (ak pole necháte prázdne, zariadenie použije už uložené heslo).
   * *Zrušiť PIN:* Vymaže len zámok portálu.
   * *Zmazať WiFi:* Vykoná kompletný reset celého zariadenia.

#### Špecifikum pre CareLink
Po výpadku napájania alebo reštarte sa spojenie s CareLinkom obnovuje automaticky (refresh token + discovery). Ak by obnova zlyhala, je potrebné sa znova prihlásiť cez `/carelink`. 
* **Odporúčanie:** Vytvorte si samostatné "Care Partner" konto špeciálne pre GlukoView. Rotácia Auth0 tokenov neumožňuje zdieľať jednu aktívnu session súčasne na telefóne aj na monitore.

### Bezpečnosť
*   **Šifrovanie citlivých údajov:** WiFi heslá, prístupy do Libre/Dexcom, Nightscout tokeny a CareLink tokeny sú v internej pamäti (`dia-config` NVS oblasť) šifrované pomocou **AES-GCM**. Kľúč je unikátne odvodený z MAC adresy samotného čipu. Sériový log nevypisuje žiadne heslá, iba ich dĺžky.
*   Viac detailov o bezpečnosti nájdete v súbore [SECURITY.md](SECURITY.md).

### OTA (Over-The-Air) Aktualizácie
*   Firmvér sa sťahuje **výhradne cez HTTPS** a iba vtedy, ak sedí digitálny podpis. (Prepínač „TLS bez overenia“ v nastaveniach slúži len pre pripojenie k neoficiálnym Nightscout serverom, pre OTA neplatí).
*   **Časové obmedzenie:** Aby vás aktualizácia nerušila, automatická inštalácia a reštart prebiehajú len počas dňa – v rovnakom čase, kedy je aktívny denný jas displeja (predvolene 7:00 až 22:00). Ak nie je nastavený čas, OTA sa nespustí. Manuálna aktualizácia cez menu funguje kedykoľvek.

**Pre nasadenie vlastnej aktualizácie na server:**
Potrebujete súbory `version.txt` (napr. `1.1`), `firmware.bin` a podpisový súbor `firmware.sig`.
Súkromný kľúč (`tools/ota_private.pem`) **nepatrí** do gitu. Verejný kľúč je v `include/dia_ota_pubkey.h`.
```bash
python tools/sign_ota.py .pio/build/waveshare_s3_1_28/firmware.bin -o firmware.sig
```

### Licencia
MIT Licencia — pozri [LICENSE](LICENSE).