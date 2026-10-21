#include "dia_audio.h"
#include "dia_board.h"
#include "dia_config.h"
#include "dia_glucose.h"
#include "dia_types.h"

#include <Arduino.h>
#include <math.h>
#include <string.h>

static uint32_t s_snoozeUntil = 0;
static uint32_t s_lastChimeMs = 0;
static bool s_wasCritical = false;
static constexpr uint32_t kChimeIntervalMs = 5UL * 60UL * 1000UL;

/** Staré dáta / WiFi výpadok = strata spojenia (alarm rovnaký ako critical). */
static bool slotNeedsConnAlert(const DiaSlotReading &s) {
    if (!s.has_valid_reading || s.mmol <= 0.0f) return false;
    if (s.data_is_stale) return true;
    return s.err == FetchError::Wifi;
}

bool diaAudioAnyCritical() {
#if defined(DIA_BOARD_WS_3_5)
    if (display_mode >= 2) {
        for (int i = 0; i < DIA_PROFILE_COUNT; i++) {
            if (!diaProfileActive(i)) continue;
            const DiaSlotReading &s = slotReadings[i];
            if (s.has_valid_reading && s.mmol > 0.0f && s.is_critical) return true;
            if (slotNeedsConnAlert(s)) return true;
        }
        return false;
    }
#endif
    if (is_critical && has_valid_reading && last_mmol_value > 0.0) return true;
    if (has_valid_reading && last_mmol_value > 0.0 &&
        (data_is_stale || last_fetch_error == FetchError::Wifi)) {
        return true;
    }
    return false;
}

bool diaAudioIsSnoozed(uint32_t nowMs) {
    return s_snoozeUntil != 0 && (int32_t)(s_snoozeUntil - nowMs) > 0;
}

int diaAudioSnoozeRemainMin(uint32_t nowMs) {
    if (!diaAudioIsSnoozed(nowMs)) return 0;
    uint32_t left = s_snoozeUntil - nowMs;
    return (int)((left + 59999UL) / 60000UL);
}

void diaAudioClearSnooze() {
    s_snoozeUntil = 0;
}

void diaAudioResetChimeTimer() {
    s_lastChimeMs = 0;
}

#if defined(DIA_BOARD_WS_3_5)

#include <Wire.h>
#include <driver/gpio.h>
#include <driver/i2s.h>
#include <driver/uart.h>
#include "soc/gpio_sig_map.h"
#include "soc/i2s_struct.h"
#include "es8311.h"
#include "esp_rom_sys.h"
#include "hal/gpio_ll.h"
#include "audio_startup_wav.h"
#include "audio_beep_wav.h"

// Waveshare 3.5B: IO16=DSDIN, IO14=ASDOUT, IO13=SCLK, IO15=LRCK, IO44=MCLK, EXIO7=PA
static constexpr int kSampleRate = 16000;
static constexpr int kMclkHz = kSampleRate * 256;
static constexpr i2s_port_t kI2sPort = I2S_NUM_0;

static constexpr int kPinMclk = 44;
static constexpr int kPinBclk = 13;
static constexpr int kPinWs = 15;
static constexpr int kPinDout = 16;
static constexpr int kPinDin = 14;

static bool s_i2sReady = false;
static bool s_codecOk = false;
static es8311_handle_t s_es = nullptr;

#define ALOG(fmt, ...) do { esp_rom_printf("[audio] " fmt "\n", ##__VA_ARGS__); } while (0)

static void driveI2sPin(int pin, int sig) {
    gpio_matrix_out(pin, sig, false, false);
    GPIO.func_out_sel_cfg[pin].oen_sel = 1;
    gpio_ll_output_enable(&GPIO, (gpio_num_t)pin);
}

static int probePinFlips(int pin, int loops) {
    int flips = 0;
    int last = gpio_get_level((gpio_num_t)pin);
    for (int i = 0; i < loops; i++) {
        int v = gpio_get_level((gpio_num_t)pin);
        if (v != last) {
            flips++;
            last = v;
        }
    }
    return flips;
}

static void enableTxSlots() {
    // S3 Arduino 2.0: TDM default + channels off → DOUT=0
    I2S0.tx_tdm_ctrl.tx_tdm_chan0_en = 1;
    I2S0.tx_tdm_ctrl.tx_tdm_chan1_en = 1;
    I2S0.tx_tdm_ctrl.tx_tdm_tot_chan_num = 1;
    I2S0.rx_tdm_ctrl.rx_tdm_chan0_en = 1;
    I2S0.rx_tdm_ctrl.rx_tdm_chan1_en = 1;
    I2S0.rx_tdm_ctrl.rx_tdm_tot_chan_num = 1;
    I2S0.tx_conf.tx_tdm_en = 1;
    I2S0.rx_conf.rx_tdm_en = 1;
    I2S0.tx_conf.tx_pcm_bypass = 1;
    I2S0.tx_conf.tx_mono = 0;
    I2S0.tx_conf.tx_chan_mod = 0;
    // SDP chip drží 18-bit (0x08) → 32-bit stereo slot (BCLK=1.024MHz = coeff)
    I2S0.tx_conf1.tx_tdm_ws_width = 31;
    I2S0.rx_conf1.rx_tdm_ws_width = 31;
    I2S0.tx_conf1.tx_bits_mod = 31;
    I2S0.rx_conf1.rx_bits_mod = 31;
    I2S0.tx_conf.tx_update = 1;
    I2S0.rx_conf.rx_update = 1;
    I2S0.tx_conf.tx_start = 1;
}

static uint8_t esRead(uint8_t reg) {
    Wire.beginTransmission(0x18);
    Wire.write(reg);
    if (Wire.endTransmission(true) != 0) return 0xEE;
    if (Wire.requestFrom((uint8_t)0x18, (uint8_t)1) != 1) return 0xEF;
    return (uint8_t)Wire.read();
}

static bool esWrite(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(0x18);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission(true) == 0;
}

static void dumpEs() {
    ALOG("00=%02X 01=%02X 03=%02X 04=%02X 09=%02X 12=%02X 13=%02X 31=%02X 32=%02X",
         esRead(0x00), esRead(0x01), esRead(0x03), esRead(0x04), esRead(0x09),
         esRead(0x12), esRead(0x13), esRead(0x31), esRead(0x32));
}

// largeDma: startup WAV (~64 kB DMA). Po WiFi/fetch I2S odinštalujeme – inak
// esp-sha/mbedTLS nemá interný DRAM (Libre TLS padá).
static bool i2sInit(bool largeDma) {
    if (s_i2sReady) return true;

    uart_set_pin(UART_NUM_0, 43, 46, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    gpio_reset_pin((gpio_num_t)kPinMclk);
    gpio_reset_pin((gpio_num_t)kPinBclk);
    gpio_reset_pin((gpio_num_t)kPinWs);
    gpio_reset_pin((gpio_num_t)kPinDout);
    gpio_reset_pin((gpio_num_t)kPinDin);

    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX);
    cfg.sample_rate = kSampleRate;
    // 32-bit sloty – zodpovedá SDP=18-bit + bclk_div=4
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
    cfg.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;
    cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    // 16×512×8 ≈ 64 kB interného DMA; chime stačí ~6×256×8 ≈ 12 kB
    cfg.dma_buf_count = largeDma ? 16 : 6;
    cfg.dma_buf_len = largeDma ? 512 : 256;
    // PLL je stabilnejší než APLL na S3 Arduino 2.0
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;
    cfg.fixed_mclk = 0;

    esp_err_t err = i2s_driver_install(kI2sPort, &cfg, 0, nullptr);
    if (err != ESP_OK) {
        ALOG("i2s install fail=%d", (int)err);
        return false;
    }

    i2s_pin_config_t pins = {};
    pins.mck_io_num = kPinMclk;
    pins.bck_io_num = kPinBclk;
    pins.ws_io_num = kPinWs;
    pins.data_out_num = kPinDout;
    pins.data_in_num = kPinDin;
    err = i2s_set_pin(kI2sPort, &pins);
    if (err != ESP_OK) {
        ALOG("i2s_set_pin fail=%d", (int)err);
        i2s_driver_uninstall(kI2sPort);
        return false;
    }

    i2s_set_clk(kI2sPort, kSampleRate, I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_STEREO);
    // Waveshare STD: bit_shift + left_align
    I2S0.tx_conf1.tx_msb_shift = 1;
    I2S0.tx_conf.tx_left_align = 1;
    I2S0.tx_conf.tx_bit_order = 0;
    I2S0.rx_conf1.rx_msb_shift = 1;
    I2S0.rx_conf.rx_left_align = 1;
    enableTxSlots();
    i2s_start(kI2sPort);
    enableTxSlots();

    I2S0.tx_clkm_conf.clk_en = 1;
    I2S0.tx_clkm_conf.tx_clk_active = 1;
    I2S0.rx_clkm_conf.rx_clk_active = 1;

    gpio_set_direction((gpio_num_t)kPinBclk, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction((gpio_num_t)kPinWs, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction((gpio_num_t)kPinDout, GPIO_MODE_INPUT_OUTPUT);
    driveI2sPin(kPinBclk, I2S0O_BCK_OUT_IDX);
    driveI2sPin(kPinWs, I2S0O_WS_OUT_IDX);
    driveI2sPin(kPinDout, I2S0O_SD_OUT_IDX);

    gpio_set_direction((gpio_num_t)kPinMclk, GPIO_MODE_INPUT_OUTPUT);
    auto probeSel = [](int sel) -> int {
        I2S0.rx_clkm_conf.mclk_sel = sel;
        gpio_matrix_out(kPinMclk, I2S0_MCLK_OUT_IDX, false, false);
        GPIO.func_out_sel_cfg[kPinMclk].oen_sel = 1;
        gpio_ll_output_enable(&GPIO, (gpio_num_t)kPinMclk);
        return probePinFlips(kPinMclk, 40000);
    };
    int flipsTx = probeSel(0);
    int flipsRx = probeSel(1);
    I2S0.rx_clkm_conf.mclk_sel = (flipsTx > 1000) ? 0 : 1;
    driveI2sPin(kPinMclk, I2S0_MCLK_OUT_IDX);
    ALOG("MCLK tx=%d rx=%d bclk=%d ws=%d", flipsTx, flipsRx,
         probePinFlips(kPinBclk, 20000), probePinFlips(kPinWs, 20000));

    int32_t z[32] = {};
    size_t written = 0;
    i2s_write(kI2sPort, z, sizeof(z), &written, pdMS_TO_TICKS(50));

    s_i2sReady = true;
    ALOG("I2S OK 32-bit %d Hz", kSampleRate);
    return true;
}

/**
 * Reset podľa datasheetu: 1F → 00 → 80.
 * NIKDY nenechávať REG00=0xFF (reset bity ostanú aktívne → ticho).
 * OSR 0x10 chip maže; 0x1F ostane.
 */
static bool es8311InitWithClocks() {
    s_es = es8311_create(I2C_NUM_0, ES8311_ADDRESS_0);
    if (!s_es) {
        ALOG("es8311_create FAIL");
        return false;
    }

    // Manuálny power-on PRED es8311_init – knižnica potom doplní clock/fmt
    esWrite(0x00, 0x1F);
    delay(20);
    esWrite(0x00, 0x00);
    delay(5);
    esWrite(0x00, 0x80);
    delay(20);

    es8311_clock_config_t clk = {};
    clk.mclk_inverted = false;
    clk.sclk_inverted = false;
    clk.mclk_from_mclk_pin = true;
    clk.mclk_frequency = kMclkHz;
    clk.sample_frequency = kSampleRate;

    esp_err_t err = es8311_init(s_es, &clk, ES8311_RESOLUTION_18, ES8311_RESOLUTION_18);
    if (err != ESP_OK) {
        ALOG("es8311_init err=%d", (int)err);
        return false;
    }

    // Po knižnici: CSM znova (init môže REG00 pokaziť) + OSR ktoré chip prijme
    esWrite(0x00, 0x80);
    delay(5);
    esWrite(0x01, 0x3F);
    es8311_sample_frequency_config(s_es, kMclkHz, kSampleRate);
    esWrite(0x03, 0x1F);
    esWrite(0x04, 0x1F);
    esWrite(0x0D, 0x01);
    esWrite(0x0E, 0x02);
    esWrite(0x12, 0x00); // DAC on
    esWrite(0x13, 0x00); // line-out → NS4150
    esWrite(0x37, 0x08);
    delay(30);

    int volSet = 0;
    es8311_voice_volume_set(s_es, 100, &volSet);
    // Zostáva mute – inak pri I2S init unikne „stotinka“ pred WAV
    es8311_voice_mute(s_es, true);

    dumpEs();
    ALOG("ES8311 OK vol=%d (muted)", volSet);
    return true;
}

static void releaseI2sRfPins() {
    // Odpojiť hodiny z GPIO – MCLK@44 inak ruší WiFi na 3.5B
    gpio_matrix_out(kPinMclk, 0x100, false, false);
    gpio_matrix_out(kPinBclk, 0x100, false, false);
    gpio_matrix_out(kPinWs, 0x100, false, false);
    gpio_matrix_out(kPinDout, 0x100, false, false);
    gpio_set_direction((gpio_num_t)kPinMclk, GPIO_MODE_INPUT);
    gpio_set_direction((gpio_num_t)kPinBclk, GPIO_MODE_INPUT);
    gpio_set_pull_mode((gpio_num_t)kPinMclk, GPIO_FLOATING);
    gpio_set_pull_mode((gpio_num_t)kPinBclk, GPIO_FLOATING);
}

static int codecVolume() {
    static const int kSteps[4] = {40, 62, 86, 100};
    int v = sound_volume;
    if (v < 0) v = 0;
    if (v > 3) v = 3;
    return kSteps[v];
}

static void prepareCodecPlay(int volume) {
    if (s_i2sReady) {
        I2S0.tx_clkm_conf.clk_en = 1;
        I2S0.tx_clkm_conf.tx_clk_active = 1;
        I2S0.rx_clkm_conf.rx_clk_active = 1;
        i2s_start(kI2sPort);
    }
    enableTxSlots();
    gpio_set_direction((gpio_num_t)kPinDout, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction((gpio_num_t)kPinBclk, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction((gpio_num_t)kPinWs, GPIO_MODE_INPUT_OUTPUT);
    gpio_set_direction((gpio_num_t)kPinMclk, GPIO_MODE_INPUT_OUTPUT);
    driveI2sPin(kPinBclk, I2S0O_BCK_OUT_IDX);
    driveI2sPin(kPinWs, I2S0O_WS_OUT_IDX);
    driveI2sPin(kPinDout, I2S0O_SD_OUT_IDX);
    driveI2sPin(kPinMclk, I2S0_MCLK_OUT_IDX);
    if (s_es) {
        es8311_voice_mute(s_es, true);
        // Po I2S re-inite (quiesce) znova sync sample rate – inak znie „vysoko“
        es8311_sample_frequency_config(s_es, kMclkHz, kSampleRate);
        esWrite(0x00, 0x80);
        esWrite(0x01, 0x3F);
        esWrite(0x03, 0x1F);
        esWrite(0x04, 0x1F);
        esWrite(0x12, 0x00);
        esWrite(0x13, 0x00);
        es8311_voice_volume_set(s_es, volume, nullptr);
    }
    // PA zapnúť až keď je mute + DMA vynulované (žiadny blip pred WAV)
    diaBoardSpeakerAmp(true);
    {
        int32_t z[256] = {};
        size_t b = 0;
        i2s_zero_dma_buffer(kI2sPort);
        i2s_write(kI2sPort, z, sizeof(z), &b, pdMS_TO_TICKS(80));
    }
    delay(15);
    if (s_es) {
        esWrite(0x31, 0x00);
        es8311_voice_mute(s_es, false);
    }
}

enum class WavPlayStyle : uint8_t { Startup, Alarm };

/**
 * Mono 16-bit PCM @ 16 kHz → stereo 32-bit I2S.
 * Startup: HP+LP+slew (proti chrčaniu pri dlhšom WAV).
 * Alarm: verný prehrávanie (~300 Hz beep) – len fade + mierny LP, BEZ slew/HP
 *         (slew robí ostré HF „plechové“ pípnutie namiesto pôvodného tónu).
 */
static void playPcm16Mono(const int16_t *pcm, int sampleCount, float gain, int volume,
                          WavPlayStyle style) {
    if (!s_i2sReady || !pcm || sampleCount <= 0) return;
    prepareCodecPlay(volume);

    int32_t buf[512];
    const bool startup = (style == WavPlayStyle::Startup);
    const int fadeN = (kSampleRate * (startup ? 50 : 25)) / 1000;
    const float hpA = 0.90f;
    const float lpA = startup ? 0.72f : 0.82f; // alarm: skoro priepustný, len zjemní hrany
    const float maxDelta = 0.12f;
    const float clip = startup ? 0.70f : 0.62f;
    float hpPrevX = 0.0f, hpPrevY = 0.0f;
    float lpY = 0.0f;
    float prevOut = 0.0f;
    int written = 0;
    int chunks = 0;
    while (written < sampleCount) {
        int chunk = sampleCount - written;
        if (chunk > 256) chunk = 256;
        for (int i = 0; i < chunk; i++) {
            const int idx = written + i;
            float env = 1.0f;
            if (idx < fadeN) {
                float t = (float)idx / (float)fadeN;
                env = 0.5f - 0.5f * cosf((float)M_PI * t);
            } else if (idx >= sampleCount - fadeN) {
                float t = (float)(sampleCount - idx) / (float)fadeN;
                env = 0.5f - 0.5f * cosf((float)M_PI * t);
            }
            float x = (float)pcm[idx] / 32768.0f;
            float y = x;
            if (startup) {
                float hp = hpA * (hpPrevY + x - hpPrevX);
                hpPrevX = x;
                hpPrevY = hp;
                y = hp;
            }
            lpY += lpA * (y - lpY);
            float s = lpY * gain * env;
            if (s > clip) s = clip;
            else if (s < -clip) s = -clip;
            if (startup) {
                float d = s - prevOut;
                if (d > maxDelta) s = prevOut + maxDelta;
                else if (d < -maxDelta) s = prevOut - maxDelta;
                prevOut = s;
            }
            int32_t v = ((int32_t)(s * 131071.0f)) << 14;
            buf[i * 2] = v;
            buf[i * 2 + 1] = v;
        }
        size_t bytes = 0;
        esp_err_t e = i2s_write(kI2sPort, buf, (size_t)chunk * 8, &bytes, pdMS_TO_TICKS(500));
        if (e != ESP_OK || bytes == 0) {
            ALOG("wav i2s_write err=%d bytes=%u", (int)e, (unsigned)bytes);
            return;
        }
        written += chunk;
        if ((++chunks & 7) == 0) yield();
    }
}

static void playStartupWav() {
    ALOG("startup.wav %d samples", kStartupWavSampleCount);
    playPcm16Mono(kStartupWavPcm, kStartupWavSampleCount, 0.48f, codecVolume(), WavPlayStyle::Startup);
    if (s_es) {
        es8311_voice_mute(s_es, true);
        es8311_voice_volume_set(s_es, 100, nullptr);
    }
    diaBoardSpeakerAmp(false);
}

/** Alarm beep (high/low + strata spojenia). */
static void playBeepWav() {
    ALOG("beep.wav %d samples", kBeepWavSampleCount);
    // User remaster ~1 kHz – verné prehrávanie (súbor už normalizovaný v headri)
    playPcm16Mono(kBeepWavPcm, kBeepWavSampleCount, 0.55f, codecVolume(), WavPlayStyle::Alarm);
    if (s_es) {
        es8311_voice_mute(s_es, true);
        es8311_voice_volume_set(s_es, 100, nullptr);
    }
    diaBoardSpeakerAmp(false);
}

static void playTone(float freqHz, uint32_t durMs, float amp) {
    if (!s_i2sReady) return;
    prepareCodecPlay(codecVolume());

    const int n = (int)((kSampleRate * durMs) / 1000);
    if (n <= 0) return;
    // kratší fade – dlhý release pri krátkych tónoch = skoro ticho
    int attackN = (int)((kSampleRate * 12) / 1000);
    int releaseN = (int)((kSampleRate * 40) / 1000);
    if (attackN + releaseN > n) {
        attackN = n / 5;
        releaseN = n / 3;
    }
    const float twoPiF = 2.0f * (float)M_PI * freqHz / (float)kSampleRate;
    int32_t buf[256];
    int written = 0;
    while (written < n) {
        int chunk = n - written;
        if (chunk > 128) chunk = 128;
        for (int i = 0; i < chunk; i++) {
            const int idx = written + i;
            float env = 1.0f;
            if (freqHz <= 0.0f) {
                env = 0.0f;
            } else if (idx < attackN) {
                env = (float)idx / (float)(attackN > 0 ? attackN : 1);
            } else if (idx >= n - releaseN) {
                int left = n - idx;
                env = (float)left / (float)(releaseN > 0 ? releaseN : 1);
            }
            float s = (freqHz > 0.0f) ? (sinf(twoPiF * (float)idx) * amp * env) : 0.0f;
            // rovnaký formát ako keď to pípalo (18-bit << 14)
            int32_t v = ((int32_t)(s * 131071.0f)) << 14;
            buf[i * 2] = v;
            buf[i * 2 + 1] = v;
        }
        size_t bytes = 0;
        esp_err_t e = i2s_write(kI2sPort, buf, (size_t)chunk * 8, &bytes, pdMS_TO_TICKS(300));
        if (e != ESP_OK || bytes == 0) {
            ALOG("i2s_write err=%d bytes=%u", (int)e, (unsigned)bytes);
            return;
        }
        written += chunk;
        yield();
    }
}

void diaAudioInit() {
    if (!diaBoardHasAudio()) return;
    if (s_codecOk && s_i2sReady) return;
    ALOG("init…");

    Wire.beginTransmission(0x18);
    if (Wire.endTransmission() != 0) {
        ALOG("ES8311 NACK @0x18");
        return;
    }

    // PA off počas init – inak počuť krátky blip ešte pred WAV
    diaBoardSpeakerAmp(false);

    if (!i2sInit(true)) return;
    if (!es8311InitWithClocks()) return;
    s_codecOk = true;

    playStartupWav();
    i2s_zero_dma_buffer(kI2sPort);
    dumpEs();
    ALOG("ready");
}

static void i2sUninstallFreeDma() {
    if (!s_i2sReady) return;
    if (s_es) es8311_voice_mute(s_es, true);
    diaBoardSpeakerAmp(false);
    i2s_zero_dma_buffer(kI2sPort);
    i2s_stop(kI2sPort);
    I2S0.tx_clkm_conf.tx_clk_active = 0;
    I2S0.rx_clkm_conf.rx_clk_active = 0;
    releaseI2sRfPins();
    i2s_driver_uninstall(kI2sPort);
    s_i2sReady = false;
    ALOG("I2S uninstall – DMA free for TLS");
}

void diaAudioQuiesceForWifi() {
#if defined(DIA_BOARD_WS_3_5)
    i2sUninstallFreeDma();
#endif
}

void diaAudioPlayChime() {
    if (!s_codecOk) return;
    // Rovnaké veľké DMA ako startup – dlhý WAV + light DMA = ticho / underrun
    if (!s_i2sReady && !i2sInit(true)) {
        ALOG("chime: i2sInit FAIL");
        return;
    }
    ALOG("chime=beep.wav");
    delay(30); // MCLK settle po re-inite (po WiFi quiesce)
    playBeepWav();
    i2s_zero_dma_buffer(kI2sPort);
    i2sUninstallFreeDma();
}

void diaAudioPlayAck() {
    if (!s_codecOk) return;
    if (!s_i2sReady && !i2sInit(false)) return;
    delay(20);
    diaBoardSpeakerAmp(true);
    if (s_es) es8311_voice_mute(s_es, false);
    playTone(587.33f, 140, 0.70f);
    i2s_zero_dma_buffer(kI2sPort);
    if (s_es) es8311_voice_mute(s_es, true);
    diaBoardSpeakerAmp(false);
    i2sUninstallFreeDma();
}

#else

void diaAudioInit() {}
void diaAudioQuiesceForWifi() {}
void diaAudioPlayChime() {}
void diaAudioPlayAck() {}

#endif // DIA_BOARD_WS_3_5

void diaAudioSnooze() {
    if (!sound_on) return;
    uint32_t mins = (uint32_t)snooze_min;
    if (mins != 15 && mins != 30 && mins != 60) mins = 30;
    s_snoozeUntil = millis() + mins * 60000UL;
    diaAudioPlayAck();
    Serial.printf("[audio] snooze %lu min\n", (unsigned long)mins);
}

void diaAudioTick(uint32_t nowMs) {
    if (!sound_on || !diaBoardHasAudio()) return;

    const bool crit = diaAudioAnyCritical();
    if (!crit) {
        if (s_wasCritical) diaAudioClearSnooze();
        s_wasCritical = false;
        return;
    }
    s_wasCritical = true;

    if (diaAudioIsSnoozed(nowMs)) return;
    if (s_lastChimeMs != 0 && (nowMs - s_lastChimeMs) < kChimeIntervalMs) return;
    s_lastChimeMs = nowMs;
    diaAudioPlayChime();
}
