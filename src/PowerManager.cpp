// PowerManager.cpp
#include "PowerManager.h"

// nRF52 SoftDevice / NVIC headers для WFI и GPIO sense
#include <nrf_gpio.h>
#include <nrf_soc.h>       // sd_app_evt_wait
#include <nrf_power.h>

PowerManager::PowerManager(const PowerManagerConfig &cfg) : _cfg(cfg) {
}

// ─── begin ────────────────────────────────────────────────────────────────────

void PowerManager::begin(const uint8_t *col_pins, uint8_t col_count,
                         int encoder_btn_pin) {
    _colPins = col_pins;
    _colCount = col_count;
    _encBtnPin = encoder_btn_pin;
    _lastActivity = millis();
}

// ─── tick ─────────────────────────────────────────────────────────────────────

void PowerManager::tick() {
    const uint32_t now = millis();
    const uint32_t idle_ms = now - _lastActivity;

    // Walk state machine — only forward (deeper sleep) unless woken
    switch (_state) {
        case PWR_ACTIVE:
            if (_cfg.dim_ms && idle_ms >= _cfg.dim_ms)
                _transition(PWR_DIM);
            break;

        case PWR_DIM:
            if (_cfg.idle_ms && idle_ms >= _cfg.idle_ms)
                _transition(PWR_IDLE);
            break;

        case PWR_IDLE:
            if (_cfg.light_sleep_ms && idle_ms >= _cfg.light_sleep_ms)
                _transition(PWR_LIGHT_SLEEP);
            break;

        case PWR_LIGHT_SLEEP:
            if (_cfg.deep_sleep_ms && idle_ms >= _cfg.deep_sleep_ms) {
                _transition(PWR_DEEP_SLEEP);
                // _enterDeepSleep blocks until wakeup
                _enterDeepSleep();
                // After wakeup: CPU resumes here
                _wakeupFlag = true;
                _lastActivity = millis();
                _transition(PWR_ACTIVE);
            } else {
                // WFI — CPU halts until next interrupt (SysTick ~1ms)
                // Saves ~2-4mA without disturbing RTOS tick
                __WFI();
            }
            break;

        case PWR_DEEP_SLEEP:
            // Handled inside _enterDeepSleep; this case is transient
            break;
    }
}

// ─── reportActivity ───────────────────────────────────────────────────────────

void PowerManager::reportActivity() {
    _lastActivity = millis();
    if (_state != PWR_ACTIVE) {
        _wakeupFlag = (_state >= PWR_LIGHT_SLEEP);
        _transition(PWR_ACTIVE);
    }
}

// ─── consumeWakeup ────────────────────────────────────────────────────────────

bool PowerManager::consumeWakeup() {
    if (_wakeupFlag) {
        _wakeupFlag = false;
        return true;
    }
    return false;
}

// ─── forceState / forceWake ───────────────────────────────────────────────────

void PowerManager::forceState(PowerState s) {
    _lastActivity = millis() - _cfg.deep_sleep_ms; // pretend long idle
    _transition(s);
}

void PowerManager::forceWake() {
    reportActivity();
}

// ─── scanIntervalMs ───────────────────────────────────────────────────────────

uint32_t PowerManager::scanIntervalMs() const {
    switch (_state) {
        case PWR_ACTIVE: return 1;
        case PWR_DIM: return 1;
        case PWR_IDLE: return 20;
        case PWR_LIGHT_SLEEP: return 50;
        case PWR_DEEP_SLEEP: return 0; // GPIO wakeup, no polling
    }
    return 1;
}

// ─── _transition ─────────────────────────────────────────────────────────────

void PowerManager::_transition(PowerState next) {
    if (next == _state) return;
    const PowerState prev = _state;
    _state = next;
    Serial.print("[PWR] ");
    const char *names[] = {"ACTIVE", "DIM", "IDLE", "LIGHT_SLEEP", "DEEP_SLEEP"};
    Serial.print(names[prev]);
    Serial.print(" → ");
    Serial.println(names[next]);
    if (_cb) _cb(prev, next);
}

// ─── _configWakeupPins ────────────────────────────────────────────────────────

void PowerManager::_configWakeupPins() {
    // Configure each column pin as GPIO SENSE wakeup source (active LOW)
    // Row pins are driven LOW before deep sleep so any key press pulls a col pin low
    for (uint8_t i = 0; i < _colCount; ++i) {
        nrf_gpio_cfg_sense_input(
            digitalPinToPinName(_colPins[i]), // Arduino pin -> nRF pin number
            NRF_GPIO_PIN_PULLUP,
            NRF_GPIO_PIN_SENSE_LOW
        );
    }
    // Encoder button (active LOW)
    if (_encBtnPin >= 0) {
        nrf_gpio_cfg_sense_input(
            digitalPinToPinName(_encBtnPin),
            NRF_GPIO_PIN_PULLUP,
            NRF_GPIO_PIN_SENSE_LOW
        );
    }
}

// ─── _enterDeepSleep ─────────────────────────────────────────────────────────

void PowerManager::_enterDeepSleep() {
    Serial.println("[PWR] Entering deep sleep...");
    Serial.flush();

    // Stop BLE advertising to save power (optional — comment out to stay discoverable)
    // Bluefruit.Advertising.stop();

    // Reduce TX power to minimum
    Bluefruit.setTxPower(-40);

    // Configure GPIO sense wakeup
    _configWakeupPins();

    // SoftDevice managed sleep — CPU halts, radio can still run (BLE)
    // Wakes on: GPIO sense event, BLE event, RTC tick
    // This is the lowest power state compatible with BLE staying alive (~20µA idle)
    while (true) {
        // sd_app_evt_wait puts CPU into WFE/sleep until SoftDevice signals an event
        const uint32_t err = sd_app_evt_wait();
        (void) err;

        // Check if any col pin is asserted (key pressed)
        bool woken = false;
        for (uint8_t i = 0; i < _colCount; ++i) {
            if (digitalRead(_colPins[i]) == LOW) {
                woken = true;
                break;
            }
        }
        if (!woken && _encBtnPin >= 0 && digitalRead(_encBtnPin) == LOW)
            woken = true;

        if (woken) break;

        // BLE event woke us — go back to sleep
    }

    // Restore TX power
    Bluefruit.setTxPower(4);

    // Clear GPIO sense config to restore normal operation
    for (uint8_t i = 0; i < _colCount; ++i) {
        nrf_gpio_cfg_input(
            digitalPinToPinName(_colPins[i]),
            NRF_GPIO_PIN_PULLUP
        );
    }
    if (_encBtnPin >= 0) {
        nrf_gpio_cfg_input(
            digitalPinToPinName(_encBtnPin),
            NRF_GPIO_PIN_PULLUP
        );
    }

    Serial.println("[PWR] Wakeup from deep sleep");
}
