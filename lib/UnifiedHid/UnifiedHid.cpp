#include "UnifiedHid.h"
using namespace Adafruit_LittleFS_Namespace;

UnifiedHid::UnifiedHid(uint8_t const *desc_report, uint16_t len)
    : _usb(desc_report, len, HID_ITF_PROTOCOL_NONE, 2, false) {
}

// ─── begin ────────────────────────────────────────────────────────────────────

void UnifiedHid::begin(const char *ble_name) {
    if (!_bleInited && !_usbInited) {
        InternalFS.begin();
        _mode = _loadMode();
    }

    if (!_usbInited) {
        _usb.begin();
        _usbInited = true;
    }

    if (!_bleInited) {
        Bluefruit.begin();
        Bluefruit.setTxPower(4);
        Bluefruit.setName(ble_name);

        _bledis.setManufacturer("Custom KB");
        _bledis.setModel("XIAO nRF52840");
        _bledis.begin();

        _ble.begin();

        Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
        Bluefruit.Advertising.addTxPower();
        Bluefruit.Advertising.addAppearance(BLE_APPEARANCE_HID_KEYBOARD);
        Bluefruit.Advertising.addService(_ble);
        Bluefruit.Advertising.addName();
        Bluefruit.Advertising.restartOnDisconnect(true);
        Bluefruit.Advertising.setInterval(32, 244);
        Bluefruit.Advertising.setFastTimeout(30);
        Bluefruit.Advertising.start(0);

        _bleInited = true;
    }
}

void UnifiedHid::reinitUsb() {
    if (!_usbInited) return;
    // Soft re-init: re-register descriptor and re-attach
    // Works when host did a hard reset but didn't physically unplug
    _usb.begin();
    Serial.println("[HID] USB re-init requested");
}

// ─── Mode ─────────────────────────────────────────────────────────────────────

void UnifiedHid::setMode(uint8_t mode) {
    const auto m = static_cast<HidMode>(mode);
    if (m == _mode) return;


    if (_usbInited || _bleInited) {
        releaseKeyboard();
        releaseConsumer();
        releaseMouse();
    }

    _mode = m;
    _saveMode(m);
}

// ─── Status ───────────────────────────────────────────────────────────────────

bool UnifiedHid::usbReady() { return _usb.ready(); }
bool UnifiedHid::bleConnected() { return Bluefruit.connected(); }
bool UnifiedHid::bleReady() { return Bluefruit.connected(); }

bool UnifiedHid::ready() {
    switch (_mode) {
        case AUTO_MODE: return usbReady() || bleReady();
        case BLE_MODE: return bleReady() || usbReady();
        case ONLY_USB_MODE: return usbReady();
        case ONLY_BLE_MODE: return bleReady();
    }
    return false;
}

uint8_t UnifiedHid::getTransport() {
    switch (_mode) {
        case AUTO_MODE:
            if (usbReady()) return 1;
            if (bleReady()) return 2;
            return 0;
        case BLE_MODE:
            if (bleReady()) return 2;
            if (usbReady()) return 1;
            return 0;
        case ONLY_USB_MODE:
            return usbReady() ? 1 : 0;
        case ONLY_BLE_MODE:
            return bleReady() ? 2 : 0;
    }
    return 0;
}

// ─── Keyboard ─────────────────────────────────────────────────────────────────

static void bitmapTo6kro(const uint8_t *bitmap, uint8_t *keys6) {
    uint8_t n = 0;
    for (uint16_t kc = 4; kc < 256 && n < 6; ++kc)
        if (bitmap[kc >> 3] & (1u << (kc & 7u)))
            keys6[n++] = static_cast<uint8_t>(kc);
}

void UnifiedHid::sendNkro(uint8_t mods, const uint8_t *bitmap) {
    uint8_t buf[33];
    buf[0] = mods;
    memcpy(buf + 1, bitmap, 32);

    switch (getTransport()) {
        case 1:
            _usb.sendReport(RID_KEYBOARD, buf, sizeof(buf));
            break;
        case 2: {
            uint8_t keys6[6] = {};
            bitmapTo6kro(bitmap, keys6);
            _ble.keyboardReport(mods, keys6);
            break;
        }
        default: break;
    }
}

void UnifiedHid::releaseKeyboard() {
    if (_useUsb() && usbReady()) {
        uint8_t buf[33] = {};
        _usb.sendReport(RID_KEYBOARD, buf, sizeof(buf));
    }
    if (_useBle() && bleReady()) {
        uint8_t keys6[6] = {};
        _ble.keyboardReport(0, keys6);
    }
}

// ─── Consumer ─────────────────────────────────────────────────────────────────

void UnifiedHid::sendConsumer(uint16_t usage) {
    switch (getTransport()) {
        case 1: _usb.sendReport(RID_CONSUMER, &usage, sizeof(usage));
            break;
        case 2: _ble.consumerKeyPress(usage);
            break;
        default: break;
    }
}

void UnifiedHid::releaseConsumer() {
    if (_useUsb() && usbReady()) {
        uint16_t zero = 0;
        _usb.sendReport(RID_CONSUMER, &zero, sizeof(zero));
    }
    if (_useBle() && bleReady()) {
        _ble.consumerKeyRelease();
    }
}

// ─── Mouse ────────────────────────────────────────────────────────────────────

void UnifiedHid::sendMouse(uint8_t buttons, int8_t x, int8_t y, int8_t v, int8_t h) {
    uint8_t mbuf[5] = {buttons, (uint8_t) x, (uint8_t) y, (uint8_t) v, (uint8_t) h};

    switch (getTransport()) {
        case 1: _usb.sendReport(RID_MOUSE, mbuf, sizeof(mbuf));
            break;
        case 2: _ble.mouseReport(buttons, x, y, v, h);
            break;
        default: break;
    }
}

void UnifiedHid::releaseMouse() { sendMouse(0, 0, 0, 0, 0); }

// ─── Persist ──────────────────────────────────────────────────────────────────

void UnifiedHid::_saveMode(HidMode m) {
    File f(InternalFS);
    if (f.open(MODE_FILE, FILE_O_WRITE)) {
        auto val = static_cast<uint8_t>(m);
        f.write(&val, 1);
        f.close();
    }
}

HidMode UnifiedHid::_loadMode() {
    File f(InternalFS);
    if (f.open(MODE_FILE, FILE_O_READ)) {
        uint8_t val = 0;
        f.read(&val, 1);
        f.close();
        if (val <= static_cast<uint8_t>(ONLY_BLE_MODE))
            return static_cast<HidMode>(val);
    }
    return AUTO_MODE;
}
