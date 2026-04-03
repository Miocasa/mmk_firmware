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
#include <keymap.hpp>


#ifdef OLED_SSD1306_ENABLED // init ssd1306 oled

#ifdef OLED_SEPARATED_TASK  // user header with oled_task_kb inside
#include "oled.h"
#endif

#define SCREEN_WIDTH_DEFAULT 128
#define SCREEN_HEIGHT_DEFAULT 64
#define  SCREEN_ADDRESS_DEFAULT 0x3C

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH SCREEN_WIDTH_DEFAULT
#endif

#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT SCREEN_HEIGHT_DEFAULT
#endif

#ifndef SCREEN_ADDRESS
#define SCREEN_ADDRESS SCREEN_ADDRESS_DEFAULT
#endif


#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)


Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
#endif


// ─── Pinout ───────────────────────────────────────────────────────────────────




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
    // TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(1)),
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
                matrixRaw[r][c]           = raw;
                matrixDebTimer[r][c]      = now;
                matrixPendingChange[r][c] = true;
            }
            if (matrixPendingChange[r][c] &&
                (now - matrixDebTimer[r][c]) >= MATRIX_DEB_MS) {
                matrixPendingChange[r][c] = false;

                // ── Step 1: Rotate ───────────────────────────────────────────
                uint8_t nr, nc;
                #if   MATRIX_ROTATION == 90  && MATRIX_ROWS == MATRIX_COLS
                    nr = c;                      nc = (MATRIX_ROWS - 1) - r;
                #elif MATRIX_ROTATION == 180 && MATRIX_ROWS == MATRIX_COLS
                    nr = (MATRIX_ROWS - 1) - r;  nc = (MATRIX_COLS - 1) - c;
                #elif MATRIX_ROTATION == 270 && MATRIX_ROWS == MATRIX_COLS
                    nr = (MATRIX_COLS - 1) - c;  nc = r;
                #else // 0°
                    nr = r;                      nc = c;
                #endif

                // ── Step 2: Invert ───────────────────────────────────────────
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
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0
};
constexpr int8_t ENC_DETENT = 4;

volatile int8_t encDelta     = 0;
uint8_t         encStatePrev = 0;

void tickEncoder() {
    const uint8_t a   = digitalRead(ENCODER_A_PIN);
    const uint8_t b   = digitalRead(ENCODER_B_PIN);
    const uint8_t cur = (a << 1) | b;
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
#endif

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
#ifdef ENCODER_ENABLE
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
#endif

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    Serial.begin(115200);

    engine.setTapDance(td_entries, TD_COUNT);
    engine.TAP_HOLD_MS    = 130;  // ms до hold для MT/LT
    engine.TAP_DANCE_TERM = 150;  // ms между тапами в tap dance

    Wire.setPins(D10, D9);
    Wire.begin();

#ifdef OLED_SSD1306_ENABLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed");
    }
    display.clearDisplay();

    display.setTextColor(WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.setRotation(SCREEN_ROTATION / 90);
    display.printf("Hello Mio!");

    display.display();
#endif

    usbHid.begin();
    while (!TinyUSBDevice.mounted()) delay(1);

    for (uint8_t r = 0; r < MATRIX_ROWS; ++r) {
        pinMode(rowPins[r], OUTPUT);
        digitalWrite(rowPins[r], HIGH);
    }
    for (uint8_t c = 0; c < MATRIX_COLS; ++c)
        pinMode(colPins[c], INPUT_PULLUP);

#ifdef ENCODER_ENABLE
    pinMode(ENCODER_A_PIN,   INPUT_PULLUP);
    pinMode(ENCODER_B_PIN,   INPUT_PULLUP);
    pinMode(ENCODER_BTN_PIN, INPUT_PULLUP);

    encStatePrev = (digitalRead(ENCODER_A_PIN) << 1) | digitalRead(ENCODER_B_PIN);
#endif
    // Optional: set tap-hold threshold (default 200ms)
    engine.TAP_HOLD_MS = 200;
}

// ─── Loop ─────────────────────────────────────────────────────────────────────
void loop() {


    // 1. Encoder tick (called every loop for fine resolution)
#ifdef ENCODER_ENABLE
    tickEncoder();

    // 2. Encoder rotation — layer-aware
    if (encDelta >= ENC_DETENT) {
        encDelta -= ENC_DETENT;
        sendKeyPulse(resolveEncoderKey(&EncoderMap::cw));
        Serial.print("Encoder CW  layer="); Serial.println(engine.highestActiveLayer());
    }
    if (encDelta <= -ENC_DETENT) {
        encDelta += ENC_DETENT;
        sendKeyPulse(resolveEncoderKey(&EncoderMap::ccw));
        Serial.print("Encoder CCW layer="); Serial.println(engine.highestActiveLayer());
    }
#endif

    // 3. Matrix debounce — feeds engine.matrixCurrent[][]
    debounceMatrix();

    // 4. Encoder button debounce
#ifdef ENCODER_ENABLE
    debounceEncBtn();
#endif

    if (!usbHid.ready()) {
        // display.clearDisplay();
        // display.setTextSize(1);
        // display.setCursor(0,0);
        // display.printf("Hid host not connected or hid not ready");
        // display.display();
        return;
    }

    // 5. Encoder button — layer-aware, edge-triggered
#ifdef ENCODER_ENABLE
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
#endif

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
            static_cast<uint8_t>(mouse.x),
            static_cast<uint8_t>(mouse.y),
            static_cast<uint8_t>(mouse.v),
            static_cast<uint8_t>(mouse.h)
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

#ifdef OLED_SSD1306_ENABLED
    oled_task_kb(&display, engine.highestActiveLayer());
#endif
}

