# Security

## Reporting

Ak nájdeš chybu, ktorá môže ohroziť heslá, tokeny alebo PIN na zariadení v sieti, napíš súkromne maintainerovi (GitHub Security Advisory alebo e-mail z profilu), nie verejný Issue s dumpom tokenov.

## Čo je zámerne obmedzené

- Web portál na LAN môže byť zamknutý PINom (uložený len hash).
- Heslá a CareLink tokeny v NVS sú AES-GCM; kľúč je odvodený z MAC čipu — fyzický prístup k doske = možnosť odvodiť kľúč.
- OTA ide len cez HTTPS a ECDSA podpis. `tools/ota_private.pem` do gitu nepatrí.

## Neoficiálne API

Libre LinkUp, Dexcom Share a Medtronic CareLink sú reverse-engineered napojenia. Výrobca ich môže zmeniť alebo zablokovať bez varovania. GlukoView nie je oficiálna aplikácia týchto služieb. Použitie je na vlastné riziko a môže byť v rozpore s podmienkami služby výrobcu.

## Čo neposielať do Issues

- CareLink / Libre / Dexcom heslá a refresh tokeny
- `logindata.json`, sériové logy s tokenmi
- Obsah `tools/ota_private.pem`
