#ifndef UNIFIEDHID_H
#define UNIFIEDHID_H

#include <Adafruit_TinyUSB.h>


class UnifiedHid {
public:
    UnifiedHid(uint8_t const* desc_report, uint16_t len,
               uint8_t num_input   = 1,
               uint8_t num_output  = 0,
               uint8_t num_feature = 0);

    void begin();
    void sendKeyboard(uint8_t modifier, uint8_t *keys);
    void releaseKeyboard();
    void sendConsumer(uint16_t usage);
    void releaseConsumer();
    bool ready();

private:
    Adafruit_USBD_HID usb;
    // BLEHidGeneric     ble;
    // BLEDis            bledis;

    // void startAdv();
};

#endif