#pragma once

#if __has_include("../include/usb_config.h")
#include "../include/usb_config.h"
#endif

#ifndef ARDUINO_USB_MODE
#define ARDUINO_USB_MODE 1
#endif

#ifndef ARDUINO_USB_CDC_ON_BOOT
#define ARDUINO_USB_CDC_ON_BOOT 1
#endif
