#include "dia_board.h"

#if defined(DIA_BOARD_WS_1_28)

#include <Arduino_GFX_Library.h>
#include <CST816S.h>

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

#endif // DIA_BOARD_WS_1_28
