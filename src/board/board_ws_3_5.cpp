#include "dia_board.h"

#if defined(DIA_BOARD_WS_3_5)

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <TCA9554.h>
#include <string.h>
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>

#define LCD_QSPI_CS 12
#define LCD_QSPI_CLK 5
#define LCD_QSPI_D0 1
#define LCD_QSPI_D1 2
#define LCD_QSPI_D2 3
#define LCD_QSPI_D3 4
#define TFT_BL_PIN 6

#define I2C_SDA 8
#define I2C_SCL 7
#define TOUCH_ADDR 0x3B

#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RES 8

static Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_QSPI_CS, LCD_QSPI_CLK, LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3);

static Arduino_GFX *gfx = new Arduino_AXS15231B(
    bus, GFX_NOT_DEFINED, 0 /* rotation */, false, DIA_PANEL_W, DIA_PANEL_H);

static TCA9554 tca(0x20);
static XPowersPMU pmu;
static bool touch_ok = false;
static bool gfx_ok = false;
static bool tca_ok = false;

/** 0=USB vpravo (default), 1=USB vľavo (180°). */
static uint8_t s_orientMode = 0;
static bool s_flip = false;

// Blok riadkov v internej RAM – menej CS toggle než 1 riadok = 1 writePixels
static constexpr int PRESENT_ROWS = 8;
static uint16_t present_block[DIA_PANEL_W * PRESENT_ROWS];

static void logFlush(const char *msg) {
    Serial.println(msg);
    Serial.flush();
}

static void initPmu() {
    if (!pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        logFlush("[board] AXP2101 not found");
        return;
    }
    // Railly podľa Waveshare factory (bsp_axp2101) – DLDO1/2 treba pre periférie
    pmu.setALDO1Voltage(3300);
    pmu.setALDO2Voltage(3300);
    pmu.setALDO3Voltage(3300);
    pmu.setALDO4Voltage(3300);
    pmu.setBLDO1Voltage(1500);
    pmu.setBLDO2Voltage(2800);
    pmu.setDLDO1Voltage(3300);
    pmu.setDLDO2Voltage(3300);
    pmu.enableALDO1();
    pmu.enableALDO2();
    pmu.enableALDO3();
    pmu.enableALDO4();
    pmu.enableBLDO1();
    pmu.enableBLDO2();
    pmu.enableDLDO1();
    pmu.enableDLDO2();
    pmu.disableTSPinMeasure();
    logFlush("[board] AXP2101 rails ON (+DLDO1/2)");
}

static void lcdResetViaTca() {
    if (!tca.begin()) {
        logFlush("[board] TCA9554 begin failed");
        tca_ok = false;
        return;
    }
    tca_ok = true;
    // EXIO7 = NS4150B PA_CTRL (active HIGH). Najprv output-register HIGH,
    // až potom pinMode OUTPUT – inak default LOW vypne amp a pretlačí pull-up.
    tca.write1(7, 1);
    tca.pinMode1(7, OUTPUT);
    tca.write1(7, 1);

    tca.pinMode1(1, OUTPUT);
    tca.write1(1, 1);
    delay(10);
    tca.write1(1, 0);
    delay(10);
    tca.write1(1, 1);
    delay(200);
    tca.write1(7, 1);
    Serial.printf("[board] TCA PA EXIO7=%d\n", (int)tca.read1(7));
    Serial.flush();
    logFlush("[board] TCA9554 LCD reset OK (PA EXIO7 HIGH)");
}

static bool touchReadRawPanel(int16_t *px, int16_t *py) {
    const uint8_t read_cmd[11] = {
        0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 14, 0x00, 0x00, 0x00};
    Wire.beginTransmission(TOUCH_ADDR);
    if (Wire.write(read_cmd, sizeof(read_cmd)) != sizeof(read_cmd)) return false;
    if (Wire.endTransmission(false) != 0) return false;

    const int n = 14;
    if (Wire.requestFrom((int)TOUCH_ADDR, n) != n) return false;
    uint8_t buf[14];
    for (int i = 0; i < n; i++) buf[i] = Wire.read();

    uint8_t points = buf[1] & 0x0F;
    if (points == 0 || points > 2) return false;

    int16_t tx = ((buf[2] & 0x0F) << 8) | buf[3];
    int16_t ty = ((buf[4] & 0x0F) << 8) | buf[5];
    if (tx < 0 || ty < 0 || tx >= DIA_PANEL_W || ty >= DIA_PANEL_H) return false;
    *px = tx;
    *py = ty;
    return true;
}

static void panelToLvgl(int16_t px, int16_t py, int16_t *lx, int16_t *ly) {
    if (!s_flip) {
        // 90° CW: USB vpravo (predvolené)
        *lx = py;
        *ly = (int16_t)(DIA_PANEL_W - 1 - px);
    } else {
        // 90° CCW: USB vľavo (= 180° voči predvolenému landscape)
        *lx = (int16_t)(DIA_PANEL_H - 1 - py);
        *ly = px;
    }
}

/**
 * Landscape 480×320 → panel 320×480.
 * flip=false: 90° CW; flip=true: 90° CCW.
 */
static void presentLandToPanel(const uint16_t *src) {
    if (!gfx_ok || !src) return;

    Arduino_TFT *tft = (Arduino_TFT *)gfx;
    tft->startWrite();
    tft->writeAddrWindow(0, 0, DIA_PANEL_W, DIA_PANEL_H);

    for (int16_t py0 = 0; py0 < DIA_PANEL_H; py0 += PRESENT_ROWS) {
        const int16_t rows =
            (py0 + PRESENT_ROWS <= DIA_PANEL_H) ? PRESENT_ROWS : (int16_t)(DIA_PANEL_H - py0);
        for (int16_t r = 0; r < rows; r++) {
            const int16_t py = (int16_t)(py0 + r);
            uint16_t *dst = present_block + (int32_t)r * DIA_PANEL_W;
            if (!s_flip) {
                // panel(px,py) = land(py, PANEL_W-1-px)
                const uint16_t *p = src + (int32_t)(DIA_PANEL_W - 1) * DIA_SCREEN_W + py;
                for (int16_t px = 0; px < DIA_PANEL_W; px++) {
                    dst[px] = *p;
                    p -= DIA_SCREEN_W;
                }
            } else {
                // panel(px,py) = land(PANEL_H-1-py, px)
                const int16_t lx = (int16_t)(DIA_PANEL_H - 1 - py);
                for (int16_t px = 0; px < DIA_PANEL_W; px++) {
                    dst[px] = src[(int32_t)px * DIA_SCREEN_W + lx];
                }
            }
        }
        tft->writePixels(present_block, (uint32_t)DIA_PANEL_W * (uint32_t)rows);
        yield();
    }
    tft->endWrite();
}

void diaBoardInit() {
    Serial.printf("[board] %s init LVGL %dx%d panel %dx%d psram=%d\n",
                  diaBoardName(), DIA_SCREEN_W, DIA_SCREEN_H,
                  DIA_PANEL_W, DIA_PANEL_H, (int)psramFound());
    Serial.flush();

    // Backlight OFF kým nie je čierna / logo – inak sum z neinicializovanej GRAM
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
    ledcAttachPin(TFT_BL_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);

    Wire.begin(I2C_SDA, I2C_SCL);
    initPmu();
    lcdResetViaTca();

    if (!gfx->begin()) {
        logFlush("[board] AXS15231B begin FAILED");
        gfx_ok = false;
    } else {
        gfx_ok = true;
        gfx->fillScreen(RGB565_BLACK);
        logFlush("[board] AXS15231B OK (black, BL off)");
    }

    Wire.beginTransmission(TOUCH_ADDR);
    touch_ok = (Wire.endTransmission() == 0);
    Serial.printf("[board] touch 0x%02X %s\n", TOUCH_ADDR, touch_ok ? "OK" : "missing");
    Serial.flush();
}

void diaBoardSetBrightness(uint8_t level) {
    ledcWrite(PWM_CHANNEL, level);
}

void diaBoardFlush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint16_t *pixels) {
    // Full-refresh: LVGL buffer sa presentuje cez diaBoardPresentLand.
    (void)x1;
    (void)y1;
    (void)x2;
    (void)y2;
    (void)pixels;
}

void diaBoardFlushPresent() {}

void diaBoardPresentLand(const uint16_t *land) {
    presentLandToPanel(land);
}

bool diaBoardTouchRead(int16_t *x, int16_t *y) {
    if (!touch_ok) return false;
    int16_t px = 0, py = 0;
    if (!touchReadRawPanel(&px, &py)) return false;
    int16_t lx = 0, ly = 0;
    panelToLvgl(px, py, &lx, &ly);
    if (lx < 0 || ly < 0 || lx >= DIA_SCREEN_W || ly >= DIA_SCREEN_H) return false;
    *x = lx;
    *y = ly;
    return true;
}

bool diaBoardIsRound() { return false; }

const char *diaBoardName() { return "WS-3.5B"; }

bool diaBoardPreferFullRefresh() { return true; }

bool diaBoardIsLandscape() { return true; }

bool diaBoardHasAudio() { return true; }

void diaBoardSpeakerAmp(bool enable) {
    if (!tca_ok) return;
    // NS4150B CTRL: HIGH = on. Držať stabilne – nepreklikávať (prichádza prask).
    tca.write1(7, enable ? 1 : 0);
}

bool diaBoardSupportsOrient() { return true; }

uint8_t diaBoardGetOrientMode() { return s_orientMode; }

bool diaBoardIsFlipped() { return s_flip; }

void diaBoardSetOrientMode(uint8_t mode) {
    s_orientMode = mode ? 1 : 0;
    s_flip = (s_orientMode == 1);
    Serial.printf("[board] orient mode=%u flip=%d\n",
                  (unsigned)s_orientMode, (int)s_flip);
    Serial.flush();
}

#endif // DIA_BOARD_WS_3_5
