# Security Policy / Bezpečnostná politika

🌍 *[Read in English](#-english-version) | 🇸🇰 [Čítať po slovensky](#-slovenská-verzia)*

---

## 🇬🇧 English Version

### 1. Reporting a Vulnerability

If you discover a security vulnerability that could compromise passwords, tokens, or the PIN of a device on a local network, please report it privately.

**Do NOT open a public Issue with token dumps or exploit details.**
Instead, please contact the maintainer directly via **GitHub Security Advisory** or send an email to the address listed on the maintainer's GitHub profile. All legitimate security reports will be reviewed and addressed as quickly as possible.

### 2. Known Security Scope & Limitations (By Design)

GlukoView implements several security measures to protect user data, but it is an IoT device operating in a local environment. Please be aware of the following design choices and limitations:

*   **Setup network:** On first boot the board opens a Wi-Fi network named `GlukoView` with no password. Anyone nearby can join it until a home network is saved. After that, the configuration portal is on the home LAN.
*   **Portal PIN Lock:** The local web portal can be locked with a PIN. Only a SHA-256 hash of a random salt plus the PIN is stored, never the PIN itself. The PIN is exactly 4 digits, so someone who can read the chip can try all 10,000 values offline. Until a PIN is set, anyone on the same LAN can open the portal. After several wrong tries the portal pauses for 30 seconds; that pause is kept only in memory and is cleared by a reboot. A correct PIN starts a browser session that also ends on reboot.
*   **Data Encryption in NVS:** GlukoView encrypts Wi-Fi passwords, Libre and Dexcom passwords, the Nightscout or Gluroo token, and CareLink tokens with **AES-128-GCM**. Account names, the Nightscout address, and patient names are stored as plain text.
*   **Physical Access Caveat:** The encryption key is derived from the ESP32 Wi-Fi MAC address and a fixed label. Physical access to the board is enough to read the NVS partition and derive that key. The ESP32 Wi-Fi stack also keeps the current network password so it can reconnect. That copy is separate from the AES-GCM blob.
*   **OTA Updates:** Over-the-air updates are downloaded only over HTTPS and only when the ECDSA P-256 signature matches. The signing key (`tools/ota_private.pem`) must **never** be committed to this repository. The switch “TLS without verification” applies to Nightscout and the cloud logins, not to OTA.

### 3. Unofficial APIs (Disclaimer)

The integrations for Libre LinkUp, Dexcom Share, and Medtronic CareLink are based on reverse-engineered endpoints. Nightscout is an open project. Gluroo is used through its Nightscout-compatible link and token, not as a separate source in the firmware.

*   **No guarantees:** The manufacturers may change, restrict, or completely block access to these endpoints at any time without prior warning.
*   **Not official:** GlukoView is not an official application of Abbott, Dexcom, or Medtronic.
*   **Dexcom followers:** This code does not bypass the Dexcom follower-account signature. A follower login is not supported.
*   **Gluroo and Nightscout tokens** can read and write that account. Treat them like a password.
*   **Terms of Service:** Using these integrations is entirely at your own risk and may violate the Terms of Service of the respective manufacturers.

### 4. What NOT to Share in Issues or Pull Requests

To protect your personal and medical data, **never** post the following information in public GitHub Issues, discussions, or Pull Requests:

*   Passwords or refresh tokens for CareLink, Libre, Dexcom, Nightscout, or Gluroo.
*   Account emails, patient names, and Nightscout or Gluroo URLs.
*   The `logindata.json` file.
*   Raw serial logs, even when they look harmless. The firmware is written to print lengths rather than secrets, but a log can still contain a name, a URL, or a token.
*   The contents of your `tools/ota_private.pem` file.
*   A support report (`/diag`) is safe to share only after you check that it contains no name, URL, or token.

---

## 🇸🇰 Slovenská verzia

### 1. Nahlásenie zraniteľnosti (Reporting)

Ak objavíte bezpečnostnú chybu, ktorá by mohla ohroziť heslá, tokeny alebo PIN zariadenia v lokálnej sieti, nahláste ju, prosím, súkromne.

**NEVYTVÁRAJTE verejný Issue s dumpom tokenov alebo detailmi o zraniteľnosti.**
Namiesto toho kontaktujte maintainera priamo cez **GitHub Security Advisory** alebo e-mailom, ktorý nájdete v profile maintainera na GitHube. Všetky relevantné hlásenia o zraniteľnostiach budú preverené a vyriešené v čo najkratšom čase.

### 2. Známe obmedzenia a rozsah zabezpečenia (By Design)

GlukoView implementuje viaceré bezpečnostné prvky na ochranu údajov používateľa, no ide o IoT zariadenie fungujúce v domácej sieti. Berte na vedomie nasledujúce technické špecifiká a obmedzenia:

*   **Sieť pri prvom nastavení:** Po prvom zapnutí doska otvorí WiFi sieť `GlukoView` bez hesla. Kým sa neuloží domáca sieť, môže sa k nej pripojiť ktokoľvek nablízku. Potom je konfiguračný portál v domácej sieti.
*   **Zámok portálu (PIN):** Webový portál sa dá zamknúť PINom. V doske nie je uložený samotný PIN, ale SHA-256 z náhodnej soli a PINu. PIN má presne 4 číslice, takže kto vie prečítať čip, vie offline vyskúšať všetkých 10 000 možností. Kým PIN nie je nastavený, portál otvorí ktokoľvek v tej istej sieti. Po sérii zlých pokusov portál na 30 sekúnd čaká. Toto čakanie je len v pamäti a reštart ho zmaže. Správny PIN otvorí reláciu v prehliadači, ktorá reštartom tiež skončí.
*   **Šifrovanie dát v NVS:** GlukoView šifruje heslá WiFi, heslá Libre a Dexcom, token Nightscoutu alebo Gluroo a tokeny CareLink pomocou **AES-128-GCM**. Mená účtov, adresa Nightscoutu a mená pacientov sú v pamäti ako obyčajný text.
*   **Riziko fyzického prístupu:** Šifrovací kľúč je odvodený z WiFi MAC adresy čipu ESP32 a pevného označenia. Fyzický prístup k doske stačí na prečítanie partície NVS a odvodenie tohto kľúča. WiFi vrstva ESP32 si navyše pamätá heslo aktuálnej siete, aby sa vedela znova pripojiť. Táto kópia je oddelená od bloku AES-GCM.
*   **OTA aktualizácie:** Bezdrôtové aktualizácie sa sťahujú len cez HTTPS a len keď sedí podpis ECDSA P-256. Súkromný kľúč (`tools/ota_private.pem`) **nepatrí** do tohto gitu. Prepínač „TLS bez overenia“ platí pre Nightscout a prihlásenia do cloudu, nie pre OTA.

### 3. Neoficiálne API (Upozornenie)

Napojenia na Libre LinkUp, Dexcom Share a Medtronic CareLink sú postavené na tzv. reverse-engineeringu. Nightscout je otvorený projekt. Gluroo sa používa cez jeho odkaz a token v tvare Nightscout, nie ako samostatný zdroj vo firmvéri.

*   **Bez záruky:** Výrobcovia môžu tieto neoficiálne prístupy kedykoľvek zmeniť alebo zablokovať bez varovania.
*   **Nie sme oficiálna aplikácia:** GlukoView nie je oficiálnou aplikáciou spoločností Abbott, Dexcom ani Medtronic.
*   **Dexcom follower:** Tento kód neobchádza podpis follower účtu Dexcom. Prihlásenie followera nie je podporované.
*   **Token Gluroo a Nightscout** vie z účtu čítať aj doň zapisovať. Zaobchádza sa s ním ako s heslom.
*   **Podmienky používania:** Použitie týchto integrácií je výhradne na vlastné riziko a môže byť v rozpore s podmienkami služby daného výrobcu.

### 4. Čo NEPOSIELAŤ do verejných Issues a Pull Requestov

Pre ochranu vašich osobných a zdravotných údajov **nikdy** nezverejňujte nasledujúce informácie vo verejných Issues, diskusiách či Pull Requestoch:

*   Heslá a refresh tokeny pre CareLink, Libre, Dexcom, Nightscout alebo Gluroo.
*   E-maily účtov, mená pacientov a adresy Nightscoutu alebo Gluroo.
*   Súbor `logindata.json`.
*   Sériové logy, aj keď vyzerajú neškodne. Firmvér je písaný tak, aby vypisoval dĺžky, nie tajomstvá, ale v logu môže ostať meno, adresa alebo token.
*   Obsah súkromného kľúča `tools/ota_private.pem`.
*   Správa z `/diag` sa dá poslať až po kontrole, že v nej nie je meno, adresa ani token.
