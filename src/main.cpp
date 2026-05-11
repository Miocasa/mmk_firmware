/**
 * HID Keyboard - XIAO nRF52840 or another
 * USB (priority) + BLE simultaneous
 *
 * Uses qmk_engine.h — standalone QMK-compatible layer/keycode engine.
 * No dependency on QMK firmware source.
 */

#include <Arduino.h>
#include "UnifiedHid.h"
#include "qmk_engine.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <keymap.hpp>

#include "hid_instance.h"
#include "PowerManager.h"


#ifdef OLED_SSD1306_ENABLED

#ifdef OLED_SEPARATED_TASK
#include "oled.h"
#endif

#define SCREEN_WIDTH_DEFAULT 128
#define SCREEN_HEIGHT_DEFAULT 64
#define SCREEN_ADDRESS_DEFAULT 0x3C

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH SCREEN_WIDTH_DEFAULT
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT SCREEN_HEIGHT_DEFAULT
#endif
#ifndef SCREEN_ADDRESS
#define SCREEN_ADDRESS SCREEN_ADDRESS_DEFAULT
#endif

#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif

PowerManagerConfig pwrCfg = {
    15000,
    25000,
    40000,
    120000,
};
PowerManager pwr(pwrCfg);

// ─── HID object ───────────────────────────────────────────────────────────────

// ─── QMK engine ───────────────────────────────────────────────────────────────
MAKE_QMK_ENGINE(keymaps, rowPins, colPins, ROW_TO_COL);

// ─── Matrix debounce ──────────────────────────────────────────────────────────

constexpr uint32_t MATRIX_DEB_MS = 10;

bool matrixSettled[MATRIX_ROWS][MATRIX_COLS] = {};
bool matrixRaw[MATRIX_ROWS][MATRIX_COLS] = {};
bool matrixPendingChange[MATRIX_ROWS][MATRIX_COLS] = {};
uint32_t matrixDebTimer[MATRIX_ROWS][MATRIX_COLS] = {};

void debounceMatrix() {
    uint32_t now = millis();

    for (uint8_t r = 0; r < MATRIX_ROWS; ++r) {
        digitalWrite(rowPins[r], LOW);
        delayMicroseconds(5);
        for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
            bool raw = (digitalRead(colPins[c]) == LOW);
            if (raw != matrixRaw[r][c]) {
                matrixRaw[r][c] = raw;
                matrixDebTimer[r][c] = now;
                matrixPendingChange[r][c] = true;
            }
            if (matrixPendingChange[r][c] &&
                (now - matrixDebTimer[r][c]) >= MATRIX_DEB_MS) {
                matrixPendingChange[r][c] = false;

                uint8_t nr, nc;
#if   MATRIX_ROTATION == 90  && MATRIX_ROWS == MATRIX_COLS
                nr = c;
                nc = (MATRIX_ROWS - 1) - r;
#elif MATRIX_ROTATION == 180 && MATRIX_ROWS == MATRIX_COLS
                nr = (MATRIX_ROWS - 1) - r; nc = (MATRIX_COLS - 1) - c;
#elif MATRIX_ROTATION == 270 && MATRIX_ROWS == MATRIX_COLS
                nr = (MATRIX_COLS - 1) - c; nc = r;
#else
                nr = r; nc = c;
#endif

#if defined(MATRIX_INVERT_VERTICAL)
#if (MATRIX_ROTATION == 90 || MATRIX_ROTATION == 270) && MATRIX_ROWS == MATRIX_COLS
                nr = (MATRIX_COLS - 1) - nr;
#else
                nr = (MATRIX_ROWS - 1) - nr;
#endif
#endif

#if defined(MATRIX_INVERT_HORIZONTAL)
#if MATRIX_ROTATION == 90 || MATRIX_ROTATION == 270
                nc = (MATRIX_ROWS - 1) - nc;
#else
                nc = (MATRIX_COLS - 1) - nc;
#endif
#endif

                engine.matrixCurrent[nr][nc] = matrixRaw[r][c];
            }
        }
        digitalWrite(rowPins[r], HIGH);
    }
}

// ─── Encoder ──────────────────────────────────────────────────────────────────
#ifdef ENCODER_ENABLE
constexpr int8_t ENC_TABLE[16] = {
    0, -1, 1, 0,
    1, 0, 0, -1,
    -1, 0, 0, 1,
    0, 1, -1, 0
};
constexpr int8_t ENC_DETENT = 4;

volatile int8_t encDelta = 0;
uint8_t encStatePrev = 0;

void tickEncoder() {
    const uint8_t a = digitalRead(ENCODER_A_PIN);
    const uint8_t b = digitalRead(ENCODER_B_PIN);
    const uint8_t cur = (a << 1) | b;
    encDelta += ENC_TABLE[((encStatePrev << 2) | cur) & 0x0F];
    encStatePrev = cur;
}

uint16_t resolveEncoderKey(uint16_t EncoderMap::*field) {
    for (int8_t l = LAYER_COUNT - 1; l >= 0; --l) {
        if (!(engine.getLayerState() & (1u << l))) continue;
        const uint16_t kc = encoderMaps[l].*field;
        if (kc != KC_TRNS) return kc;
    }
    return KC_NO;
}
#endif

// ─── sendKeyPulse ─────────────────────────────────────────────────────────────
// Sends any keycode as a press+release pulse via UnifiedHid.
// For NKRO keyboard: packs mods + 32-byte bitmap into a raw sendReport call,
// because UnifiedHid::sendKeyboard() uses the legacy 6KRO keyboardReport().

void sendKeyPulse(uint16_t kc) {
    if (kc == KC_NO) return;
    ResolvedKey rk = engine.resolveRaw(kc);

    if (rk.consumer) {
        hid.sendConsumer(rk.consumer);
        delay(12);
        hid.releaseConsumer();
    } else if (rk.hid_keycode || rk.hid_mods) {
        uint8_t buf[33] = {};
        buf[0] = rk.hid_mods;
        if (rk.hid_keycode && rk.hid_keycode < 0xE0u)
            buf[1 + (rk.hid_keycode >> 3)] |= (1u << (rk.hid_keycode & 7u));
        hid.sendNkro(buf[0], buf + 1); // mods + first 6 bytes (best-effort for pulse)
        delay(12);
        hid.releaseKeyboard();
    }
}

// ─── Encoder button debounce ──────────────────────────────────────────────────
#ifdef ENCODER_ENABLE
bool encBtnSettled = HIGH;
bool encBtnRaw = HIGH;
uint32_t encBtnTimer = 0;
constexpr uint32_t BTN_DEB_MS = 20;

void debounceEncBtn() {
    bool raw = digitalRead(ENCODER_BTN_PIN);
    if (raw != encBtnRaw) {
        encBtnRaw = raw;
        encBtnTimer = millis();
    }
    if ((millis() - encBtnTimer) >= BTN_DEB_MS)
        encBtnSettled = encBtnRaw;
}
#endif

// ─── Setup ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    engine.setTapDance(td_entries, TD_COUNT);
    engine.TAP_HOLD_MS = 200;
    engine.TAP_DANCE_TERM = 150;

    Wire.setPins(D10, D9);
    Wire.begin();

#ifdef OLED_SSD1306_ENABLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS))
        Serial.println("SSD1306 allocation failed");
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.setRotation(SCREEN_ROTATION / 90);
    display.display();
#endif


    hid.begin("Miopad");
    pwr.begin(colPins, MATRIX_COLS,
#ifdef ENCODER_ENABLE
              ENCODER_BTN_PIN
#else
              -1
#endif
    );

    // Callback for oled and backlight
    pwr.setCallback([](PowerState prev, PowerState next) {
#ifdef OLED_SSD1306_ENABLED
        if (next >= PWR_IDLE) {
            display.ssd1306_command(SSD1306_DISPLAYOFF);
        } else if (prev >= PWR_LIGHT_SLEEP && next == PWR_ACTIVE) {
            Serial.println("[OLED] Wakeup recovery");

            if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
                Serial.println("SSD1306 re-init failed");
            }

            display.setTextColor(WHITE);
            display.setTextSize(1);
            display.setRotation(SCREEN_ROTATION / 90);
            display.clearDisplay();
            display.display();

            delay(20);
        } else if (prev >= PWR_IDLE && next < PWR_IDLE) {
            display.ssd1306_command(SSD1306_DISPLAYON);
            display.dim(false);
        }
#endif
    });

    for (uint8_t r = 0; r < MATRIX_ROWS; ++r) {
        pinMode(rowPins[r], OUTPUT);
        digitalWrite(rowPins[r], HIGH);
    }
    for (uint8_t c = 0; c < MATRIX_COLS; ++c)
        pinMode(colPins[c], INPUT_PULLUP);

#ifdef ENCODER_ENABLE
    pinMode(ENCODER_A_PIN, INPUT_PULLUP);
    pinMode(ENCODER_B_PIN, INPUT_PULLUP);
    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);
    encStatePrev = (digitalRead(ENCODER_A_PIN) << 1) | digitalRead(ENCODER_B_PIN);
#endif
}

void ReInitUsb() {
    static bool lastVbus = false;
    const bool currentVbus = (NRF_POWER->USBREGSTATUS & POWER_USBREGSTATUS_VBUSDETECT_Msk) != 0;

    if (currentVbus && !lastVbus && !hid.usbReady()) {
        Serial.println("[USB] VBUS detected -> reinit");

        TinyUSB_Device_Init(0);
        tud_init(0);
        delay(10);
        hid.reinitUsb();
        delay(150);
    }

    lastVbus = currentVbus;
}

void loop() {
    // ── 0. HID + Power tick ───────────────────────────────────────────────────
    ReInitUsb();
    pwr.tick();

    using NkroReport = QmkEngine<MATRIX_ROWS, MATRIX_COLS, LAYER_COUNT>::NkroReport;
    static NkroReport prevReport = {};
    static MouseReport prevMouse = {};
    static uint16_t prevConsumer = 0;

    // После переподключения или выхода из сна — форс-ресенд
    if (hid.consumeForceResend() || pwr.consumeWakeup()) {
        memset(&prevReport, 0xFF, sizeof(prevReport));
        memset(&prevMouse, 0xFF, sizeof(prevMouse));
        prevConsumer = 0xFFFF;
    }

    // ── 1. Serial команды ─────────────────────────────────────────────────────
    const int ch = tolower(Serial.read());
    if (ch == 'r') { enterUf2Dfu(); }
    if (ch == 'b') {
        hid.setMode(BLE_MODE);
        Serial.println("BLE_MODE");
    }
    if (ch == 'a') {
        hid.setMode(AUTO_MODE);
        Serial.println("AUTO_MODE");
    }
    if (ch == 'u') {
        hid.setMode(ONLY_USB_MODE);
        Serial.println("ONLY_USB_MODE");
    }
    if (ch == 'o') {
        hid.setMode(ONLY_BLE_MODE);
        Serial.println("ONLY_BLE_MODE");
    }
    if (ch >= 0) { pwr.reportActivity(); } // любой serial символ = активность

    // ── 2. Throttle matrix scan по power state ────────────────────────────────
    static uint32_t lastScan = 0;
    const uint32_t scanInterval = pwr.scanIntervalMs();
    const uint32_t now = millis();
    const bool doScan = (scanInterval <= 1) || ((now - lastScan) >= scanInterval);

    if (doScan) {
        lastScan = now;
        debounceMatrix();
    }

    // ── 3. Encoder ────────────────────────────────────────────────────────────
#ifdef ENCODER_ENABLE
    tickEncoder();

    if (encDelta >= ENC_DETENT) {
        encDelta -= ENC_DETENT;
        pwr.reportActivity();
        if (hid.ready()) {
            sendKeyPulse(resolveEncoderKey(&EncoderMap::cw));
            Serial.print("Encoder CW  layer=");
            Serial.println(engine.highestActiveLayer());
        }
    }
    if (encDelta <= -ENC_DETENT) {
        encDelta += ENC_DETENT;
        pwr.reportActivity();
        if (hid.ready()) {
            sendKeyPulse(resolveEncoderKey(&EncoderMap::ccw));
            Serial.print("Encoder CCW layer=");
            Serial.println(engine.highestActiveLayer());
        }
    }

    // ── 4. Encoder button debounce ────────────────────────────────────────────
    debounceEncBtn();
#endif

    // ── 5. OLED ───────────────────────────────────────────────────────────────
#ifdef OLED_SSD1306_ENABLED
    if (!pwr.isAsleep()) {
        auto input = engine.getInputActivity(encDelta, ENC_DETENT, encBtnSettled);
        oled_task_kb(engine.highestActiveLayer(), input, &display);
    }
#endif

    // ── 6. Активность от матрицы ──────────────────────────────────────────────
    // Проверяем изменения в матрице — если что-то нажато, будим power manager
    if (doScan) {
        bool anyPressed = false;
        for (uint8_t r = 0; r < MATRIX_ROWS && !anyPressed; ++r)
            for (uint8_t c = 0; c < MATRIX_COLS && !anyPressed; ++c)
                if (engine.matrixCurrent[r][c]) anyPressed = true;
        if (anyPressed) pwr.reportActivity();
    }

    // ── 7. Ранний выход если HID не готов ─────────────────────────────────────
    if (!hid.ready()) return;

    // ── 8. Encoder button — edge-triggered ───────────────────────────────────
#ifdef ENCODER_ENABLE
    static bool encBtnPrev = HIGH;
    if (encBtnSettled != encBtnPrev) {
        encBtnPrev = encBtnSettled;
        pwr.reportActivity();

        const uint16_t kc = resolveEncoderKey(&EncoderMap::btn);
        const ResolvedKey rk = engine.resolveRaw(kc);

        if (encBtnSettled == LOW) {
            if (rk.consumer) {
                hid.sendConsumer(rk.consumer);
            } else if (rk.hid_keycode || rk.hid_mods) {
                uint8_t buf[33] = {};
                buf[0] = rk.hid_mods;
                if (rk.hid_keycode < 0xE0u)
                    buf[1 + (rk.hid_keycode >> 3)] |= (1u << (rk.hid_keycode & 7u));
                hid.sendNkro(buf[0], buf + 1);
            }
        } else {
            hid.releaseConsumer();
            hid.releaseKeyboard();
        }
    }
#endif

    // ── 9. Build + send HID reports ───────────────────────────────────────────
    NkroReport report = {};
    MouseReport mouse = {};
    uint16_t consumer = 0;
    engine.buildReport(report, mouse, consumer);

    // Keyboard
    if (report != prevReport) {
        prevReport = report;
        hid.sendNkro(report.mods, report.bitmap);
        // Любое изменение отчёта = активность
        if (report.mods || memchr(report.bitmap, 0xFF, 32))
            pwr.reportActivity();
    }

    // Mouse
    if (mouse != prevMouse) {
        prevMouse = mouse;
        hid.sendMouse(mouse.buttons, mouse.x, mouse.y, mouse.v, mouse.h);
        pwr.reportActivity();
    }

    // Consumer
    if (consumer != prevConsumer) {
        prevConsumer = consumer;
        if (consumer) {
            hid.sendConsumer(consumer);
            pwr.reportActivity();
        } else {
            hid.releaseConsumer();
        }
    }
}
