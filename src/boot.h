#ifndef CODE_SAFE_BOOT_H
#define CODE_SAFE_BOOT_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>

bool bootAnimation(Adafruit_SSD1306 *display);

constexpr uint32_t BOOT_TIMEOUT = 1000 / 5;

#endif //CODE_SAFE_BOOT_H