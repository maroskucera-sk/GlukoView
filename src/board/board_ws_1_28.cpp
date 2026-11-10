#include "dia_board.h"

#if defined(DIA_BOARD_WS_1_28)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <CST816S.h>
#include <esp_rom_sys.h>

#define TOUCH_SDA 6
#define TOUCH_SCL 7
#define TOUCH_RST 13
#define TOUCH_INT 5
#define TFT_BL_PIN 2

#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RES 8

static Arduino_DataBus *bus = new Arduino_ESP32SPI(
    8 /* DC */, 9 /* CS */, 10 /* SCK */, 11 /* MOSI */, GFX_NOT_DEFINED /* MISO */);

static Arduino_GFX *gfx = new Arduino_GC9A01(
    bus, 14 /* RST */, 0 /* rotation */, true /* IPS */);

static CST816S touch(TOUCH_SDA, TOUCH_SCL, TOUCH_RST, TOUCH_INT);

void diaBoardInit() {
    // Backlight OFF kým nie je čierna / logo – rovnaký čistý štart ako 3.5B
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
    ledcAttachPin(TFT_BL_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);
    touch.begin();
    gfx->begin();
    gfx->fillScreen(RGB565_BLACK);
    delay(20);
    gfx->fillScreen(RGB565_BLACK);
}

void diaBoardSetBrightness(uint8_t level) {
    ledcWrite(PWM_CHANNEL, level);
}

void diaBoardFlush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint16_t *pixels) {
    uint32_t w = (uint32_t)(x2 - x1 + 1);
    uint32_t h = (uint32_t)(y2 - y1 + 1);
    gfx->draw16bitRGBBitmap(x1, y1, (uint16_t *)pixels, w, h);
}

bool diaBoardTouchRead(int16_t *x, int16_t *y) {
    if (!touch.available()) return false;
    *x = touch.data.x;
    *y = touch.data.y;
    return true;
}

bool diaBoardTouchOk() { return true; }

bool diaBoardIsRound() { return true; }

const char *diaBoardName() { return "WS-1.28"; }

bool diaBoardPreferFullRefresh() { return false; }

void diaBoardFlushPresent() {}

void diaBoardPresentLand(const uint16_t *land) { (void)land; }

bool diaBoardIsLandscape() { return false; }

bool diaBoardHasAudio() { return false; }
int diaBoardI2sMclkPin() { return -1; }

void diaBoardSpeakerAmp(bool enable) { (void)enable; }

bool diaBoardSupportsOrient() { return false; }
uint8_t diaBoardGetOrientMode() { return 0; }
void diaBoardSetOrientMode(uint8_t mode) { (void)mode; }
bool diaBoardIsFlipped() { return false; }

// BAT_ADC: 200k+100k, Vbat = Vpin * 3. STAT nie je na GPIO.
// ETA6096 po EOC vypne buck → spotreba ide z článku aj pri USB (VBUS dióda
// na VSYS by mala držať napájanie; pri glitchi/BMS cut pomôže len re-plug).
static int battPinMv() {
    static bool ready = false;
    if (!ready) {
        analogSetPinAttenuation(1, ADC_11db);
        ready = true;
    }
    int sum = 0;
    int n = 0;
    for (int i = 0; i < 8; i++) {
        int mv = analogReadMilliVolts(1);
        if (mv < 0) continue;
        sum += mv;
        n++;
    }
    if (n == 0) return 0;
    return (sum / n) * 3;
}

static bool s_battPowerSave = false;

bool diaBoardBattPowerSave() { return s_battPowerSave; }

void diaBoardBattRead(DiaBattView *out) {
    static DiaBattView cached = {};
    static uint32_t lastMs = 0;
    static int hist[7];
    static int histN = 0;
    static int histPos = 0;
    static bool charging = false;
    static uint8_t bars = 0;
    static bool haveBars = false;
    static bool loggedAbsent = false;
    static int lastLogMv = -10000;
    static int lastCellMv = 0;
    static bool sawCharge = false;

    const uint32_t now = millis();
    if (lastMs != 0 && (uint32_t)(now - lastMs) < 10000) {
        if (out) *out = cached;
        return;
    }
    lastMs = now;

    const int mv = battPinMv();
    DiaBattView v = {};
    // USB bez článku: float ~4120–4200. Plný článok na USB je tiež ~4.15–4.2 V –
    // NIKDY to nebrať ako „batéria preč“ (starý prah >=4110 ×3 to robil).
    // Článok: 2× mv<=4090, alebo už sme ho mali a napätie ostáva v 2.8–4.5 V.
    // Preč: mimo rozsah, alebo skok hore do floatu z jasne nižšieho článku (≥80 mV).
    static constexpr int kCellMax = 4090;
    static bool wasPresent = false;
    static uint8_t cellHits = 0;
    bool cell = false;
    if (mv < 2800 || mv >= 4500) {
        cell = false;
        cellHits = 0;
        sawCharge = false;
        s_battPowerSave = false;
    } else if (wasPresent) {
        cell = true;
        if (lastCellMv > 0 && lastCellMv < 4080 && mv >= 4110 && (mv - lastCellMv) >= 80) {
            // Odpojený článok → návrat na USB float
            cell = false;
            cellHits = 0;
            sawCharge = false;
            s_battPowerSave = false;
        }
    } else if (mv <= kCellMax) {
        if (cellHits < 2) cellHits++;
        cell = (cellHits >= 2);
    } else {
        cellHits = 0;
        cell = false;
    }
    wasPresent = cell;
    if (cell) {
        loggedAbsent = false;
        v.present = true;
        lastCellMv = mv;
        bars = diaBattBarsStable(bars, haveBars, mv);
        haveBars = true;
        const int oldest = (histN == 7) ? hist[histPos] : mv;
        hist[histPos] = mv;
        histPos = (histPos + 1) % 7;
        if (histN < 7) histN++;
        if (histN == 7) {
            const int delta = mv - oldest;
            if (delta >= 30) {
                charging = true;
                sawCharge = true;
                s_battPowerSave = false;
            } else if (delta <= -25) {
                charging = false;
                // Po nabití / pri plnom článku: klesá → ETA6096 EOC, beží z BAT
                if (sawCharge || mv >= 4000) {
                    if (!s_battPowerSave) {
                        esp_rom_printf("[batt] EOC/vybijanie %d mV – setrim jas\n", mv);
                    }
                    s_battPowerSave = true;
                }
            }
            // Stabilný float / plný na USB: ukazuj nabíjanie (blesk), nie „odpojené“
            if (mv >= 4150 && delta > -15) {
                charging = true;
                sawCharge = true;
                s_battPowerSave = false;
            }
            // Auto-recharge prah ETA6096 ~ EOC−160 mV → znova nabíja
            if (s_battPowerSave && mv <= 4040 && delta >= 0) {
                s_battPowerSave = false;
            }
        }
        v.bars = bars;
        v.charging = charging;
        if (lastLogMv < -1000 || v.bars != cached.bars || v.charging != cached.charging ||
            (mv - lastLogMv >= 40) || (lastLogMv - mv >= 40) ||
            (s_battPowerSave != (cached.charging == false && sawCharge))) {
            esp_rom_printf("[batt] %d mV bars=%u chg=%d save=%d\n", mv, v.bars,
                           v.charging ? 1 : 0, s_battPowerSave ? 1 : 0);
            lastLogMv = mv;
        }
    } else {
        haveBars = false;
        histN = 0;
        histPos = 0;
        charging = false;
        bars = 0;
        lastCellMv = 0;
        s_battPowerSave = false;
        if (!loggedAbsent || lastLogMv < -1000 || (mv - lastLogMv >= 20) || (lastLogMv - mv >= 20)) {
            esp_rom_printf("[batt] ziadna (%d mV)\n", mv);
            loggedAbsent = true;
            lastLogMv = mv;
        }
    }
    cached = v;
    if (out) *out = v;
}

#endif // DIA_BOARD_WS_1_28
