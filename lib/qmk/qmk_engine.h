/**
 * qmk_engine.h
 *
 * Standalone QMK-compatible keycode definitions + layer engine.
 * Replaces keycodes.h + quantum_keycodes.h + layer/matrix logic from QMK.
 *
 * Implements:
 *   - Full HID keycode table  (KC_*)
 *   - Modifier wrappers       (LCTL, LSFT, LALT, LGUI, RCTL, …)
 *   - Layer macros            (MO, TO, TG, DF, OSL, TT, LT, LM)
 *   - Mod-tap                 (MT, LCTL_T, LSFT_T, …)
 *   - Special keys            (KC_TRNS, KC_NO, HYPR, MEH)
 *   - 32-bit layer state stack
 *   - QmkEngine class: scanMatrix → resolveKey → sendHID
 *
 * Tap Dance supports all four QMK actions:
 *   - Single tap   (count=1, pressed=false)
 *   - Double tap   (count=2, pressed=false)
 *   - Hold         (count=1, pressed=true)
 *   - Tap + Hold   (count=2, pressed=true)
 *
 * Usage in main.ino:
 *   #include "qmk_engine.h"
 *   // define keymaps[][][] and pin arrays
 *   // instantiate QmkEngine engine(keymaps, rowPins, colPins, hid);
 *   // call engine.tick() in loop()
 */

#pragma once
#include <Arduino.h>

// ═══════════════════════════════════════════════════════════════════════════════
// §0  Diode Direction
// ═══════════════════════════════════════════════════════════════════════════════

enum DiodeDirection : uint8_t {
    ROW_TO_COL,
    COL_TO_ROW
};

// ═══════════════════════════════════════════════════════════════════════════════
// §1  Keycode storage layout  (uint16_t)
//
//  [15:13]  command family tag
//  [12: 8]  modifier bits / layer / mod index
//  [ 7: 0]  base HID keycode
//
// ═══════════════════════════════════════════════════════════════════════════════

// ── Modifier bits (bits 8-12 in QK_MODS range) ────────────────────────────────
#define QK_LCTL        0x0100u
#define QK_LSFT        0x0200u
#define QK_LALT        0x0400u
#define QK_LGUI        0x0800u
#define QK_RMODS_MIN   0x1000u
#define QK_RCTL        0x1100u
#define QK_RSFT        0x1200u
#define QK_RALT        0x1400u
#define QK_RGUI        0x1800u

// ── Quantum keycode range bases ───────────────────────────────────────────────
#define QK_MOMENTARY            0x5100u   // MO
#define QK_DEF_LAYER            0x5200u   // DF
#define QK_TOGGLE_LAYER         0x5300u   // TG
#define QK_ONE_SHOT_LAYER       0x5400u   // OSL
#define QK_ONE_SHOT_MOD         0x5500u   // OSM
#define QK_LAYER_TAP_TOGGLE     0x5600u   // TT
#define QK_PERSISTENT_DEF_LAYER 0x5700u   // PDF
#define QK_TO                   0x5000u   // TO
#define QK_LAYER_MOD            0x6000u   // LM
#define QK_MOD_TAP              0x2000u   // MT  bits[12:8]=mod  bits[7:0]=kc
#define QK_LAYER_TAP            0x4000u   // LT  bits[11:8]=layer bits[7:0]=kc

#define QK_MAGIC                0x7000
#define QK_MAGIC_MAX            0x70FF
#define QK_USER                 0x7E40u
#define QK_USER_MAX             0x7FFF

#define SAFE_RANGE              QK_USER

// ── Decode helpers ────────────────────────────────────────────────────────────
#define QK_MODS_GET_MODS(kc)            (((kc) >> 8) & 0x1Fu)
#define QK_MODS_GET_BASIC_KEYCODE(kc)   ((kc) & 0xFFu)
#define QK_LAYER_TAP_GET_LAYER(kc)      (((kc) >> 8) & 0x0Fu)
#define QK_LAYER_TAP_GET_TAP_KEYCODE(kc) ((kc) & 0xFFu)
#define QK_MOD_TAP_GET_MODS(kc)        (((kc) >> 8) & 0x1Fu)
#define QK_MOD_TAP_GET_TAP_KEYCODE(kc)  ((kc) & 0xFFu)
#define QK_MOMENTARY_GET_LAYER(kc)      ((kc) & 0x1Fu)
#define QK_TOGGLE_LAYER_GET_LAYER(kc)   ((kc) & 0x1Fu)
#define QK_DEF_LAYER_GET_LAYER(kc)      ((kc) & 0x1Fu)
#define QK_TO_GET_LAYER(kc)             ((kc) & 0x1Fu)
#define QK_ONE_SHOT_LAYER_GET_LAYER(kc) ((kc) & 0x1Fu)
#define QK_ONE_SHOT_MOD_GET_MODS(kc)    ((kc) & 0x1Fu)
#define QK_LAYER_TAP_TOGGLE_GET_LAYER(kc) ((kc) & 0x1Fu)

// ─────────────────────────────────────────────────────────────────────────────
// §2  Modifier wrappers
// ─────────────────────────────────────────────────────────────────────────────

#define LCTL(kc)  ((uint16_t)(QK_LCTL | (kc)))
#define LSFT(kc)  ((uint16_t)(QK_LSFT | (kc)))
#define LALT(kc)  ((uint16_t)(QK_LALT | (kc)))
#define LGUI(kc)  ((uint16_t)(QK_LGUI | (kc)))
#define RCTL(kc)  ((uint16_t)(QK_RCTL | (kc)))
#define RSFT(kc)  ((uint16_t)(QK_RSFT | (kc)))
#define RALT(kc)  ((uint16_t)(QK_RALT | (kc)))
#define RGUI(kc)  ((uint16_t)(QK_RGUI | (kc)))

#define LOPT(kc)  LALT(kc)
#define LCMD(kc)  LGUI(kc)
#define LWIN(kc)  LGUI(kc)
#define ROPT(kc)  RALT(kc)
#define RCMD(kc)  RGUI(kc)
#define RWIN(kc)  RGUI(kc)
#define ALGR(kc)  RALT(kc)

#define HYPR(kc)  ((uint16_t)(QK_LCTL | QK_LSFT | QK_LALT | QK_LGUI | (kc)))
#define MEH(kc)   ((uint16_t)(QK_LCTL | QK_LSFT | QK_LALT | (kc)))
#define LCAG(kc)  ((uint16_t)(QK_LCTL | QK_LALT | QK_LGUI | (kc)))
#define LSG(kc)   ((uint16_t)(QK_LSFT | QK_LGUI | (kc)))
#define LAG(kc)   ((uint16_t)(QK_LALT | QK_LGUI | (kc)))
#define LCA(kc)   ((uint16_t)(QK_LCTL | QK_LALT | (kc)))
#define LSA(kc)   ((uint16_t)(QK_LSFT | QK_LALT | (kc)))
#define RSA(kc)   ((uint16_t)(QK_RSFT | QK_RALT | (kc)))
#define RCS(kc)   ((uint16_t)(QK_RCTL | QK_RSFT | (kc)))
#define RSG(kc)   ((uint16_t)(QK_RSFT | QK_RGUI | (kc)))
#define RAG(kc)   ((uint16_t)(QK_RALT | QK_RGUI | (kc)))

#define C(kc) LCTL(kc)
#define S(kc) LSFT(kc)
#define A(kc) LALT(kc)
#define G(kc) LGUI(kc)

// ─────────────────────────────────────────────────────────────────────────────
// §3  Layer macros
// ─────────────────────────────────────────────────────────────────────────────

#define MO(layer)    ((uint16_t)(QK_MOMENTARY            | ((layer) & 0x1Fu)))
#define TG(layer)    ((uint16_t)(QK_TOGGLE_LAYER         | ((layer) & 0x1Fu)))
#define TO(layer)    ((uint16_t)(QK_TO                   | ((layer) & 0x1Fu)))
#define DF(layer)    ((uint16_t)(QK_DEF_LAYER            | ((layer) & 0x1Fu)))
#define PDF(layer)   ((uint16_t)(QK_PERSISTENT_DEF_LAYER | ((layer) & 0x1Fu)))
#define OSL(layer)   ((uint16_t)(QK_ONE_SHOT_LAYER       | ((layer) & 0x1Fu)))
#define TT(layer)    ((uint16_t)(QK_LAYER_TAP_TOGGLE     | ((layer) & 0x1Fu)))
#define LM(layer, mod) ((uint16_t)(QK_LAYER_MOD | (((layer) & 0xFu) << 5) | ((mod) & 0x1Fu)))

// Layer-tap: hold = layer, tap = kc  (layer 0-15, kc 0-255)
#define LT(layer, kc) ((uint16_t)(QK_LAYER_TAP | (((layer) & 0x0Fu) << 8) | ((kc) & 0xFFu)))

// ─────────────────────────────────────────────────────────────────────────────
// §4  Mod-tap macros
// ─────────────────────────────────────────────────────────────────────────────

// MOD_ bits (used inside MT, distinct from QK_ bits above)
#define MOD_LCTL  0x01u
#define MOD_LSFT  0x02u
#define MOD_LALT  0x04u
#define MOD_LGUI  0x08u
#define MOD_RCTL  0x10u
#define MOD_RSFT  0x20u
#define MOD_RALT  0x40u
#define MOD_RGUI  0x80u

// MT packs mod into bits[12:8]
#define MT(mod, kc) ((uint16_t)(QK_MOD_TAP | (((mod) & 0x1Fu) << 8) | ((kc) & 0xFFu)))

#define LCTL_T(kc)  MT(MOD_LCTL, kc)
#define RCTL_T(kc)  MT(MOD_RCTL, kc)
#define CTL_T(kc)   LCTL_T(kc)
#define LSFT_T(kc)  MT(MOD_LSFT, kc)
#define RSFT_T(kc)  MT(MOD_RSFT, kc)
#define SFT_T(kc)   LSFT_T(kc)
#define LALT_T(kc)  MT(MOD_LALT, kc)
#define RALT_T(kc)  MT(MOD_RALT, kc)
#define ALT_T(kc)   LALT_T(kc)
#define LOPT_T(kc)  LALT_T(kc)
#define ROPT_T(kc)  RALT_T(kc)
#define ALGR_T(kc)  RALT_T(kc)
#define LGUI_T(kc)  MT(MOD_LGUI, kc)
#define RGUI_T(kc)  MT(MOD_RGUI, kc)
#define GUI_T(kc)   LGUI_T(kc)
#define LCMD_T(kc)  LGUI_T(kc)
#define RCMD_T(kc)  RGUI_T(kc)
#define LWIN_T(kc)  LGUI_T(kc)
#define RWIN_T(kc)  RGUI_T(kc)
#define C_S_T(kc)   MT(MOD_LCTL | MOD_LSFT, kc)
#define MEH_T(kc)   MT(MOD_LCTL | MOD_LSFT | MOD_LALT, kc)
#define HYPR_T(kc)  MT(MOD_LCTL | MOD_LSFT | MOD_LALT | MOD_LGUI, kc)
#define LCAG_T(kc)  MT(MOD_LCTL | MOD_LALT | MOD_LGUI, kc)
#define LSG_T(kc)   MT(MOD_LSFT | MOD_LGUI, kc)
#define LAG_T(kc)   MT(MOD_LALT | MOD_LGUI, kc)
#define LCA_T(kc)   MT(MOD_LCTL | MOD_LALT, kc)
#define LSA_T(kc)   MT(MOD_LSFT | MOD_LALT, kc)
#define RSA_T(kc)   MT(MOD_RSFT | MOD_RALT, kc)
#define RSG_T(kc)   MT(MOD_RSFT | MOD_RGUI, kc)
#define RAG_T(kc)   MT(MOD_RALT | MOD_RGUI, kc)
#define RCS_T(kc)   MT(MOD_RCTL | MOD_RSFT, kc)

// Standalone Hyper / Meh keys
#define KC_HYPR HYPR(KC_NO)
#define KC_MEH  MEH(KC_NO)

// ─────────────────────────────────────────────────────────────────────────────
// §5  Basic HID keycodes  (USB HID Usage Page 0x07)
// ─────────────────────────────────────────────────────────────────────────────

#define KC_NO               0x00u
#define KC_TRNS             0x01u   // transparent — falls through to lower layer
#define XXX                 KC_NO
// Boot Magick
#define QK_MAGIC_BOOTLOADER 0x7001u

// Letters
#define KC_A  0x04u
#define KC_B  0x05u
#define KC_C  0x06u
#define KC_D  0x07u
#define KC_E  0x08u
#define KC_F  0x09u
#define KC_G  0x0Au
#define KC_H  0x0Bu
#define KC_I  0x0Cu
#define KC_J  0x0Du
#define KC_K  0x0Eu
#define KC_L  0x0Fu
#define KC_M  0x10u
#define KC_N  0x11u
#define KC_O  0x12u
#define KC_P  0x13u
#define KC_Q  0x14u
#define KC_R  0x15u
#define KC_S  0x16u
#define KC_T  0x17u
#define KC_U  0x18u
#define KC_V  0x19u
#define KC_W  0x1Au
#define KC_X  0x1Bu
#define KC_Y  0x1Cu
#define KC_Z  0x1Du

// Digits (row)
#define KC_1  0x1Eu
#define KC_2  0x1Fu
#define KC_3  0x20u
#define KC_4  0x21u
#define KC_5  0x22u
#define KC_6  0x23u
#define KC_7  0x24u
#define KC_8  0x25u
#define KC_9  0x26u
#define KC_0  0x27u

// Core control
#define KC_ENTER        0x28u
#define KC_ENT          KC_ENTER
#define KC_ESCAPE       0x29u
#define KC_ESC          KC_ESCAPE
#define KC_BACKSPACE    0x2Au
#define KC_BSPC         KC_BACKSPACE
#define KC_TAB          0x2Bu
#define KC_SPACE        0x2Cu
#define KC_SPC          KC_SPACE
#define KC_MINUS        0x2Du
#define KC_MINS         KC_MINUS
#define KC_EQUAL        0x2Eu
#define KC_EQL          KC_EQUAL
#define KC_LEFT_BRACKET  0x2Fu
#define KC_LBRC          KC_LEFT_BRACKET
#define KC_RIGHT_BRACKET 0x30u
#define KC_RBRC          KC_RIGHT_BRACKET
#define KC_BACKSLASH    0x31u
#define KC_BSLS         KC_BACKSLASH
#define KC_NONUS_HASH   0x32u
#define KC_NUHS         KC_NONUS_HASH
#define KC_SEMICOLON    0x33u
#define KC_SCLN         KC_SEMICOLON
#define KC_QUOTE        0x34u
#define KC_QUOT         KC_QUOTE
#define KC_GRAVE        0x35u
#define KC_GRV          KC_GRAVE
#define KC_COMMA        0x36u
#define KC_COMM         KC_COMMA
#define KC_DOT          0x37u
#define KC_SLASH        0x38u
#define KC_SLSH         KC_SLASH
#define KC_CAPS_LOCK    0x39u
#define KC_CAPS         KC_CAPS_LOCK

// Function keys
#define KC_F1  0x3Au
#define KC_F2  0x3Bu
#define KC_F3  0x3Cu
#define KC_F4  0x3Du
#define KC_F5  0x3Eu
#define KC_F6  0x3Fu
#define KC_F7  0x40u
#define KC_F8  0x41u
#define KC_F9  0x42u
#define KC_F10 0x43u
#define KC_F11 0x44u
#define KC_F12 0x45u
#define KC_F13 0x68u
#define KC_F14 0x69u
#define KC_F15 0x6Au
#define KC_F16 0x6Bu
#define KC_F17 0x6Cu
#define KC_F18 0x6Du
#define KC_F19 0x6Eu
#define KC_F20 0x6Fu
#define KC_F21 0x70u
#define KC_F22 0x71u
#define KC_F23 0x72u
#define KC_F24 0x73u

// Navigation cluster
#define KC_PRINT_SCREEN 0x46u
#define KC_PSCR         KC_PRINT_SCREEN
#define KC_SCROLL_LOCK  0x47u
#define KC_SCRL         KC_SCROLL_LOCK
#define KC_PAUSE        0x48u
#define KC_PAUS         KC_PAUSE
#define KC_INSERT       0x49u
#define KC_INS          KC_INSERT
#define KC_HOME         0x4Au
#define KC_PAGE_UP      0x4Bu
#define KC_PGUP         KC_PAGE_UP
#define KC_DELETE       0x4Cu
#define KC_DEL          KC_DELETE
#define KC_END          0x4Du
#define KC_PAGE_DOWN    0x4Eu
#define KC_PGDN         KC_PAGE_DOWN
#define KC_RIGHT        0x4Fu
#define KC_LEFT         0x50u
#define KC_DOWN         0x51u
#define KC_UP           0x52u

// Numpad
#define KC_NUM_LOCK     0x53u
#define KC_NUM          KC_NUM_LOCK
#define KC_KP_SLASH     0x54u
#define KC_PSLS         KC_KP_SLASH
#define KC_KP_ASTERISK  0x55u
#define KC_PAST         KC_KP_ASTERISK
#define KC_KP_MINUS     0x56u
#define KC_PMNS         KC_KP_MINUS
#define KC_KP_PLUS      0x57u
#define KC_PPLS         KC_KP_PLUS
#define KC_KP_ENTER     0x58u
#define KC_PENT         KC_KP_ENTER
#define KC_KP_1         0x59u
#define KC_KP_2         0x5Au
#define KC_KP_3         0x5Bu
#define KC_KP_4         0x5Cu
#define KC_KP_5         0x5Du
#define KC_KP_6         0x5Eu
#define KC_KP_7         0x5Fu
#define KC_KP_8         0x60u
#define KC_KP_9         0x61u
#define KC_KP_0         0x62u
#define KC_KP_DOT       0x63u
#define KC_PDOT         KC_KP_DOT
#define KC_NONUS_BACKSLASH 0x64u
#define KC_NUBS         KC_NONUS_BACKSLASH
#define KC_APPLICATION  0x65u
#define KC_APP          KC_APPLICATION

// Modifier keys (standalone HID codes)
#define KC_LEFT_CTRL    0xE0u
#define KC_LCTL         KC_LEFT_CTRL
#define KC_LEFT_SHIFT   0xE1u
#define KC_LSFT         KC_LEFT_SHIFT
#define KC_LEFT_ALT     0xE2u
#define KC_LALT         KC_LEFT_ALT
#define KC_LEFT_GUI     0xE3u
#define KC_LGUI         KC_LEFT_GUI
#define KC_RIGHT_CTRL   0xE4u
#define KC_RCTL         KC_RIGHT_CTRL
#define KC_RIGHT_SHIFT  0xE5u
#define KC_RSFT         KC_RIGHT_SHIFT
#define KC_RIGHT_ALT    0xE6u
#define KC_RALT         KC_RIGHT_ALT
#define KC_RIGHT_GUI    0xE7u
#define KC_RGUI         KC_RIGHT_GUI

// Media / system (mapped to HID consumer page internally)
#define KC_AUDIO_VOL_UP     0xF0u
#define KC_VOLU             KC_AUDIO_VOL_UP
#define KC_AUDIO_VOL_DOWN   0xF1u
#define KC_VOLD             KC_AUDIO_VOL_DOWN
#define KC_AUDIO_MUTE       0xF2u
#define KC_MUTE             KC_AUDIO_MUTE
#define KC_MEDIA_PLAY_PAUSE 0xF3u
#define KC_MPLY             KC_MEDIA_PLAY_PAUSE
#define KC_MEDIA_NEXT_TRACK 0xF4u
#define KC_MNXT             KC_MEDIA_NEXT_TRACK
#define KC_MEDIA_PREV_TRACK 0xF5u
#define KC_MPRV             KC_MEDIA_PREV_TRACK
#define KC_MEDIA_STOP       0xF6u
#define KC_MSTP             KC_MEDIA_STOP
#define KC_MEDIA_SELECT = 0x00AF
#define KC_MEDIA_EJECT 0x00B0
#define KC_MAIL 0x00B1
#define KC_CALCULATOR 0x00B2
#define KC_MY_COMPUTER 0x00B3
#define KC_WWW_SEARCH 0x00B4
#define KC_WWW_HOME 0x00B5
#define KC_WWW_BACK 0x00B6
#define KC_WWW_FORWARD 0x00B7
#define KC_WWW_STOP 0x00B8
#define KC_WWW_REFRESH 0x00B9
#define KC_WWW_FAVORITES 0x00BA
#define KC_MEDIA_FAST_FORWARD 0x00BB
#define KC_MEDIA_REWIND 0x00BC
#define KC_BRIGHTNESS_UP 0x00BD
#define KC_BRIGHTNESS_DOWN 0x00BE
#define KC_CONTROL_PANEL 0x00BF
#define KC_ASSISTANT 0x00C0
#define KC_MISSION_CONTROL 0x00C1
#define KC_LAUNCHPAD 0x00C2
#define KC_EXECUTE 0x0074
#define KC_HELP 0x0075
#define KC_MENU 0x0076
#define KC_SELECT 0x0077
#define KC_STOP 0x0078
#define KC_AGAIN 0x0079
#define KC_UNDO 0x007A
#define KC_CUT 0x007B
#define KC_COPY 0x007C
#define KC_PASTE 0x007D
#define KC_FIND 0x007E
#define KC_KB_MUTE 0x007F
#define KC_KB_VOLUME_UP 0x0080
#define KC_KB_VOLUME_DOWN 0x0081
#define KC_LOCKING_CAPS_LOCK 0x0082
#define KC_LOCKING_NUM_LOCK 0x0083
#define KC_LOCKING_SCROLL_LOCK 0x0084
#define KC_KP_COMMA 0x0085
#define KC_KP_EQUAL_AS400 0x0086
#define KC_INTERNATIONAL_1 0x0087
#define KC_INTERNATIONAL_2 0x0088
#define KC_INTERNATIONAL_3 0x0089
#define KC_INTERNATIONAL_4 0x008A
#define KC_INTERNATIONAL_5 0x008B
#define KC_INTERNATIONAL_6 0x008C
#define KC_INTERNATIONAL_7 0x008D
#define KC_INTERNATIONAL_8 0x008E
#define KC_INTERNATIONAL_9 0x008F
#define KC_LANGUAGE_1 0x0090
#define KC_LANGUAGE_2 0x0091
#define KC_LANGUAGE_3 0x0092
#define KC_LANGUAGE_4 0x0093
#define KC_LANGUAGE_5 0x0094
#define KC_LANGUAGE_6 0x0095
#define KC_LANGUAGE_7 0x0096
#define KC_LANGUAGE_8 0x0097
#define KC_LANGUAGE_9 0x0098
#define KC_ALTERNATE_ERASE 0x0099
#define KC_SYSTEM_REQUEST 0x009A
#define KC_CANCEL 0x009B
#define KC_CLEAR 0x009C
#define KC_PRIOR 0x009D
#define KC_RETURN 0x009E
#define KC_SEPARATOR 0x009F
#define KC_OUT 0x00A0
#define KC_OPER 0x00A1
#define KC_CLEAR_AGAIN 0x00A2
#define KC_CRSEL 0x00A3
#define KC_EXSEL 0x00A4
#define KC_SYSTEM_POWER 0x00A5
#define KC_SYSTEM_SLEEP 0x00A6
#define KC_SYSTEM_WAKE 0x00A7

// US ANSI shifted aliases
#define KC_TILDE        LSFT(KC_GRAVE)
#define KC_TILD         KC_TILDE
#define KC_EXCLAIM      LSFT(KC_1)
#define KC_EXLM         KC_EXCLAIM
#define KC_AT           LSFT(KC_2)
#define KC_HASH         LSFT(KC_3)
#define KC_DOLLAR       LSFT(KC_4)
#define KC_DLR          KC_DOLLAR
#define KC_PERCENT      LSFT(KC_5)
#define KC_PERC         KC_PERCENT
#define KC_CIRCUMFLEX   LSFT(KC_6)
#define KC_CIRC         KC_CIRCUMFLEX
#define KC_AMPERSAND    LSFT(KC_7)
#define KC_AMPR         KC_AMPERSAND
#define KC_ASTERISK     LSFT(KC_8)
#define KC_ASTR         KC_ASTERISK
#define KC_LEFT_PAREN   LSFT(KC_9)
#define KC_LPRN         KC_LEFT_PAREN
#define KC_RIGHT_PAREN  LSFT(KC_0)
#define KC_RPRN         KC_RIGHT_PAREN
#define KC_UNDERSCORE   LSFT(KC_MINUS)
#define KC_UNDS         KC_UNDERSCORE
#define KC_PLUS         LSFT(KC_EQUAL)
#define KC_LEFT_CURLY_BRACE  LSFT(KC_LBRC)
#define KC_LCBR              KC_LEFT_CURLY_BRACE
#define KC_RIGHT_CURLY_BRACE LSFT(KC_RBRC)
#define KC_RCBR              KC_RIGHT_CURLY_BRACE
#define KC_PIPE         LSFT(KC_BACKSLASH)
#define KC_COLON        LSFT(KC_SEMICOLON)
#define KC_COLN         KC_COLON
#define KC_DOUBLE_QUOTE LSFT(KC_QUOTE)
#define KC_DQUO         KC_DOUBLE_QUOTE
#define KC_LEFT_ANGLE_BRACKET  LSFT(KC_COMMA)
#define KC_LABK         KC_LEFT_ANGLE_BRACKET
#define KC_RIGHT_ANGLE_BRACKET LSFT(KC_DOT)
#define KC_RABK         KC_RIGHT_ANGLE_BRACKET
#define KC_QUESTION     LSFT(KC_SLASH)
#define KC_QUES         KC_QUESTION

// ─────────────────────────────────────────────────────────────────────────────
// §6  HID Consumer / System usage IDs  (for encoder, media keys, etc.)
//     These match TinyUSB / Adafruit TinyUSB consumer page values.
// ─────────────────────────────────────────────────────────────────────────────

#define HID_USAGE_CONSUMER_PLAY_PAUSE           0x00CDu
#define HID_USAGE_CONSUMER_SCAN_NEXT_TRACK      0x00B5u
#define HID_USAGE_CONSUMER_SCAN_PREV_TRACK      0x00B6u
#define HID_USAGE_CONSUMER_STOP                 0x00B7u
#define HID_USAGE_CONSUMER_VOLUME_INCREMENT     0x00E9u
#define HID_USAGE_CONSUMER_VOLUME_DECREMENT     0x00EAu
#define HID_USAGE_CONSUMER_MUTE                 0x00E2u
#define HID_USAGE_CONSUMER_FAST_FORWARD         0x00B3u
#define HID_USAGE_CONSUMER_REWIND               0x00B4u
#define HID_USAGE_CONSUMER_BRIGHTNESS_INCREMENT 0x006Fu
#define HID_USAGE_CONSUMER_BRIGHTNESS_DECREMENT 0x0070u
#define HID_USAGE_CONSUMER_AL_CALCULATOR        0x0192u
#define HID_USAGE_CONSUMER_AL_EMAIL             0x018Au
#define HID_USAGE_CONSUMER_AC_SEARCH            0x0221u
#define HID_USAGE_CONSUMER_AC_HOME              0x0223u
#define HID_USAGE_CONSUMER_AC_BACK              0x0224u
#define HID_USAGE_CONSUMER_AC_FORWARD           0x0225u

#define HID_USAGE_SYSTEM_POWER_DOWN             0x0081u
#define HID_USAGE_SYSTEM_SLEEP                  0x0082u
#define HID_USAGE_SYSTEM_WAKE_UP                0x0083u

// ─────────────────────────────────────────────────────────────────────────────
// §7  TinyUSB HID report descriptor macros
//     (passthrough — already provided by TinyUSB/Adafruit; kept here for
//      reference so the file is self-documenting)
// ─────────────────────────────────────────────────────────────────────────────

// These are provided by <Adafruit_TinyUSB.h> / tusb.h:
//   TUD_HID_REPORT_DESC_KEYBOARD(...)
//   TUD_HID_REPORT_DESC_SYSTEM_CONTROL(...)
//   TUD_HID_REPORT_DESC_CONSUMER(...)
//   HID_REPORT_ID(n)
// No redefinition needed — just include <Adafruit_TinyUSB.h> before this file.

// ═══════════════════════════════════════════════════════════════════════════════
// §8  QmkEngine — layer stack + key resolver
// ═══════════════════════════════════════════════════════════════════════════════

#define QMK_MAX_LAYERS  16
#define QMK_MAX_KEYS    16   // max keys in one report slot

// ── Mouse keycode ranges (from QMK keycodes.h) ────────────────────────────────
#define QK_MOUSE_CURSOR_UP      0x00CDu
#define QK_MOUSE_CURSOR_DOWN    0x00CEu
#define QK_MOUSE_CURSOR_LEFT    0x00CFu
#define QK_MOUSE_CURSOR_RIGHT   0x00D0u
#define QK_MOUSE_BUTTON_1       0x00D1u
#define QK_MOUSE_BUTTON_2       0x00D2u
#define QK_MOUSE_BUTTON_3       0x00D3u
#define QK_MOUSE_BUTTON_4       0x00D4u
#define QK_MOUSE_BUTTON_5       0x00D5u
#define QK_MOUSE_BUTTON_6       0x00D6u
#define QK_MOUSE_BUTTON_7       0x00D7u
#define QK_MOUSE_BUTTON_8       0x00D8u
#define QK_MOUSE_WHEEL_UP       0x00D9u
#define QK_MOUSE_WHEEL_DOWN     0x00DAu
#define QK_MOUSE_WHEEL_LEFT     0x00DBu
#define QK_MOUSE_WHEEL_RIGHT    0x00DCu
#define QK_MOUSE_ACCELERATION_0 0x00DDu
#define QK_MOUSE_ACCELERATION_1 0x00DEu
#define QK_MOUSE_ACCELERATION_2 0x00DFu

// Aliases matching QMK names
#define MS_UP   QK_MOUSE_CURSOR_UP
#define MS_DOWN QK_MOUSE_CURSOR_DOWN
#define MS_LEFT QK_MOUSE_CURSOR_LEFT
#define MS_RGHT QK_MOUSE_CURSOR_RIGHT
#define MS_BTN1 QK_MOUSE_BUTTON_1
#define MS_BTN2 QK_MOUSE_BUTTON_2
#define MS_BTN3 QK_MOUSE_BUTTON_3
#define MS_BTN4 QK_MOUSE_BUTTON_4
#define MS_BTN5 QK_MOUSE_BUTTON_5
#define MS_WHLU QK_MOUSE_WHEEL_UP
#define MS_WHLD QK_MOUSE_WHEEL_DOWN
#define MS_WHLL QK_MOUSE_WHEEL_LEFT
#define MS_WHLR QK_MOUSE_WHEEL_RIGHT
#define MS_ACL0 QK_MOUSE_ACCELERATION_0
#define MS_ACL1 QK_MOUSE_ACCELERATION_1
#define MS_ACL2 QK_MOUSE_ACCELERATION_2

// Mouse movement speed in units per tick (can be tuned)
#define MOUSE_MOVE_STEP  5
#define MOUSE_WHEEL_STEP 1
// Acceleration multipliers
#define MOUSE_ACL0_MUL 1
#define MOUSE_ACL1_MUL 4
#define MOUSE_ACL2_MUL 16


enum alias {
    // Alias
    KC_BRMD = KC_SCROLL_LOCK,
    KC_BRK = KC_PAUSE,
    KC_BRMU = KC_PAUSE,
    KC_P1 = KC_KP_1,
    KC_P2 = KC_KP_2,
    KC_P3 = KC_KP_3,
    KC_P4 = KC_KP_4,
    KC_P5 = KC_KP_5,
    KC_P6 = KC_KP_6,
    KC_P7 = KC_KP_7,
    KC_P8 = KC_KP_8,
    KC_P9 = KC_KP_9,
    KC_P0 = KC_KP_0,
    KC_EXEC = KC_EXECUTE,
    KC_SLCT = KC_SELECT,
    KC_AGIN = KC_AGAIN,
    KC_PSTE = KC_PASTE,
    KC_LCAP = KC_LOCKING_CAPS_LOCK,
    KC_LNUM = KC_LOCKING_NUM_LOCK,
    KC_LSCR = KC_LOCKING_SCROLL_LOCK,
    KC_PCMM = KC_KP_COMMA,
    KC_INT1 = KC_INTERNATIONAL_1,
    KC_INT2 = KC_INTERNATIONAL_2,
    KC_INT3 = KC_INTERNATIONAL_3,
    KC_INT4 = KC_INTERNATIONAL_4,
    KC_INT5 = KC_INTERNATIONAL_5,
    KC_INT6 = KC_INTERNATIONAL_6,
    KC_INT7 = KC_INTERNATIONAL_7,
    KC_INT8 = KC_INTERNATIONAL_8,
    KC_INT9 = KC_INTERNATIONAL_9,
    KC_LNG1 = KC_LANGUAGE_1,
    KC_LNG2 = KC_LANGUAGE_2,
    KC_LNG3 = KC_LANGUAGE_3,
    KC_LNG4 = KC_LANGUAGE_4,
    KC_LNG5 = KC_LANGUAGE_5,
    KC_LNG6 = KC_LANGUAGE_6,
    KC_LNG7 = KC_LANGUAGE_7,
    KC_LNG8 = KC_LANGUAGE_8,
    KC_LNG9 = KC_LANGUAGE_9,
    KC_ERAS = KC_ALTERNATE_ERASE,
    KC_SYRQ = KC_SYSTEM_REQUEST,
    KC_CNCL = KC_CANCEL,
    KC_CLR = KC_CLEAR,
    KC_PRIR = KC_PRIOR,
    KC_RETN = KC_RETURN,
    KC_SEPR = KC_SEPARATOR,
    KC_CLAG = KC_CLEAR_AGAIN,
    KC_CRSL = KC_CRSEL,
    KC_EXSL = KC_EXSEL,
    KC_PWR = KC_SYSTEM_POWER,
    KC_SLEP = KC_SYSTEM_SLEEP,
    KC_WAKE = KC_SYSTEM_WAKE,
    KC_MSEL = 0x00AF, // KC_MEDIA_SELECT,
    KC_EJCT = KC_MEDIA_EJECT,
    KC_CALC = KC_CALCULATOR,
    KC_MYCM = KC_MY_COMPUTER,
    KC_WSCH = KC_WWW_SEARCH,
    KC_WHOM = KC_WWW_HOME,
    KC_WBAK = KC_WWW_BACK,
    KC_WFWD = KC_WWW_FORWARD,
    KC_WSTP = KC_WWW_STOP,
    KC_WREF = KC_WWW_REFRESH,
    KC_WFAV = KC_WWW_FAVORITES,
    KC_MFFD = KC_MEDIA_FAST_FORWARD,
    KC_MRWD = KC_MEDIA_REWIND,
    KC_BRIU = KC_BRIGHTNESS_UP,
    KC_BRID = KC_BRIGHTNESS_DOWN,
    KC_CPNL = KC_CONTROL_PANEL,
    KC_ASST = KC_ASSISTANT,
    KC_MCTL = KC_MISSION_CONTROL,
    KC_LPAD = KC_LAUNCHPAD,
    KC_LOPT = KC_LEFT_ALT,
    KC_LCMD = KC_LEFT_GUI,
    KC_LWIN = KC_LEFT_GUI,
    KC_ROPT = KC_RIGHT_ALT,
    KC_ALGR = KC_RIGHT_ALT,
    KC_RCMD = KC_RIGHT_GUI,
    KC_RWIN = KC_RIGHT_GUI,
};

// Keycode family checks
// IS_BASIC covers 0x00-0xCC and 0xE0-0xFF (excludes mouse range 0xCD-0xDF)
#define IS_BASIC(kc)        ((kc) < 0x00CDu || ((kc) > 0x00DFu && (kc) < 0x0100u))
#define IS_MOUSE(kc)        ((kc) >= QK_MOUSE_CURSOR_UP && (kc) <= QK_MOUSE_ACCELERATION_2)
#define IS_MOD_KEY(kc)      ((kc) >= 0x0100u && (kc) < 0x2000u)
#define IS_MOD_TAP(kc)      (((kc) & 0xE000u) == QK_MOD_TAP)
#define IS_LAYER_TAP(kc)    (((kc) & 0xF000u) == 0x4000u && ((kc) >> 12) == 4)
#define IS_MOMENTARY(kc)    (((kc) & 0xFF00u) == QK_MOMENTARY)
#define IS_TOGGLE(kc)       (((kc) & 0xFF00u) == QK_TOGGLE_LAYER)
#define IS_GOTO(kc)         (((kc) & 0xFF00u) == QK_TO)
#define IS_DEF_LAYER(kc)    (((kc) & 0xFF00u) == QK_DEF_LAYER)
#define IS_TRANSPARENT(kc)  ((kc) == KC_TRNS)
#define IS_NOOP(kc)         ((kc) == KC_NO)
#define IS_CONSUMER(kc)     ((kc) >= 0xF0u && (kc) <= 0xF6u)
#define IS_MAGIC_KEYCODE(code) ((code) >= QK_MAGIC && (code) <= QK_MAGIC_MAX)

// MOD_ → HID modifier byte bit
static inline uint8_t mod_bits_to_hid(uint8_t mod) {
    uint8_t hid = 0;
    if (mod & MOD_LCTL) hid |= 0x01u;
    if (mod & MOD_LSFT) hid |= 0x02u;
    if (mod & MOD_LALT) hid |= 0x04u;
    if (mod & MOD_LGUI) hid |= 0x08u;
    if (mod & MOD_RCTL) hid |= 0x10u;
    if (mod & MOD_RSFT) hid |= 0x20u;
    if (mod & MOD_RALT) hid |= 0x40u;
    if (mod & MOD_RGUI) hid |= 0x80u;
    return hid;
}

// QK_ modifier bits → HID modifier byte bit  (used by LCTL(kc) etc.)
static inline uint8_t qk_mods_to_hid(uint16_t kc) {
    uint16_t m = QK_MODS_GET_MODS(kc);
    uint8_t hid = 0;
    if (m & 0x01u) hid |= 0x01u; // LCTL
    if (m & 0x02u) hid |= 0x02u; // LSFT
    if (m & 0x04u) hid |= 0x04u; // LALT
    if (m & 0x08u) hid |= 0x08u; // LGUI
    if (m & 0x10u) {
        // RMODS set
        uint8_t rm = m & 0x0Fu;
        if (rm & 0x01u) hid |= 0x10u; // RCTL
        if (rm & 0x02u) hid |= 0x20u; // RSFT
        if (rm & 0x04u) hid |= 0x40u; // RALT
        if (rm & 0x08u) hid |= 0x80u; // RGUI
    }
    return hid;
}

// ── Mouse report ─────────────────────────────────────────────────────────────
struct MouseReport {
    uint8_t buttons;
    int8_t x, y;
    int8_t v, h;

    bool operator==(const MouseReport &o) const {
        return buttons == o.buttons && x == o.x && y == o.y &&
               v == o.v && h == o.h;
    }

    bool operator!=(const MouseReport &o) const { return !(*this == o); }
    bool empty() const { return buttons == 0 && x == 0 && y == 0 && v == 0 && h == 0; }
};

// ── Resolved key: what actually gets sent over HID ───────────────────────────
struct ResolvedKey {
    uint8_t hid_keycode; // 0 = none / modifier-only
    uint8_t hid_mods; // bitmask for the HID modifier byte
    uint16_t consumer; // non-zero if this is a consumer key
    MouseReport mouse; // non-empty if this is a mouse key
};

// ── LT/MT per-key hold state ──────────────────────────────────────────────────
struct TapHoldState {
    uint16_t keycode; // the LT/MT keycode
    uint8_t row, col;
    uint32_t press_time;
    bool held; // true once hold threshold exceeded
    bool tapped; // tap resolved
    bool active;
};

// ═══════════════════════════════════════════════════════════════════════════════
// §9  Tap Dance  —  corrected QMK-compatible implementation
// ═══════════════════════════════════════════════════════════════════════════════
//
// Four actions exactly matching QMK:
//
//   count=1, pressed=false  →  single tap   (tap and release within term)
//   count=2, pressed=false  →  double tap   (two taps within term)
//   count=1, pressed=true   →  hold         (held beyond term)
//   count=2, pressed=true   →  tap + hold   (tap once, then hold on 2nd press)
//
// Timer rules:
//   • Restarted on every press  → detects hold (key held longer than term)
//   • Restarted on every release → detects inter-tap gap (no 2nd tap arrived)
//
// ── Usage example ────────────────────────────────────────────────────────────
//
//   enum { TD_ESC_CAPS, TD_COUNT };
//
//   void td_esc_caps_finished(TapDanceState *s, void *) {
//       switch (s->count) {
//           case 1:
//               if (s->pressed) { /* hold  → maybe enable layer */ }
//               else            { s->resolved_kc = KC_ESC; }
//               break;
//           case 2:
//               s->resolved_kc = KC_CAPS;
//               break;
//       }
//   }
//
//   static TapDanceEntry td_entries[TD_COUNT] = {
//       [TD_ESC_CAPS] = ACTION_TAP_DANCE_FN(td_esc_caps_finished),
//   };
//
//   // In setup():
//   engine.setTapDance(td_entries, TD_COUNT);
//
//   // In keymap:
//   TD(TD_ESC_CAPS)
//
// ── Built-in helpers ─────────────────────────────────────────────────────────
//
//   // Simple: tap=KC_A, double-tap=KC_B
//   static TapDanceDoubleData my_td_data = { KC_A, KC_B };
//   ACTION_TAP_DANCE_DOUBLE(&my_td_data)
//
// ─────────────────────────────────────────────────────────────────────────────

#define QK_TAP_DANCE     0xF500u
#define TD(i)            ((uint16_t)(QK_TAP_DANCE | ((i) & 0xFFu)))
#define IS_TAP_DANCE(kc) (((kc) & 0xFF00u) == QK_TAP_DANCE)
#define TD_INDEX(kc)     ((uint8_t)((kc) & 0xFFu))
#define TAP_DANCE_TERM_MS 200u

// ── TapDanceState ─────────────────────────────────────────────────────────────
//   Passed to every callback. The callback reads count/pressed to decide
//   the action, and writes resolved_kc when the result is a plain keycode.
struct TapDanceState {
    uint8_t count; // taps counted so far (1-based)
    bool pressed; // key is physically held when on_dance_finished fires
    bool finished; // internal: dance has been resolved
    bool interrupted; // another key was pressed before the term expired
    uint32_t timer; // timestamp of last press or release event
    uint16_t resolved_kc; // set by on_dance_finished; used as the output keycode
};

using TapDanceFn = void (*)(TapDanceState *, void *);

// ── TapDanceEntry ─────────────────────────────────────────────────────────────
//   Mirrors QMK's action_tap_dance_t.
//   on_each_tap      — optional; called immediately on every tap press
//   on_dance_finished — called once the dance resolves (timeout, interrupt, or hold)
//   on_dance_reset    — called after finished; use to clean up any side-effects
//   user_data        — forwarded to every callback
//   term_ms          — per-entry override; 0 = use engine TAP_DANCE_TERM
struct TapDanceEntry {
    TapDanceFn on_each_tap;
    TapDanceFn on_dance_finished;
    TapDanceFn on_dance_reset;
    void *user_data;
    uint32_t term_ms;
};

// ── Constructor macros (QMK naming) ──────────────────────────────────────────
#define ACTION_TAP_DANCE_FN(fn) \
    { nullptr, (fn), nullptr, nullptr, 0 }

#define ACTION_TAP_DANCE_FN_ADVANCED(each_fn, finished_fn, reset_fn) \
    { (each_fn), (finished_fn), (reset_fn), nullptr, 0 }

#define ACTION_TAP_DANCE_FN_ADVANCED_WITH_STATE(each_fn, finished_fn, reset_fn, ud) \
    { (each_fn), (finished_fn), (reset_fn), (void *)(ud), 0 }

// Legacy alias kept for compatibility
#define TAP_DANCE_FN(fn) ACTION_TAP_DANCE_FN(fn)

// ── Built-in: double-tap dance ────────────────────────────────────────────────
//   tap → kc1,  double-tap → kc2
//   hold → kc1 held,  tap+hold → kc2 held
struct TapDanceDoubleData {
    uint16_t kc_tap;
    uint16_t kc_dtap;
};

static void td_double_finished(TapDanceState *s, void *ud) {
    const TapDanceDoubleData *d = (const TapDanceDoubleData *) ud;
    s->resolved_kc = (s->count >= 2) ? d->kc_dtap : d->kc_tap;
}

#define ACTION_TAP_DANCE_DOUBLE(data_ptr) \
    { nullptr, _td_double_finished, nullptr, (void *)(data_ptr), 0 }

// ── Internal per-slot state ───────────────────────────────────────────────────
//   active           — slot is in use
//   hold_active      — dance resolved as hold; key is still physically pressed;
//                      resolved_kc is kept in the report until physical release
//   tap_emit_pending — dance resolved as tap (not hold); resolved_kc will be
//                      injected into the report for exactly one buildReport cycle
struct TapDanceActive {
    uint8_t entry_index;
    uint8_t row, col;
    TapDanceState state;
    bool active;
    bool hold_active; // hold: key still pressed, kc in report
    bool tap_emit_pending; // tap:  inject kc into report once
};

// ─────────────────────────────────────────────────────────────────────────────

#define MAKE_QMK_ENGINE(km, rp, cp, dir) \
    QmkEngine<sizeof(rp)/sizeof(rp[0]), sizeof(cp)/sizeof(cp[0]), LAYER_COUNT> \
    engine(km, rp, cp, dir)

template<uint8_t ROWS, uint8_t COLS, uint8_t LAYERS>
class QmkEngine {
public:
    const uint16_t (*keymaps)[ROWS][COLS];
    const uint8_t *rowPins;
    const uint8_t *colPins;
    const DiodeDirection diodeDirection;

    uint32_t TAP_HOLD_MS = 200;
    uint32_t TAP_DANCE_TERM = TAP_DANCE_TERM_MS;

    QmkEngine(const uint16_t (*km)[ROWS][COLS],
              const uint8_t *rp,
              const uint8_t *cp,
              DiodeDirection dir = ROW_TO_COL)
        : keymaps(km), rowPins(rp), colPins(cp), diodeDirection(dir) {
        memset(&layerState, 0, sizeof(layerState));
        memset(matrixPrev, 0, sizeof(matrixPrev));
        memset(matrixCurrent, 0, sizeof(matrixCurrent));
        memset(pressedKey, 0, sizeof(pressedKey));
        memset(tapHold, 0, sizeof(tapHold));
        memset(tdActive, 0, sizeof(tdActive));
        defaultLayer = 0;
        layerState |= (1u << defaultLayer);
        tdEntries = nullptr;
        tdCount = 0;
    }

    void setTapDance(TapDanceEntry *entries, uint8_t count) {
        tdEntries = entries;
        tdCount = count;
    }

    // ── Layer API ─────────────────────────────────────────────────────────────

    void layerOn(uint8_t l) { if (l < QMK_MAX_LAYERS) layerState |= (1u << l); }

    void layerOff(uint8_t l) {
        if (l < QMK_MAX_LAYERS) layerState &= ~(1u << l);
        if (l == defaultLayer) layerState |= (1u << defaultLayer);
    }

    void layerToggle(uint8_t l) {
        if (layerState & (1u << l)) layerOff(l);
        else layerOn(l);
    }

    void layerGoto(uint8_t l) {
        layerState = (1u << l);
        defaultLayer = l;
    }

    void setDefaultLayer(uint8_t l) {
        layerOff(defaultLayer);
        defaultLayer = l;
        layerOn(l);
    }

    uint8_t highestActiveLayer() const {
        for (int8_t l = QMK_MAX_LAYERS - 1; l >= 0; --l)
            if (layerState & (1u << l)) return (uint8_t) l;
        return 0;
    }

    uint32_t getLayerState() const { return layerState; }

    // ── resolveRaw — arbitrary keycode, no row/col ────────────────────────────
    ResolvedKey resolveRaw(uint16_t kc) const {
        ResolvedKey out = {0, 0, 0, {}};
        if (kc == KC_NO || kc == KC_TRNS) return out;
        if (IS_MOD_KEY(kc)) {
            out.hid_mods = qk_mods_to_hid(kc);
            out.hid_keycode = QK_MODS_GET_BASIC_KEYCODE(kc);
            return out;
        }
        if (kc >= KC_LEFT_CTRL && kc <= KC_RIGHT_GUI) {
            out.hid_mods = 1u << (kc - KC_LEFT_CTRL);
            return out;
        }
        switch (kc) {
            case KC_AUDIO_VOL_UP: out.consumer = HID_USAGE_CONSUMER_VOLUME_INCREMENT;
                return out;
            case KC_AUDIO_VOL_DOWN: out.consumer = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
                return out;
            case KC_AUDIO_MUTE: out.consumer = HID_USAGE_CONSUMER_MUTE;
                return out;
            case KC_MEDIA_PLAY_PAUSE: out.consumer = HID_USAGE_CONSUMER_PLAY_PAUSE;
                return out;
            case KC_MEDIA_NEXT_TRACK: out.consumer = HID_USAGE_CONSUMER_SCAN_NEXT_TRACK;
                return out;
            case KC_MEDIA_PREV_TRACK: out.consumer = HID_USAGE_CONSUMER_SCAN_PREV_TRACK;
                return out;
            case KC_MEDIA_STOP: out.consumer = HID_USAGE_CONSUMER_STOP;
                return out;
            default: break;
        }
        if (IS_MOUSE(kc)) {
            resolveMouse(kc, out);
            return out;
        }
        if (kc >= 0x0080u) {
            out.consumer = kc;
            return out;
        }
        if (IS_BASIC(kc)) {
            out.hid_keycode = (uint8_t) kc;
            return out;
        }
        return out;
    }

    // ── resolveKeycode — walk layer stack top-down ────────────────────────────
    uint16_t resolveKeycode(uint8_t row, uint8_t col) const {
        for (int8_t l = QMK_MAX_LAYERS - 1; l >= 0; --l) {
            if (!(layerState & (1u << l))) continue;
            if (l >= LAYERS) continue;
            uint16_t kc = keymaps[l][row][col];
            if (kc != KC_TRNS) return kc;
        }
        return KC_NO;
    }

    // ── Matrix scan ───────────────────────────────────────────────────────────
    bool scanMatrix() {
        bool changed = false;
        if (diodeDirection == ROW_TO_COL) {
            for (uint8_t r = 0; r < ROWS; ++r) {
                digitalWrite(rowPins[r], LOW);
                delayMicroseconds(5);
                for (uint8_t c = 0; c < COLS; ++c) {
                    bool p = (digitalRead(colPins[c]) == LOW);
                    if (p != matrixCurrent[r][c]) {
                        matrixCurrent[r][c] = p;
                        changed = true;
                    }
                }
                digitalWrite(rowPins[r], HIGH);
            }
        } else {
            for (uint8_t c = 0; c < COLS; ++c) {
                digitalWrite(colPins[c], LOW);
                delayMicroseconds(5);
                for (uint8_t r = 0; r < ROWS; ++r) {
                    bool p = (digitalRead(rowPins[r]) == LOW);
                    if (p != matrixCurrent[r][c]) {
                        matrixCurrent[r][c] = p;
                        changed = true;
                    }
                }
                digitalWrite(colPins[c], HIGH);
            }
        }
        return changed;
    }

    // ── NKRO report ───────────────────────────────────────────────────────────
    struct NkroReport {
        uint8_t mods;
        uint8_t bitmap[32];

        bool operator==(const NkroReport &o) const {
            return mods == o.mods && memcmp(bitmap, o.bitmap, 32) == 0;
        }

        bool operator!=(const NkroReport &o) const { return !(*this == o); }
    };

    // ── buildReport ───────────────────────────────────────────────────────────
    //
    //  Pass 1: detect press/release edges
    //    • On press of a non-TD key  → interrupt any pending (unresolved) TD
    //    • On press of a TD key      → increment tap count, restart timer
    //    • On release of a TD key    → restart inter-tap timer (don't resolve yet)
    //    • On release of a hold TD   → clear hold_active slot
    //
    //  Pass 2: tick timers
    //    • tap-hold: fire hold when threshold exceeded while pressed
    //    • tap-dance: fire on_dance_finished when term exceeded
    //                 either pressed (hold) or not (taps done)
    //
    //  Pass 3: collect active keys
    //    • Skip TD keycodes — output comes from hold_active / tap_emit_pending
    //    • Emit hold_active TD resolved_kc while key held
    //    • Emit tap_emit_pending TD resolved_kc for exactly one cycle
    //
    void handleBootMagick(const uint16_t kc) {
        if (kc == QK_MAGIC_BOOTLOADER)
            enterUf2Dfu();
    }

    void buildReport(NkroReport &out, MouseReport &out_mouse, uint16_t &out_consumer) {
        out.mods = 0;
        out_consumer = 0;
        out_mouse = {};
        memset(out.bitmap, 0, sizeof(out.bitmap));

        uint32_t now = millis();

        // ── Pass 1: press/release edges ───────────────────────────────────
        for (uint8_t r = 0; r < ROWS; ++r) {
            for (uint8_t c = 0; c < COLS; ++c) {
                bool cur = matrixCurrent[r][c];
                bool prev = matrixPrev[r][c];
                if (cur == prev) continue;

                if (cur) {
                    // ── Key press ──────────────────────────────────────────
                    uint16_t kc = resolveKeycode(r, c);
                    pressedKey[r][c] = kc;

                    if (IS_MAGIC_KEYCODE(kc)) {
                        handleBootMagick(kc);
                    }

                    if (IS_TAP_DANCE(kc)) {
                        // Don't interrupt — this IS a tap-dance press
                        handleTdPress(r, c, kc, now);
                    } else {
                        // Any non-TD key interrupts pending (unresolved) TD dances
                        interruptTapDances(now);

                        if (IS_MOMENTARY(kc)) layerOn(QK_MOMENTARY_GET_LAYER(kc));
                        else if (IS_TOGGLE(kc)) layerToggle(QK_TOGGLE_LAYER_GET_LAYER(kc));
                        else if (IS_GOTO(kc)) layerGoto(QK_TO_GET_LAYER(kc));
                        else if (IS_DEF_LAYER(kc)) setDefaultLayer(QK_DEF_LAYER_GET_LAYER(kc));
                        else if (IS_LAYER_TAP(kc) || IS_MOD_TAP(kc))
                            startTapHold(r, c, kc, now);
                    }
                } else {
                    // ── Key release ────────────────────────────────────────
                    uint16_t kc = pressedKey[r][c];
                    pressedKey[r][c] = KC_NO;

                    if (IS_TAP_DANCE(kc)) {
                        handleTdRelease(r, c, now);
                    } else if (IS_MOMENTARY(kc)) {
                        layerOff(QK_MOMENTARY_GET_LAYER(kc));
                    } else if (IS_LAYER_TAP(kc) || IS_MOD_TAP(kc)) {
                        releaseTapHold(r, c, now);
                    }
                }
            }
        }

        // Sync prev matrix
        for (uint8_t r = 0; r < ROWS; ++r)
            for (uint8_t c = 0; c < COLS; ++c)
                matrixPrev[r][c] = matrixCurrent[r][c];

        // ── Pass 2: tick timers ───────────────────────────────────────────
        tickTapHold(now);
        tickTapDance(now);

        // ── Pass 3a: collect active physical keys ─────────────────────────
        uint8_t mouse_accel = 1;
        for (uint8_t r = 0; r < ROWS; ++r) {
            for (uint8_t c = 0; c < COLS; ++c) {
                if (!matrixCurrent[r][c]) continue;
                uint16_t kc = pressedKey[r][c];
                if (kc == KC_NO || kc == KC_TRNS) continue;
                if (IS_MOMENTARY(kc) || IS_TOGGLE(kc) ||
                    IS_GOTO(kc) || IS_DEF_LAYER(kc))
                    continue;

                if (kc == QK_MOUSE_ACCELERATION_0) {
                    mouse_accel = MOUSE_ACL0_MUL;
                    continue;
                }
                if (kc == QK_MOUSE_ACCELERATION_1) {
                    mouse_accel = MOUSE_ACL1_MUL;
                    continue;
                }
                if (kc == QK_MOUSE_ACCELERATION_2) {
                    mouse_accel = MOUSE_ACL2_MUL;
                    continue;
                }

                // TD keys output via hold_active (see Pass 3b)
                if (IS_TAP_DANCE(kc)) {
                    TapDanceActive *td = findTd(r, c);
                    if (td && td->hold_active && td->state.resolved_kc) {
                        addResolvedToReport(resolveRaw(td->state.resolved_kc),
                                            out, out_mouse, out_consumer);
                    }
                    continue;
                }

                ResolvedKey rk = resolveToHid(r, c, kc, now);
                addResolvedToReport(rk, out, out_mouse, out_consumer);
            }
        }

        // ── Pass 3b: inject one-shot tap emissions ────────────────────────
        //   These slots have already been physically released but resolved to
        //   a keycode that needs exactly one report cycle to register.
        for (uint8_t i = 0; i < TD_SLOTS; ++i) {
            TapDanceActive &td = tdActive[i];
            if (!td.tap_emit_pending) continue;
            td.tap_emit_pending = false; // consume — emit once only

            if (td.state.resolved_kc) {
                addResolvedToReport(resolveRaw(td.state.resolved_kc),
                                    out, out_mouse, out_consumer);
            }
            // Slot fully done — was already marked inactive in finishTd
        }

        // Apply mouse acceleration
        out_mouse.x = (int8_t) constrain((int)out_mouse.x * mouse_accel, -127, 127);
        out_mouse.y = (int8_t) constrain((int)out_mouse.y * mouse_accel, -127, 127);
    }

    // Public: written by external debounce
    bool matrixCurrent[ROWS][COLS]{};

private:
    uint32_t layerState{};
    uint8_t defaultLayer{};
    bool matrixPrev[ROWS][COLS]{};

    // Stores keycode at press time — fixes MO release bug
    uint16_t pressedKey[ROWS][COLS]{};

    // ── Helper: add a ResolvedKey into the running report ─────────────────────
    static void addResolvedToReport(const ResolvedKey &rk,
                                    NkroReport &out,
                                    MouseReport &out_mouse,
                                    uint16_t &out_consumer) {
        if (!rk.mouse.empty()) {
            out_mouse.buttons |= rk.mouse.buttons;
            out_mouse.x = (int8_t) constrain((int)out_mouse.x + rk.mouse.x, -127, 127);
            out_mouse.y = (int8_t) constrain((int)out_mouse.y + rk.mouse.y, -127, 127);
            out_mouse.v = (int8_t) constrain((int)out_mouse.v + rk.mouse.v, -127, 127);
            out_mouse.h = (int8_t) constrain((int)out_mouse.h + rk.mouse.h, -127, 127);
        } else if (rk.consumer) {
            out_consumer = rk.consumer;
        } else {
            out.mods |= rk.hid_mods;
            if (rk.hid_keycode && rk.hid_keycode < 0xE0u)
                out.bitmap[rk.hid_keycode >> 3] |= (1u << (rk.hid_keycode & 7u));
        }
    }

    // ── Tap-hold ──────────────────────────────────────────────────────────────
    static constexpr uint8_t TH_SLOTS = 8;
    TapHoldState tapHold[TH_SLOTS]{};

    TapHoldState *findTapHold(uint8_t r, uint8_t c) {
        for (uint8_t i = 0; i < TH_SLOTS; ++i)
            if (tapHold[i].active && tapHold[i].row == r && tapHold[i].col == c)
                return &tapHold[i];
        return nullptr;
    }

    TapHoldState *freeTapHold() {
        for (uint8_t i = 0; i < TH_SLOTS; ++i)
            if (!tapHold[i].active) return &tapHold[i];
        return nullptr;
    }

    void startTapHold(uint8_t r, uint8_t c, uint16_t kc, uint32_t now) {
        TapHoldState *s = freeTapHold();
        if (!s) return;
        s->active = true;
        s->held = false;
        s->tapped = false;
        s->row = r;
        s->col = c;
        s->keycode = kc;
        s->press_time = now;
    }

    void releaseTapHold(uint8_t r, uint8_t c, uint32_t now) {
        TapHoldState *s = findTapHold(r, c);
        if (!s) return;
        if (!s->held) s->tapped = true;
        s->active = false;
        if (IS_LAYER_TAP(s->keycode) && s->held)
            layerOff(QK_LAYER_TAP_GET_LAYER(s->keycode));
    }

    void tickTapHold(uint32_t now) {
        for (uint8_t i = 0; i < TH_SLOTS; ++i) {
            TapHoldState &s = tapHold[i];
            if (!s.active || s.held) continue;
            if ((now - s.press_time) >= TAP_HOLD_MS) {
                s.held = true;
                if (IS_LAYER_TAP(s.keycode))
                    layerOn(QK_LAYER_TAP_GET_LAYER(s.keycode));
            }
        }
    }

    // ── Tap dance ─────────────────────────────────────────────────────────────
    static constexpr uint8_t TD_SLOTS = 4;
    TapDanceActive tdActive[TD_SLOTS]{};
    TapDanceEntry *tdEntries{};
    uint8_t tdCount{};

    TapDanceActive *findTd(uint8_t r, uint8_t c) {
        for (uint8_t i = 0; i < TD_SLOTS; ++i)
            if (tdActive[i].active && tdActive[i].row == r && tdActive[i].col == c)
                return &tdActive[i];
        return nullptr;
    }

    // Also search inactive slots with tap_emit_pending (already released but
    // still waiting to inject one report cycle)
    TapDanceActive *findTdEmitting(uint8_t r, uint8_t c) {
        for (uint8_t i = 0; i < TD_SLOTS; ++i)
            if (!tdActive[i].active && tdActive[i].tap_emit_pending
                && tdActive[i].row == r && tdActive[i].col == c)
                return &tdActive[i];
        return nullptr;
    }

    TapDanceActive *freeTd() {
        for (uint8_t i = 0; i < TD_SLOTS; ++i)
            if (!tdActive[i].active && !tdActive[i].tap_emit_pending
                && !tdActive[i].hold_active)
                return &tdActive[i];
        return nullptr;
    }

    // Called on every press of a TD key.
    void handleTdPress(uint8_t r, uint8_t c, uint16_t kc, uint32_t now) {
        if (!tdEntries) return;
        uint8_t idx = TD_INDEX(kc);
        if (idx >= tdCount) return;

        TapDanceActive *td = findTd(r, c);
        if (!td) {
            td = freeTd();
            if (!td) return;
        }

        if (!td->active) {
            // Fresh dance
            td->active = true;
            td->hold_active = false;
            td->tap_emit_pending = false;
            td->entry_index = idx;
            td->row = r;
            td->col = c;
            td->state = {0, false, false, false, now, 0};
        }

        td->state.count++;
        td->state.pressed = true;
        td->state.timer = now; // restart: hold detection starts from this press

        TapDanceEntry &e = tdEntries[idx];
        if (e.on_each_tap) e.on_each_tap(&td->state, e.user_data);
    }

    // Called on every release of a TD key.
    void handleTdRelease(uint8_t r, uint8_t c, uint32_t now) {
        TapDanceActive *td = findTd(r, c);
        if (!td) return;

        if (td->hold_active) {
            // The dance already resolved as a hold; key is now released — done.
            td->hold_active = false;
            td->active = false;
            return;
        }

        // Dance not yet resolved: key released, restart timer for inter-tap gap.
        td->state.pressed = false;
        td->state.timer = now;
        // tickTapDance will fire on_dance_finished when the inter-tap gap expires.
    }

    // Interrupt all pending (unresolved) TD dances.
    // Called when any non-TD key is pressed.
    void interruptTapDances(uint32_t now) {
        for (uint8_t i = 0; i < TD_SLOTS; ++i) {
            TapDanceActive &td = tdActive[i];
            if (!td.active || td.state.finished || td.hold_active) continue;
            td.state.interrupted = true;
            finishTd(&td, now);
        }
    }

    // Execute layer keycodes (TO/MO/TG/DF) that cannot go through HID pipeline.
    // Returns true if kc was a layer command and was handled.
    bool handleLayerKeycode(uint16_t kc) {
        if (IS_GOTO(kc)) {
            layerGoto(QK_TO_GET_LAYER(kc));
            return true;
        }
        if (IS_MOMENTARY(kc)) {
            layerOn(QK_MOMENTARY_GET_LAYER(kc));
            return true;
        }
        if (IS_TOGGLE(kc)) {
            layerToggle(QK_TOGGLE_LAYER_GET_LAYER(kc));
            return true;
        }
        if (IS_DEF_LAYER(kc)) {
            setDefaultLayer(QK_DEF_LAYER_GET_LAYER(kc));
            return true;
        }
        return false;
    }

    // Resolve a dance: call callbacks, then dispatch resolved_kc.
    //
    // resolved_kc dispatch priority:
    //   1. Layer command (TO/MO/TG/DF)  → execute immediately, no HID emission
    //   2. Held HID keycode             → hold_active until physical release
    //   3. Tapped HID keycode           → tap_emit_pending for one report cycle
    void finishTd(TapDanceActive *td, uint32_t /*now*/) {
        if (!td->active || td->state.finished) return;
        td->state.finished = true;

        TapDanceEntry &e = tdEntries[td->entry_index];
        if (e.on_dance_finished) e.on_dance_finished(&td->state, e.user_data);
        if (e.on_dance_reset) e.on_dance_reset(&td->state, e.user_data);

        uint16_t kc = td->state.resolved_kc;

        if (!kc) {
            // Callback handled everything itself (e.g. called layerOn directly)
            td->active = false;
            return;
        }

        // ── Layer commands: execute now, nothing to emit over HID ────────────
        if (handleLayerKeycode(kc)) {
            td->state.resolved_kc = 0;
            td->active = false;
            return;
        }

        // ── HID keycode ───────────────────────────────────────────────────────
        if (td->state.pressed) {
            // Hold or tap+hold: keep in report while key physically held
            td->hold_active = true;
        } else {
            // Tap or double-tap: inject into exactly one report cycle
            td->tap_emit_pending = true;
            td->active = false;
        }
    }

    // Per-tick: expire inter-tap gap or hold threshold.
    void tickTapDance(uint32_t now) {
        if (!tdEntries) return;
        for (uint8_t i = 0; i < TD_SLOTS; ++i) {
            TapDanceActive &td = tdActive[i];
            if (!td.active || td.state.finished || td.hold_active) continue;

            TapDanceEntry &e = tdEntries[td.entry_index];
            uint32_t term = e.term_ms ? e.term_ms : TAP_DANCE_TERM;

            if ((now - td.state.timer) >= term) {
                // Timer expired.
                //   pressed=true  → key has been held beyond term → hold action
                //   pressed=false → inter-tap gap elapsed → taps are done
                finishTd(&td, now);
            }
        }
    }

    // ── resolveToHid ──────────────────────────────────────────────────────────
    ResolvedKey resolveToHid(uint8_t r, uint8_t c, uint16_t kc, uint32_t now) {
        ResolvedKey out = {0, 0, 0, {}};
        (void) now;

        if (IS_LAYER_TAP(kc)) {
            TapHoldState *s = findTapHold(r, c);
            if (s && s->tapped) out.hid_keycode = QK_LAYER_TAP_GET_TAP_KEYCODE(kc);
            return out;
        }
        if (IS_MOD_TAP(kc)) {
            TapHoldState *s = findTapHold(r, c);
            if (s && s->tapped) out.hid_keycode = QK_MOD_TAP_GET_TAP_KEYCODE(kc);
            else if (!s || s->held) out.hid_mods = mod_bits_to_hid(QK_MOD_TAP_GET_MODS(kc));
            return out;
        }
        if (IS_MOD_KEY(kc)) {
            out.hid_mods = qk_mods_to_hid(kc);
            out.hid_keycode = QK_MODS_GET_BASIC_KEYCODE(kc);
            return out;
        }
        if (kc >= KC_LEFT_CTRL && kc <= KC_RIGHT_GUI) {
            out.hid_mods = 1u << (kc - KC_LEFT_CTRL);
            return out;
        }
        switch (kc) {
            case KC_AUDIO_VOL_UP: out.consumer = HID_USAGE_CONSUMER_VOLUME_INCREMENT;
                return out;
            case KC_AUDIO_VOL_DOWN: out.consumer = HID_USAGE_CONSUMER_VOLUME_DECREMENT;
                return out;
            case KC_AUDIO_MUTE: out.consumer = HID_USAGE_CONSUMER_MUTE;
                return out;
            case KC_MEDIA_PLAY_PAUSE: out.consumer = HID_USAGE_CONSUMER_PLAY_PAUSE;
                return out;
            case KC_MEDIA_NEXT_TRACK: out.consumer = HID_USAGE_CONSUMER_SCAN_NEXT_TRACK;
                return out;
            case KC_MEDIA_PREV_TRACK: out.consumer = HID_USAGE_CONSUMER_SCAN_PREV_TRACK;
                return out;
            case KC_MEDIA_STOP: out.consumer = HID_USAGE_CONSUMER_STOP;
                return out;
            default: break;
        }
        if (IS_BASIC(kc)) {
            out.hid_keycode = (uint8_t) kc;
            return out;
        }
        if (IS_MOUSE(kc)) {
            resolveMouse(kc, out);
            return out;
        }
        return out;
    }

    static void resolveMouse(uint16_t kc, ResolvedKey &out) {
        switch (kc) {
            case QK_MOUSE_CURSOR_UP: out.mouse.y = -MOUSE_MOVE_STEP;
                break;
            case QK_MOUSE_CURSOR_DOWN: out.mouse.y = MOUSE_MOVE_STEP;
                break;
            case QK_MOUSE_CURSOR_LEFT: out.mouse.x = -MOUSE_MOVE_STEP;
                break;
            case QK_MOUSE_CURSOR_RIGHT: out.mouse.x = MOUSE_MOVE_STEP;
                break;
            case QK_MOUSE_WHEEL_UP: out.mouse.v = MOUSE_WHEEL_STEP;
                break;
            case QK_MOUSE_WHEEL_DOWN: out.mouse.v = -MOUSE_WHEEL_STEP;
                break;
            case QK_MOUSE_WHEEL_LEFT: out.mouse.h = -MOUSE_WHEEL_STEP;
                break;
            case QK_MOUSE_WHEEL_RIGHT: out.mouse.h = MOUSE_WHEEL_STEP;
                break;
            default:
                if (kc >= QK_MOUSE_BUTTON_1 && kc <= QK_MOUSE_BUTTON_8)
                    out.mouse.buttons |= (1u << (kc - QK_MOUSE_BUTTON_1));
                break;
        }
    }

public:
    // ── InputActivity ─────────────────────────────────────────────────────────────
    struct InputActivity {
        bool anyActive = false;
        bool matrix[ROWS][COLS]{};

        bool encoderCW{};
        bool encoderCCW{};
        bool encoderBtn{};
    };

    InputActivity getInputActivity(
        const int8_t encDelta,
        const int8_t encDetent,
        const bool encBtnSettled
    ) const {
        InputActivity act = {};

        for (uint8_t r = 0; r < ROWS; ++r) {
            for (uint8_t c = 0; c < COLS; ++c) {
                act.matrix[r][c] = matrixCurrent[r][c];
                if (matrixCurrent[r][c])
                    act.anyActive = true;
            }
        }

        act.encoderCW = (encDelta >= encDetent);
        act.encoderCCW = (encDelta <= -encDetent);
        act.encoderBtn = (encBtnSettled == LOW);

        if (act.encoderCW || act.encoderCCW || act.encoderBtn)
            act.anyActive = true;

        return act;
    }
};
