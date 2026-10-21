// Generates src/dia_i18n.cpp from translation tables. Run: node tools/gen_i18n.js
const fs = require("fs");
const path = require("path");

const keys = [
  "SysTitle","Info","Refresh","AutoOn","AutoOff","Update","ForgetWifi","Back","LimitsTitle",
  "Low","High","Crit","UnitsMg","UnitsMmol","DeltaOn","DeltaOff","BrightTitle",
  "DayBrFmt","NightBrFmt","DayFromFmt","NightFromFmt","Save","LangSk","LangEn",
  "EditDay","EditNight","EditLowMg","EditLow","EditHighMg","EditHigh","EditCritMg","EditCrit",
  "EditDayStart","EditNightStart","InfoKeys","ConfirmErase","YesErase","Loading","Stale","Sync",
  "OtaPct","OtaCheck","WifiOff","ServerDown","UpToDate","OtaFail","OtaZero",
  "BootConnecting","BootWifiAgain","BootScanning","BootConnected","BootSetupHint","BootRetry","BootSaving",
  "ErrWifi","ErrAuth","ErrNet","ErrEmpty","ErrConfig",
  "PortalTitle","PortalSource","PortalPickSource","PortalNs","PortalNsHint","PortalNsUrl","PortalNsToken",
  "PortalAccount","PortalAccountHint","PortalEmail","PortalPassword","PortalPatient","PortalPatientHint",
  "PortalRegion","PortalDisplay","PortalUnits","PortalTls","PortalTlsYes","PortalTlsNo",
  "PortalLow","PortalHigh","PortalCrit","PortalGuardian","PortalGuardianHint","PortalGuardianPatient",
  "PortalGuardianPatientHint","PortalCarelinkJson","PortalSave","PortalBack","PortalSavedWifi",
  "PortalNetsTitle","PortalNetsHint","PortalConfigWifi","PortalBackMenu","PortalSavedTitle",
  "PortalNoneYet","PortalCurrent","PortalLang","PortalLast","PortalNoPwd",
  "SourceOn","SourceOff","OtaFound","PortalDexcom","PortalDexcomHint",
  "PortalUserA","PortalUserB","PortalName","PortalDispMode","PortalMode1","PortalMode2","PortalSrcOff",
  "SoundOn","SoundOff","SnoozeFmt","SnoozeNow","SnoozeActiveFmt","ShowChild","ModeBtn1","ModeBtn2",
  "PortalTokensOk","PortalTokensMissing","PortalDualClHint","PortalClJsonPh",
  "ClTitle","ClTokensOk","ClTokensMissing","ClPickPatient","ClForUser","ClSharedHint","ClSavePatient",
  "ClEmptyList","ClLoadFail","ClPendingCode","ClHowTitle","ClHowHtml","ClFollowerHint",
  "ClRegion","ClRegionEu","ClRegionUs","ClSaveFor","ClSlotA","ClSlotB","ClBtnStart","ClCbLabel",
  "ClBtnFinish","ClBackParam","ClMenu","ClNoPatient","ClPatientSaved","ClBackCarelink","ClParams",
  "ClStep1Done","ClOpenLogin","ClOpenBtn","ClAfterLoginHint","ClPasteLabel","ClDoneTokens",
  "ClPickPatientBtn","ClHome","ClRetry"
];

const ClHow = {
  sk:
    "<li>Vyber región a ťukni <b>1. Prihlásiť CareLink</b>.</li>" +
    "<li>Prihlás sa Care Partner účtom a vyrieš captcha.</li>" +
    "<li>Prehliadač ukáže chybu typu „nejde otvoriť aplikáciu“ — <b>to je v poriadku</b>.</li>" +
    "<li>Skopíruj odkaz z adresného riadku (začína <code>com.medtronic.carepartner</code>) alebo zdieľaj stránku → Kopírovať odkaz.</li>" +
    "<li>Vráť sa sem, vlož odkaz do poľa a ťukni <b>2. Uložiť tokeny</b>.</li>" +
    "<li>Vyber správneho pacienta vyššie.</li>",
  cs:
    "<li>Vyber region a klepni na <b>1. Přihlásit CareLink</b>.</li>" +
    "<li>Přihlas se účtem Care Partner a vyřeš captcha.</li>" +
    "<li>Prohlížeč ukáže chybu typu „nelze otevřít aplikaci“ — <b>to je v pořádku</b>.</li>" +
    "<li>Zkopíruj odkaz z adresního řádku (začíná <code>com.medtronic.carepartner</code>) nebo Sdílet → Kopírovat odkaz.</li>" +
    "<li>Vrať se sem, vlož odkaz a klepni na <b>2. Uložit tokeny</b>.</li>" +
    "<li>Vyber správného pacienta výše.</li>",
  en:
    "<li>Pick a region and tap <b>1. Sign in to CareLink</b>.</li>" +
    "<li>Sign in with a Care Partner account and solve the captcha.</li>" +
    "<li>The browser shows an error like “can’t open the app” — <b>that is OK</b>.</li>" +
    "<li>Copy the link from the address bar (starts with <code>com.medtronic.carepartner</code>) or Share page → Copy link.</li>" +
    "<li>Come back here, paste the link and tap <b>2. Save tokens</b>.</li>" +
    "<li>Pick the correct patient above.</li>",
  de:
    "<li>Region wählen und <b>1. Bei CareLink anmelden</b> tippen.</li>" +
    "<li>Mit Care-Partner-Konto anmelden und Captcha lösen.</li>" +
    "<li>Der Browser zeigt einen Fehler wie „App kann nicht geöffnet werden“ — <b>das ist in Ordnung</b>.</li>" +
    "<li>Link aus der Adresszeile kopieren (beginnt mit <code>com.medtronic.carepartner</code>) oder Seite teilen → Link kopieren.</li>" +
    "<li>Hierher zurück, Link einfügen und <b>2. Tokens speichern</b> tippen.</li>" +
    "<li>Oben den richtigen Patienten wählen.</li>",
  es:
    "<li>Elige la región y pulsa <b>1. Iniciar sesión en CareLink</b>.</li>" +
    "<li>Inicia sesión con una cuenta Care Partner y resuelve el captcha.</li>" +
    "<li>El navegador muestra un error tipo “no se puede abrir la app” — <b>está bien</b>.</li>" +
    "<li>Copia el enlace de la barra de direcciones (empieza por <code>com.medtronic.carepartner</code>) o Compartir → Copiar enlace.</li>" +
    "<li>Vuelve aquí, pega el enlace y pulsa <b>2. Guardar tokens</b>.</li>" +
    "<li>Elige el paciente correcto arriba.</li>",
  fr:
    "<li>Choisissez la région et touchez <b>1. Se connecter à CareLink</b>.</li>" +
    "<li>Connectez-vous avec un compte Care Partner et résolvez le captcha.</li>" +
    "<li>Le navigateur affiche une erreur du type « impossible d’ouvrir l’app » — <b>c’est normal</b>.</li>" +
    "<li>Copiez le lien de la barre d’adresse (commence par <code>com.medtronic.carepartner</code>) ou Partager → Copier le lien.</li>" +
    "<li>Revenez ici, collez le lien et touchez <b>2. Enregistrer les jetons</b>.</li>" +
    "<li>Choisissez le bon patient ci-dessus.</li>",
};

const T = {
sk: [
"Systém 1/3","Informácie","Obnoviť","Auto-upd: ZAP","Auto-upd: VYP","Aktualizácia","Zmazať WiFi","Späť","Limity 2/3",
"Spodný","Vrchný","Kritické","Jednotky: mg/dL","Jednotky: mmol","Delta: ZAP","Delta: VYP","Jas a čas 3/3",
"Denný jas: %d","Nočný jas: %d","Deň od: %02d:00","Noc od: %02d:00","Uložiť","Jazyk: SK","Language: EN",
"Denný jas","Nočný jas","Spodný (mg/dL)","Spodný cieľ","Vrchný (mg/dL)","Vrchný cieľ","Kritické (mg/dL)","Kritické",
"Začiatok dňa (h)","Začiatok noci (h)","IP\nWiFi\nTeplota\nVerzia","Naozaj zmazať\nvšetky siete?","Áno, zmazať",
"Načítavam...","Staré","Sync...","Aktualizujem... %d%%","Kontrolujem...","WiFi nie je\npripojené.","Server\nnedostupný",
"Máš najnovšiu\nverziu.","Chyba sťahovania","Aktualizujem... 0%",
"Pripájam sa...","WiFi znova","Skenujem siete...","Pripojené!",
"1. WiFi: GlukoView\n2. Web: 10.0.0.1\n3. Vyber sieť","Znova skúšam\nuložené siete...","Ukladám a\nreštartujem...",
"WiFi","Auth","Sieť","Prázdne","Config",
"GlukoView","Zdroj dát","Vyber zdroj","Nightscout","Len ak je zdroj Nightscout","Nightscout URL","Nightscout token",
"Libre LinkUp","1 Libre konto + 2 osoby: rovnaký e-mail/heslo na A aj B, rôzny pacient. 2 kontá: rôzne prihlasovacie údaje.",
"E-mail / používateľ","Heslo","Pacient (meno alebo ID)",
"Vyber meno. Zoznam sa doplní po prihlásení na Libre. Pri dual môžu mať A/B rôznych pacientov.",
"Región","Displej","Jednotky","TLS bez overenia","Áno","Nie",
"Spodný cieľ (mmol)","Vrchný cieľ (mmol)","Kritické hyper (mmol)",
"Guardian (CareLink)","Prihlásenie raz na /carelink. 1 konto + 2 osoby = jeden login, rôzny pacient A/B. 2 kontá = login zvlášť pre B.",
"Pacient Guardian","Vyber po prihlásení CareLink. Pri dual režime môže mať A a B rôznych pacientov z rovnakého konta.",
"CareLink JSON","Uložiť","Späť","Uložené WiFi","Uložené WiFi",
"Až 5 sietí. Pri štarte a výpadku sa skúsia všetky. Nová sieť sa uloží po pripojení.",
"Konfigurovať WiFi","Späť do menu","Uložené WiFi (až 5)","Zatiaľ žiadne. Uložia sa po pripojení.","(aktuálna)","Jazyk",
"(posledná)","(bez hesla!)","Zdroj: ZAP","Zdroj: VYP","Nová verzia.\nInštalujem...",
"Dexcom Share","Len keď je zvolené Dexcom. Prihlásenie z aplikácie Dexcom u pacienta, nie konto followera. Zdieľanie musí byť zapnuté.",
"Používateľ A","Používateľ B","Meno","Režim displeja","1 — jeden používateľ","2 — dvaja vedľa seba","Vypnuté (off)",
"Zvuk: ZAP","Zvuk: VYP","Snooze: %d min","Stlmiť teraz","Snooze %d m","Zobraz: %s","Režim: 1 osoba","Režim: 2 osoby",
"Tokeny A: uložené","Tokeny A: chýbajú — prihlás na /carelink",
"1 konto → rovnaké tokeny A, rôzny pacient A/B. 2 CareLink kontá → na /carelink prihlás zvlášť pre B.",
"voliteľné: logindata.json — inak /carelink",
"CareLink / Guardian","Tokeny sú uložené.","Zatiaľ žiadne tokeny — dokonči prihlásenie nižšie.",
"Vyber pacienta","Pre používateľa","1 CareLink konto: prihlás raz (A), tu vyber rôznych pacientov pre A a B.",
"Uložiť pacienta pre ","Zoznam pacientov je prázdny.","Pacientov sa nepodarilo načítať: ",
"Čaká sa na kód z CareLink (platí ~10 min).","Ako na to (mobil)",ClHow.sk,
"Použi Care Partner (follower), nie pacientsky účet.","Región","EU / Slovensko / Európa","USA",
"Uložiť tokeny pre","A (hlavné / zdieľané)","B (len ak máš druhé CareLink konto)",
"1. Prihlásiť CareLink","Odkaz alebo kód po prihlásení","2. Uložiť tokeny","← Späť na parametre","Menu",
"Nevybral si pacienta.","Pacient uložený (","Späť na CareLink","Parametre","Krok 1 hotový",
"Otvor CareLink, prihlás sa a vyrieš captcha.","Otvoriť CareLink prihlásenie",
"Po prihlásení uvidíš chybu o aplikácii — skopíruj odkaz (com.medtronic.carepartner…) a vlož ho nižšie.",
"Vlož skopírovaný odkaz / kód","Hotovo — tokeny uložené pre ","Vybrať pacienta →","Domov","Skúsiť znova",
],
cs: [
"Systém 1/3","Informace","Obnovit","Auto-upd: ZAP","Auto-upd: VYP","Aktualizace","Smazat WiFi","Zpět","Limity 2/3",
"Spodní","Horní","Kritické","Jednotky: mg/dL","Jednotky: mmol","Delta: ZAP","Delta: VYP","Jas a čas 3/3",
"Denní jas: %d","Noční jas: %d","Den od: %02d:00","Noc od: %02d:00","Uložit","Jazyk: SK","Language: EN",
"Denní jas","Noční jas","Spodní (mg/dL)","Spodní cíl","Horní (mg/dL)","Horní cíl","Kritické (mg/dL)","Kritické",
"Začátek dne (h)","Začátek noci (h)","IP\nWiFi\nTeplota\nVerze","Opravdu smazat\nvšechny sítě?","Ano, smazat",
"Načítám...","Staré","Sync...","Aktualizuji... %d%%","Kontroluji...","WiFi není\npřipojené.","Server\nnedostupný",
"Máte nejnovější\nverzi.","Chyba stahování","Aktualizuji... 0%",
"Připojuji se...","WiFi znovu","Skenuji sítě...","Připojeno!",
"1. WiFi: GlukoView\n2. Web: 10.0.0.1\n3. Vyber síť","Znovu zkouším\nuložené sítě...","Ukládám a\nrestartuji...",
"WiFi","Auth","Síť","Prázdné","Config",
"GlukoView","Zdroj dat","Vyber zdroj","Nightscout","Jen pokud je zdroj Nightscout","Nightscout URL","Nightscout token",
"Libre LinkUp","1 Libre účet + 2 osoby: stejný e-mail/heslo na A i B, jiný pacient. 2 účty: jiné přihlašovací údaje.",
"E-mail / uživatel","Heslo","Pacient (jméno nebo ID)",
"Vyber jméno. Seznam se doplní po přihlášení na Libre. V dual režimu mohou mít A/B různé pacienty.",
"Region","Displej","Jednotky","TLS bez ověření","Ano","Ne",
"Spodní cíl (mmol)","Horní cíl (mmol)","Kritická hyper (mmol)",
"Guardian (CareLink)","Přihlášení jednou na /carelink. 1 účet + 2 osoby = jeden login, jiný pacient A/B. 2 účty = login zvlášť pro B.",
"Pacient Guardian","Vyber po přihlášení CareLink. V dual režimu mohou mít A a B různé pacienty ze stejného účtu.",
"CareLink JSON","Uložit","Zpět","Uložené WiFi","Uložené WiFi",
"Až 5 sítí. Při startu a výpadku se zkusí všechny. Nová síť se uloží po připojení.",
"Konfigurovat WiFi","Zpět do menu","Uložené WiFi (až 5)","Zatím žádné. Uloží se po připojení.","(aktuální)","Jazyk",
"(poslední)","(bez hesla!)","Zdroj: ZAP","Zdroj: VYP","Nová verze.\nInstaluji...",
"Dexcom Share","Jen když je zvoleno Dexcom. Přihlášení z aplikace Dexcom u pacienta, ne účet followera. Sdílení musí být zapnuté.",
"Uživatel A","Uživatel B","Jméno","Režim displeje","1 — jeden uživatel","2 — dva vedle sebe","Vypnuto (off)",
"Zvuk: ZAP","Zvuk: VYP","Snooze: %d min","Ztlumit teď","Snooze %d m","Zobraz: %s","Režim: 1 osoba","Režim: 2 osoby",
"Tokeny A: uložené","Tokeny A: chybí — přihlas na /carelink",
"1 účet → stejné tokeny A, různý pacient A/B. 2 CareLink účty → na /carelink přihlas zvlášť pro B.",
"volitelné: logindata.json — jinak /carelink",
"CareLink / Guardian","Tokeny jsou uložené.","Zatím žádné tokeny — dokonči přihlášení níže.",
"Vyber pacienta","Pro uživatele","1 CareLink účet: přihlas jednou (A), zde vyber různé pacienty pro A a B.",
"Uložit pacienta pro ","Seznam pacientů je prázdný.","Pacienty se nepodařilo načíst: ",
"Čeká se na kód z CareLink (platí ~10 min).","Jak na to (mobil)",ClHow.cs,
"Použij Care Partner (follower), ne pacientský účet.","Region","EU / Česko / Evropa","USA",
"Uložit tokeny pro","A (hlavní / sdílené)","B (jen pokud máš druhé CareLink konto)",
"1. Přihlásit CareLink","Odkaz nebo kód po přihlášení","2. Uložit tokeny","← Zpět na parametry","Menu",
"Nevybral jsi pacienta.","Pacient uložen (","Zpět na CareLink","Parametry","Krok 1 hotový",
"Otevři CareLink, přihlas se a vyřeš captcha.","Otevřít CareLink přihlášení",
"Po přihlášení uvidíš chybu o aplikaci — zkopíruj odkaz (com.medtronic.carepartner…) a vlož ho níže.",
"Vlož zkopírovaný odkaz / kód","Hotovo — tokeny uložené pro ","Vybrat pacienta →","Domů","Zkusit znovu",
],
en: [
"System 1/3","Info","Refresh","Auto-upd: ON","Auto-upd: OFF","Update","Erase WiFi","Back","Limits 2/3",
"Low","High","Critical","Units: mg/dL","Units: mmol","Delta: ON","Delta: OFF","Light 3/3",
"Day brightness: %d","Night brightness: %d","Day from: %02d:00","Night from: %02d:00","Save","Jazyk: SK","Language: EN",
"Day brightness","Night brightness","Low (mg/dL)","Low target","High (mg/dL)","High target","Critical (mg/dL)","Critical",
"Day starts (h)","Night starts (h)","IP\nWiFi\nTemp\nVersion","Erase all\nnetworks?","Yes, erase",
"Loading...","Old","Sync...","Updating... %d%%","Checking...","WiFi is not\nconnected.","Server\nunavailable",
"You have the\nlatest version.","Download failed","Updating... 0%",
"Connecting...","WiFi again","Scanning...","Connected!",
"1. WiFi: GlukoView\n2. Web: 10.0.0.1\n3. Pick a network","Retrying saved\nnetworks...","Saving and\nrestarting...",
"WiFi","Auth","Network","Empty","Config",
"GlukoView","Data source","Choose source","Nightscout","Only if the source is Nightscout","Nightscout URL","Nightscout token",
"Libre LinkUp","One Libre account + 2 people: same email/password on A and B, different patient. Two accounts: different logins.",
"Email / user","Password","Patient (name or ID)",
"Pick a name. List fills after Libre login. In dual mode A/B can use different patients.",
"Region","Display","Units","Skip TLS verify","Yes","No",
"Low target (mmol)","High target (mmol)","Critical high (mmol)",
"Guardian (CareLink)","Sign in once on /carelink. One account + 2 people = one login, different patient A/B. Two accounts = separate login for B.",
"Guardian patient","Pick after CareLink sign-in. In dual mode A and B can use different patients from the same account.",
"CareLink JSON","Save","Back","Saved WiFi","Saved WiFi",
"Up to 5 networks. All of them are tried at boot and after a dropout. A new network is saved after it connects.",
"Configure WiFi","Back to menu","Saved WiFi (up to 5)","None yet. They are saved after a connection.","(current)","Language",
"(last)","(no password!)","Source: ON","Source: OFF","New version.\nInstalling...",
"Dexcom Share","Used only when Dexcom is selected. Use the patient's Dexcom app login, not a follower account. Sharing must be on.",
"User A","User B","Name","Display mode","1 — single user","2 — two side by side","Off",
"Sound: ON","Sound: OFF","Snooze: %d min","Mute now","Snooze %d m","Show: %s","Mode: 1 person","Mode: 2 people",
"Tokens A: saved","Tokens A: missing — sign in at /carelink",
"1 account → shared A tokens, different patient A/B. 2 CareLink accounts → sign in separately for B at /carelink.",
"optional: logindata.json — otherwise use /carelink",
"CareLink / Guardian","Tokens are saved.","No tokens yet — finish sign-in below.",
"Pick a patient","For user","1 CareLink account: sign in once (A), then pick different patients for A and B here.",
"Save patient for ","Patient list is empty.","Could not load patients: ",
"Waiting for CareLink code (valid ~10 min).","How to (phone)",ClHow.en,
"Use a Care Partner (follower) account, not the patient account.","Region","EU / Slovakia / Europe","USA",
"Save tokens for","A (main / shared)","B (only if you have a second CareLink account)",
"1. Sign in to CareLink","Link or code after sign-in","2. Save tokens","← Back to parameters","Menu",
"You did not pick a patient.","Patient saved (","Back to CareLink","Parameters","Step 1 done",
"Open CareLink, sign in and solve the captcha.","Open CareLink sign-in",
"After sign-in you will see an app error — copy the link (com.medtronic.carepartner…) and paste it below.",
"Paste the copied link / code","Done — tokens saved for ","Pick patient →","Home","Try again",
],
de: [
"System 1/3","Info","Aktualisieren","Auto-upd: AN","Auto-upd: AUS","Update","WiFi löschen","Zurück","Grenzen 2/3",
"Niedrig","Hoch","Kritisch","Einheiten: mg/dL","Einheiten: mmol","Delta: AN","Delta: AUS","Helligkeit 3/3",
"Taghelligkeit: %d","Nachthelligkeit: %d","Tag ab: %02d:00","Nacht ab: %02d:00","Speichern","Jazyk: SK","Language: EN",
"Taghelligkeit","Nachthelligkeit","Niedrig (mg/dL)","Untergrenze","Hoch (mg/dL)","Obergrenze","Kritisch (mg/dL)","Kritisch",
"Tagesbeginn (h)","Nachtbeginn (h)","IP\nWiFi\nTemp\nVersion","Alle Netze\nwirklich löschen?","Ja, löschen",
"Laden...","Alt","Sync...","Aktualisiere... %d%%","Prüfe...","WiFi ist nicht\nverbunden.","Server\nnicht erreichbar",
"Sie haben die\nneueste Version.","Download fehlgeschlagen","Aktualisiere... 0%",
"Verbinde...","WiFi erneut","Scanne...","Verbunden!",
"1. WiFi: GlukoView\n2. Web: 10.0.0.1\n3. Netz wählen","Gespeicherte Netze\nwerden erneut versucht...","Speichern und\nNeustart...",
"WiFi","Auth","Netz","Leer","Config",
"GlukoView","Datenquelle","Quelle wählen","Nightscout","Nur wenn die Quelle Nightscout ist","Nightscout-URL","Nightscout-Token",
"Libre LinkUp","1 Libre-Konto + 2 Personen: gleiche E-Mail/Passwort für A und B, anderer Patient. 2 Konten: unterschiedliche Logins.",
"E-Mail / Benutzer","Passwort","Patient (Name oder ID)",
"Namen wählen. Liste erscheint nach Libre-Login. Im Dual-Modus können A/B verschiedene Patienten nutzen.",
"Region","Display","Einheiten","TLS-Prüfung überspringen","Ja","Nein",
"Untergrenze (mmol)","Obergrenze (mmol)","Kritisch hoch (mmol)",
"Guardian (CareLink)","Einmal anmelden unter /carelink. 1 Konto + 2 Personen = ein Login, anderer Patient A/B. 2 Konten = separates Login für B.",
"Guardian-Patient","Nach CareLink-Anmeldung wählen. Im Dual-Modus können A und B verschiedene Patienten desselben Kontos nutzen.",
"CareLink-JSON","Speichern","Zurück","Gespeicherte WiFi","Gespeicherte WiFi",
"Bis zu 5 Netze. Beim Start und bei Ausfall werden alle versucht. Neues Netz wird nach Verbindung gespeichert.",
"WiFi konfigurieren","Zurück zum Menü","Gespeicherte WiFi (bis 5)","Noch keine. Werden nach Verbindung gespeichert.","(aktuell)","Sprache",
"(letzte)","(kein Passwort!)","Quelle: AN","Quelle: AUS","Neue Version.\nInstalliere...",
"Dexcom Share","Nur bei gewähltem Dexcom. Login aus der Dexcom-App des Patienten, kein Follower-Konto. Teilen muss aktiv sein.",
"Benutzer A","Benutzer B","Name","Anzeigemodus","1 — ein Benutzer","2 — zwei nebeneinander","Aus",
"Ton: AN","Ton: AUS","Snooze: %d min","Jetzt stummschalten","Snooze %d m","Anzeige: %s","Modus: 1 Person","Modus: 2 Personen",
"Tokens A: gespeichert","Tokens A: fehlen — anmelden unter /carelink",
"1 Konto → gemeinsame A-Tokens, anderer Patient A/B. 2 CareLink-Konten → unter /carelink separat für B anmelden.",
"optional: logindata.json — sonst /carelink",
"CareLink / Guardian","Tokens sind gespeichert.","Noch keine Tokens — Anmeldung unten abschließen.",
"Patient wählen","Für Benutzer","1 CareLink-Konto: einmal anmelden (A), hier verschiedene Patienten für A und B wählen.",
"Patient speichern für ","Patientenliste ist leer.","Patienten konnten nicht geladen werden: ",
"Warte auf CareLink-Code (gültig ~10 Min.).","Anleitung (Handy)",ClHow.de,
"Care-Partner-(Follower-)Konto nutzen, nicht das Patientenkonto.","Region","EU / Slowakei / Europa","USA",
"Tokens speichern für","A (Haupt / gemeinsam)","B (nur bei zweitem CareLink-Konto)",
"1. Bei CareLink anmelden","Link oder Code nach Anmeldung","2. Tokens speichern","← Zurück zu Parametern","Menü",
"Kein Patient gewählt.","Patient gespeichert (","Zurück zu CareLink","Parameter","Schritt 1 fertig",
"CareLink öffnen, anmelden und Captcha lösen.","CareLink-Anmeldung öffnen",
"Nach der Anmeldung erscheint ein App-Fehler — Link kopieren (com.medtronic.carepartner…) und unten einfügen.",
"Kopierten Link / Code einfügen","Fertig — Tokens gespeichert für ","Patient wählen →","Start","Erneut versuchen",
],
es: [
"Sistema 1/3","Info","Actualizar","Auto-upd: ON","Auto-upd: OFF","Actualización","Borrar WiFi","Atrás","Límites 2/3",
"Bajo","Alto","Crítico","Unidades: mg/dL","Unidades: mmol","Delta: ON","Delta: OFF","Brillo 3/3",
"Brillo día: %d","Brillo noche: %d","Día desde: %02d:00","Noche desde: %02d:00","Guardar","Jazyk: SK","Language: EN",
"Brillo día","Brillo noche","Bajo (mg/dL)","Objetivo bajo","Alto (mg/dL)","Objetivo alto","Crítico (mg/dL)","Crítico",
"Inicio del día (h)","Inicio de la noche (h)","IP\nWiFi\nTemp\nVersión","¿Borrar todas\nlas redes?","Sí, borrar",
"Cargando...","Antiguo","Sync...","Actualizando... %d%%","Comprobando...","WiFi no está\nconectado.","Servidor\nno disponible",
"Tienes la\núltima versión.","Error de descarga","Actualizando... 0%",
"Conectando...","WiFi de nuevo","Escaneando...","¡Conectado!",
"1. WiFi: GlukoView\n2. Web: 10.0.0.1\n3. Elige una red","Reintentando redes\nguardadas...","Guardando y\nreiniciando...",
"WiFi","Auth","Red","Vacío","Config",
"GlukoView","Fuente de datos","Elige fuente","Nightscout","Solo si la fuente es Nightscout","URL Nightscout","Token Nightscout",
"Libre LinkUp","1 cuenta Libre + 2 personas: mismo email/contraseña en A y B, paciente distinto. 2 cuentas: inicios distintos.",
"Email / usuario","Contraseña","Paciente (nombre o ID)",
"Elige un nombre. La lista se rellena tras iniciar sesión en Libre. En dual A/B pueden usar pacientes distintos.",
"Región","Pantalla","Unidades","Omitir verificación TLS","Sí","No",
"Objetivo bajo (mmol)","Objetivo alto (mmol)","Hiper crítica (mmol)",
"Guardian (CareLink)","Inicia sesión una vez en /carelink. 1 cuenta + 2 personas = un login, paciente A/B distinto. 2 cuentas = login aparte para B.",
"Paciente Guardian","Elige tras iniciar sesión en CareLink. En dual A y B pueden usar pacientes distintos de la misma cuenta.",
"JSON CareLink","Guardar","Atrás","WiFi guardadas","WiFi guardadas",
"Hasta 5 redes. Al arrancar y tras un corte se prueban todas. Una red nueva se guarda al conectar.",
"Configurar WiFi","Volver al menú","WiFi guardadas (hasta 5)","Ninguna aún. Se guardan tras conectar.","(actual)","Idioma",
"(última)","(¡sin contraseña!)","Fuente: ON","Fuente: OFF","Nueva versión.\nInstalando...",
"Dexcom Share","Solo si eliges Dexcom. Usa el login de la app Dexcom del paciente, no una cuenta follower. El compartir debe estar activo.",
"Usuario A","Usuario B","Nombre","Modo de pantalla","1 — un usuario","2 — dos lado a lado","Apagado",
"Sonido: ON","Sonido: OFF","Snooze: %d min","Silenciar ahora","Snooze %d m","Mostrar: %s","Modo: 1 persona","Modo: 2 personas",
"Tokens A: guardados","Tokens A: faltan — inicia sesión en /carelink",
"1 cuenta → tokens A compartidos, paciente A/B distinto. 2 cuentas CareLink → inicia sesión aparte para B en /carelink.",
"opcional: logindata.json — si no, /carelink",
"CareLink / Guardian","Los tokens están guardados.","Aún no hay tokens — termina el inicio de sesión abajo.",
"Elige un paciente","Para el usuario","1 cuenta CareLink: inicia sesión una vez (A), aquí elige pacientes distintos para A y B.",
"Guardar paciente para ","La lista de pacientes está vacía.","No se pudieron cargar pacientes: ",
"Esperando código de CareLink (válido ~10 min).","Cómo hacerlo (móvil)",ClHow.es,
"Usa una cuenta Care Partner (follower), no la del paciente.","Región","UE / Eslovaquia / Europa","EE. UU.",
"Guardar tokens para","A (principal / compartida)","B (solo si tienes una segunda cuenta CareLink)",
"1. Iniciar sesión en CareLink","Enlace o código tras iniciar sesión","2. Guardar tokens","← Volver a parámetros","Menú",
"No elegiste un paciente.","Paciente guardado (","Volver a CareLink","Parámetros","Paso 1 listo",
"Abre CareLink, inicia sesión y resuelve el captcha.","Abrir inicio de sesión CareLink",
"Tras iniciar sesión verás un error de la app — copia el enlace (com.medtronic.carepartner…) y pégalo abajo.",
"Pega el enlace / código copiado","Listo — tokens guardados para ","Elegir paciente →","Inicio","Reintentar",
],
fr: [
"Système 1/3","Infos","Actualiser","Auto-upd: ON","Auto-upd: OFF","Mise à jour","Effacer WiFi","Retour","Limites 2/3",
"Bas","Haut","Critique","Unités: mg/dL","Unités: mmol","Delta: ON","Delta: OFF","Luminosité 3/3",
"Luminosité jour: %d","Luminosité nuit: %d","Jour dès: %02d:00","Nuit dès: %02d:00","Enregistrer","Jazyk: SK","Language: EN",
"Luminosité jour","Luminosité nuit","Bas (mg/dL)","Objectif bas","Haut (mg/dL)","Objectif haut","Critique (mg/dL)","Critique",
"Début du jour (h)","Début de la nuit (h)","IP\nWiFi\nTemp\nVersion","Effacer tous\nles réseaux ?","Oui, effacer",
"Chargement...","Ancien","Sync...","Mise à jour... %d%%","Vérification...","WiFi non\nconnecté.","Serveur\nindisponible",
"Vous avez la\ndernière version.","Échec du téléchargement","Mise à jour... 0%",
"Connexion...","WiFi à nouveau","Scan...","Connecté !",
"1. WiFi: GlukoView\n2. Web: 10.0.0.1\n3. Choisir un réseau","Nouveaux essais des\nréseaux enregistrés...","Enregistrement et\nredémarrage...",
"WiFi","Auth","Réseau","Vide","Config",
"GlukoView","Source de données","Choisir la source","Nightscout","Uniquement si la source est Nightscout","URL Nightscout","Jeton Nightscout",
"Libre LinkUp","1 compte Libre + 2 personnes: même e-mail/mot de passe pour A et B, patient différent. 2 comptes: identifiants différents.",
"E-mail / utilisateur","Mot de passe","Patient (nom ou ID)",
"Choisissez un nom. La liste se remplit après connexion Libre. En dual A/B peuvent avoir des patients différents.",
"Région","Affichage","Unités","Ignorer la vérif. TLS","Oui","Non",
"Objectif bas (mmol)","Objectif haut (mmol)","Hyper critique (mmol)",
"Guardian (CareLink)","Connexion une fois sur /carelink. 1 compte + 2 personnes = un login, patient A/B différent. 2 comptes = login séparé pour B.",
"Patient Guardian","Choisir après connexion CareLink. En dual A et B peuvent utiliser des patients différents du même compte.",
"JSON CareLink","Enregistrer","Retour","WiFi enregistrés","WiFi enregistrés",
"Jusqu’à 5 réseaux. Au démarrage et après une coupure, tous sont essayés. Un nouveau réseau est enregistré après connexion.",
"Configurer le WiFi","Retour au menu","WiFi enregistrés (max 5)","Aucun pour l’instant. Enregistrés après connexion.","(actuel)","Langue",
"(dernier)","(sans mot de passe !)","Source: ON","Source: OFF","Nouvelle version.\nInstallation...",
"Dexcom Share","Uniquement si Dexcom est sélectionné. Connexion de l’app Dexcom du patient, pas un compte follower. Le partage doit être activé.",
"Utilisateur A","Utilisateur B","Nom","Mode d’affichage","1 — un utilisateur","2 — deux côte à côte","Désactivé",
"Son: ON","Son: OFF","Snooze: %d min","Couper le son","Snooze %d m","Afficher: %s","Mode: 1 personne","Mode: 2 personnes",
"Jetons A: enregistrés","Jetons A: manquants — connectez-vous sur /carelink",
"1 compte → jetons A partagés, patient A/B différent. 2 comptes CareLink → connexion séparée pour B sur /carelink.",
"optionnel: logindata.json — sinon /carelink",
"CareLink / Guardian","Les jetons sont enregistrés.","Pas encore de jetons — terminez la connexion ci-dessous.",
"Choisir un patient","Pour l’utilisateur","1 compte CareLink: connectez-vous une fois (A), choisissez ici des patients différents pour A et B.",
"Enregistrer le patient pour ","La liste des patients est vide.","Impossible de charger les patients: ",
"En attente du code CareLink (valide ~10 min).","Mode d’emploi (mobile)",ClHow.fr,
"Utilisez un compte Care Partner (follower), pas le compte patient.","Région","UE / Slovaquie / Europe","USA",
"Enregistrer les jetons pour","A (principal / partagé)","B (uniquement si 2e compte CareLink)",
"1. Se connecter à CareLink","Lien ou code après connexion","2. Enregistrer les jetons","← Retour aux paramètres","Menu",
"Vous n’avez pas choisi de patient.","Patient enregistré (","Retour à CareLink","Paramètres","Étape 1 terminée",
"Ouvrez CareLink, connectez-vous et résolvez le captcha.","Ouvrir la connexion CareLink",
"Après connexion vous verrez une erreur d’app — copiez le lien (com.medtronic.carepartner…) et collez-le ci-dessous.",
"Collez le lien / code copié","Terminé — jetons enregistrés pour ","Choisir le patient →","Accueil","Réessayer",
],
};

const langs = ["sk","cs","en","de","es","fr"];
const enumNames = ["Sk","Cs","En","De","Es","Fr"];

for (const L of langs) {
  if (T[L].length !== keys.length) {
    console.error(`${L}: got ${T[L].length}, expected ${keys.length}`);
    process.exit(1);
  }
}

function esc(s) {
  return '"' + s.replace(/\\/g, "\\\\").replace(/"/g, '\\"').replace(/\n/g, "\\n") + '"';
}

let out = `#include "dia_i18n.h"
#include "dia_config.h"

static constexpr int LANG_COUNT = ${langs.length};
static DiaLang s_lang = DiaLang::Sk;
static bool s_lang_ready = false;

static const char *const kNativeName[LANG_COUNT] = {
    "Slovenčina", "Čeština", "English", "Deutsch", "Español", "Français"
};
static const char *const kCode[LANG_COUNT] = {
    "SK", "CS", "EN", "DE", "ES", "FR"
};

static const char *const kText[LANG_COUNT][(int)Str::StrCount] = {
`;

for (const L of langs) {
  out += "    {\n";
  const rows = [];
  for (let i = 0; i < T[L].length; i += 4) {
    rows.push("        " + T[L].slice(i, i + 4).map(esc).join(", "));
  }
  out += rows.join(",\n") + "\n    },\n";
}
out += `};

`;

for (let i = 0; i < langs.length; i++) {
  out += `static_assert(sizeof(kText[${i}]) / sizeof(kText[${i}][0]) == (int)Str::StrCount, "${langs[i].toUpperCase()} table size");\n`;
}

out += `
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

DiaLang diaI18nFromCode(const char *code) {
    if (!code || !code[0]) return DiaLang::Sk;
    for (int i = 0; i < LANG_COUNT; i++) {
        if (strcasecmp(code, kCode[i]) == 0) return (DiaLang)i;
        // also accept lowercase locale tags like "sk"
        if (code[0] && tolower((unsigned char)code[0]) == tolower((unsigned char)kCode[i][0]) &&
            code[1] && tolower((unsigned char)code[1]) == tolower((unsigned char)kCode[i][1]) &&
            code[2] == '\\0') {
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
    uint8_t v = preferences.isKey("lang") ? preferences.getUChar("lang", 0) : 0;
    if (v >= LANG_COUNT) v = 0;
    s_lang = (DiaLang)v;
    s_lang_ready = preferences.isKey("lang_set") && preferences.getBool("lang_set", false);
    // Legacy: language was already chosen (SK/EN) without lang_set flag
    if (!s_lang_ready && preferences.isKey("lang")) {
        s_lang_ready = true;
        preferences.putBool("lang_set", true);
    }
}
`;

const outPath = path.join(__dirname, "..", "src", "dia_i18n.cpp");
fs.writeFileSync(outPath, out, "utf8");
console.log("Wrote", outPath, "keys=", keys.length, "langs=", langs.length);
