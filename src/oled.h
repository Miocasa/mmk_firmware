
#ifndef NRF52MACROPAD_OLED_H
#define NRF52MACROPAD_OLED_H

#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <Fonts/Picopixel.h>

#include "keymap.hpp"
#include "oled_bitmaps.h"
#include "oled_helpers.hpp"


inline void oled_task_kb(Adafruit_SSD1306 *display, const uint8_t layer) {


#define LAYER_TILE_WIDTH  46
#define LAYER_TILE_HEIGHT 48
#define LAYER_TILE_X     ((display->width() - LAYER_TILE_WIDTH) / 2)
#define LAYER_TILE_Y      0

    const char* LayerNames[] = {
        "Media",
        "Krita",
        "Krita Alt",
        "Pixelorama",
        "Functions"
    };

    static uint8_t  prev_layer      = LAYER_COUNT;
    static uint8_t  current_frame   = 0;
    static uint32_t last_frame_time = 0;
    static bool     reverce         = false;

    #define FPS_TO_MS(n) (1000 / n)

    enum AnimationTypes : uint8_t {
        CYCLE_LOOP,
        PING_PONG,
        NO_LOOP,
        NOT_ANIMATED
    };

    struct AnimationParams {
        AnimationTypes anim_type;
        uint16_t       frame_delay;
        uint8_t        frame_count;
        uint8_t        key_icons[4][3];
    };

    constexpr AnimationParams LAYER_ANIM[LAYER_COUNT] = {
        [MEDIA]        = {
            PING_PONG,
            FPS_TO_MS(4),
            media_layer_anim_frames,
            {

                { ICON_PLAY_PAUSE,   ICON_VOL_DOWN,     ICON_VOL_UP},
                { ICON_PREV_TRACK,   ICON_MUTE,         ICON_NEXT_TRACK},
                { ICON_REWIND_TRACK, ICON_PLAY_PAUSE,   ICON_FAST_FORWARD_TRACK},
                { ICON_TAP_DANCE,       ICON_UNDO,         ICON_REDO}
            }
        },
        [KRITA]        = {
            NO_LOOP,
            FPS_TO_MS(5),
            krita_layer_anim_frames,
{
            { ICONS_COUNT, ICON_PLUS,  ICON_MINUS},
            { ICON_BRUSH,  ICONS_COUNT, ICON_COLOR_PICKER},
            { ICON_MOVE_CANVA, ICON_BACKED, ICON_GROUP},
            { ICON_TAP_DANCE, ICONS_COUNT, ICONS_COUNT}
            }
        },
        [KRITA_SECOND] = {
            NO_LOOP,
            FPS_TO_MS(4),
            krita_alt_layer_anim_frames,
{
            { ICONS_COUNT, ICONS_COUNT, ICONS_COUNT},
            { ICONS_COUNT, ICONS_COUNT, ICONS_COUNT},
            { ICON_LAYER_UP, ICON_LAYER_DOWN, ICON_UNDO},
            { ICON_TAP_DANCE, ICONS_COUNT, ICONS_COUNT}
            }
        },
        [PIXELORAMA]   = {
            NO_LOOP,
            FPS_TO_MS(5),
            hornet_layer_anim_frames,
{
                { ICONS_COUNT, ICONS_COUNT, ICONS_COUNT},
                { ICON_BRUSH,  ICONS_COUNT, ICON_COLOR_PICKER},
                { ICON_BACKED, ICON_SELECT_RECTANGLE, ICON_UNDO},
                { ICON_TAP_DANCE, ICONS_COUNT, ICONS_COUNT}
            }
        },
        [FUNC]         = {
            CYCLE_LOOP,
            FPS_TO_MS(8),
            func_layer_anim_frames,
{
                { ICONS_COUNT, ICONS_COUNT, ICONS_COUNT},
                { ICONS_COUNT, ICONS_COUNT, ICONS_COUNT},
                { ICONS_COUNT, ICONS_COUNT, ICONS_COUNT},
                { ICON_TAP_DANCE, ICONS_COUNT, ICONS_COUNT}
            }
        }
    };

    const AnimationParams &anim = LAYER_ANIM[layer];

    bool layer_changed = (prev_layer != layer);
    if (layer_changed) {
        if (prev_layer == KRITA_SECOND && layer == KRITA) {
            current_frame   = krita_layer_anim_frames - 1;
        } else {
            current_frame   = 0;
        }
        reverce         = false;
        prev_layer      = layer;
        last_frame_time = millis();
    }

    uint32_t now         = millis();
    bool     need_redraw = layer_changed;

    if (anim.anim_type != NOT_ANIMATED && (now - last_frame_time >= anim.frame_delay)) {
        switch (anim.anim_type) {
            case CYCLE_LOOP:
                current_frame = (current_frame + 1) % anim.frame_count;
                break;
            case PING_PONG:
                if (!reverce) {
                    if (current_frame + 1 >= anim.frame_count) reverce = true;
                    else current_frame++;
                } else {
                    if (current_frame == 0) reverce = false;
                    else current_frame--;
                }
                break;
            case NO_LOOP:
                if (current_frame + 1 < anim.frame_count)
                    current_frame++;
                break;
            default: break;
        }
        last_frame_time = now;
        need_redraw     = true;
    }

    if (!need_redraw) return;

    display->clearDisplay();

    switch (layer) {
        case MEDIA:
            display->drawBitmap(LAYER_TILE_X, LAYER_TILE_Y,
                                media_layer_anim[current_frame],
                                LAYER_TILE_WIDTH, LAYER_TILE_HEIGHT,
                                SSD1306_WHITE);
            break;
        case KRITA:
            display->drawBitmap(LAYER_TILE_X, LAYER_TILE_Y,
                                krita_layer_anim[current_frame],
                                LAYER_TILE_WIDTH, LAYER_TILE_HEIGHT,
                                SSD1306_WHITE);
            break;
        case KRITA_SECOND:
            display->drawBitmap(LAYER_TILE_X, LAYER_TILE_Y,
                                krita_alt_layer_anim[current_frame],
                                LAYER_TILE_WIDTH, LAYER_TILE_HEIGHT,
                                SSD1306_WHITE);
            break;
        case PIXELORAMA:
            display->drawBitmap(LAYER_TILE_X, LAYER_TILE_Y,
                                hornet_layer_anim[current_frame],
                                LAYER_TILE_WIDTH, LAYER_TILE_HEIGHT,
                                SSD1306_WHITE);
            break;
        case FUNC:
            display->drawBitmap(LAYER_TILE_X, LAYER_TILE_Y,
                                func_layer_anim[current_frame],
                                LAYER_TILE_WIDTH, LAYER_TILE_HEIGHT,
                                SSD1306_WHITE);
            break;
        default: break;
    }

    display->drawLine(8, 50, 56, 50, SSD1306_WHITE);



    display->setFont(&Picopixel);
    int16_t  tx, ty;
    uint16_t tw, th;
    display->getTextBounds(LayerNames[layer], 0, 0, &tx, &ty, &tw, &th);
    display->setCursor((display->width() - tw) / 2, 56);
    display->print(LayerNames[layer]);


#define KEYS_ART_POS_X 8
#define KEYS_ART_POS_Y 60

    // constexpr int16_t y = 60;
    constexpr uint16_t color = SSD1306_WHITE;

    display->drawRoundRect(
        KEYS_ART_POS_X,
        KEYS_ART_POS_Y,
        14,
        14,
        2,
        SSD1306_WHITE
    );
    drawIcon(
        display,
        KEYS_ART_POS_X,
        KEYS_ART_POS_Y,
        LAYER_ANIM[layer].key_icons[0][0],
        SSD1306_WHITE
    );

    drawDirectedCircle(display, KEYS_ART_POS_X + 16,    KEYS_ART_POS_Y, color, true);
    drawIcon(
        display,
        KEYS_ART_POS_X + 16,
        KEYS_ART_POS_Y,
        LAYER_ANIM[layer].key_icons[0][1],
        SSD1306_WHITE
    );

    drawDirectedCircle(display, KEYS_ART_POS_X + 32,    KEYS_ART_POS_Y, color, false);
    drawIcon(
        display,
        KEYS_ART_POS_X + 32,
        KEYS_ART_POS_Y,
        LAYER_ANIM[layer].key_icons[0][2],
        SSD1306_WHITE
    );


    for (uint8_t i = 0; i < 3; i++) {
        for (uint8_t j = 0; j < 3; j++) {
            drawIcon(
                display,
                KEYS_ART_POS_X + 16 * i,
                KEYS_ART_POS_Y + 16 * (j + 1),
                LAYER_ANIM[layer].key_icons[j + 1][i],
                SSD1306_WHITE
            );
            display->drawRoundRect(
                KEYS_ART_POS_X + 16 * i,
                KEYS_ART_POS_Y + 16 * (j + 1),
                14, 14, 2, SSD1306_WHITE);
        }
    }



    display->display();
}

#endif //NRF52MACROPAD_OLED_H