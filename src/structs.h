#ifndef NRF52MACROPAD_STRUCTS_H
#define NRF52MACROPAD_STRUCTS_H

#include <Arduino.h>

struct EncoderMap {
    uint16_t cw;
    uint16_t ccw;
    uint16_t btn;
};

struct Encoder {
    uint16_t PIN_BUTTON;
    uint16_t PIN_A;
    uint16_t PIN_B;
};

#endif //NRF52MACROPAD_STRUCTS_H
