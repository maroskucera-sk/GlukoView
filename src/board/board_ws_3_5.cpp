#include "dia_board.h"

#if defined(DIA_BOARD_WS_3_5)

/**
 * Waveshare 3.5" rodina – DVA nekompatibilné panely, rovnaký tvar PCB:
 *
 *   ESP32-S3-Touch-LCD-3.5   = ST7796 SPI  + FT6336 @0x38
 *   ESP32-S3-Touch-LCD-3.5B  = AXS15231B QSPI + touch @0x3B
 *
 * Firmware Waveshare 3.5B na 3.5 = čierna obrazovka (a naopak).
 * Auto-detect: I2C 0x3B → 3.5B, inak → 3.5 (ST7796). Historický twin COM4 = 3.5B.
 */

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <TCA9554.h>
#include <string.h>
#include <esp_heap_caps.h>
#include <esp_rom_sys.h>
#define XPOWERS_CHIP_AXP2101
#include <XPowersLib.h>
#include "axs15231b_type2_init.h"

#define TFT_BL_PIN 6
#define I2C_SDA 8
#define I2C_SCL 7
#define TOUCH_ADDR_AXS 0x3B
#define TOUCH_ADDR_FT 0x38

#define LCD_QSPI_CS 12
#define LCD_QSPI_CLK 5
#define LCD_QSPI_D0 1
#define LCD_QSPI_D1 2
#define LCD_QSPI_D2 3
#define LCD_QSPI_D3 4

// Oficiálny Waveshare 3.5 helloworld
#define LCD_SPI_DC 3
#define LCD_SPI_CS GFX_NOT_DEFINED
#define LCD_SPI_SCLK 5
#define LCD_SPI_MOSI 1
#define LCD_SPI_MISO 2

#define PWM_CHANNEL 0
#define PWM_FREQ 5000
#define PWM_RES 8

static Arduino_DataBus *bus = nullptr;
static Arduino_GFX *gfx = nullptr;

static TCA9554 tca(0x20);
static XPowersPMU pmu;
static bool touch_ok = false;
static bool gfx_ok = false;
static bool tca_ok = false;
static uint8_t touch_addr = TOUCH_ADDR_FT;
static bool touch_ft = true;
static bool panel_st7796 = true;
static bool bl_pwm = false;
static const char *board_name = "WS-3.5";

static uint8_t s_orientMode = 0;
static bool s_flip = false;
static uint16_t *present_fb = nullptr;

static void logFlush(const char *msg) { esp_rom_printf("%s\n", msg); }

static void blDetachPwm() {
    if (!bl_pwm) return;
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
    ledcDetach(TFT_BL_PIN);
#else
    ledcDetachPin(TFT_BL_PIN);
#endif
    bl_pwm = false;
}

static void blSet(uint8_t level) {
    if (level >= 255) {
        blDetachPwm();
        pinMode(TFT_BL_PIN, OUTPUT);
        digitalWrite(TFT_BL_PIN, HIGH);
        return;
    }
    if (level == 0) {
        blDetachPwm();
        pinMode(TFT_BL_PIN, OUTPUT);
        digitalWrite(TFT_BL_PIN, LOW);
        return;
    }
    if (!bl_pwm) {
        ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RES);
        ledcAttachPin(TFT_BL_PIN, PWM_CHANNEL);
        bl_pwm = true;
    }
    ledcWrite(PWM_CHANNEL, level);
}

static void initPmu() {
    if (!pmu.begin(Wire, AXP2101_SLAVE_ADDRESS, I2C_SDA, I2C_SCL)) {
        logFlush("[board] AXP2101 not found");
        return;
    }
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
    pmu.enableCPUSLDO();
    pmu.disableTSPinMeasure();
    delay(50);
    logFlush("[board] AXP2101 rails ON");
}

static void lcdResetViaTca() {
    if (!tca.begin()) {
        logFlush("[board] TCA9554 begin failed");
        tca_ok = false;
        return;
    }
    tca_ok = true;
    tca.pinMode1(1, OUTPUT);
    tca.write1(1, 1);
    delay(10);
    tca.write1(1, 0);
    delay(10);
    tca.write1(1, 1);
    delay(200);
    logFlush("[board] TCA9554 LCD reset OK");
}

static void i2cScanLog() {
    esp_rom_printf("[board] I2C:");
    int n = 0;
    for (uint8_t a = 1; a < 127; a++) {
        Wire.beginTransmission(a);
        if (Wire.endTransmission() == 0) {
            esp_rom_printf(" 0x%02X", a);
            n++;
        }
    }
    if (!n) esp_rom_printf(" (none)");
    esp_rom_printf("\n");
}

static bool i2cAck(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

/** 0x3B = 3.5B (AXS), inak 3.5 (ST7796 + FT@0x38). */
static void selectPanelByTouch() {
    if (i2cAck(TOUCH_ADDR_AXS)) {
        panel_st7796 = false;
        touch_ft = false;
        touch_addr = TOUCH_ADDR_AXS;
        touch_ok = true;
        board_name = "WS-3.5B";
        logFlush("[board] detect: AXS@0x3B → 3.5B QSPI");
        return;
    }
    panel_st7796 = true;
    touch_ft = true;
    touch_addr = TOUCH_ADDR_FT;
    touch_ok = i2cAck(TOUCH_ADDR_FT);
    board_name = "WS-3.5";
    esp_rom_printf("[board] detect: no 0x3B → 3.5 SPI (FT@0x38 %s)\n",
                   touch_ok ? "OK" : "missing");
}

static bool touchReadRawAxs(int16_t *px, int16_t *py) {
    const uint8_t read_cmd[11] = {
        0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 14, 0x00, 0x00, 0x00};
    Wire.beginTransmission(touch_addr);
    if (Wire.write(read_cmd, sizeof(read_cmd)) != sizeof(read_cmd)) return false;
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)touch_addr, 14) != 14) return false;
    uint8_t buf[14];
    for (int i = 0; i < 14; i++) buf[i] = Wire.read();
    uint8_t points = buf[1] & 0x0F;
    if (points == 0 || points > 2) return false;
    int16_t tx = ((buf[2] & 0x0F) << 8) | buf[3];
    int16_t ty = ((buf[4] & 0x0F) << 8) | buf[5];
    if (tx < 0 || ty < 0 || tx >= DIA_PANEL_W || ty >= DIA_PANEL_H) return false;
    *px = tx;
    *py = ty;
    return true;
}

/** FT6336: bez monitor módu (inak I2C „spi“ a lag), rýchly report. */
static void ftTouchInit() {
    auto wr = [](uint8_t reg, uint8_t val) {
        Wire.beginTransmission(TOUCH_ADDR_FT);
        Wire.write(reg);
        Wire.write(val);
        Wire.endTransmission();
    };
    wr(0x86, 0);   // CTRL: zakáž auto → MONITOR (v monitori bus mlčí)
    wr(0xA5, 0);   // PMODE = ACTIVE
    wr(0x87, 60);  // TIMEENTERMONITOR (s) – poistka ak by CTRL zlyhalo
    wr(0x88, 4);   // PERIOD_ACTIVE min (4…14) = najrýchlejší scan
    wr(0x89, 4);   // PERIOD_MONITOR
    wr(0x80, 22);  // THGROUP – citlivejší než stock ~40
}

static bool touchReadRawFt(int16_t *px, int16_t *py) {
    // Jeden I2C burst ako TouchDrvFT6X36 (reg 0x02…Y)
    Wire.beginTransmission(touch_addr);
    Wire.write(0x02);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)touch_addr, 5) != 5) return false;
    uint8_t n = Wire.read() & 0x0F;
    if (n == 0 || n > 2) {
        while (Wire.available()) Wire.read();
        return false;
    }
    uint8_t xh = Wire.read(), xl = Wire.read(), yh = Wire.read(), yl = Wire.read();
    int16_t tx = (int16_t)(((xh & 0x0F) << 8) | xl);
    int16_t ty = (int16_t)(((yh & 0x0F) << 8) | yl);
    if (tx < 0 || ty < 0 || tx >= DIA_PANEL_W + 8 || ty >= DIA_PANEL_H + 8) return false;
    if (tx >= DIA_PANEL_W) tx = DIA_PANEL_W - 1;
    if (ty >= DIA_PANEL_H) ty = DIA_PANEL_H - 1;
    *px = tx;
    *py = ty;
    return true;
}

static bool touchReadRawPanel(int16_t *px, int16_t *py) {
    return touch_ft ? touchReadRawFt(px, py) : touchReadRawAxs(px, py);
}

/**
 * Panel portrait → LVGL landscape (3.5 FT aj 3.5B AXS).
 * Sedí na soft-rotate present aj na ST7796 HW setRotation(1/3).
 */
static void panelToLvgl(int16_t px, int16_t py, int16_t *lx, int16_t *ly) {
    if (!s_flip) {
        *lx = py;
        *ly = (int16_t)(DIA_PANEL_W - 1 - px);
    } else {
        *lx = (int16_t)(DIA_PANEL_H - 1 - py);
        *ly = px;
    }
}

/** ST7796: HW landscape (MADCTL) = rovnaká geometria ako soft-rotate, bez CPU rotate. */
static void applySt7796Rotation() {
    if (!gfx || !panel_st7796) return;
    gfx->setRotation(s_flip ? 3 : 1);
}

static void presentLandToPanel(const uint16_t *src) {
    if (!gfx_ok || !src || !present_fb) return;

    // AXS (a ST7796 fallback): CPU rotate + jeden full SPI/QSPI burst
    if (!s_flip) {
        for (int16_t py = 0; py < DIA_PANEL_H; py++) {
            uint16_t *dst = present_fb + (int32_t)py * DIA_PANEL_W;
            const uint16_t *p = src + (int32_t)(DIA_PANEL_W - 1) * DIA_SCREEN_W + py;
            for (int16_t px = 0; px < DIA_PANEL_W; px++) {
                dst[px] = *p;
                p -= DIA_SCREEN_W;
            }
        }
    } else {
        for (int16_t py = 0; py < DIA_PANEL_H; py++) {
            uint16_t *dst = present_fb + (int32_t)py * DIA_PANEL_W;
            const int16_t lx = (int16_t)(DIA_PANEL_H - 1 - py);
            for (int16_t px = 0; px < DIA_PANEL_W; px++) {
                dst[px] = src[(int32_t)px * DIA_SCREEN_W + lx];
            }
        }
    }

    Arduino_TFT *tft = (Arduino_TFT *)gfx;
    tft->startWrite();
    tft->writeAddrWindow(0, 0, DIA_PANEL_W, DIA_PANEL_H);
    tft->writePixels(present_fb, (uint32_t)DIA_PANEL_W * (uint32_t)DIA_PANEL_H);
    tft->endWrite();
}

static void createPanel() {
    if (panel_st7796) {
        // Waveshare 08_gfx_helloworld 1:1 (default FSPI, shared)
        bus = new Arduino_ESP32SPI(
            LCD_SPI_DC, LCD_SPI_CS, LCD_SPI_SCLK, LCD_SPI_MOSI, LCD_SPI_MISO);
        gfx = new Arduino_ST7796(
            bus, GFX_NOT_DEFINED, 0, true /* IPS */, DIA_PANEL_W, DIA_PANEL_H);
        logFlush("[board] panel=ST7796 SPI");
    } else {
        bus = new Arduino_ESP32QSPI(
            LCD_QSPI_CS, LCD_QSPI_CLK, LCD_QSPI_D0, LCD_QSPI_D1, LCD_QSPI_D2, LCD_QSPI_D3);
        gfx = new Arduino_AXS15231B(
            bus, GFX_NOT_DEFINED, 0, false, DIA_PANEL_W, DIA_PANEL_H);
        logFlush("[board] panel=AXS15231B QSPI");
    }
}

void diaBoardInit() {
    esp_rom_printf("[board] %s init %dx%d psram=%d\n",
                   "WS-3.5x", DIA_PANEL_W, DIA_PANEL_H, (int)psramFound());

    present_fb = (uint16_t *)heap_caps_malloc(
        (size_t)DIA_PANEL_W * (size_t)DIA_PANEL_H * sizeof(uint16_t),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!present_fb) {
        present_fb = (uint16_t *)heap_caps_malloc(
            (size_t)DIA_PANEL_W * (size_t)DIA_PANEL_H * sizeof(uint16_t), MALLOC_CAP_8BIT);
    }

    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, LOW);

    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    initPmu();
    // Po AXP znova Wire – XPowersLib môže prebiť bus
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    lcdResetViaTca();
    i2cScanLog();
    selectPanelByTouch();
    createPanel();

    // ST7796: 80 MHz + HW landscape (partial flush). AXS: 16 MHz + soft-rotate.
    bool ok = panel_st7796 ? gfx->begin(80000000) : gfx->begin(16000000);
    if (!ok) {
        logFlush("[board] panel begin FAILED");
        gfx_ok = false;
    } else {
        gfx_ok = true;
        if (panel_st7796) {
            applySt7796Rotation();
        } else {
            bus->batchOperation(axs15231b_320480_type2_init_operations,
                                sizeof(axs15231b_320480_type2_init_operations));
            bus->sendCommand(0x13);
            delay(10);
            bus->sendCommand(0x29);
            delay(50);
        }
        logFlush("[board] panel begin OK");
    }

    // Touch znova po LCD (SPI init občas „zhodí“ I2C na FT)
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    if (panel_st7796) {
        touch_ok = i2cAck(TOUCH_ADDR_FT);
        touch_addr = TOUCH_ADDR_FT;
        touch_ft = true;
        if (touch_ok) ftTouchInit();
    } else {
        touch_ok = i2cAck(TOUCH_ADDR_AXS);
        touch_addr = TOUCH_ADDR_AXS;
        touch_ft = false;
    }
    esp_rom_printf("[board] touch 0x%02X ok=%d ft=%d name=%s\n",
                   touch_addr, (int)touch_ok, (int)touch_ft, board_name);

    if (tca_ok) {
        tca.pinMode1(7, OUTPUT);
        tca.write1(7, 1);
    }

    if (gfx_ok) {
        gfx->fillScreen(RGB565_BLACK);
        // BL zapne UI cez diaBoardSetBrightness po splash
    }
}

void diaBoardSetBrightness(uint8_t level) { blSet(level); }

void diaBoardFlush(int32_t x1, int32_t y1, int32_t x2, int32_t y2, const uint16_t *pixels) {
    if (!gfx_ok || !pixels || !panel_st7796) return;
    const int16_t w = (int16_t)(x2 - x1 + 1);
    const int16_t h = (int16_t)(y2 - y1 + 1);
    if (w <= 0 || h <= 0) return;
    // writePixels = jeden SPI burst (draw16bitRGBBitmap je často pomalší)
    Arduino_TFT *tft = (Arduino_TFT *)gfx;
    tft->startWrite();
    tft->writeAddrWindow((int16_t)x1, (int16_t)y1, (uint16_t)w, (uint16_t)h);
    tft->writePixels((uint16_t *)pixels, (uint32_t)w * (uint32_t)h);
    tft->endWrite();
}

void diaBoardFlushPresent() {}

void diaBoardPresentLand(const uint16_t *land) {
    if (panel_st7796) return; // ST7796: partial cez diaBoardFlush
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

bool diaBoardTouchOk() { return touch_ok; }
bool diaBoardIsRound() { return false; }
const char *diaBoardName() { return board_name; }
// ST7796: partial HW landscape. AXS: full soft-rotate.
bool diaBoardPreferFullRefresh() { return !panel_st7796; }
bool diaBoardIsLandscape() { return true; }
bool diaBoardHasAudio() { return true; }

// ST7796: CS strap GND → GPIO12 = MCLK. AXS QSPI: CS=12 → MCLK=44.
int diaBoardI2sMclkPin() { return panel_st7796 ? 12 : 44; }

void diaBoardSpeakerAmp(bool enable) {
    if (!tca_ok) return;
    tca.write1(7, enable ? 1 : 0);
}

bool diaBoardSupportsOrient() { return true; }
uint8_t diaBoardGetOrientMode() { return s_orientMode; }
bool diaBoardIsFlipped() { return s_flip; }

void diaBoardSetOrientMode(uint8_t mode) {
    s_orientMode = mode ? 1 : 0;
    s_flip = (s_orientMode == 1);
    applySt7796Rotation();
}

#endif // DIA_BOARD_WS_3_5
