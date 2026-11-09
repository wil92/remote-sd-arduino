#pragma once

#include <cstddef>
#include <cstdint>

inline void copyBytes(uint8_t *destination, const uint8_t *source,
                      size_t length) {
    for (size_t index = 0; index < length; ++index) {
        destination[index] = source[index];
    }
}
