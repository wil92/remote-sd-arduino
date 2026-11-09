#pragma once

#include <Arduino.h>

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

    void stop() {}

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

class FlashManager {
public:
    String ssidNetwork = "";
    String passwordNetwork = "";
    bool isSetup = true;

    void loadSetup() {}

    void saveSetup() {}

    bool resetSetup() { return true; }

    void setVariable(const String &key, const String &value) {}
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
