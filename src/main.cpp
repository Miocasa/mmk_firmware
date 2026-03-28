/**
 * HID Keyboard – XIAO nRF52840
 * 3×3 matrix + rotary encoder
 * USB (priority) + BLE simultaneous
 *
 * Uses qmk_engine.h — standalone QMK-compatible layer/keycode engine.
 * No dependency on QMK firmware source.
 */

#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include "qmk_engine.h"
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ─── Pinout ───────────────────────────────────────────────────────────────────

constexpr uint8_t rowPins[] = { D2, D1, D0 };
constexpr uint8_t colPins[] = { D6, D5, D4 };

constexpr uint8_t MATRIX_ROWS = sizeof(rowPins) / sizeof(rowPins[0]);
constexpr uint8_t MATRIX_COLS = sizeof(colPins) / sizeof(colPins[0]);

#define ENCODER_BTN_PIN D3
#define ENCODER_A_PIN   D7
#define ENCODER_B_PIN   D8

// ─── Layers ───────────────────────────────────────────────────────────────────

enum layers : uint8_t {
    DEFAULT = 0,
    KRITA,
    KRITA_SECOND,
    PIXELORAMA,
    LAYER_COUNT
};

// ─── Keymap ───────────────────────────────────────────────────────────────────
//
// LAYOUT maps the physical wiring order to a logical row/col grid.
// Your wiring: row0=[D2,D1,D0] col=[D6,D5,D4]
// The LAYOUT macro just labels positions for readability.

#define LAYOUT(k0A, k0B, k0C, \
               k1A, k1B, k1C, \
               k2A, k2B, k2C) \
  { { k0A, k0B, k0C },        \
    { k1A, k1B, k1C },        \
    { k2A, k2B, k2C } }

constexpr uint16_t PROGMEM keymaps[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {

    [DEFAULT] = LAYOUT(
        KC_1,    KC_2,     KC_3,
        KC_4,    KC_5,     KC_6,
        KC_7,    TO(KRITA),KC_8
    ),

    [KRITA] = LAYOUT(
        KC_F1,   KC_F2,    KC_F3,
        KC_F4,   KC_F5,    KC_F6,
        KC_F7,   TO(DEFAULT),  KC_F9
    ),

    [KRITA_SECOND] = LAYOUT(
        KC_F1,   KC_F2,    KC_F3,
        KC_F4,   KC_F5,    KC_F6,
        KC_F7,   KC_TRNS,  KC_F9
    ),

    [PIXELORAMA] = LAYOUT(
        KC_F1,   KC_F2,    KC_F3,
        KC_F4,   KC_F5,    KC_F6,
        KC_F7,   KC_TRNS,  KC_F9
    ),
};

// ─── Encoder keymaps ──────────────────────────────────────────────────────────
//
// Three actions per layer: { CW, CCW, BTN }
// Supports any keycode: KC_*, LCTL(KC_Z), HID_USAGE_CONSUMER_*, …
// KC_TRNS falls through to the layer below (same stack logic as matrix).

struct EncoderMap { uint16_t cw; uint16_t ccw; uint16_t btn; };

constexpr EncoderMap encoderMaps[LAYER_COUNT] = {
    [DEFAULT]       = { HID_USAGE_CONSUMER_VOLUME_INCREMENT,
                        HID_USAGE_CONSUMER_VOLUME_DECREMENT,
                        HID_USAGE_CONSUMER_PLAY_PAUSE       },

    [KRITA]         = { LCTL(KC_RBRC),   // rotate canvas right
                        LCTL(KC_LBRC),      // rotate canvas left
                        KC_TRNS          },

    [KRITA_SECOND]  = { LCTL(KC_Z),      // undo
                        RCS(KC_Z),          // redo
                        KC_TRNS          },

    [PIXELORAMA]    = { LCTL(KC_RBRC),
                        LCTL(KC_LBRC),
                        KC_TRNS          },
};

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
    /* 8 modifier bits (usage 224-231) */                            \
    HID_USAGE_PAGE   ( HID_USAGE_PAGE_KEYBOARD )                   ,\
    HID_USAGE_MIN    ( 224                     )                   ,\
    HID_USAGE_MAX    ( 231                     )                   ,\
    HID_LOGICAL_MIN  ( 0                       )                   ,\
    HID_LOGICAL_MAX  ( 1                       )                   ,\
    HID_REPORT_COUNT ( 8                       )                   ,\
    HID_REPORT_SIZE  ( 1                       )                   ,\
    HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE )    ,\
    /* 256-bit keycode bitmap: keycodes 0-255, 1 bit each */        \
    /* REPORT_COUNT(256) needs 2-byte encoding → HID_REPORT_COUNT_N */ \
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
    TUD_HID_REPORT_DESC_NKRO_KEYBOARD( HID_REPORT_ID(1) ),
    TUD_HID_REPORT_DESC_SYSTEM_CONTROL(HID_REPORT_ID(2) ),
    TUD_HID_REPORT_DESC_CONSUMER(      HID_REPORT_ID(3) ),
    TUD_HID_REPORT_DESC_MOUSE(         HID_REPORT_ID(4) ),
};

// ─── HID objects ──────────────────────────────────────────────────────────────
//
// Adafruit_USBD_HID::sendReport(report_id, data, len) is the correct API.
// UnifiedHid wraps it but doesn't expose sendReport — so we use
// Adafruit_USBD_HID directly. One instance handles all three report types
// because they share a single composite descriptor.

Adafruit_USBD_HID usbHid(desc_hid_report, sizeof(desc_hid_report),
                          HID_ITF_PROTOCOL_NONE, /*interval_ms*/ 2,
                          /*enable_out_ep*/ false);

// ─── QMK engine ───────────────────────────────────────────────────────────────
MAKE_QMK_ENGINE(keymaps, rowPins, colPins, ROW_TO_COL);
// QmkEngine<MATRIX_ROWS, MATRIX_COLS, LAYER_COUNT> engine(keymaps, rowPins, colPins); // if no need in macros

// ─── Matrix debounce ──────────────────────────────────────────────────────────

constexpr uint32_t MATRIX_DEB_MS = 10;

bool     matrixSettled[MATRIX_ROWS][MATRIX_COLS] = {};
bool     matrixRaw[MATRIX_ROWS][MATRIX_COLS]     = {};
bool     matrixPendingChange[MATRIX_ROWS][MATRIX_COLS] = {};
uint32_t matrixDebTimer[MATRIX_ROWS][MATRIX_COLS]= {};

// Per-key debounce: only commit a changed reading after it has been
// stable for MATRIX_DEB_MS ms, then push into engine.matrixCurrent.
void debounceMatrix() {
    uint32_t now = millis();

    for (uint8_t r = 0; r < MATRIX_ROWS; ++r) {
        digitalWrite(rowPins[r], LOW);
        delayMicroseconds(5);
        for (uint8_t c = 0; c < MATRIX_COLS; ++c) {
            bool raw = (digitalRead(colPins[c]) == LOW);
            if (raw != matrixRaw[r][c]) {
                matrixRaw[r][c]        = raw;
                matrixDebTimer[r][c]   = now;
                matrixPendingChange[r][c] = true;
            }
            if (matrixPendingChange[r][c] &&
                (now - matrixDebTimer[r][c]) >= MATRIX_DEB_MS) {
                matrixPendingChange[r][c]   = false;
                // Feed settled reading directly into engine
                engine.matrixCurrent[r][c] = matrixRaw[r][c];
            }
        }
        digitalWrite(rowPins[r], HIGH);
    }
}

// ─── Encoder ──────────────────────────────────────────────────────────────────

constexpr int8_t ENC_TABLE[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};
constexpr int8_t ENC_DETENT = 4;

volatile int8_t encDelta     = 0;
uint8_t         encStatePrev = 0;

void tickEncoder() {
    uint8_t a   = digitalRead(ENCODER_A_PIN);
    uint8_t b   = digitalRead(ENCODER_B_PIN);
    uint8_t cur = (a << 1) | b;
    encDelta   += ENC_TABLE[((encStatePrev << 2) | cur) & 0x0F];
    encStatePrev = cur;
}

// Walk encoder layer stack top-down, respecting KC_TRNS
uint16_t resolveEncoderKey(uint16_t EncoderMap::*field) {
    for (int8_t l = LAYER_COUNT - 1; l >= 0; --l) {
        if (!(engine.getLayerState() & (1u << l))) continue;
        uint16_t kc = encoderMaps[l].*field;
        if (kc != KC_TRNS) return kc;
    }
    return KC_NO;
}

// Send any keycode (keyboard or consumer) as a pulse
void sendKeyPulse(uint16_t kc) {
    if (kc == KC_NO) return;
    ResolvedKey rk = engine.resolveRaw(kc);
    if (rk.consumer) {
        usbHid.sendReport(3, &rk.consumer, sizeof(rk.consumer));
        delay(12);
        uint16_t zero = 0;
        usbHid.sendReport(3, &zero, sizeof(zero));
    } else if (rk.hid_keycode || rk.hid_mods) {
        uint8_t buf[33] = {};
        buf[0] = rk.hid_mods;
        if (rk.hid_keycode && rk.hid_keycode < 0xE0u)
            buf[1 + (rk.hid_keycode >> 3)] |= (1u << (rk.hid_keycode & 7u));
        usbHid.sendReport(1, buf, sizeof(buf));
        delay(12);
        uint8_t zero[33] = {};
        usbHid.sendReport(1, zero, sizeof(zero));
    }
}

// ─── Encoder button debounce ──────────────────────────────────────────────────

bool     encBtnSettled = HIGH;
bool     encBtnRaw     = HIGH;
uint32_t encBtnTimer   = 0;
constexpr uint32_t BTN_DEB_MS = 20;

void debounceEncBtn() {
    bool raw = digitalRead(ENCODER_BTN_PIN);
    if (raw != encBtnRaw) {
        encBtnRaw   = raw;
        encBtnTimer = millis();
    }
    if ((millis() - encBtnTimer) >= BTN_DEB_MS)
        encBtnSettled = encBtnRaw;
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    Wire.setPins(D10, D9);
    Wire.begin();

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed");
    }
    display.clearDisplay();

    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.printf("Hello Mio!");

    display.display();

    usbHid.begin();
    while (!TinyUSBDevice.mounted()) delay(1);

    for (uint8_t r = 0; r < MATRIX_ROWS; ++r) {
        pinMode(rowPins[r], OUTPUT);
        digitalWrite(rowPins[r], HIGH);
    }
    for (uint8_t c = 0; c < MATRIX_COLS; ++c)
        pinMode(colPins[c], INPUT_PULLUP);

    pinMode(ENCODER_A_PIN,   INPUT_PULLUP);
    pinMode(ENCODER_B_PIN,   INPUT_PULLUP);
    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);

    encStatePrev = (digitalRead(ENCODER_A_PIN) << 1) | digitalRead(ENCODER_B_PIN);

    // Optional: set tap-hold threshold (default 200ms)
    engine.TAP_HOLD_MS = 200;
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
    static uint8_t prev_layer_name = LAYER_COUNT;
    const uint8_t layer_name = engine.highestActiveLayer();
    if (prev_layer_name != layer_name) {
        prev_layer_name = layer_name;


        display.clearDisplay();
        display.setCursor(0,0);

        switch ( layer_name ) {
            case DEFAULT:
                display.println("DEFAULT");
                break;
            case KRITA:
                display.println("KRITA");
                break;
            case KRITA_SECOND:
                display.println("KRITA_SECOND");
                break;
            case PIXELORAMA:
                display.println("PIXELORAMA");
                break;
            default:
                display.println("UNKNOWN");
        }


        display.display();
    }

    // 1. Encoder tick (called every loop for fine resolution)
    tickEncoder();

    // 2. Encoder rotation — layer-aware
    if (encDelta >= ENC_DETENT) {
        encDelta -= ENC_DETENT;
        sendKeyPulse(resolveEncoderKey(&EncoderMap::cw));
        Serial.print("Encoder CW  layer="); Serial.println(engine.highestActiveLayer());
        return;
    }
    if (encDelta <= -ENC_DETENT) {
        encDelta += ENC_DETENT;
        sendKeyPulse(resolveEncoderKey(&EncoderMap::ccw));
        Serial.print("Encoder CCW layer="); Serial.println(engine.highestActiveLayer());
        return;
    }

    // 3. Matrix debounce — feeds engine.matrixCurrent[][]
    debounceMatrix();

    // 4. Encoder button debounce
    debounceEncBtn();

    if (!usbHid.ready()) return;

    // 5. Encoder button — layer-aware, edge-triggered
    static bool encBtnPrev = HIGH;
    if (encBtnSettled != encBtnPrev) {
        encBtnPrev = encBtnSettled;
        uint16_t kc = resolveEncoderKey(&EncoderMap::btn);
        ResolvedKey rk = engine.resolveRaw(kc);
        if (encBtnSettled == LOW) {
            if (rk.consumer)
                usbHid.sendReport(3, &rk.consumer, sizeof(rk.consumer));
            else if (rk.hid_keycode || rk.hid_mods) {
                uint8_t buf[33] = {};
                buf[0] = rk.hid_mods;
                if (rk.hid_keycode < 0xE0u)
                    buf[1 + (rk.hid_keycode >> 3)] |= (1u << (rk.hid_keycode & 7u));
                usbHid.sendReport(1, buf, sizeof(buf));
            }
        } else {
            uint8_t zero16[2] = {};
            usbHid.sendReport(3, zero16, sizeof(zero16));
            uint8_t zero33[33] = {};
            usbHid.sendReport(1, zero33, sizeof(zero33));
        }
    }

    // 6. Build NKRO + mouse HID reports via QMK engine
    using NkroReport  = QmkEngine<MATRIX_ROWS, MATRIX_COLS, LAYER_COUNT>::NkroReport;

    NkroReport  report   = {};
    MouseReport mouse    = {};
    uint16_t    consumer = 0;

    engine.buildReport(report, mouse, consumer);

    // 7. Send NKRO keyboard report (only on change)
    //    report ID 1: [mods (1 byte)] + [bitmap (32 bytes)] = 33 bytes total
    static NkroReport prevReport = {};

    if (report != prevReport) {
        prevReport = report;

        uint8_t buf[33];
        buf[0] = report.mods;
        memcpy(buf + 1, report.bitmap, 32);

        while (!usbHid.ready()) { tud_task(); }
        usbHid.sendReport(1, buf, sizeof(buf));
    }

    // 8. Send mouse report (ID 4) — always send so cursor/buttons release
    //    [buttons(1), x(1), y(1), v(1), h(1)] = 5 bytes
    static MouseReport prevMouse = {};

    if (mouse != prevMouse) {
        prevMouse = mouse;
        uint8_t mbuf[5] = {
            mouse.buttons,
            (uint8_t)mouse.x,
            (uint8_t)mouse.y,
            (uint8_t)mouse.v,
            (uint8_t)mouse.h
        };
        while (!usbHid.ready()) { tud_task(); }
        usbHid.sendReport(4, mbuf, sizeof(mbuf));
    }

    // 9. Send consumer report (report ID 3), only on change
    static uint16_t prevConsumer = 0;
    if (consumer != prevConsumer) {
        prevConsumer = consumer;
        usbHid.sendReport(3, &consumer, sizeof(consumer));
    }
}
