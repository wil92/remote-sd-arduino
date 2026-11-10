#pragma once

#include <Arduino.h>

#include "../flash/flash_manager.h"
#include "../sdcard/file_info.h"

#include <vector>

namespace webserverfake {
class WiFiClient {
public:
    virtual ~WiFiClient() {}

    operator bool() { return true; }

    virtual bool connected() { return true; }
    virtual bool available() { return true; }

    virtual String readStringUntil(char delimiter) { return ""; }

    virtual void stop() {}

    virtual void print(String ignore) {}

    virtual int read() { return '\0'; }
};

class WiFiServer {
public:
    WiFiServer(int port) {}

    void begin() {}

    void setNoDelay(bool ignore) {}

    WiFiClient accept() { return WiFiClient(); }
};

class SDCardManager {
public:
    virtual std::vector<FileInfo> listFiles(bool beginConnectionToSD) {
        return std::vector<FileInfo>();
    }

    virtual void removeFile(const String &fileName, bool beginConnectionToSD) {}

    virtual void appendFile(const String &fileName, const byte *chunk,
                            size_t size) {}

    virtual void openFile(const String &fileName) {}

    virtual void closeFile() {}

    virtual void begin() {}

    virtual void end() {}
};
} // namespace webserverfake
