#include <ArduinoFake.h>
#include <EEPROM.h>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <unity.h>
#include <vector>

#include "../../../lib/webserver/web_server_manager.h"
#include "../../../lib/scan_status.h"

using namespace fakeit;

uint8_t eepromData[CONFIG_STORAGE_SIZE];
unsigned long fakeMillis;

class SDCardManagerTmp : public SDCardManager {
public:
    std::vector<byte> data;
    size_t beginCalls = 0;
    size_t removeCalls = 0;
    size_t openCalls = 0;
    size_t closeCalls = 0;
    size_t endCalls = 0;
    size_t listCalls = 0;
    std::vector<FileInfo> files;

    std::vector<FileInfo> listFiles(bool) override {
        ++listCalls;
        return files;
    }

    void appendFile(const String &, const byte *chunk, size_t size) override {
        data.insert(data.end(), chunk, chunk + size);
    }

    void begin() override { ++beginCalls; }

    void removeFile(const String &, bool) override { ++removeCalls; }

    void openFile(const String &) override { ++openCalls; }

    void closeFile() override { ++closeCalls; }

    void end() override { ++endCalls; }
};

class WiFiClientTmp : public webserverfake::WiFiClient {
public:
    std::vector<byte> data;
    std::vector<byte> fileBytes;
    size_t index = 0;
    std::string response;

    bool connected() override { return index < data.size(); }

    bool available() override { return index < data.size(); }

    String readStringUntil(char delimiter) override {
        String result;
        while (index < data.size() &&
               data[index] != static_cast<byte>(delimiter)) {
            result += static_cast<char>(data[index++]);
        }
        if (index < data.size()) {
            ++index;
        }
        return result;
    }

    int read() override {
        if (index >= data.size()) {
            return -1;
        }
        return data[index++];
    }

    void print(String value) override { response += value.c_str(); }

    void buildUploadRequest(const std::vector<byte> &payload,
                            bool includeContentLength = true,
                            bool includeContentType = true,
                            bool includeFilename = true,
                            const char *initialBoundary =
                                "------WebKitFormBoundarypCbNHDfIecaHIigU",
                            const char *fileName = "test.bin",
                            bool includeBoundaryParameter = true) {
        std::string multipartHeaders = initialBoundary;
        multipartHeaders += "\r\n";
        if (includeFilename) {
            multipartHeaders +=
                "Content-Disposition: form-data; name=\"file\"; "
                "filename=\"";
            multipartHeaders += fileName;
            multipartHeaders += "\"\r\n";
        } else {
            multipartHeaders +=
                "Content-Disposition: form-data; name=\"file\"\r\n";
        }
        multipartHeaders += "Content-Type: application/octet-stream\r\n\r\n";
        const std::string multipartFooter =
            "\r\n------WebKitFormBoundarypCbNHDfIecaHIigU--\r\n";

        addData("POST /upload HTTP/1.1\r\nHost: localhost\r\n");
        if (includeContentType) {
            addData("Content-Type: multipart/form-data;");
            if (includeBoundaryParameter) {
                addData(" boundary=----WebKitFormBoundarypCbNHDfIecaHIigU");
            }
            addData("\r\n");
        }
        if (includeContentLength) {
            addData("Content-Length: ");
            addData(std::to_string(multipartHeaders.size() + payload.size() +
                                   multipartFooter.size())
                        .c_str());
            addData("\r\n");
        }
        addData("\r\n");
        addData(multipartHeaders.c_str());
        addBytes(payload);
        fileBytes = payload;
        addData(multipartFooter.c_str());
    }

    void buildFixtureUploadRequest() {
        std::ifstream file("test/host/sent.txt", std::ios::binary);
        TEST_ASSERT_TRUE_MESSAGE(file.is_open(),
                                 "Test fixture could not be opened");

        std::vector<byte> payload;
        char value;
        while (file.get(value)) {
            payload.push_back(static_cast<byte>(value));
        }
        buildUploadRequest(payload);
    }

    void buildHeaders() { addData("Host: localhost\r\n\r\n"); }

    void buildSensitiveHeaders() {
        addData("Referer: http://localhost/?ssid=net&password=dummy-secret\r\n\r\n");
    }

private:
    void addData(const char *message) {
        data.insert(data.end(), message, message + std::strlen(message));
    }

    void addBytes(const std::vector<byte> &bytes) {
        data.insert(data.end(), bytes.begin(), bytes.end());
    }
};

class SetupConnectionClient : public WiFiClientTmp {
public:
    bool keepOpen = false;
    bool stopped = false;

    bool connected() override {
        return !stopped && (keepOpen || index < data.size());
    }

    void stop() override { stopped = true; }

    void setRequest(const std::string &request) {
        data.assign(request.begin(), request.end());
    }
};

void testDeviceInfoIsJsonWithoutAccessingSD() {
    WiFiClientTmp client;
    client.buildHeaders();
    WebServerManager manager;
    FlashManager flash;
    flash.isSetup = true;
    SDCardManagerTmp sd;
    manager.flashManager = &flash;
    manager.sdCardManager = &sd;
    manager.deviceId = "remote-sd-001122334455";
    int switches = 0;
    manager.sf = [&](bool) { ++switches; };

    TEST_ASSERT_FALSE(manager.checkEndpoints(&client, "GET /api/device HTTP/1.1\r"));
    TEST_ASSERT_EQUAL_STRING(
        "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
        "Cache-Control: no-store\r\nConnection: close\r\n\r\n"
        "{\"type\":\"remote-sd\",\"id\":\"remote-sd-001122334455\","
        "\"name\":\"Remote SD\",\"api_version\":1}",
        client.response.c_str());
    TEST_ASSERT_EQUAL(0, switches);
    TEST_ASSERT_EQUAL(0, sd.beginCalls);
}

void testFileListReturnsCacheWithoutAccessingSD() {
    WiFiClientTmp client;
    client.buildHeaders();
    WebServerManager manager;
    FlashManager flash;
    flash.isSetup = true;
    SDCardManagerTmp sd;
    manager.flashManager = &flash;
    manager.sdCardManager = &sd;
    manager.filesInSD = {{"quote\"\\\n.gcode", 42}, {"other", 0}};
    int switches = 0;
    manager.sf = [&](bool) { ++switches; };

    manager.checkEndpoints(&client, "GET /api/files HTTP/1.1\r");
    TEST_ASSERT_TRUE(client.response.find("application/json") != std::string::npos);
    TEST_ASSERT_TRUE(client.response.find("Cache-Control: no-store") != std::string::npos);
    TEST_ASSERT_TRUE(client.response.find(
        "[{\"name\":\"quote\\\"\\\\\\u000a.gcode\",\"size\":42},"
        "{\"name\":\"other\",\"size\":0}]") != std::string::npos);
    TEST_ASSERT_EQUAL(0, switches);
    TEST_ASSERT_EQUAL(0, sd.listCalls);

    manager.filesInSD.clear();
    WiFiClientTmp emptyClient;
    emptyClient.buildHeaders();
    manager.checkEndpoints(&emptyClient, "GET /api/files HTTP/1.1\r");
    TEST_ASSERT_TRUE(emptyClient.response.find("\r\n\r\n[]") != std::string::npos);
    flash.isSetup = false;
    WiFiClientTmp setupClient;
    setupClient.buildHeaders();
    manager.checkEndpoints(&setupClient, "GET /api/files HTTP/1.1\r");
    TEST_ASSERT_TRUE(setupClient.response.find("404 Not Found") != std::string::npos);
    TEST_ASSERT_EQUAL(0, switches);
}

void testUploadPageEscapesFileNames() {
    WiFiClientTmp client;
    client.buildHeaders();
    WebServerManager manager;
    FlashManager flash;
    flash.isSetup = true;
    manager.flashManager = &flash;
    manager.filesInSD = {{"a'\"<b>&.gcode", 3}};
    manager.checkEndpoints(&client, "GET / HTTP/1.1\r");
    TEST_ASSERT_TRUE(client.response.find("a&#39;&quot;&lt;b&gt;&amp;.gcode") !=
                     std::string::npos);
}

void testSetupSavesNameAndEndpointReturnsIt() {
    FlashManager flash;
    WebServerManager manager;
    manager.flashManager = &flash;
    manager.deviceId = "remote-sd-001122334455";
    WiFiClientTmp setupClient;
    setupClient.buildHeaders();
    TEST_ASSERT_TRUE(manager.checkEndpoints(
        &setupClient,
        "POST /?ssid=net&password=pwd&name=Printer+%22A%22+%5C+%C3%A9 HTTP/1.1\r"));
    TEST_ASSERT_TRUE(setupClient.response.find("204 No Content") !=
                     std::string::npos);
    FlashManager restored;
    restored.loadSetup();
    TEST_ASSERT_EQUAL_STRING("Printer \"A\" \\ \xc3\xa9", restored.deviceName.c_str());
    manager.flashManager = &restored;
    WiFiClientTmp infoClient;
    infoClient.buildHeaders();
    manager.checkEndpoints(&infoClient, "GET /api/device HTTP/1.1\r");
    TEST_ASSERT_TRUE(infoClient.response.find(
        "\"name\":\"Printer \\\"A\\\" \\\\ \xc3\xa9\"") != std::string::npos);
}

void testSetupDoesNotLogRequestHeaders() {
    FlashManager flash;
    WebServerManager manager;
    manager.flashManager = &flash;
    WiFiClientTmp client;
    client.buildSensitiveHeaders();

    TEST_ASSERT_TRUE(manager.checkEndpoints(
        &client, "POST /?ssid=net&password=dummy-secret HTTP/1.1\r"));
    Verify(OverloadedMethod(ArduinoFake(Serial), println,
                            size_t(const String &))).Never();
}

void testSetupRestartsAfterResponseEvenWithMoreClientData() {
    FlashManager flash;
    WebServerManager manager;
    SetupConnectionClient client;
    manager.flashManager = &flash;
    client.keepOpen = true;
    const std::string request = "POST /?ssid=net&name=Printer HTTP/1.1\r\n"
                                "Host: localhost\r\n\r\n";
    client.setRequest(request + "unexpected trailing data\r\n");
    int restarts = 0;
    int resetPolls = 0;
    manager.checkResetButton = [&]() { ++resetPolls; };
    manager.rf = [&]() {
        ++restarts;
        TEST_ASSERT_TRUE(client.stopped);
        TEST_ASSERT_TRUE(client.response.find("204 No Content") !=
                         std::string::npos);
        TEST_ASSERT_TRUE(flash.isSetup);
    };

    manager.handleClient(client);

    TEST_ASSERT_EQUAL(1, restarts);
    TEST_ASSERT_EQUAL(request.size(), client.index);
    TEST_ASSERT_GREATER_OR_EQUAL(CLIENT_TIMEOUT, fakeMillis);
    TEST_ASSERT_LESS_THAN(CLIENT_TIMEOUT + 20, fakeMillis);
    TEST_ASSERT_GREATER_THAN(1, resetPolls);
}

void testSetupRestartsPromptlyWhenClientDisconnects() {
    FlashManager flash;
    WebServerManager manager;
    SetupConnectionClient client;
    manager.flashManager = &flash;
    client.setRequest("POST /?ssid=net&name=Printer HTTP/1.1\r\n"
                      "Host: localhost\r\n\r\n");
    int restarts = 0;
    manager.rf = [&]() {
        ++restarts;
        TEST_ASSERT_TRUE(client.stopped);
        TEST_ASSERT_TRUE(client.response.find("204 No Content") !=
                         std::string::npos);
    };

    manager.handleClient(client);

    TEST_ASSERT_EQUAL(1, restarts);
    TEST_ASSERT_LESS_THAN(CLIENT_TIMEOUT, fakeMillis);
}

void testRejectedSetupDoesNotRestart() {
    FlashManager flash;
    WebServerManager manager;
    SetupConnectionClient client;
    manager.flashManager = &flash;
    client.setRequest("POST /?ssid=net&name= HTTP/1.1\r\n"
                      "Host: localhost\r\n\r\n");
    int restarts = 0;
    manager.rf = [&]() { ++restarts; };

    manager.handleClient(client);

    TEST_ASSERT_EQUAL(0, restarts);
    TEST_ASSERT_TRUE(client.stopped);
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") != std::string::npos);
    TEST_ASSERT_EQUAL_UINT8(0xff, eepromData[CONFIG_STATUS_ADDRESS]);
}

void testSetupRejectsInvalidNameWithoutSavingOrRestarting() {
    FlashManager flash;
    WebServerManager manager;
    manager.flashManager = &flash;
    WiFiClientTmp client;
    client.buildHeaders();
    TEST_ASSERT_FALSE(manager.checkEndpoints(
        &client, "POST /?ssid=net&name= HTTP/1.1\r"));
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("Remote SD", flash.deviceName.c_str());
    TEST_ASSERT_EQUAL_STRING(NETWORK_SSID, flash.ssidNetwork.c_str());
    TEST_ASSERT_EQUAL_UINT8(0xff, eepromData[CONFIG_STATUS_ADDRESS]);

    WiFiClientTmp invalidUtf8Client;
    invalidUtf8Client.buildHeaders();
    TEST_ASSERT_FALSE(manager.checkEndpoints(
        &invalidUtf8Client, "POST /?name=%FF HTTP/1.1\r"));
    TEST_ASSERT_TRUE(invalidUtf8Client.response.find("400 Bad Request") !=
                     std::string::npos);
    TEST_ASSERT_EQUAL_UINT8(0xff, eepromData[CONFIG_STATUS_ADDRESS]);

    WiFiClientTmp nullClient;
    nullClient.buildHeaders();
    TEST_ASSERT_FALSE(manager.checkEndpoints(
        &nullClient, "POST /?name=Printer%00A HTTP/1.1\r"));
    TEST_ASSERT_TRUE(nullClient.response.find("400 Bad Request") !=
                     std::string::npos);
    TEST_ASSERT_EQUAL_UINT8(0xff, eepromData[CONFIG_STATUS_ADDRESS]);
}

void testSetupPageEscapesNameAndDisplaysDefault() {
    FlashManager flash;
    WebServerManager manager;
    manager.flashManager = &flash;
    WiFiClientTmp defaultClient;
    defaultClient.buildHeaders();
    manager.checkEndpoints(&defaultClient, "GET / HTTP/1.1\r");
    TEST_ASSERT_TRUE(defaultClient.response.find("value=\"Remote SD\"") !=
                     std::string::npos);

    flash.deviceName = "A \"<&' $$SSID$$";
    WiFiClientTmp escapedClient;
    escapedClient.buildHeaders();
    manager.checkEndpoints(&escapedClient, "GET / HTTP/1.1\r");
    TEST_ASSERT_TRUE(escapedClient.response.find(
        "value=\"A &quot;&lt;&amp;&#39; $$SSID$$\"") != std::string::npos);
}

void testDeviceInfoUnavailableInSetupMode() {
    WiFiClientTmp client;
    client.buildHeaders();
    WebServerManager manager;
    FlashManager flash;
    flash.isSetup = false;
    manager.flashManager = &flash;

    TEST_ASSERT_FALSE(manager.checkEndpoints(&client, "GET /api/device HTTP/1.1\r"));
    TEST_ASSERT_TRUE(client.response.find("404 Not Found") != std::string::npos);
    TEST_ASSERT_TRUE(client.response.find("remote-sd") == std::string::npos);
}

void testOtherGetPathsStillServeWebsite() {
    WiFiClientTmp client;
    client.buildHeaders();
    WebServerManager manager;
    FlashManager flash;
    flash.isSetup = false;
    manager.flashManager = &flash;

    TEST_ASSERT_FALSE(
        manager.checkEndpoints(&client, "GET /api/device-extra HTTP/1.1\r"));
    TEST_ASSERT_TRUE(client.response.find("200 OK") != std::string::npos);
    TEST_ASSERT_TRUE(client.response.find("Content-Type: text/html") !=
                     std::string::npos);
}

void testNetworkScanLifecycleAndJson() {
    FlashManager flash;
    WebServerManager manager;
    manager.flashManager = &flash;
    int state = NETWORK_SCAN_FAILED;
    int starts = 0;
    const std::vector<String> networks = {
        "Wi-Fi \"A\"\\", "", "\xff", "Cafe \xc3\xa9", "line\nbreak"
    };
    manager.scanComplete = [&]() { return state; };
    manager.startScan = [&]() { ++starts; state = NETWORK_SCAN_RUNNING; return true; };
    manager.scanSsid = [&](int index) { return networks[index]; };

    WiFiClientTmp startClient;
    startClient.buildHeaders();
    manager.checkEndpoints(&startClient, "POST /scan HTTP/1.1\r");
    TEST_ASSERT_TRUE(startClient.response.find("202 Accepted") != std::string::npos);
    TEST_ASSERT_EQUAL(1, starts);

    WiFiClientTmp concurrentClient;
    concurrentClient.buildHeaders();
    manager.checkEndpoints(&concurrentClient, "POST /scan HTTP/1.1\r");
    TEST_ASSERT_TRUE(concurrentClient.response.find("202 Accepted") != std::string::npos);
    TEST_ASSERT_EQUAL(1, starts);

    WiFiClientTmp pendingClient;
    pendingClient.buildHeaders();
    manager.checkEndpoints(&pendingClient, "GET /refresh-ssid HTTP/1.1\r");
    TEST_ASSERT_TRUE(pendingClient.response.find("202 Accepted") != std::string::npos);

    state = static_cast<int>(networks.size());
    WiFiClientTmp resultClient;
    resultClient.buildHeaders();
    manager.checkEndpoints(&resultClient, "GET /refresh-ssid HTTP/1.1\r");
    TEST_ASSERT_TRUE(resultClient.response.find("application/json; charset=utf-8") !=
                     std::string::npos);
    TEST_ASSERT_TRUE(resultClient.response.find(
        "[\"Wi-Fi \\\"A\\\"\\\\\",\"Cafe \xc3\xa9\",\"line\\u000abreak\"]") !=
                     std::string::npos);

    state = 0;
    WiFiClientTmp emptyClient;
    emptyClient.buildHeaders();
    manager.checkEndpoints(&emptyClient, "GET /refresh-ssid HTTP/1.1\r");
    TEST_ASSERT_TRUE(emptyClient.response.find("\r\n\r\n[]") != std::string::npos);
}

void testNetworkScanFailuresAndSetupMode() {
    FlashManager flash;
    WebServerManager manager;
    manager.flashManager = &flash;
    int calls = 0;
    manager.scanComplete = []() { return NETWORK_SCAN_FAILED; };
    manager.startScan = [&]() { ++calls; return false; };
    manager.scanSsid = [](int) { return String("unused"); };
    WiFiClientTmp startClient;
    startClient.buildHeaders();
    manager.checkEndpoints(&startClient, "POST /scan HTTP/1.1\r");
    TEST_ASSERT_TRUE(startClient.response.find("500 Internal Server Error") != std::string::npos);
    TEST_ASSERT_EQUAL(1, calls);
    WiFiClientTmp resultClient;
    resultClient.buildHeaders();
    manager.checkEndpoints(&resultClient, "GET /refresh-ssid HTTP/1.1\r");
    TEST_ASSERT_TRUE(resultClient.response.find("Wi-Fi scan failed") != std::string::npos);
    flash.isSetup = true;
    WiFiClientTmp configuredClient;
    configuredClient.buildHeaders();
    manager.checkEndpoints(&configuredClient, "POST /scan HTTP/1.1\r");
    TEST_ASSERT_TRUE(configuredClient.response.find("404 Not Found") != std::string::npos);
    TEST_ASSERT_EQUAL(1, calls);
    WiFiClientTmp configuredResult;
    configuredResult.buildHeaders();
    manager.checkEndpoints(&configuredResult, "GET /refresh-ssid HTTP/1.1\r");
    TEST_ASSERT_TRUE(configuredResult.response.find("404 Not Found") != std::string::npos);
}

void testNetworkScanCapsResults() {
    FlashManager flash;
    WebServerManager manager;
    manager.flashManager = &flash;
    int reads = 0;
    manager.scanComplete = []() { return 100; };
    manager.scanSsid = [&](int) { ++reads; return String("network"); };
    WiFiClientTmp client;
    client.buildHeaders();
    manager.checkEndpoints(&client, "GET /refresh-ssid HTTP/1.1\r");
    TEST_ASSERT_EQUAL(40, reads);

    reads = 0;
    manager.scanComplete = []() { return 300; };
    manager.scanSsid = [&](int) { ++reads; return String(""); };
    WiFiClientTmp emptyClient;
    emptyClient.buildHeaders();
    manager.checkEndpoints(&emptyClient, "GET /refresh-ssid HTTP/1.1\r");
    TEST_ASSERT_EQUAL(256, reads);
    TEST_ASSERT_TRUE(emptyClient.response.find("\r\n\r\n[]") != std::string::npos);
}

void testSetupRejectsMissingAndOversizeSsid() {
    FlashManager flash;
    WebServerManager manager;
    manager.flashManager = &flash;
    WiFiClientTmp missing;
    missing.buildHeaders();
    TEST_ASSERT_FALSE(manager.checkEndpoints(&missing, "POST /?ssid= HTTP/1.1\r"));
    TEST_ASSERT_TRUE(missing.response.find("Invalid device name, SSID") !=
                     std::string::npos);
    WiFiClientTmp oversized;
    oversized.buildHeaders();
    TEST_ASSERT_FALSE(manager.checkEndpoints(
        &oversized, "POST /?ssid=abcdefghijklmnopqrstuvwxyz1234567 HTTP/1.1\r"));
    TEST_ASSERT_TRUE(oversized.response.find("400 Bad Request") != std::string::npos);
    TEST_ASSERT_EQUAL_UINT8(0xff, eepromData[CONFIG_STATUS_ADDRESS]);
}

void testUploadPollsWhileWaitingForHeaders() {
    class WaitingClient : public WiFiClientTmp {
    public:
        bool open = true;
        bool connected() override { return open; }
        bool available() override { return false; }
    } client;

    WebServerManager webServerManager;
    int polls = 0;
    webServerManager.checkResetButton = [&]() {
        if (++polls == 3) {
            client.open = false;
        }
    };

    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_EQUAL(3, polls);
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") !=
                     std::string::npos);
}

void testUploadPollsWhileWaitingForFileData() {
    class WaitingClient : public WiFiClientTmp {
    public:
        bool open = true;
        bool connected() override { return open; }
    } client;
    client.buildUploadRequest(std::vector<byte>(4, 'x'));
    client.readStringUntil('\n');
    client.data.resize(client.data.size() - client.fileBytes.size() -
                       std::strlen("\r\n------WebKitFormBoundarypCbNHDfIecaHIigU--\r\n"));

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    webServerManager.sdCardManager = &sdCardManager;
    int emptyPolls = 0;
    webServerManager.checkResetButton = [&]() {
        if (sdCardManager.openCalls && !client.available() &&
            ++emptyPolls == 3) {
            client.open = false;
        }
    };

    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_EQUAL(3, emptyPolls);
    TEST_ASSERT_EQUAL(1, sdCardManager.openCalls);
    TEST_ASSERT_EQUAL(1, sdCardManager.closeCalls);
    TEST_ASSERT_EQUAL(1, sdCardManager.endCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.listCalls);
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") != std::string::npos);
}

void testUploadFileCopiesFixtureExactly() {
    WiFiClientTmp client;
    client.buildFixtureUploadRequest();
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    sdCardManager.files = {{"test.bin", client.fileBytes.size()}};
    webServerManager.sdCardManager = &sdCardManager;
    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_FALSE(client.fileBytes.empty());
    TEST_ASSERT_EQUAL(client.fileBytes.size(), sdCardManager.data.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(client.fileBytes.data(),
                                  sdCardManager.data.data(),
                                  client.fileBytes.size());
    TEST_ASSERT_EQUAL(1, sdCardManager.beginCalls);
    TEST_ASSERT_EQUAL(1, sdCardManager.removeCalls);
    TEST_ASSERT_EQUAL(1, sdCardManager.openCalls);
    TEST_ASSERT_EQUAL(1, sdCardManager.closeCalls);
    TEST_ASSERT_EQUAL(1, sdCardManager.endCalls);
    TEST_ASSERT_EQUAL(1, sdCardManager.listCalls);
    TEST_ASSERT_EQUAL(1, webServerManager.filesInSD.size());
    TEST_ASSERT_EQUAL_STRING("test.bin", webServerManager.filesInSD[0].name.c_str());
    TEST_ASSERT_TRUE(client.response.find("204 No Content") !=
                     std::string::npos);
}

void testUploadFilePreservesBinaryDataAcrossBufferBoundaries() {
    std::vector<byte> payload(1400);
    for (size_t index = 0; index < payload.size(); ++index) {
        payload[index] = static_cast<byte>((index * 37) % 256);
    }
    payload[0] = 0;
    payload[511] = '\r';
    payload[512] = '\n';
    payload[1023] = '\r';
    payload[1024] = '\n';

    WiFiClientTmp client;
    client.buildUploadRequest(payload);
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    webServerManager.sdCardManager = &sdCardManager;
    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_EQUAL(payload.size(), sdCardManager.data.size());
    TEST_ASSERT_EQUAL_UINT8_ARRAY(payload.data(), sdCardManager.data.data(),
                                  payload.size());
}

void testUploadFileAcceptsEmptyFile() {
    WiFiClientTmp client;
    client.buildUploadRequest(std::vector<byte>());
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    webServerManager.sdCardManager = &sdCardManager;
    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_TRUE(sdCardManager.data.empty());
    TEST_ASSERT_EQUAL(1, sdCardManager.openCalls);
    TEST_ASSERT_EQUAL(1, sdCardManager.closeCalls);
    TEST_ASSERT_TRUE(client.response.find("204 No Content") !=
                     std::string::npos);
}

void testUploadRejectsMissingRequiredHeader() {
    WiFiClientTmp client;
    client.buildUploadRequest(std::vector<byte>(), false);
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    webServerManager.sdCardManager = &sdCardManager;
    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_EQUAL(0, sdCardManager.beginCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.removeCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.openCalls);
    TEST_ASSERT_TRUE(sdCardManager.data.empty());
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") !=
                     std::string::npos);
}

void testUploadRejectsMissingContentType() {
    WiFiClientTmp client;
    client.buildUploadRequest(std::vector<byte>(), true, false);
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    webServerManager.sdCardManager = &sdCardManager;
    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_EQUAL(0, sdCardManager.beginCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.removeCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.openCalls);
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") !=
                     std::string::npos);
}

void testUploadRejectsMissingFilename() {
    WiFiClientTmp client;
    client.buildUploadRequest(std::vector<byte>(4, 'x'), true, true, false);
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    webServerManager.sdCardManager = &sdCardManager;
    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_EQUAL(0, sdCardManager.beginCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.removeCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.openCalls);
    TEST_ASSERT_TRUE(sdCardManager.data.empty());
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") !=
                     std::string::npos);
}

void testUploadRejectsEmptyFilename() {
    WiFiClientTmp client;
    client.buildUploadRequest(std::vector<byte>(4, 'x'), true, true, true,
                              "------WebKitFormBoundarypCbNHDfIecaHIigU", "");
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    webServerManager.sdCardManager = &sdCardManager;
    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_EQUAL(0, sdCardManager.beginCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.removeCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.openCalls);
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") !=
                     std::string::npos);
}

void testUploadRejectsFilenamePathSeparators() {
    WiFiClientTmp client;
    client.buildUploadRequest(std::vector<byte>(4, 'x'), true, true, true,
                              "------WebKitFormBoundarypCbNHDfIecaHIigU",
                              "../outside.bin");
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    webServerManager.sdCardManager = &sdCardManager;
    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_EQUAL(0, sdCardManager.beginCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.removeCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.openCalls);
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") !=
                     std::string::npos);
}

void testUploadRejectsUnexpectedInitialBoundary() {
    WiFiClientTmp client;
    client.buildUploadRequest(std::vector<byte>(4, 'x'), true, true, true,
                              "------unexpected-boundary");
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    webServerManager.sdCardManager = &sdCardManager;
    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_EQUAL(0, sdCardManager.beginCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.removeCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.openCalls);
    TEST_ASSERT_TRUE(sdCardManager.data.empty());
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") !=
                     std::string::npos);
}

void testUploadRejectsMissingBoundaryParameter() {
    WiFiClientTmp client;
    client.buildUploadRequest(std::vector<byte>(), true, true, true,
                              "------WebKitFormBoundarypCbNHDfIecaHIigU",
                              "test.bin", false);
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
    webServerManager.sdCardManager = &sdCardManager;
    webServerManager.handleFileUpload(&client);

    TEST_ASSERT_EQUAL(0, sdCardManager.beginCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.removeCalls);
    TEST_ASSERT_EQUAL(0, sdCardManager.openCalls);
    TEST_ASSERT_TRUE(client.response.find("400 Bad Request") !=
                     std::string::npos);
}

void testParseQueryParameters() {
    WebServerManager webServerManager;
    String query = "GET /delete?file=test.txt&other=123 HTTP/1.1";
    std::map<String, String> params =
        webServerManager.parseQueryParameters(query, 12);

    TEST_ASSERT_EQUAL(2, static_cast<int>(params.size()));
    TEST_ASSERT_EQUAL_STRING("test.txt", params["file"].c_str());
    TEST_ASSERT_EQUAL_STRING("123", params["other"].c_str());
}

void testParseQueryParametersDecodesFormValues() {
    WebServerManager webServerManager;
    String query =
        "POST /?ssid=home+network&note=tea%26coffee%3Dhot&empty= HTTP/1.1";
    std::map<String, String> params =
        webServerManager.parseQueryParameters(query, 7);

    TEST_ASSERT_EQUAL(3, static_cast<int>(params.size()));
    TEST_ASSERT_EQUAL_STRING("home network", params["ssid"].c_str());
    TEST_ASSERT_EQUAL_STRING("tea&coffee=hot", params["note"].c_str());
    TEST_ASSERT_EQUAL_STRING("", params["empty"].c_str());
}

void testParseQueryParametersPreservesMalformedEscapes() {
    WebServerManager webServerManager;
    String query = "GET /?value=bad%2&other=bad%XZ HTTP/1.1";
    std::map<String, String> params =
        webServerManager.parseQueryParameters(query, 6);

    TEST_ASSERT_EQUAL(2, static_cast<int>(params.size()));
    TEST_ASSERT_EQUAL_STRING("bad%2", params["value"].c_str());
    TEST_ASSERT_EQUAL_STRING("bad%XZ", params["other"].c_str());
}

void testParseQueryParametersHandlesEmptyQuery() {
    WebServerManager webServerManager;
    String query = "GET /? HTTP/1.1";
    std::map<String, String> params =
        webServerManager.parseQueryParameters(query, 6);

    TEST_ASSERT_TRUE(params.empty());
}

void testCopyBytesCopiesRequestedRange() {
    uint8_t destination[5] = {0, 0, 0, 0, 0};
    uint8_t source[3] = {1, 2, 3};

    copyBytes(destination, source, 3);

    TEST_ASSERT_EQUAL_UINT8_ARRAY(source, destination, 3);
    source[0] = 5;
    TEST_ASSERT_EQUAL_UINT8(1, destination[0]);
}

void testCopyBytesCopiesAtOffsetWithoutOverwritingNeighbors() {
    uint8_t destination[5] = {3, 0, 0, 0, 9};
    uint8_t source[3] = {1, 2, 3};

    copyBytes(destination + 1, source, 3);

    TEST_ASSERT_EQUAL_UINT8(3, destination[0]);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(source, destination + 1, 3);
    TEST_ASSERT_EQUAL_UINT8(9, destination[4]);
}

void testCopyBytesHandlesZeroLength() {
    uint8_t destination[2] = {7, 9};
    uint8_t source[2] = {1, 2};

    copyBytes(destination, source, 0);

    TEST_ASSERT_EQUAL_UINT8(7, destination[0]);
    TEST_ASSERT_EQUAL_UINT8(9, destination[1]);
}

void setUp(void) {
    ArduinoFakeReset();
    fakeMillis = 0;
    std::memset(eepromData, 0xff, sizeof(eepromData));
    When(Method(ArduinoFake(Function), millis))
        .AlwaysDo([]() { return fakeMillis++; });
    When(Method(ArduinoFake(Function), delay))
        .AlwaysDo([](unsigned long duration) { fakeMillis += duration; });
    When(Method(ArduinoFake(EEPROM), write))
        .AlwaysDo([](int index, uint8_t value) { eepromData[index] = value; });
    When(Method(ArduinoFake(EEPROM), read))
        .AlwaysDo([](int index) { return eepromData[index]; });
    When(OverloadedMethod(ArduinoFake(Serial), println, size_t(const String &)))
        .AlwaysReturn(0);
    When(OverloadedMethod(ArduinoFake(Serial), println, size_t(const char *)))
        .AlwaysReturn(0);
    When(OverloadedMethod(ArduinoFake(Serial), print, size_t(const String &)))
        .AlwaysReturn(0);
    When(OverloadedMethod(ArduinoFake(Serial), print, size_t(const char *)))
        .AlwaysReturn(0);
}

void tearDown(void) {}

int runUnitTests() {
    UNITY_BEGIN();
    RUN_TEST(testDeviceInfoIsJsonWithoutAccessingSD);
    RUN_TEST(testFileListReturnsCacheWithoutAccessingSD);
    RUN_TEST(testUploadPageEscapesFileNames);
    RUN_TEST(testSetupSavesNameAndEndpointReturnsIt);
    RUN_TEST(testSetupDoesNotLogRequestHeaders);
    RUN_TEST(testSetupRestartsAfterResponseEvenWithMoreClientData);
    RUN_TEST(testSetupRestartsPromptlyWhenClientDisconnects);
    RUN_TEST(testRejectedSetupDoesNotRestart);
    RUN_TEST(testSetupRejectsInvalidNameWithoutSavingOrRestarting);
    RUN_TEST(testSetupPageEscapesNameAndDisplaysDefault);
    RUN_TEST(testDeviceInfoUnavailableInSetupMode);
    RUN_TEST(testOtherGetPathsStillServeWebsite);
    RUN_TEST(testNetworkScanLifecycleAndJson);
    RUN_TEST(testNetworkScanFailuresAndSetupMode);
    RUN_TEST(testNetworkScanCapsResults);
    RUN_TEST(testSetupRejectsMissingAndOversizeSsid);
    RUN_TEST(testUploadPollsWhileWaitingForHeaders);
    RUN_TEST(testUploadPollsWhileWaitingForFileData);
    RUN_TEST(testUploadFileCopiesFixtureExactly);
    RUN_TEST(testUploadFilePreservesBinaryDataAcrossBufferBoundaries);
    RUN_TEST(testUploadFileAcceptsEmptyFile);
    RUN_TEST(testUploadRejectsMissingRequiredHeader);
    RUN_TEST(testUploadRejectsMissingContentType);
    RUN_TEST(testUploadRejectsMissingFilename);
    RUN_TEST(testUploadRejectsEmptyFilename);
    RUN_TEST(testUploadRejectsFilenamePathSeparators);
    RUN_TEST(testUploadRejectsUnexpectedInitialBoundary);
    RUN_TEST(testUploadRejectsMissingBoundaryParameter);
    RUN_TEST(testParseQueryParameters);
    RUN_TEST(testParseQueryParametersDecodesFormValues);
    RUN_TEST(testParseQueryParametersPreservesMalformedEscapes);
    RUN_TEST(testParseQueryParametersHandlesEmptyQuery);
    RUN_TEST(testCopyBytesCopiesRequestedRange);
    RUN_TEST(testCopyBytesCopiesAtOffsetWithoutOverwritingNeighbors);
    RUN_TEST(testCopyBytesHandlesZeroLength);
    return UNITY_END();
}

int main(void) { return runUnitTests(); }
