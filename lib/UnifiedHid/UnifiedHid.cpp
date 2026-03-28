#include "UnifiedHid.h"

// Report IDs – must match the descriptor
#define RID_KEYBOARD  1
#define RID_CONSUMER  3   // skip system-control (2) for BLE simplicity

UnifiedHid::UnifiedHid(uint8_t const* desc_report, uint16_t len,
                       uint8_t num_input, uint8_t num_output, uint8_t num_feature)
    : usb(desc_report, len, HID_ITF_PROTOCOL_NONE, 2, false)

{

}

void UnifiedHid::begin() {
    usb.begin();
}

bool UnifiedHid::ready() {
    return usb.ready(); //|| Bluefruit.connected();
}

void UnifiedHid::sendKeyboard(uint8_t modifier, uint8_t *keys) {
    // USB wins if mounted
    if (usb.ready()) {
        usb.keyboardReport(RID_KEYBOARD, modifier, keys);
    }
}

void UnifiedHid::releaseKeyboard() {
    static uint8_t zeros[6] = {};
    sendKeyboard(0, zeros);
}

void UnifiedHid::sendConsumer(uint16_t usage) {
    if (usb.ready()) {
        usb.sendReport16(RID_CONSUMER, usage);
    }
}

void UnifiedHid::releaseConsumer() {
    sendConsumer(0);
}