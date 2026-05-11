// UnifiedHid.h
#ifndef UNIFIEDHID_H
#define UNIFIEDHID_H

#include <Adafruit_TinyUSB.h>
#include <bluefruit.h>
#include <Adafruit_LittleFS.h>
#include <InternalFileSystem.h>

enum HidMode : uint8_t {
    AUTO_MODE = 0,
    BLE_MODE = 1,
    ONLY_USB_MODE = 2,
    ONLY_BLE_MODE = 3,
};

#define RID_KEYBOARD 1
#define RID_SYSCTRL  2
#define RID_CONSUMER 3
#define RID_MOUSE    4

class UnifiedHid {
public:
    UnifiedHid(uint8_t const *desc_report, uint16_t len);

    void begin(const char *ble_name = "BLE Keyboard");

    // Call every loop() iteration — monitors USB/BLE state,
    // re-mounts USB if it was disconnected and came back.
    // Safe to call unconditionally, no side effects when everything is up.
    // void tick();
    //
    // // Manual re-init of a specific transport (e.g. after hard USB reset)
    void reinitUsb();

    //
    // void checkAndReinitUsb();

    void setMode(uint8_t mode);

    HidMode getMode() { return _mode; }

    bool ready();

    bool usbReady();

    bool bleReady(); // не static
    bool bleConnected(); // не static

    void sendNkro(uint8_t mods, const uint8_t *bitmap);

    void releaseKeyboard();

    void sendConsumer(uint16_t usage);

    void releaseConsumer();

    void sendMouse(uint8_t buttons, int8_t x, int8_t y, int8_t v, int8_t h);

    void releaseMouse();

    // Returns: 1=USB, 2=BLE, 0=no one ready
    uint8_t getTransport();

    bool consumeForceResend() {
        if (_forceResend) {
            _forceResend = false;
            return true;
        }
        return false;
    }

    bool _bleInited = false;
    bool _usbInited = false;

private:
    Adafruit_USBD_HID _usb;
    BLEHidAdafruit _ble;
    BLEDis _bledis;

    HidMode _mode = AUTO_MODE;


    static constexpr const char *MODE_FILE = "/hid_mode.bin";

    void _saveMode(HidMode m);

    HidMode _loadMode();

    bool _useUsb() const { return _mode != ONLY_BLE_MODE; }
    bool _useBle() const { return _mode != ONLY_USB_MODE; }

    // tick() state tracking
    bool _usbWasReady = false;
    bool _bleWasReady = false;
    bool _forceResend = false;

    uint32_t _lastUsbCheck = 0;
    bool _usbPreviouslyDetected = false;
};

#endif
