// PowerManager.h
#pragma once

#include <Arduino.h>
#include <bluefruit.h>

#include "structs.h"

// ─── Power states ─────────────────────────────────────────────────────────────
//
//  ACTIVE       — full speed, all peripherals on, display on
//  DIM          — display dimmed, matrix scan at full rate, BLE/USB active
//  IDLE         — display off, matrix scan throttled (every 20ms), BLE/USB active
//  LIGHT_SLEEP  — display off, matrix scan slow (every 50ms), CPU WFI between scans
//  DEEP_SLEEP   — display off, BLE advertising only, matrix via GPIO wakeup interrupt
//                 CPU in SD_APP_EVT_WAIT_FOR_EVENT (SoftDevice managed sleep)

enum PowerState : uint8_t {
    PWR_ACTIVE = 0,
    PWR_DIM = 1,
    PWR_IDLE = 2,
    PWR_LIGHT_SLEEP = 3,
    PWR_DEEP_SLEEP = 4,
};

// ─── Callbacks ────────────────────────────────────────────────────────────────
using PowerStateCallback = void (*)(PowerState prev, PowerState next);

// Timeouts: ms of inactivity before transitioning to each state.
// Pass 0 to disable a level.
struct PowerManagerConfig {
    uint32_t dim_ms = 5000; // 5s -> DIM
    uint32_t idle_ms = 15000; // 15s -> IDLE
    uint32_t light_sleep_ms = 30000; // 30s -> LIGHT_SLEEP
    uint32_t deep_sleep_ms = 60000; // 60s -> DEEP_SLEEP

    PowerManagerConfig(
        uint32_t dim = 5000,
        uint32_t idle = 15000,
        uint32_t light_sleep = 30000,
        uint32_t deep_sleep = 60000
    ) : dim_ms(dim), idle_ms(idle),
        light_sleep_ms(light_sleep), deep_sleep_ms(deep_sleep) {
    }
};

// ─── PowerManager ─────────────────────────────────────────────────────────────
class PowerManager {
public:
    explicit PowerManager(const PowerManagerConfig &cfg = {});

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    // Call once in setup() — registers GPIO wakeup pins for deep sleep
    void begin(const uint8_t *col_pins, uint8_t col_count,
               const uint8_t *row_pins, uint8_t row_count,
               EncoderMap *enc = nullptr);

    // Call every loop() iteration — drives the state machine
    void tick();

    // ── Activity ──────────────────────────────────────────────────────────────
    // Call whenever any user input is detected (key press, encoder, etc.)
    void reportActivity();

    // ── State access ──────────────────────────────────────────────────────────
    PowerState getState() const { return _state; }
    bool isAsleep() const { return _state >= PWR_LIGHT_SLEEP; }
    bool isDimmed() const { return _state >= PWR_DIM; }

    // Returns true once after a wakeup from sleep (cleared on read)
    bool consumeWakeup();

    // ── Manual control ────────────────────────────────────────────────────────
    void forceState(PowerState s);

    void forceWake();

    // ── Callback ──────────────────────────────────────────────────────────────
    // Called on every state transition (may be null)
    void setCallback(PowerStateCallback cb) { _cb = cb; }

    // ── Scan interval hint ────────────────────────────────────────────────────
    // Returns recommended matrix scan interval in ms for current state
    uint32_t scanIntervalMs() const;

private:
    PowerManagerConfig _cfg;
    PowerState _state = PWR_ACTIVE;
    uint32_t _lastActivity = 0;
    bool _wakeupFlag = false;
    PowerStateCallback _cb = nullptr;

    // GPIO wakeup config
    const uint8_t *_colPins = nullptr;
    const uint8_t *_rowPins = nullptr;
    uint8_t _colCount = 0;
    uint8_t _rowCount = 0;
    EncoderMap *_encoder = nullptr;


    void _transition(PowerState next);

    void _enterDeepSleep();

    void _configWakeupPins();
};
