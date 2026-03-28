#ifndef POCKETDOT_CONFIG_H
#define POCKETDOT_CONFIG_H
#include <Arduino.h>
#include "macros.h"

/** ====== Main configs ====== **/
#define DEBUG

/** ====== Keyboard parameters ====== **/

struct ButtonPin {
    uint8_t pin;
    uint32_t bit;
};

/** ====== BLE parameters ====== **/
const uint8_t pnp_id[] = {
    0x02,
    0xDA, 0x01,
    0x01, 0x00,
    0x00, 0x01
};

