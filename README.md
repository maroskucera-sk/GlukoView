# GlukoView

**Toto nie je zdravotnícka pomôcka.** Hodnota na displeji je len orientačná. Nesmie byť jediný podklad na dávku inzulínu ani na rozhodnutie o liečbe. Over si ju na senzore alebo na čítačke, ktorú používaš s lekárom.

**This is not a medical device.** Do not use the number on this display as the only basis for an insulin dose or a treatment decision.

Stolný ukazovateľ glykémie pre dve dosky Waveshare s ESP32-S3. Nie je to hodinky. Jeden zdroj, dva obrazy firmvéru. Menu je po slovensky, v nastavení sú aj čeština, angličtina, nemčina, španielčina a francúzština.

## Dosky (hardware)

| Doska | PlatformIO env | OTA priečinok | Poznámka |
| --- | --- | --- | --- |
| [Waveshare ESP32-S3 1.28" round](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28) (GC9A01) | `waveshare_s3_1_28` | `update/ws128/` | 1 používateľ |
| [Waveshare ESP32-S3-Touch-LCD-3.5B](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.5B) | `waveshare_s3_3_5` | `update/ws35/` | dual A/B |

Obraz z jednej dosky sa nesmie nahrať do druhej. Ručný upload 3.5B: env `waveshare_s3_3_5_manual` (podrž BOOT → RST → pusť BOOT).

## Build a flash

Treba [PlatformIO](https://platformio.org/).

```bash
pio run -e waveshare_s3_1_28
pio run -e waveshare_s3_3_5

pio run -e waveshare_s3_1_28 -t upload --upload-port COMx
pio run -e waveshare_s3_3_5 -t upload --upload-port COMy
```

GitHub Actions preloží oba obrazy pri pushi a PR (pozri `.github/workflows/firmware.yml`).

## Prvé nastavenie (web)

1. Po štarte bez WiFi vznikne AP `GlukoView` (bez hesla AP — bežné pri tomto type portálu).
2. Otvor konfiguráciu, pripoj domácu sieť.
3. Na IP dosky v LAN: `/param` (zdroje, limity), pre Guardian `/carelink` (OAuth token, výber pacienta).
4. Na displeji nastav **PIN** portálu. Heslá sa do formulárov nevypisujú; prázdne heslo = nechaj uložené.

**Zrušiť PIN** zmaže len PIN. **Zmazať WiFi** zmaže úplne všetko.

### CareLink po odpojení napájania

Po USB reconnecte / reštarte sa má CareLink obnoviť sám (refresh token + discovery). Ak nie: `/carelink` a znova prihlásenie. Odporúčanie: **oddeliť Care Partner konto telefónu od konta na monitore** (Auth0 rotácia refresh tokenu = jedna aktívna session na účet).

## Zdroje glykémie

Nightscout, Libre LinkUp, Dexcom Share a Guardian (CareLink) sú napojené **neoficiálne**. Nie sú to verejné API tohto displeja; výrobca ich môže kedykoľvek zmeniť a ToS ich nemusia dovoliť. GlukoView nie je schválená aplikácia Libre, Dexcom ani Medtronic. Obídenie podpisu Dexcom follower účtu v tomto kóde nie je.

Použitie je na vlastné riziko (community project). Podrobnosti: [SECURITY.md](SECURITY.md).

## Aktualizácia (OTA)

Firmvér sa sťahuje len cez HTTPS a len keď sedí podpis. Prepínač „TLS bez overenia“ platí pre Nightscout a podobné služby, **nie** pre OTA.

Na serveri pre každú dosku:

- `version.txt` — napr. `1.1` (musí sedieť s buildom)
- `firmware.bin`
- `firmware.sig`

Súkromný kľúč: `tools/ota_private.pem` (**do gitu nepatrí**). Verejný: `include/dia_ota_pubkey.h`.

```bash
python tools/sign_ota.py .pio/build/waveshare_s3_1_28/firmware.bin -o firmware.sig
```

## Heslá v doske

WiFi, Libre/Dexcom heslá, Nightscout token a CareLink tokeny sú v NVS (`dia-config`) AES-GCM. Kľúč z MAC čipu. Sériový log vypisuje dĺžky, nie heslá.

## Licencia

MIT — pozri [LICENSE](LICENSE).
