#pragma once

#include "../sdcard/file_info.h"
#include "../utils.h"

#include <functional>
#include <map>
#include <memory>
#include <vector>

#ifdef ARDUINO_ARCH_ESP32
#include <WiFiClient.h>
#include <WiFiServer.h>

#include "../flash/flash_manager.h"
#include "../sdcard/sd_card_manager.h"
#else
#include "./webserverfake.h"

using namespace webserverfake;
#endif

#define CLIENT_TIMEOUT 2000

using ResetFunction = std::function<void(void)>;
using EnableSDFunction = std::function<void(bool)>;

class WebServerManager {
    bool checkEndpoints(WiFiClient *client, const String &message);

    String prepareHeaders(int code) const;

    String prepareBaseHeaders(int code) const;

    String prepareIndexHtmlPage() const;

    String prepareUploadHtmlPage();

    void checkForConnections();

    void handleWebsite(WiFiClient *client, const String &message);

    void handleUpdateConfig(WiFiClient *client, const String &message) const;

    void handleFileDeletion(WiFiClient *client, const String &message);

#ifndef ARDUINO_ARCH_ESP32
public:
#endif
    void handleFileUpload(WiFiClient *client);

    std::map<String, String> parseQueryParameters(const String &query,
                                                  int startPoint) const;
#ifndef ARDUINO_ARCH_ESP32
private:
#endif

    std::map<String, String> consumeHeaders(WiFiClient *client) const;

    String consumeIncomingData(WiFiClient *client) const;

    void setParameter(const String &key, const String &value) const;

    static String urlDecode(const String &source);

    void connectSD() const {
        if (sf) {
            sf(true);
#ifdef ARDUINO_ARCH_ESP32
            delay(100);
#endif
        }
    }

    void disconnectSD() const {
        if (sf) {
            sf(false);
        }
    }

public:
    FlashManager *flashManager = nullptr;
    std::function<void()> checkResetButton;
    std::unique_ptr<WiFiServer> server;
    ResetFunction rf;
    EnableSDFunction sf;
    SDCardManager *sdCardManager = nullptr;
    std::vector<FileInfo> filesInSD;

    void setup();

    void run() const;

    void loop();
};
