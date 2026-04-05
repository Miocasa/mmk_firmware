#pragma once
#include <Arduino.h>


#define OLED_SSD1306_ENABLED
#define OLED_SEPARATED_TASK  // include oled.h file with user configuration for oled_task_kb
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define SCREEN_ROTATION 270

#define ENCODER_ENABLE
#define ENCODER_BTN_PIN D3
#define ENCODER_A_PIN   D7
#define ENCODER_B_PIN   D8

#define MATRIX_ROTATION 90 /** Works only if rows and columns are equal **/

constexpr uint8_t rowPins[] = {D2, D1, D0};
constexpr uint8_t colPins[] = {D6, D5, D4};

constexpr uint8_t MATRIX_ROWS = sizeof(rowPins) / sizeof(rowPins[0]);
constexpr uint8_t MATRIX_COLS = sizeof(colPins) / sizeof(colPins[0]);


// ─── Layers ───────────────────────────────────────────────────────────────────

enum LAYERS : uint8_t {
    DEFAULT = 0,
    KRITA,
    KRITA_ALT,
    PIXELORAMA,
    FUNC,
    LAYER_COUNT
};

#define MEDIA DEFAULT
// ═══════════════════════════════════════════════════════════════
// 2. TAP DANCE
// ═══════════════════════════════════════════════════════════════
enum {
    DEFAULT_LAYER_TAP_DANCE,
    KRITA_LAYER_TAP_DANCE,
    PIXELORAMA_LAYER_TAP_DANCE,
    FUNC_LAYER_TAP_DANCE,
    TD_COUNT
};


// static TapDanceDoubleData DEFAULT_LAYER_TAP_FUNC = { TO(KRITA),TO(LAYER_COUNT - 1)};
// static TapDanceDoubleData KRITA_LAYER_TAP_FUNC = { TO(PIXELORAMA), TO(DEFAULT)};
// static TapDanceDoubleData PIXELORAMA_LAYER_TAP_FUNC = { TO(DEFAULT), TO(KRITA)};

inline void DEFAULT_LAYER_TAP_FUNC(TapDanceState *s, void *) {
    if (s->count == 1) {
        // tap или hold
        s->resolved_kc = KC_TRNS;
    } else if (s->count == 2 && !s->pressed) {
        // double tap
        s->resolved_kc = TO(KRITA);
    } else if (s->count == 2 && s->pressed) {
        // double tap + hold
        s->resolved_kc = TO(FUNC);
    }
}

inline void KRITA_LAYER_TAP_FUNC(TapDanceState *s, void *) {
    if (s->count == 1) s->resolved_kc = LCTL_T(KC_Z); // tap or hold
    else if (s->count == 2 && !s->pressed) s->resolved_kc = TO(PIXELORAMA); // double tap
    else if (s->count == 2 && s->pressed) s->resolved_kc = TO(DEFAULT); // tap+hold
}

inline void PIXELORAMA_LAYER_TAP_FUNC(TapDanceState *s, void *) {
    if (s->count == 1) s->resolved_kc = KC_LEFT_ALT; // tap or hold
    else if (s->count == 2 && !s->pressed) s->resolved_kc = TO(FUNC); // double tap
    else if (s->count == 2 && s->pressed) s->resolved_kc = TO(KRITA); // tap+hold
}

inline void FUNC_LAYER_TAP_FUNC(TapDanceState *s, void *) {
    if (s->count == 1 && !s->pressed) s->resolved_kc = KC_F7; // tap
    else if (s->count == 2 && !s->pressed) s->resolved_kc = TO(DEFAULT); // double tap
    else if (s->count == 1 && s->pressed) s->resolved_kc = KC_1; // hold
    else if (s->count == 2 && s->pressed) s->resolved_kc = TO(PIXELORAMA); // tap+hold
}

TapDanceEntry td_entries[TD_COUNT] = {
    // ACTION_TAP_DANCE_DOUBLE(&DEFAULT_LAYER_TAP_FUNC),
    // ACTION_TAP_DANCE_DOUBLE(&KRITA_LAYER_TAP_FUNC),
    // ACTION_TAP_DANCE_DOUBLE(&PIXELORAMA_LAYER_TAP_FUNC),
    ACTION_TAP_DANCE_FN(&DEFAULT_LAYER_TAP_FUNC),
    ACTION_TAP_DANCE_FN(&KRITA_LAYER_TAP_FUNC),
    ACTION_TAP_DANCE_FN(&PIXELORAMA_LAYER_TAP_FUNC),
    ACTION_TAP_DANCE_FN(&FUNC_LAYER_TAP_FUNC),
};

// ─── Keymap ───────────────────────────────────────────────────────────────────
//
// LAYOUT maps the physical wiring order to a logical row/col grid.
// Your wiring: row0=[D2,D1,D0] col=[D6,D5,D4]
// The LAYOUT macro just labels positions for readability.


// #define MATRIX_INVERT_VERTICAL
// #define MATRIX_INVERT_HORIZONTAL

#define LAYOUT(k0A, k0B, k0C, \
    k1A, k1B, k1C, \
    k2A, k2B, k2C) \
  { { k0A, k0B, k0C },        \
    { k1A, k1B, k1C },        \
    { k2A, k2B, k2C } }

constexpr uint16_t PROGMEM keymaps[LAYER_COUNT][MATRIX_ROWS][MATRIX_COLS] = {

    [MEDIA] = LAYOUT( // Media-слой (encoder уже делает volume/play)
        KC_MPRV, KC_MUTE, KC_MNXT, // prev / mute / next
        KC_MRWD, KC_MPLY, KC_MFFD, // rewind / play / fast-forward
        TD(DEFAULT_LAYER_TAP_DANCE), CTL_T(KC_Z), RCS(KC_Z)
    ),

    [KRITA] = LAYOUT( // Krita — цифровая живопись
        KC_B, KC_E, KC_P, // Brush / Eraser / Color Picker
        KC_T, KC_F, LCTL(KC_Z), // Move tool / Fill / Undo
        TD(KRITA_LAYER_TAP_DANCE), MO(KRITA_ALT), LCTL(KC_S) // Save
    ),

    [KRITA_ALT] = LAYOUT( // Второй слой Krita — работа со слоями
        LCTL(KC_J), LCTL(KC_E), LCTL(LSFT(KC_E)), // Duplicate / Merge down / Flatten
        LCTL(KC_PGUP), LCTL(KC_PGDN), LCTL(KC_G), // Layer ↑ / Layer ↓ / Group
        LCTL(LSFT(KC_N)), KC_TRNS, LCTL(KC_S) // New layer / Save
    ),

    [PIXELORAMA] = LAYOUT( // Pixelorama — пиксель-арт
        KC_P, KC_E, KC_B, // Pencil / Eraser / Color Picker
        KC_G, KC_R, LCTL(KC_Z), // Bucket / Rect Select / Undo
        TD(PIXELORAMA_LAYER_TAP_DANCE), LCTL(KC_C), LCTL(KC_V) // Copy / Paste
    ),

    [FUNC] = LAYOUT( // Новый общий функциональный слой (undo/redo/copy/paste и т.д.)
        LCTL(KC_Z), RCS(KC_Z), LCTL(KC_Y), // Undo / Redo / Redo (альтернатива)
        LCTL(KC_X), LCTL(KC_C), LCTL(KC_V), // Cut / Copy / Paste
        TD(FUNC_LAYER_TAP_DANCE), LCTL(KC_A), LCTL(KC_S) // Вернуться в DEFAULT / Select All / Save
    ),
};

// ─── Encoder keymaps ──────────────────────────────────────────────────────────
#ifdef ENCODER_ENABLE
struct EncoderMap {
    uint16_t cw;
    uint16_t ccw;
    uint16_t btn;
};

constexpr EncoderMap PROGMEM encoderMaps[LAYER_COUNT] = {
    [MEDIA] = {
        HID_USAGE_CONSUMER_VOLUME_INCREMENT,
        HID_USAGE_CONSUMER_VOLUME_DECREMENT,
        HID_USAGE_CONSUMER_PLAY_PAUSE
    },

    [KRITA] = {
        LCTL(KC_PLUS), // rotate canvas right
        LCTL(KC_MINUS), // rotate canvas left
        KC_TRNS
    },

    [KRITA_ALT] = {
        LCTL(KC_RBRC), // undo
        LCTL(KC_LBRC), // redo
        KC_TRNS
    },

    [PIXELORAMA] = {
        KC_EQUAL,
        KC_MINUS,
        KC_TRNS
    },

    [FUNC] = {
        LCTL(KC_Z), // undo
        RCS(KC_Z), // redo
        KC_TRNS
    },
};

#endif




