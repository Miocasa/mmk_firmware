#pragma once
#include "Adafruit_SSD1306.h"
#include <Arduino.h>

#include "oled_icons.h"


inline void drawDirectedCircle(Adafruit_SSD1306 *display, const int16_t x, const int16_t y, const uint16_t color, const bool is_left) {


    display->drawLine(
        static_cast<int16_t>(is_left ? (x + 5) : (x + 8) ),
        y,
        static_cast<int16_t>(is_left ? (x + 7) : (x + 6) ),
        static_cast<int16_t>(y + 2),
        color
    );
    display->drawLine(
        static_cast<int16_t>(is_left ? (x + 5) : (x + 8) ),
        y,
        static_cast<int16_t>(is_left ? (x + 7) : (x + 6) ),
        static_cast<int16_t>(y - 2),
        color
    );
    display->drawRoundRect(x, y, 14, 14, 7, SSD1306_WHITE);
}

inline void drawIcon(Adafruit_SSD1306 *display, const int16_t x, const int16_t y, const uint8_t icon, const uint16_t color) {
    if (icon == ICONS_COUNT) return;

    display->drawBitmap(
        static_cast<int16_t>(x + 1),
        static_cast<int16_t>(y + 1),
        ICONS[icon],
        ICONS_WIDTH,
        ICONS_HEIGHT,
        color
    );
}

// Inside of keys
// inline void drawBrush(Adafruit_SSD1306 *display, const int16_t x, const int16_t y, const uint16_t color) {
//
//
// }