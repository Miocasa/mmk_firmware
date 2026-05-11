#include "hid_instance.h"

// HID descriptor
#define TUD_HID_REPORT_DESC_NKRO_KEYBOARD(...) \
HID_USAGE_PAGE ( HID_USAGE_PAGE_DESKTOP     )                   ,\
HID_USAGE      ( HID_USAGE_DESKTOP_KEYBOARD )                   ,\
HID_COLLECTION ( HID_COLLECTION_APPLICATION )                   ,\
__VA_ARGS__                                                     \
HID_USAGE_PAGE   ( HID_USAGE_PAGE_KEYBOARD )                  ,\
HID_USAGE_MIN    ( 224                     )                  ,\
HID_USAGE_MAX    ( 231                     )                  ,\
HID_LOGICAL_MIN  ( 0                       )                  ,\
HID_LOGICAL_MAX  ( 1                       )                  ,\
HID_REPORT_COUNT ( 8                       )                  ,\
HID_REPORT_SIZE  ( 1                       )                  ,\
HID_INPUT        ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE )   ,\
HID_USAGE_PAGE     ( HID_USAGE_PAGE_KEYBOARD )                ,\
HID_USAGE_MIN      ( 0                       )                ,\
HID_USAGE_MAX_N    ( 255, 2                  )                ,\
HID_LOGICAL_MIN    ( 0                       )                ,\
HID_LOGICAL_MAX    ( 1                       )                ,\
HID_REPORT_COUNT_N ( 256, 2                  )                ,\
HID_REPORT_SIZE    ( 1                       )                ,\
HID_INPUT          ( HID_DATA | HID_VARIABLE | HID_ABSOLUTE ) ,\
HID_COLLECTION_END

static const uint8_t desc_hid_report[] = {
    TUD_HID_REPORT_DESC_NKRO_KEYBOARD(HID_REPORT_ID(1)),
    TUD_HID_REPORT_DESC_SYSTEM_CONTROL(HID_REPORT_ID(2)),
    TUD_HID_REPORT_DESC_CONSUMER(HID_REPORT_ID(3)),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(4)),
};

UnifiedHid hid(desc_hid_report, sizeof(desc_hid_report));
