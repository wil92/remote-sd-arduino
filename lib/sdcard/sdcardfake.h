#pragma once

#include <Arduino.h>

#include "file_info.h"

#define FILE_APPEND "a"
#define FILE_WRITE "w"

namespace sdcardfake {
class File {
public:
    operator bool() { return true; }

    bool isDirectory() { return false; }

    File openNextFile() { return File(); }

    String name() { return ""; }

    size_t size() { return 0; }

    bool write(const byte *buf, size_t size) { return true; }

    void flush() {}

    void close() {}
};

class SDLib {
public:
    bool begin(int csPin, SPIClass &spi) { return true; }

    void end() {}

    File open(const String &path) { return File(); }

    File open(const String &path, const String &mode) { return File(); }

    bool remove(String ignore) { return true; }
};

SDLib SD;
} // namespace sdcardfake
