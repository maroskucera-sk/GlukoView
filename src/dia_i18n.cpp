#include "dia_i18n.h"
#include "dia_config.h"

static constexpr int LANG_COUNT = 6;
static DiaLang s_lang = DiaLang::Sk;
static bool s_lang_ready = false;

static const char *const kNativeName[LANG_COUNT] = {
    "Slovenčina", "Čeština", "English", "Deutsch", "Español", "Français"
};
static const char *const kCode[LANG_COUNT] = {
    "SK", "CS", "EN", "DE", "ES", "FR"
};

static const char *const kText[LANG_COUNT][(int)Str::StrCount] = {
    {
        "Systém 1/4", "Informácie", "Obnoviť", "Auto-upd: ZAP",
        "Auto-upd: VYP", "Aktualizácia", "Factory reset", "Späť",
        "Limity 2/4", "Spodný", "Vrchný", "Kritické",
        "Jednotky: mg/dL", "Jednotky: mmol", "Delta: ZAP", "Delta: VYP",
        "Jas a čas 4/4", "Denný jas: %d", "Nočný jas: %d", "Deň od: %02d:00",
        "Noc od: %02d:00", "Uložiť", "Jazyk: SK", "Language: EN",
        "Denný jas", "Nočný jas", "Spodný (mg/dL)", "Spodný cieľ",
        "Vrchný (mg/dL)", "Vrchný cieľ", "Kritické (mg/dL)", "Kritické",
        "Začiatok dňa (h)", "Začiatok noci (h)", "IP\nWiFi\nTeplota\nVerzia", "Naozaj vymazať?",
        "Áno, reset", "Načítavam...", "Staré", "Sync...",
        "Aktualizujem... %d%%", "Kontrolujem...", "WiFi nie je\npripojené.", "Server\nnedostupný",
        "Máš najnovšiu\nverziu.", "Chyba sťahovania", "Aktualizujem... 0%", "Pripájam sa...",
        "WiFi znova", "Skenujem siete...", "Pripojené!",
        "1. Pripoj mobil na WiFi GlukoView\n"
        "2. V prehliadači napíš 10.1.1.1\n"
        "   a otvor stránku\n"
        "3. Vyber svoju WiFi a zadaj heslo",
        "Znova skúšam\nuložené siete...", "Ukladám a\nreštartujem...", "WiFi", "Auth",
        "Sieť", "Bez dát", "Config", "GlukoView",
        "Zdroj dát", "Vyber zdroj", "Nightscout", "API_SECRET z nastavenia NS, alebo access token z Admin Tools (readable-…).",
        "Nightscout URL", "Nightscout API secret", "Libre LinkUp", "1 Libre konto + 2 osoby: rovnaký e-mail/heslo na A aj B, rôzny pacient. 2 kontá: rôzne prihlasovacie údaje.",
        "E-mail / používateľ", "Heslo", "Pacient (meno alebo ID)", "Vyber meno. Zoznam sa doplní po prihlásení na Libre. Pri dual môžu mať A/B rôznych pacientov.",
        "Región", "Displej", "Jednotky", "TLS bez overenia",
        "Áno", "Nie", "Spodný cieľ (mmol)", "Vrchný cieľ (mmol)",
        "Kritické hyper (mmol)", "Guardian (CareLink)", "Prihlásenie raz na /carelink. 1 konto + 2 osoby = jeden login, rôzny pacient A/B. 2 kontá = login zvlášť pre B.", "Pacient Guardian",
        "Vyber po prihlásení CareLink. Pri dual režime môže mať A a B rôznych pacientov z rovnakého konta.", "CareLink JSON", "Uložiť", "Späť",
        "Uložené WiFi", "Uložené WiFi", "Až 5 sietí. Pri štarte a výpadku sa skúsia všetky. Nová sieť sa uloží po pripojení.", "Konfigurovať WiFi",
        "Späť do menu", "Uložené WiFi (až 5)", "Zatiaľ žiadne. Uložia sa po pripojení.", "(aktuálna)",
        "Jazyk", "(posledná)", "(bez hesla!)", "Zdroj: ZAP",
        "Zdroj: VYP", "Nová verzia.\nInštalujem...", "Dexcom Share", "Len keď je zvolené Dexcom. Prihlásenie z aplikácie Dexcom u pacienta, nie konto followera. Zdieľanie musí byť zapnuté.",
        "Používateľ A", "Používateľ B", "Meno", "Režim displeja",
        "1 — jeden používateľ", "2 — dvaja vedľa seba", "Vypnuté (off)", "Zvuk: ZAP",
        "Zvuk: VYP", "Snooze: %d min", "Stlmiť teraz", "Snooze %d m",
        "Zobraz: %s", "Režim: 1 osoba", "Režim: 2 osoby", "Tokeny A: uložené",
        "Tokeny A: chýbajú — prihlás na /carelink", "1 konto → rovnaké tokeny A, rôzny pacient A/B. 2 CareLink kontá → na /carelink prihlás zvlášť pre B.", "voliteľné: logindata.json — inak /carelink", "CareLink / Guardian",
        "Tokeny sú uložené.", "Zatiaľ žiadne tokeny — dokonči prihlásenie nižšie.", "Vyber pacienta", "Pre používateľa",
        "1 CareLink konto: prihlás raz (A), tu vyber rôznych pacientov pre A a B.", "Uložiť pacienta pre ", "Zoznam pacientov je prázdny.", "Pacientov sa nepodarilo načítať: ",
        "Čaká sa na kód z CareLink (platí ~10 min).", "Ako na to (mobil)", "<li>Vyber región a ťukni <b>1. Prihlásiť CareLink</b>.</li><li>Prihlás sa Care Partner účtom a vyrieš captcha.</li><li>Prehliadač ukáže chybu typu „nejde otvoriť aplikáciu“ — <b>to je v poriadku</b>.</li><li>Skopíruj odkaz z adresného riadku (začína <code>com.medtronic.carepartner</code>) alebo zdieľaj stránku → Kopírovať odkaz.</li><li>Vráť sa sem, vlož odkaz do poľa a ťukni <b>2. Uložiť tokeny</b>.</li><li>Vyber správneho pacienta vyššie.</li>", "Použi Care Partner (follower), nie pacientsky účet.",
        "Región", "EU / Slovensko / Európa", "USA", "Uložiť tokeny pre",
        "A (hlavné / zdieľané)", "B (len ak máš druhé CareLink konto)", "1. Prihlásiť CareLink", "Odkaz alebo kód po prihlásení",
        "2. Uložiť tokeny", "← Späť na parametre", "Menu", "Nevybral si pacienta.",
        "Pacient uložený (", "Späť na CareLink", "Parametre", "Krok 1 hotový",
        "Otvor CareLink, prihlás sa a vyrieš captcha.", "Otvoriť CareLink prihlásenie", "Po prihlásení uvidíš chybu o aplikácii — skopíruj odkaz (com.medtronic.carepartner…) a vlož ho nižšie.", "Vlož skopírovaný odkaz / kód",
        "Hotovo — tokeny uložené pre ", "Vybrať pacienta →", "Domov", "Skúsiť znova",
        "PIN: VYP", "PIN: ZAP", "Zrušiť PIN", "Zadaj 4 číslice",
        "Zopakuj PIN", "Nezhoduje sa", "Zrušiť len PIN?\nWiFi a účty ostanú.",
        "4 číslice z displeja", "Nesprávny PIN", "Veľa pokusov, počkaj",
        "CareLink odhlásený",
        "Nastavenie krok za krokom", "Koľko osôb bude na displeji?",
        "Obe osoby sú na jednom CareLink účte? Druhé prihlásenie len pre iné konto.",
        "Hotovo. Túto stránku v prehliadači môžeš zavrieť.",
        "Žiara: ZAP", "Žiara: VYP",
        "Pripájam...",
        "Čakám na server",
        "Vzhľad", "Vzhľad 3/4", "Zvuk", "Zvuk",
        "Tichšie", "Hlasnejšie", "Tichá", "Nízka", "Stredná", "Vysoká",
        "Otočenie: USB vpravo", "Otočenie: USB vľavo",
        "Rovnaká WiFi\nOtvor túto IP\na nastav zdroj",
        "Vyber, čo chceš nastaviť",
        "Parametre a zdroje",
        "Prihlásiť CareLink",
        "WiFi siete",
        "Sprievodca nastavením",
        "Správa pre podporu",
        "Pošli tento text. Nie sú v ňom heslá ani mená.",
        "Informácia",
        "GlukoView je len informatívny displej a neslúži na liečebné rozhodnutia. Hodnoty nesmú slúžiť na dávkovanie inzulínu.",
        "Projekt GlukoView je nezávislý open-source softvér. Nie je nijako prepojený, sponzorovaný, podporovaný ani inak schválený spoločnosťami Abbott (FreeStyle Libre), Dexcom ani Medtronic (Guardian). Všetky uvedené ochranné známky a logá sú majetkom ich príslušných vlastníkov.",
        "Rozumiem",
        "Pokračovať",
        "Podmienky: %s",
        "Súhlas",
        "Pred pripojením",
        "Beriem na vedomie, že GlukoView slúži výlučne ako informatívny displej a nie je zdravotníckou pomôckou. Zobrazené hodnoty nesmú byť použité na dávkovanie inzulínu ani na iné liečebné rozhodnutia. Na tieto účely sa zaväzujem používať výhradne primárnu aplikáciu alebo čítačku od výrobcu senzora.",
        "Beriem na vedomie obe uvedené skutočnosti",
        "Pokračovať",
        "Informatívny displej, nie zdravotnícka pomôcka. Nie na dávkovanie inzulínu ani na liečebné rozhodnutia.",
        "Beriem na vedomie, že GlukoView nie je určené na rozhodovanie o liečbe ani na dávkovanie inzulínu a nie je zdravotnícka pomôcka. Tento súhlas je v zariadení uložený.",
        "Súhlas nie je uložený. Zariadenie nie je určené na rozhodovanie o liečbe.",
        "GlukoView nie je zdravotnícka pomôcka. Údaje z neho neslúžia na dávkovanie inzulínu ani na iné medicínske rozhodnutia.",
        "Odsúhlasené",
        "Neodsúhlasené",
        "Zoznam pacientov sa sťahuje. Stránka sa o chvíľu sama obnoví. Potom vyber meno a ulož.",
        "Prihlásenie do Libre zlyhalo. Skontroluj e-mail, heslo a región EU.",
        "Libre nevrátilo žiadne mená. Použi účet z aplikácie LibreLinkUp, ten, ktorý tých ľudí vidí.",
        "Zoznam sa zatiaľ nestiahol. Obnov stránku ešte raz. Kým meno nevyberieš, zobrazuje sa prvý človek."
    },
    {
        "Systém 1/4", "Informace", "Obnovit", "Auto-upd: ZAP",
        "Auto-upd: VYP", "Aktualizace", "Factory reset", "Zpět",
        "Limity 2/4", "Spodní", "Horní", "Kritické",
        "Jednotky: mg/dL", "Jednotky: mmol", "Delta: ZAP", "Delta: VYP",
        "Jas a čas 4/4", "Denní jas: %d", "Noční jas: %d", "Den od: %02d:00",
        "Noc od: %02d:00", "Uložit", "Jazyk: SK", "Language: EN",
        "Denní jas", "Noční jas", "Spodní (mg/dL)", "Spodní cíl",
        "Horní (mg/dL)", "Horní cíl", "Kritické (mg/dL)", "Kritické",
        "Začátek dne (h)", "Začátek noci (h)", "IP\nWiFi\nTeplota\nVerze", "Opravdu smazat?",
        "Ano, reset", "Načítám...", "Staré", "Sync...",
        "Aktualizuji... %d%%", "Kontroluji...", "WiFi není\npřipojené.", "Server\nnedostupný",
        "Máte nejnovější\nverzi.", "Chyba stahování", "Aktualizuji... 0%", "Připojuji se...",
        "WiFi znovu", "Skenuji sítě...", "Připojeno!",
        "1. Připoj mobil k WiFi GlukoView\n"
        "2. V prohlížeči napiš 10.1.1.1\n"
        "   a otevři stránku\n"
        "3. Vyber svou WiFi a zadej heslo",
        "Znovu zkouším\nuložené sítě...", "Ukládám a\nrestartuji...", "WiFi", "Auth",
        "Síť", "Bez dat", "Config", "GlukoView",
        "Zdroj dat", "Vyber zdroj", "Nightscout", "API_SECRET z nastavení NS, nebo access token z Admin Tools (readable-…).",
        "Nightscout URL", "Nightscout API secret", "Libre LinkUp", "1 Libre účet + 2 osoby: stejný e-mail/heslo na A i B, jiný pacient. 2 účty: jiné přihlašovací údaje.",
        "E-mail / uživatel", "Heslo", "Pacient (jméno nebo ID)", "Vyber jméno. Seznam se doplní po přihlášení na Libre. V dual režimu mohou mít A/B různé pacienty.",
        "Region", "Displej", "Jednotky", "TLS bez ověření",
        "Ano", "Ne", "Spodní cíl (mmol)", "Horní cíl (mmol)",
        "Kritická hyper (mmol)", "Guardian (CareLink)", "Přihlášení jednou na /carelink. 1 účet + 2 osoby = jeden login, jiný pacient A/B. 2 účty = login zvlášť pro B.", "Pacient Guardian",
        "Vyber po přihlášení CareLink. V dual režimu mohou mít A a B různé pacienty ze stejného účtu.", "CareLink JSON", "Uložit", "Zpět",
        "Uložené WiFi", "Uložené WiFi", "Až 5 sítí. Při startu a výpadku se zkusí všechny. Nová síť se uloží po připojení.", "Konfigurovat WiFi",
        "Zpět do menu", "Uložené WiFi (až 5)", "Zatím žádné. Uloží se po připojení.", "(aktuální)",
        "Jazyk", "(poslední)", "(bez hesla!)", "Zdroj: ZAP",
        "Zdroj: VYP", "Nová verze.\nInstaluji...", "Dexcom Share", "Jen když je zvoleno Dexcom. Přihlášení z aplikace Dexcom u pacienta, ne účet followera. Sdílení musí být zapnuté.",
        "Uživatel A", "Uživatel B", "Jméno", "Režim displeje",
        "1 — jeden uživatel", "2 — dva vedle sebe", "Vypnuto (off)", "Zvuk: ZAP",
        "Zvuk: VYP", "Snooze: %d min", "Ztlumit teď", "Snooze %d m",
        "Zobraz: %s", "Režim: 1 osoba", "Režim: 2 osoby", "Tokeny A: uložené",
        "Tokeny A: chybí — přihlas na /carelink", "1 účet → stejné tokeny A, různý pacient A/B. 2 CareLink účty → na /carelink přihlas zvlášť pro B.", "volitelné: logindata.json — jinak /carelink", "CareLink / Guardian",
        "Tokeny jsou uložené.", "Zatím žádné tokeny — dokonči přihlášení níže.", "Vyber pacienta", "Pro uživatele",
        "1 CareLink účet: přihlas jednou (A), zde vyber různé pacienty pro A a B.", "Uložit pacienta pro ", "Seznam pacientů je prázdný.", "Pacienty se nepodařilo načíst: ",
        "Čeká se na kód z CareLink (platí ~10 min).", "Jak na to (mobil)", "<li>Vyber region a klepni na <b>1. Přihlásit CareLink</b>.</li><li>Přihlas se účtem Care Partner a vyřeš captcha.</li><li>Prohlížeč ukáže chybu typu „nelze otevřít aplikaci“ — <b>to je v pořádku</b>.</li><li>Zkopíruj odkaz z adresního řádku (začíná <code>com.medtronic.carepartner</code>) nebo Sdílet → Kopírovat odkaz.</li><li>Vrať se sem, vlož odkaz a klepni na <b>2. Uložit tokeny</b>.</li><li>Vyber správného pacienta výše.</li>", "Použij Care Partner (follower), ne pacientský účet.",
        "Region", "EU / Česko / Evropa", "USA", "Uložit tokeny pro",
        "A (hlavní / sdílené)", "B (jen pokud máš druhé CareLink konto)", "1. Přihlásit CareLink", "Odkaz nebo kód po přihlášení",
        "2. Uložit tokeny", "← Zpět na parametry", "Menu", "Nevybral jsi pacienta.",
        "Pacient uložen (", "Zpět na CareLink", "Parametry", "Krok 1 hotový",
        "Otevři CareLink, přihlas se a vyřeš captcha.", "Otevřít CareLink přihlášení", "Po přihlášení uvidíš chybu o aplikaci — zkopíruj odkaz (com.medtronic.carepartner…) a vlož ho níže.", "Vlož zkopírovaný odkaz / kód",
        "Hotovo — tokeny uložené pro ", "Vybrat pacienta →", "Domů", "Zkusit znovu",
        "PIN: VYP", "PIN: ZAP", "Zrušit PIN", "Zadej 4 číslice",
        "Zopakuj PIN", "Neshoduje se", "Zrušit jen PIN?\nWiFi a účty zůstanou.",
        "4 číslice z displeje", "Špatný PIN", "Moc pokusů, počkej",
        "CareLink odhlášen",
        "Nastavení krok za krokem", "Kolik osob bude na displeji?",
        "Obě osoby jsou na jednom CareLink účtu? Druhé přihlášení jen pro jiné konto.",
        "Hotovo. Tuto stránku v prohlížeči můžeš zavřít.",
        "Záře: ZAP", "Záře: VYP",
        "Připojuji...",
        "Čekám na server",
        "Vzhled", "Vzhled 3/4", "Zvuk", "Zvuk",
        "Tišeji", "Hlasitěji", "Tichá", "Nízká", "Střední", "Vysoká",
        "Otočení: USB vpravo", "Otočení: USB vlevo",
        "Stejná WiFi\nOtevři tuto IP\na nastav zdroj",
        "Vyber, co chceš nastavit",
        "Parametry a zdroje",
        "Přihlásit CareLink",
        "WiFi sítě",
        "Průvodce nastavením",
        "Zpráva pro podporu",
        "Pošli tento text. Nejsou v něm hesla ani jména.",
        "Informace",
        "GlukoView je jen informativní displej a neslouží k léčebným rozhodnutím. Hodnoty nesmí sloužit k dávkování inzulinu.",
        "Projekt GlukoView je nezávislý open-source software. Není nijak propojen, sponzorován, podporován ani jinak schválen společnostmi Abbott (FreeStyle Libre), Dexcom ani Medtronic (Guardian). Všechny uvedené ochranné známky a loga jsou majetkem jejich příslušných vlastníků.",
        "Rozumím",
        "Pokračovat",
        "Podmínky: %s",
        "Souhlas",
        "Před připojením",
        "Beru na vědomí, že GlukoView slouží výhradně jako informativní displej a není zdravotnickým prostředkem. Zobrazené hodnoty nesmí být použity k dávkování inzulinu ani k jiným léčebným rozhodnutím. K těmto účelům se zavazuji používat výhradně primární aplikaci nebo čtečku od výrobce senzoru.",
        "Beru na vědomí obě uvedené skutečnosti",
        "Pokračovat",
        "Informativní displej, ne zdravotnický prostředek. Ne k dávkování inzulinu ani k léčebným rozhodnutím.",
        "Beru na vědomí, že GlukoView není určeno k rozhodování o léčbě ani k dávkování inzulinu a není zdravotnický prostředek. Tento souhlas je v zařízení uložen.",
        "Souhlas není uložen. Zařízení není určeno k rozhodování o léčbě.",
        "GlukoView není zdravotnický prostředek. Údaje z něj neslouží k dávkování inzulinu ani k jiným medicínským rozhodnutím.",
        "Odsouhlaseno",
        "Neodsouhlaseno",
        "Seznam pacientů se stahuje. Stránka se za chvíli sama obnoví. Pak vyber jméno a ulož.",
        "Přihlášení do Libre selhalo. Zkontroluj e-mail, heslo a region EU.",
        "Libre nevrátilo žádná jména. Použij účet z aplikace LibreLinkUp, ten, který ty lidi vidí.",
        "Seznam se zatím nestáhl. Obnov stránku ještě jednou. Dokud jméno nevybereš, zobrazuje se první člověk."
    },
    {
        "System 1/4", "Info", "Refresh", "Auto-upd: ON",
        "Auto-upd: OFF", "Update", "Factory reset", "Back",
        "Limits 2/4", "Low", "High", "Critical",
        "Units: mg/dL", "Units: mmol", "Delta: ON", "Delta: OFF",
        "Light 4/4", "Day brightness: %d", "Night brightness: %d", "Day from: %02d:00",
        "Night from: %02d:00", "Save", "Jazyk: SK", "Language: EN",
        "Day brightness", "Night brightness", "Low (mg/dL)", "Low target",
        "High (mg/dL)", "High target", "Critical (mg/dL)", "Critical",
        "Day starts (h)", "Night starts (h)", "IP\nWiFi\nTemp\nVersion", "Really erase?",
        "Yes, reset", "Loading...", "Old", "Sync...",
        "Updating... %d%%", "Checking...", "WiFi is not\nconnected.", "Server\nunavailable",
        "You have the\nlatest version.", "Download failed", "Updating... 0%", "Connecting...",
        "WiFi again", "Scanning...", "Connected!",
        "1. Connect phone to WiFi GlukoView\n"
        "2. In browser type 10.1.1.1\n"
        "   and open the page\n"
        "3. Pick your WiFi and enter password",
        "Retrying saved\nnetworks...", "Saving and\nrestarting...", "WiFi", "Auth",
        "Network", "No data", "Config", "GlukoView",
        "Data source", "Choose source", "Nightscout", "API_SECRET from NS settings, or an access token from Admin Tools (readable-…).",
        "Nightscout URL", "Nightscout API secret", "Libre LinkUp", "One Libre account + 2 people: same email/password on A and B, different patient. Two accounts: different logins.",
        "Email / user", "Password", "Patient (name or ID)", "Pick a name. List fills after Libre login. In dual mode A/B can use different patients.",
        "Region", "Display", "Units", "Skip TLS verify",
        "Yes", "No", "Low target (mmol)", "High target (mmol)",
        "Critical high (mmol)", "Guardian (CareLink)", "Sign in once on /carelink. One account + 2 people = one login, different patient A/B. Two accounts = separate login for B.", "Guardian patient",
        "Pick after CareLink sign-in. In dual mode A and B can use different patients from the same account.", "CareLink JSON", "Save", "Back",
        "Saved WiFi", "Saved WiFi", "Up to 5 networks. All of them are tried at boot and after a dropout. A new network is saved after it connects.", "Configure WiFi",
        "Back to menu", "Saved WiFi (up to 5)", "None yet. They are saved after a connection.", "(current)",
        "Language", "(last)", "(no password!)", "Source: ON",
        "Source: OFF", "New version.\nInstalling...", "Dexcom Share", "Used only when Dexcom is selected. Use the patient's Dexcom app login, not a follower account. Sharing must be on.",
        "User A", "User B", "Name", "Display mode",
        "1 — single user", "2 — two side by side", "Off", "Sound: ON",
        "Sound: OFF", "Snooze: %d min", "Mute now", "Snooze %d m",
        "Show: %s", "Mode: 1 person", "Mode: 2 people", "Tokens A: saved",
        "Tokens A: missing — sign in at /carelink", "1 account → shared A tokens, different patient A/B. 2 CareLink accounts → sign in separately for B at /carelink.", "optional: logindata.json — otherwise use /carelink", "CareLink / Guardian",
        "Tokens are saved.", "No tokens yet — finish sign-in below.", "Pick a patient", "For user",
        "1 CareLink account: sign in once (A), then pick different patients for A and B here.", "Save patient for ", "Patient list is empty.", "Could not load patients: ",
        "Waiting for CareLink code (valid ~10 min).", "How to (phone)", "<li>Pick a region and tap <b>1. Sign in to CareLink</b>.</li><li>Sign in with a Care Partner account and solve the captcha.</li><li>The browser shows an error like “can’t open the app” — <b>that is OK</b>.</li><li>Copy the link from the address bar (starts with <code>com.medtronic.carepartner</code>) or Share page → Copy link.</li><li>Come back here, paste the link and tap <b>2. Save tokens</b>.</li><li>Pick the correct patient above.</li>", "Use a Care Partner (follower) account, not the patient account.",
        "Region", "EU / Slovakia / Europe", "USA", "Save tokens for",
        "A (main / shared)", "B (only if you have a second CareLink account)", "1. Sign in to CareLink", "Link or code after sign-in",
        "2. Save tokens", "← Back to parameters", "Menu", "You did not pick a patient.",
        "Patient saved (", "Back to CareLink", "Parameters", "Step 1 done",
        "Open CareLink, sign in and solve the captcha.", "Open CareLink sign-in", "After sign-in you will see an app error — copy the link (com.medtronic.carepartner…) and paste it below.", "Paste the copied link / code",
        "Done — tokens saved for ", "Pick patient →", "Home", "Try again",
        "PIN: OFF", "PIN: ON", "Clear PIN", "Enter 4 digits",
        "Repeat the PIN", "Does not match", "Clear only the PIN?\nWiFi and accounts stay.",
        "4 digits from the display", "Wrong PIN", "Too many tries, wait",
        "CareLink signed out",
        "Step-by-step setup", "How many people on the display?",
        "Both people on one CareLink account? A second login only for a different account.",
        "Done. You can close this page in the browser.",
        "Glow: ON", "Glow: OFF",
        "Connecting...",
        "Waiting for server",
        "Look", "Look 3/4", "Sound", "Sound",
        "Quieter", "Louder", "Quiet", "Low", "Medium", "High",
        "Rotate: USB right", "Rotate: USB left",
        "Same WiFi\nOpen this IP\nand set the source",
        "Choose what to set up",
        "Parameters & sources",
        "Sign in to CareLink",
        "WiFi networks",
        "Setup wizard",
        "Support report",
        "Send this text. It has no passwords or names.",
        "Notice",
        "GlukoView is an informational display only and is not for treatment decisions. Readings must not be used to dose insulin.",
        "The GlukoView project is independent open-source software. It is not affiliated with, sponsored by, endorsed by, or otherwise approved by Abbott (FreeStyle Libre), Dexcom, or Medtronic (Guardian). All trademarks and logos mentioned are the property of their respective owners.",
        "I understand",
        "Continue",
        "Notice: %s",
        "Accepted",
        "Before connecting",
        "I acknowledge that GlukoView is solely an informational display and is not a medical device. Displayed values must not be used for insulin dosing or other treatment decisions. For those purposes I will use only the sensor maker's primary app or reader.",
        "I acknowledge both statements above",
        "Continue",
        "Informational display, not a medical device. Not for insulin dosing or treatment decisions.",
        "I acknowledge that GlukoView is not for treatment decisions or insulin dosing and is not a medical device. This acknowledgment is stored on the device.",
        "Acknowledgment is not stored. The device is not for treatment decisions.",
        "GlukoView is not a medical device. Its readings are not for insulin dosing or other medical decisions.",
        "Acknowledged",
        "Not acknowledged",
        "The patient list is loading. This page will refresh shortly. Then pick a name and save.",
        "Libre sign-in failed. Check the email, password and the EU region.",
        "Libre returned no names. Use the LibreLinkUp account that can see those people.",
        "The list has not arrived yet. Refresh the page once more. Until you pick a name, the first person is shown."
    },
    {
        "System 1/4", "Info", "Aktualisieren", "Auto-upd: AN",
        "Auto-upd: AUS", "Update", "Factory reset", "Zurück",
        "Grenzen 2/4", "Niedrig", "Hoch", "Kritisch",
        "Einheiten: mg/dL", "Einheiten: mmol", "Delta: AN", "Delta: AUS",
        "Helligkeit 4/4", "Taghelligkeit: %d", "Nachthelligkeit: %d", "Tag ab: %02d:00",
        "Nacht ab: %02d:00", "Speichern", "Jazyk: SK", "Language: EN",
        "Taghelligkeit", "Nachthelligkeit", "Niedrig (mg/dL)", "Untergrenze",
        "Hoch (mg/dL)", "Obergrenze", "Kritisch (mg/dL)", "Kritisch",
        "Tagesbeginn (h)", "Nachtbeginn (h)", "IP\nWiFi\nTemp\nVersion", "Wirklich löschen?",
        "Ja, Reset", "Laden...", "Alt", "Sync...",
        "Aktualisiere... %d%%", "Prüfe...", "WiFi ist nicht\nverbunden.", "Server\nnicht erreichbar",
        "Sie haben die\nneueste Version.", "Download fehlgeschlagen", "Aktualisiere... 0%", "Verbinde...",
        "WiFi erneut", "Scanne...", "Verbunden!",
        "1. Handy mit WiFi GlukoView verbinden\n"
        "2. Im Browser 10.1.1.1 tippen\n"
        "   und Seite öffnen\n"
        "3. Dein WiFi wählen und Passwort eingeben",
        "Gespeicherte Netze\nwerden erneut versucht...", "Speichern und\nNeustart...", "WiFi", "Auth",
        "Netz", "Keine Daten", "Config", "GlukoView",
        "Datenquelle", "Quelle wählen", "Nightscout", "API_SECRET aus den NS-Einstellungen oder Access-Token aus Admin Tools (readable-…).",
        "Nightscout-URL", "Nightscout API secret", "Libre LinkUp", "1 Libre-Konto + 2 Personen: gleiche E-Mail/Passwort für A und B, anderer Patient. 2 Konten: unterschiedliche Logins.",
        "E-Mail / Benutzer", "Passwort", "Patient (Name oder ID)", "Namen wählen. Liste erscheint nach Libre-Login. Im Dual-Modus können A/B verschiedene Patienten nutzen.",
        "Region", "Display", "Einheiten", "TLS-Prüfung überspringen",
        "Ja", "Nein", "Untergrenze (mmol)", "Obergrenze (mmol)",
        "Kritisch hoch (mmol)", "Guardian (CareLink)", "Einmal anmelden unter /carelink. 1 Konto + 2 Personen = ein Login, anderer Patient A/B. 2 Konten = separates Login für B.", "Guardian-Patient",
        "Nach CareLink-Anmeldung wählen. Im Dual-Modus können A und B verschiedene Patienten desselben Kontos nutzen.", "CareLink-JSON", "Speichern", "Zurück",
        "Gespeicherte WiFi", "Gespeicherte WiFi", "Bis zu 5 Netze. Beim Start und bei Ausfall werden alle versucht. Neues Netz wird nach Verbindung gespeichert.", "WiFi konfigurieren",
        "Zurück zum Menü", "Gespeicherte WiFi (bis 5)", "Noch keine. Werden nach Verbindung gespeichert.", "(aktuell)",
        "Sprache", "(letzte)", "(kein Passwort!)", "Quelle: AN",
        "Quelle: AUS", "Neue Version.\nInstalliere...", "Dexcom Share", "Nur bei gewähltem Dexcom. Login aus der Dexcom-App des Patienten, kein Follower-Konto. Teilen muss aktiv sein.",
        "Benutzer A", "Benutzer B", "Name", "Anzeigemodus",
        "1 — ein Benutzer", "2 — zwei nebeneinander", "Aus", "Ton: AN",
        "Ton: AUS", "Snooze: %d min", "Jetzt stummschalten", "Snooze %d m",
        "Anzeige: %s", "Modus: 1 Person", "Modus: 2 Personen", "Tokens A: gespeichert",
        "Tokens A: fehlen — anmelden unter /carelink", "1 Konto → gemeinsame A-Tokens, anderer Patient A/B. 2 CareLink-Konten → unter /carelink separat für B anmelden.", "optional: logindata.json — sonst /carelink", "CareLink / Guardian",
        "Tokens sind gespeichert.", "Noch keine Tokens — Anmeldung unten abschließen.", "Patient wählen", "Für Benutzer",
        "1 CareLink-Konto: einmal anmelden (A), hier verschiedene Patienten für A und B wählen.", "Patient speichern für ", "Patientenliste ist leer.", "Patienten konnten nicht geladen werden: ",
        "Warte auf CareLink-Code (gültig ~10 Min.).", "Anleitung (Handy)", "<li>Region wählen und <b>1. Bei CareLink anmelden</b> tippen.</li><li>Mit Care-Partner-Konto anmelden und Captcha lösen.</li><li>Der Browser zeigt einen Fehler wie „App kann nicht geöffnet werden“ — <b>das ist in Ordnung</b>.</li><li>Link aus der Adresszeile kopieren (beginnt mit <code>com.medtronic.carepartner</code>) oder Seite teilen → Link kopieren.</li><li>Hierher zurück, Link einfügen und <b>2. Tokens speichern</b> tippen.</li><li>Oben den richtigen Patienten wählen.</li>", "Care-Partner-(Follower-)Konto nutzen, nicht das Patientenkonto.",
        "Region", "EU / Slowakei / Europa", "USA", "Tokens speichern für",
        "A (Haupt / gemeinsam)", "B (nur bei zweitem CareLink-Konto)", "1. Bei CareLink anmelden", "Link oder Code nach Anmeldung",
        "2. Tokens speichern", "← Zurück zu Parametern", "Menü", "Kein Patient gewählt.",
        "Patient gespeichert (", "Zurück zu CareLink", "Parameter", "Schritt 1 fertig",
        "CareLink öffnen, anmelden und Captcha lösen.", "CareLink-Anmeldung öffnen", "Nach der Anmeldung erscheint ein App-Fehler — Link kopieren (com.medtronic.carepartner…) und unten einfügen.", "Kopierten Link / Code einfügen",
        "Fertig — Tokens gespeichert für ", "Patient wählen →", "Start", "Erneut versuchen",
        "PIN: AUS", "PIN: EIN", "PIN löschen", "4 Ziffern eingeben",
        "PIN wiederholen", "Stimmt nicht", "Nur die PIN löschen?\nWLAN und Konten bleiben.",
        "4 Ziffern vom Display", "Falsche PIN", "Zu viele Versuche",
        "CareLink abgemeldet",
        "Einrichtung Schritt für Schritt", "Wie viele Personen auf dem Display?",
        "Beide Personen auf einem CareLink-Konto? Zweites Login nur für ein anderes Konto.",
        "Fertig. Du kannst diese Seite im Browser schließen.",
        "Schein: AN", "Schein: AUS",
        "Verbinde...",
        "Warte auf Server",
        "Aussehen", "Aussehen 3/4", "Ton", "Ton",
        "Leiser", "Lauter", "Leise", "Niedrig", "Mittel", "Laut",
        "Drehung: USB rechts", "Drehung: USB links",
        "Gleiches WiFi\nDiese IP öffnen\nund Quelle einstellen",
        "Wähle, was du einstellen willst",
        "Parameter & Quellen",
        "Bei CareLink anmelden",
        "WiFi-Netzwerke",
        "Einrichtungsassistent",
        "Bericht für den Support",
        "Schick diesen Text. Er enthält keine Passwörter und keine Namen.",
        "Hinweis",
        "GlukoView ist nur eine informative Anzeige und dient nicht für Behandlungsentscheidungen. Werte dürfen nicht zur Insulindosierung verwendet werden.",
        "Das Projekt GlukoView ist unabhängige Open-Source-Software. Es ist in keiner Weise mit Abbott (FreeStyle Libre), Dexcom oder Medtronic (Guardian) verbunden, von ihnen gesponsert, unterstützt oder anderweitig genehmigt. Alle genannten Marken und Logos sind Eigentum ihrer jeweiligen Inhaber.",
        "Ich verstehe",
        "Weiter",
        "Hinweis: %s",
        "Zugestimmt",
        "Vor dem Verbinden",
        "Ich nehme zur Kenntnis, dass GlukoView ausschließlich als informative Anzeige dient und kein Medizinprodukt ist. Angezeigte Werte dürfen nicht zur Insulindosierung oder für andere Behandlungsentscheidungen verwendet werden. Dafür verwende ich ausschließlich die App oder das Lesegerät des Sensorherstellers.",
        "Ich nehme beide genannten Punkte zur Kenntnis",
        "Weiter",
        "Nur informative Anzeige, kein Medizinprodukt. Nicht zur Insulindosierung oder für Behandlungsentscheidungen.",
        "Ich nehme zur Kenntnis, dass GlukoView nicht für Behandlungsentscheidungen oder die Insulindosierung bestimmt ist und kein Medizinprodukt ist. Diese Zustimmung ist im Gerät gespeichert.",
        "Zustimmung ist nicht gespeichert. Das Gerät ist nicht für Behandlungsentscheidungen bestimmt.",
        "GlukoView ist kein Medizinprodukt. Die Angaben dienen nicht zur Insulindosierung und nicht zu anderen medizinischen Entscheidungen.",
        "Zugestimmt",
        "Nicht zugestimmt",
        "Die Patientenliste wird geladen. Die Seite aktualisiert sich gleich. Dann einen Namen wählen und speichern.",
        "Libre-Anmeldung fehlgeschlagen. E-Mail, Passwort und Region EU prüfen.",
        "Libre hat keine Namen geliefert. Das LibreLinkUp-Konto verwenden, das diese Personen sieht.",
        "Die Liste ist noch nicht da. Seite noch einmal neu laden. Ohne Auswahl wird die erste Person angezeigt."
    },
    {
        "Sistema 1/4", "Info", "Actualizar", "Auto-upd: ON",
        "Auto-upd: OFF", "Actualización", "Factory reset", "Atrás",
        "Límites 2/4", "Bajo", "Alto", "Crítico",
        "Unidades: mg/dL", "Unidades: mmol", "Delta: ON", "Delta: OFF",
        "Brillo 4/4", "Brillo día: %d", "Brillo noche: %d", "Día desde: %02d:00",
        "Noche desde: %02d:00", "Guardar", "Jazyk: SK", "Language: EN",
        "Brillo día", "Brillo noche", "Bajo (mg/dL)", "Objetivo bajo",
        "Alto (mg/dL)", "Objetivo alto", "Crítico (mg/dL)", "Crítico",
        "Inicio del día (h)", "Inicio de la noche (h)", "IP\nWiFi\nTemp\nVersión", "¿Borrar?",
        "Sí, reset", "Cargando...", "Antiguo", "Sync...",
        "Actualizando... %d%%", "Comprobando...", "WiFi no está\nconectado.", "Servidor\nno disponible",
        "Tienes la\núltima versión.", "Error de descarga", "Actualizando... 0%", "Conectando...",
        "WiFi de nuevo", "Escaneando...", "¡Conectado!",
        "1. Conecta el móvil a WiFi GlukoView\n"
        "2. En el navegador escribe 10.1.1.1\n"
        "   y abre la página\n"
        "3. Elige tu WiFi y escribe la contraseña",
        "Reintentando redes\nguardadas...", "Guardando y\nreiniciando...", "WiFi", "Auth",
        "Red", "Sin datos", "Config", "GlukoView",
        "Fuente de datos", "Elige fuente", "Nightscout", "API_SECRET de la config de NS, o access token de Admin Tools (readable-…).",
        "URL Nightscout", "Nightscout API secret", "Libre LinkUp", "1 cuenta Libre + 2 personas: mismo email/contraseña en A y B, paciente distinto. 2 cuentas: inicios distintos.",
        "Email / usuario", "Contraseña", "Paciente (nombre o ID)", "Elige un nombre. La lista se rellena tras iniciar sesión en Libre. En dual A/B pueden usar pacientes distintos.",
        "Región", "Pantalla", "Unidades", "Omitir verificación TLS",
        "Sí", "No", "Objetivo bajo (mmol)", "Objetivo alto (mmol)",
        "Hiper crítica (mmol)", "Guardian (CareLink)", "Inicia sesión una vez en /carelink. 1 cuenta + 2 personas = un login, paciente A/B distinto. 2 cuentas = login aparte para B.", "Paciente Guardian",
        "Elige tras iniciar sesión en CareLink. En dual A y B pueden usar pacientes distintos de la misma cuenta.", "JSON CareLink", "Guardar", "Atrás",
        "WiFi guardadas", "WiFi guardadas", "Hasta 5 redes. Al arrancar y tras un corte se prueban todas. Una red nueva se guarda al conectar.", "Configurar WiFi",
        "Volver al menú", "WiFi guardadas (hasta 5)", "Ninguna aún. Se guardan tras conectar.", "(actual)",
        "Idioma", "(última)", "(¡sin contraseña!)", "Fuente: ON",
        "Fuente: OFF", "Nueva versión.\nInstalando...", "Dexcom Share", "Solo si eliges Dexcom. Usa el login de la app Dexcom del paciente, no una cuenta follower. El compartir debe estar activo.",
        "Usuario A", "Usuario B", "Nombre", "Modo de pantalla",
        "1 — un usuario", "2 — dos lado a lado", "Apagado", "Sonido: ON",
        "Sonido: OFF", "Snooze: %d min", "Silenciar ahora", "Snooze %d m",
        "Mostrar: %s", "Modo: 1 persona", "Modo: 2 personas", "Tokens A: guardados",
        "Tokens A: faltan — inicia sesión en /carelink", "1 cuenta → tokens A compartidos, paciente A/B distinto. 2 cuentas CareLink → inicia sesión aparte para B en /carelink.", "opcional: logindata.json — si no, /carelink", "CareLink / Guardian",
        "Los tokens están guardados.", "Aún no hay tokens — termina el inicio de sesión abajo.", "Elige un paciente", "Para el usuario",
        "1 cuenta CareLink: inicia sesión una vez (A), aquí elige pacientes distintos para A y B.", "Guardar paciente para ", "La lista de pacientes está vacía.", "No se pudieron cargar pacientes: ",
        "Esperando código de CareLink (válido ~10 min).", "Cómo hacerlo (móvil)", "<li>Elige la región y pulsa <b>1. Iniciar sesión en CareLink</b>.</li><li>Inicia sesión con una cuenta Care Partner y resuelve el captcha.</li><li>El navegador muestra un error tipo “no se puede abrir la app” — <b>está bien</b>.</li><li>Copia el enlace de la barra de direcciones (empieza por <code>com.medtronic.carepartner</code>) o Compartir → Copiar enlace.</li><li>Vuelve aquí, pega el enlace y pulsa <b>2. Guardar tokens</b>.</li><li>Elige el paciente correcto arriba.</li>", "Usa una cuenta Care Partner (follower), no la del paciente.",
        "Región", "UE / Eslovaquia / Europa", "EE. UU.", "Guardar tokens para",
        "A (principal / compartida)", "B (solo si tienes una segunda cuenta CareLink)", "1. Iniciar sesión en CareLink", "Enlace o código tras iniciar sesión",
        "2. Guardar tokens", "← Volver a parámetros", "Menú", "No elegiste un paciente.",
        "Paciente guardado (", "Volver a CareLink", "Parámetros", "Paso 1 listo",
        "Abre CareLink, inicia sesión y resuelve el captcha.", "Abrir inicio de sesión CareLink", "Tras iniciar sesión verás un error de la app — copia el enlace (com.medtronic.carepartner…) y pégalo abajo.", "Pega el enlace / código copiado",
        "Listo — tokens guardados para ", "Elegir paciente →", "Inicio", "Reintentar",
        "PIN: NO", "PIN: SÍ", "Borrar PIN", "Escribe 4 dígitos",
        "Repite el PIN", "No coincide", "¿Borrar solo el PIN?\nWiFi y cuentas siguen.",
        "4 dígitos de la pantalla", "PIN incorrecto", "Demasiados intentos",
        "CareLink desconectado",
        "Configuración paso a paso", "¿Cuántas personas en la pantalla?",
        "¿Las dos personas en una cuenta CareLink? El segundo acceso solo para otra cuenta.",
        "Listo. Puedes cerrar esta página en el navegador.",
        "Brillo: ON", "Brillo: OFF",
        "Conectando...",
        "Espero al servidor",
        "Aspecto", "Aspecto 3/4", "Sonido", "Sonido",
        "Más bajo", "Más alto", "Suave", "Baja", "Media", "Alta",
        "Giro: USB derecha", "Giro: USB izquierda",
        "Misma WiFi\nAbre esta IP\ny configura la fuente",
        "Elige qué configurar",
        "Parámetros y fuentes",
        "Iniciar sesión en CareLink",
        "Redes WiFi",
        "Asistente de configuración",
        "Informe para soporte",
        "Envía este texto. No tiene contraseñas ni nombres.",
        "Aviso",
        "GlukoView es solo una pantalla informativa y no sirve para decisiones de tratamiento. Los valores no deben usarse para dosificar insulina.",
        "El proyecto GlukoView es software de código abierto independiente. No está vinculado, patrocinado, respaldado ni aprobado de ningún modo por Abbott (FreeStyle Libre), Dexcom ni Medtronic (Guardian). Todas las marcas y los logotipos mencionados son propiedad de sus respectivos titulares.",
        "Lo entiendo",
        "Continuar",
        "Aviso: %s",
        "Aceptado",
        "Antes de conectar",
        "Reconozco que GlukoView sirve únicamente como pantalla informativa y no es un producto sanitario. Los valores mostrados no deben usarse para dosificar insulina ni para otras decisiones de tratamiento. Para ello me comprometo a usar solo la aplicación o el lector del fabricante del sensor.",
        "Reconozco ambas afirmaciones anteriores",
        "Continuar",
        "Pantalla informativa, no es un producto sanitario. No para dosificar insulina ni para decisiones de tratamiento.",
        "Reconozco que GlukoView no está destinado a decisiones de tratamiento ni a dosificar insulina y no es un producto sanitario. Este consentimiento está guardado en el dispositivo.",
        "El consentimiento no está guardado. El dispositivo no es para decisiones de tratamiento.",
        "GlukoView no es un producto sanitario. Sus datos no sirven para dosificar insulina ni para otras decisiones médicas.",
        "Aceptado",
        "No aceptado",
        "La lista de pacientes se está descargando. La página se actualizará sola. Luego elige un nombre y guarda.",
        "El inicio de sesión en Libre falló. Revisa el correo, la contraseña y la región EU.",
        "Libre no devolvió nombres. Usa la cuenta de LibreLinkUp que ve a esas personas.",
        "La lista aún no ha llegado. Actualiza la página otra vez. Hasta que elijas un nombre, se muestra la primera persona."
    },
    {
        "Système 1/4", "Infos", "Actualiser", "Auto-upd: ON",
        "Auto-upd: OFF", "Mise à jour", "Factory reset", "Retour",
        "Limites 2/4", "Bas", "Haut", "Critique",
        "Unités: mg/dL", "Unités: mmol", "Delta: ON", "Delta: OFF",
        "Luminosité 4/4", "Luminosité jour: %d", "Luminosité nuit: %d", "Jour dès: %02d:00",
        "Nuit dès: %02d:00", "Enregistrer", "Jazyk: SK", "Language: EN",
        "Luminosité jour", "Luminosité nuit", "Bas (mg/dL)", "Objectif bas",
        "Haut (mg/dL)", "Objectif haut", "Critique (mg/dL)", "Critique",
        "Début du jour (h)", "Début de la nuit (h)", "IP\nWiFi\nTemp\nVersion", "Effacer ?",
        "Oui, reset", "Chargement...", "Ancien", "Sync...",
        "Mise à jour... %d%%", "Vérification...", "WiFi non\nconnecté.", "Serveur\nindisponible",
        "Vous avez la\ndernière version.", "Échec du téléchargement", "Mise à jour... 0%", "Connexion...",
        "WiFi à nouveau", "Scan...", "Connecté !",
        "1. Connecte le téléphone au WiFi GlukoView\n"
        "2. Dans le navigateur tape 10.1.1.1\n"
        "   et ouvre la page\n"
        "3. Choisis ton WiFi et saisis le mot de passe",
        "Nouveaux essais des\nréseaux enregistrés...", "Enregistrement et\nredémarrage...", "WiFi", "Auth",
        "Réseau", "Pas de données", "Config", "GlukoView",
        "Source de données", "Choisir la source", "Nightscout", "API_SECRET des réglages NS, ou access token des Admin Tools (readable-…).",
        "URL Nightscout", "Nightscout API secret", "Libre LinkUp", "1 compte Libre + 2 personnes: même e-mail/mot de passe pour A et B, patient différent. 2 comptes: identifiants différents.",
        "E-mail / utilisateur", "Mot de passe", "Patient (nom ou ID)", "Choisissez un nom. La liste se remplit après connexion Libre. En dual A/B peuvent avoir des patients différents.",
        "Région", "Affichage", "Unités", "Ignorer la vérif. TLS",
        "Oui", "Non", "Objectif bas (mmol)", "Objectif haut (mmol)",
        "Hyper critique (mmol)", "Guardian (CareLink)", "Connexion une fois sur /carelink. 1 compte + 2 personnes = un login, patient A/B différent. 2 comptes = login séparé pour B.", "Patient Guardian",
        "Choisir après connexion CareLink. En dual A et B peuvent utiliser des patients différents du même compte.", "JSON CareLink", "Enregistrer", "Retour",
        "WiFi enregistrés", "WiFi enregistrés", "Jusqu’à 5 réseaux. Au démarrage et après une coupure, tous sont essayés. Un nouveau réseau est enregistré après connexion.", "Configurer le WiFi",
        "Retour au menu", "WiFi enregistrés (max 5)", "Aucun pour l’instant. Enregistrés après connexion.", "(actuel)",
        "Langue", "(dernier)", "(sans mot de passe !)", "Source: ON",
        "Source: OFF", "Nouvelle version.\nInstallation...", "Dexcom Share", "Uniquement si Dexcom est sélectionné. Connexion de l’app Dexcom du patient, pas un compte follower. Le partage doit être activé.",
        "Utilisateur A", "Utilisateur B", "Nom", "Mode d’affichage",
        "1 — un utilisateur", "2 — deux côte à côte", "Désactivé", "Son: ON",
        "Son: OFF", "Snooze: %d min", "Couper le son", "Snooze %d m",
        "Afficher: %s", "Mode: 1 personne", "Mode: 2 personnes", "Jetons A: enregistrés",
        "Jetons A: manquants — connectez-vous sur /carelink", "1 compte → jetons A partagés, patient A/B différent. 2 comptes CareLink → connexion séparée pour B sur /carelink.", "optionnel: logindata.json — sinon /carelink", "CareLink / Guardian",
        "Les jetons sont enregistrés.", "Pas encore de jetons — terminez la connexion ci-dessous.", "Choisir un patient", "Pour l’utilisateur",
        "1 compte CareLink: connectez-vous une fois (A), choisissez ici des patients différents pour A et B.", "Enregistrer le patient pour ", "La liste des patients est vide.", "Impossible de charger les patients: ",
        "En attente du code CareLink (valide ~10 min).", "Mode d’emploi (mobile)", "<li>Choisissez la région et touchez <b>1. Se connecter à CareLink</b>.</li><li>Connectez-vous avec un compte Care Partner et résolvez le captcha.</li><li>Le navigateur affiche une erreur du type « impossible d’ouvrir l’app » — <b>c’est normal</b>.</li><li>Copiez le lien de la barre d’adresse (commence par <code>com.medtronic.carepartner</code>) ou Partager → Copier le lien.</li><li>Revenez ici, collez le lien et touchez <b>2. Enregistrer les jetons</b>.</li><li>Choisissez le bon patient ci-dessus.</li>", "Utilisez un compte Care Partner (follower), pas le compte patient.",
        "Région", "UE / Slovaquie / Europe", "USA", "Enregistrer les jetons pour",
        "A (principal / partagé)", "B (uniquement si 2e compte CareLink)", "1. Se connecter à CareLink", "Lien ou code après connexion",
        "2. Enregistrer les jetons", "← Retour aux paramètres", "Menu", "Vous n’avez pas choisi de patient.",
        "Patient enregistré (", "Retour à CareLink", "Paramètres", "Étape 1 terminée",
        "Ouvrez CareLink, connectez-vous et résolvez le captcha.", "Ouvrir la connexion CareLink", "Après connexion vous verrez une erreur d’app — copiez le lien (com.medtronic.carepartner…) et collez-le ci-dessous.", "Collez le lien / code copié",
        "Terminé — jetons enregistrés pour ", "Choisir le patient →", "Accueil", "Réessayer",
        "PIN : NON", "PIN : OUI", "Effacer le PIN", "Saisis 4 chiffres",
        "Répète le PIN", "Ne correspond pas", "Effacer seulement le PIN ?\nWiFi et comptes restent.",
        "4 chiffres de l'écran", "Mauvais PIN", "Trop d'essais, attends",
        "CareLink déconnecté",
        "Réglage étape par étape", "Combien de personnes sur l'écran ?",
        "Les deux personnes sur un seul compte CareLink ? Un second login seulement pour un autre compte.",
        "Terminé. Tu peux fermer cette page dans le navigateur.",
        "Halo: ON", "Halo: OFF",
        "Connexion...",
        "J'attends le serveur",
        "Aspect", "Aspect 3/4", "Son", "Son",
        "Moins fort", "Plus fort", "Faible", "Basse", "Moyenne", "Haute",
        "Rotation: USB droite", "Rotation: USB gauche",
        "Même WiFi\nOuvre cette IP\net règle la source",
        "Choisis ce que tu veux régler",
        "Paramètres et sources",
        "Se connecter à CareLink",
        "Réseaux WiFi",
        "Assistant de configuration",
        "Rapport pour le support",
        "Envoie ce texte. Il ne contient ni mots de passe ni noms.",
        "Information",
        "GlukoView est un affichage informatif seulement et ne sert pas aux décisions de traitement. Les valeurs ne doivent pas servir à doser l'insuline.",
        "Le projet GlukoView est un logiciel open source indépendant. Il n'est en aucun cas lié, sponsorisé, soutenu ou autrement approuvé par Abbott (FreeStyle Libre), Dexcom ou Medtronic (Guardian). Toutes les marques et tous les logos mentionnés appartiennent à leurs propriétaires respectifs.",
        "Je comprends",
        "Continuer",
        "Avis : %s",
        "Accepté",
        "Avant la connexion",
        "Je reconnais que GlukoView sert uniquement d'affichage informatif et n'est pas un dispositif médical. Les valeurs affichées ne doivent pas servir à doser l'insuline ni à d'autres décisions de traitement. Pour cela, je m'engage à utiliser uniquement l'application ou le lecteur du fabricant du capteur.",
        "Je reconnais les deux points ci-dessus",
        "Continuer",
        "Affichage informatif, pas un dispositif médical. Pas pour doser l'insuline ni pour des décisions de traitement.",
        "Je reconnais que GlukoView n'est pas destiné aux décisions de traitement ni au dosage de l'insuline et n'est pas un dispositif médical. Cet accord est enregistré dans l'appareil.",
        "L'accord n'est pas enregistré. L'appareil n'est pas destiné aux décisions de traitement.",
        "GlukoView n'est pas un dispositif médical. Ses données ne servent pas à doser l'insuline ni à d'autres décisions médicales.",
        "Accepté",
        "Non accepté",
        "La liste des patients est en cours de chargement. La page va s’actualiser. Choisissez ensuite un nom et enregistrez.",
        "La connexion à Libre a échoué. Vérifiez l’e-mail, le mot de passe et la région EU.",
        "Libre n’a renvoyé aucun nom. Utilisez le compte LibreLinkUp qui voit ces personnes.",
        "La liste n’est pas encore arrivée. Actualisez la page encore une fois. Tant qu’aucun nom n’est choisi, la première personne s’affiche."
    },
};

static_assert(sizeof(kText[0]) / sizeof(kText[0][0]) == (int)Str::StrCount, "SK table size");
static_assert(sizeof(kText[1]) / sizeof(kText[1][0]) == (int)Str::StrCount, "CS table size");
static_assert(sizeof(kText[2]) / sizeof(kText[2][0]) == (int)Str::StrCount, "EN table size");
static_assert(sizeof(kText[3]) / sizeof(kText[3][0]) == (int)Str::StrCount, "DE table size");
static_assert(sizeof(kText[4]) / sizeof(kText[4][0]) == (int)Str::StrCount, "ES table size");
static_assert(sizeof(kText[5]) / sizeof(kText[5][0]) == (int)Str::StrCount, "FR table size");

static int langIndex() {
    int i = (int)s_lang;
    if (i < 0 || i >= LANG_COUNT) return 0;
    return i;
}

const char *S(Str id) {
    int n = (int)id;
    if (n < 0 || n >= (int)Str::StrCount) return "";
    return kText[langIndex()][n];
}

DiaLang diaI18nGet() { return s_lang; }

int diaI18nCount() { return LANG_COUNT; }

const char *diaI18nCode() { return kCode[langIndex()]; }

const char *diaI18nNativeName(DiaLang lang) {
    int i = (int)lang;
    if (i < 0 || i >= LANG_COUNT) i = 0;
    return kNativeName[i];
}

const char *diaI18nNativeNameAt(int index) {
    if (index < 0 || index >= LANG_COUNT) return kNativeName[0];
    return kNativeName[index];
}

static char lowerAscii(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

DiaLang diaI18nFromCode(const char *code) {
    if (!code || !code[0]) return DiaLang::Sk;
    char a = lowerAscii(code[0]);
    char b = code[1] ? lowerAscii(code[1]) : 0;
    for (int i = 0; i < LANG_COUNT; i++) {
        if (a == lowerAscii(kCode[i][0]) && b == lowerAscii(kCode[i][1]) &&
            (code[2] == '\0' || code[2] == ' ')) {
            return (DiaLang)i;
        }
    }
    return DiaLang::Sk;
}

void diaI18nSet(DiaLang lang) {
    if ((int)lang < 0 || (int)lang >= LANG_COUNT) lang = DiaLang::Sk;
    s_lang = lang;
    preferences.putUChar("lang", (uint8_t)s_lang);
}

void diaI18nCycle() {
    int next = ((int)s_lang + 1) % LANG_COUNT;
    diaI18nSet((DiaLang)next);
}

bool diaI18nNeedsSetup() { return !s_lang_ready; }

void diaI18nMarkSetupDone() {
    s_lang_ready = true;
    preferences.putBool("lang_set", true);
}

void diaI18nLoad() {
    const bool hasLangSet = preferences.isKey("lang_set") && preferences.getBool("lang_set", false);
    if (!hasLangSet && preferences.isKey("lang")) {
        // Legacy schema: 0=Sk, 1=En (before Cs/De/Es/Fr were inserted).
        uint8_t old = preferences.getUChar("lang", 0);
        s_lang = (old == 1) ? DiaLang::En : DiaLang::Sk;
        preferences.putUChar("lang", (uint8_t)s_lang);
        s_lang_ready = true;
        preferences.putBool("lang_set", true);
        return;
    }
    uint8_t v = preferences.isKey("lang") ? preferences.getUChar("lang", 0) : 0;
    if (v >= LANG_COUNT) v = 0;
    s_lang = (DiaLang)v;
    s_lang_ready = hasLangSet;
}
