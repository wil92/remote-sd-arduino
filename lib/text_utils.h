#pragma once

#include <Arduino.h>

inline bool isValidUtf8(const String &text, bool rejectControls) {
    for (unsigned int i = 0; i < text.length();) {
        const unsigned char first = static_cast<unsigned char>(text[i++]);
        if (first < 0x80) {
            if (rejectControls && (first < 0x20 || first == 0x7f)) {
                return false;
            }
            continue;
        }
        unsigned int remaining;
        unsigned char secondMin = 0x80, secondMax = 0xbf;
        if (first >= 0xc2 && first <= 0xdf) {
            remaining = 1;
        } else if (first >= 0xe0 && first <= 0xef) {
            remaining = 2;
            if (first == 0xe0) secondMin = 0xa0;
            if (first == 0xed) secondMax = 0x9f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            remaining = 3;
            if (first == 0xf0) secondMin = 0x90;
            if (first == 0xf4) secondMax = 0x8f;
        } else {
            return false;
        }
        if (i + remaining > text.length()) {
            return false;
        }
        const unsigned char second = static_cast<unsigned char>(text[i++]);
        if (second < secondMin || second > secondMax) {
            return false;
        }
        while (--remaining) {
            const unsigned char next = static_cast<unsigned char>(text[i++]);
            if (next < 0x80 || next > 0xbf) {
                return false;
            }
        }
    }
    return true;
}
