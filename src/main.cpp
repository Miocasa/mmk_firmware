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


// ─── HID descriptor ───────────────────────────────────────────────────────────
//
// Report layout:
//   ID 1 — NKRO keyboard  : mods (1 byte) + keycode bitmap (32 bytes)
//   ID 2 — System control : 2-bit power + 6-bit padding
//   ID 3 — Consumer       : 16-bit usage ID
//   ID 4 — Mouse          : buttons (1 byte) + x,y,v,h (4 × int8)

#define TUD_HID_REPORT_DESC_NKRO_KEYBOARD(...) \
  HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                    ,\
  HID_USAGE      ( HID_USAGE_DESKTOP_KEYBOARD )                    ,\
  HID_COLLECTION ( HID_COLLECTION_APPLICATION )                    ,\
    __VA_ARGS__                                                      \
    HID_USAGE_PAGE   ( HID_USAGE_PAGE_KEYBOARD )                   ,\
    HID_USAGE_MIN    ( 224                     )                   ,\
    HID_USAGE_MAX    ( 231                     )                   ,\
    HID_LOGICAL_MIN  ( 0                       )                   ,\
    HID_LOGICAL_MAX  ( 1                       )                   ,\
    HID_REPORT_COUNT ( 8                       )                   ,\
    HID_REPORT_SIZE  ( 1                       )                   ,\
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE )    ,\
    HID_USAGE_PAGE     ( HID_USAGE_PAGE_KEYBOARD )                 ,\
    HID_USAGE_MIN      ( 0                       )                 ,\
    HID_USAGE_MAX_N    ( 255, 2                  )                 ,\
    HID_LOGICAL_MIN    ( 0                       )                 ,\
    HID_LOGICAL_MAX    ( 1                       )                 ,\
    HID_REPORT_COUNT_N ( 256, 2                  )                 ,\
    HID_REPORT_SIZE    ( 1                       )                 ,\
    HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE )  ,\
  HID_COLLECTION_END

static const uint8_t desc_hid_report[] = {
    TUD_HID_REPORT_DESC_NKRO_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_SYSTEM_CONTROL(HID_REPORT_ID(2)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(3)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(4)),
};

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
    hid.setMode(AUTO_MODE);

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

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {
    // ——— Reinit tiny usb
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


    int ch = Serial.read();
    ch = tolower(ch);
    if (ch == 'r') {
        Serial.println("reboot to bootloader");
        enterUf2Dfu();
    }
    if (ch == 'b') {
        Serial.println("Ble mode set");
        hid.setMode(BLE_MODE);
    }
    if (ch == 'a' || ch == 'u') {
        Serial.println("Auto mode set");
        hid.setMode(AUTO_MODE);
    }
    // if (ch == 'f') {
    //     Serial.println("Flash format");
    //     hid.setMode(AUTO_MODE);
    // }

#ifdef ENCODER_ENABLE
    // 1. Encoder tick
    tickEncoder();

    // 2. Encoder rotation
    if (encDelta >= ENC_DETENT) {
        encDelta -= ENC_DETENT;
        sendKeyPulse(resolveEncoderKey(&EncoderMap::cw));
        Serial.print("Encoder CW  layer=");
        Serial.println(engine.highestActiveLayer());
    }
    if (encDelta <= -ENC_DETENT) {
        encDelta += ENC_DETENT;
        sendKeyPulse(resolveEncoderKey(&EncoderMap::ccw));
        Serial.print("Encoder CCW layer=");
        Serial.println(engine.highestActiveLayer());
    }
#endif

    // 3. Matrix debounce
    debounceMatrix();

    // 4. Encoder button debounce
#ifdef ENCODER_ENABLE
    debounceEncBtn();
#endif

    // Read input
    const auto input = engine.getInputActivity(encDelta, ENC_DETENT, encBtnSettled);


    // Oled task
#ifdef OLED_SSD1306_ENABLED
    oled_task_kb(engine.highestActiveLayer(), input, &display);
#endif

    if (!hid.ready()) { return; }

    // 5. Encoder button — edge-triggered
#ifdef ENCODER_ENABLE
    static bool encBtnPrev = HIGH;
    if (encBtnSettled != encBtnPrev) {
        encBtnPrev = encBtnSettled;
        uint16_t kc = resolveEncoderKey(&EncoderMap::btn);
        ResolvedKey rk = engine.resolveRaw(kc);
        if (encBtnSettled == LOW) {
            if (rk.consumer)
                hid.sendConsumer(rk.consumer);
            else if (rk.hid_keycode || rk.hid_mods) {
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

    // 6. Build NKRO + mouse reports via QMK engine
    using NkroReport = QmkEngine<MATRIX_ROWS, MATRIX_COLS, LAYER_COUNT>::NkroReport;

    NkroReport report = {};
    MouseReport mouse = {};
    uint16_t consumer = 0;

    engine.buildReport(report, mouse, consumer);

    // 7. NKRO keyboard report — only on change
    static NkroReport prevReport = {};
    if (report != prevReport) {
        prevReport = report;
        hid.sendNkro(report.mods, report.bitmap); // ← было sendKeyboard
    }

    // 8. Mouse report
    static MouseReport prevMouse = {};
    if (mouse != prevMouse) {
        prevMouse = mouse;
        hid.sendMouse(mouse.buttons, mouse.x, mouse.y, mouse.v, mouse.h); // ← теперь работает
    }

    // 9. Consumer report
    static uint16_t prevConsumer = 0;
    if (consumer != prevConsumer) {
        prevConsumer = consumer;
        if (consumer) hid.sendConsumer(consumer);
        else hid.releaseConsumer();
    }
}
