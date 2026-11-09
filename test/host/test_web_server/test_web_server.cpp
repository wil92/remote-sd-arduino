#include <Arduino.h>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <unity.h>
#include <vector>

#include "../../../lib/webserver/web_server_manager.h"

#define STRINGIGY(v) #v
#define TOSTRING(v) STRINGIGY(v)

class SDCardManagerTmp : public SDCardManager {
public:
    std::vector<byte> data;
    size_t beginCalls = 0;
    size_t removeCalls = 0;
    size_t openCalls = 0;
    size_t closeCalls = 0;
    size_t endCalls = 0;

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
        char testFilePath[] = TOSTRING(TEST_FILE_PATH);
        std::ifstream file(testFilePath, std::ios::binary);
        TEST_ASSERT_TRUE_MESSAGE(file.is_open(),
                                 "Test fixture could not be opened");

        std::vector<byte> payload;
        char value;
        while (file.get(value)) {
            payload.push_back(static_cast<byte>(value));
        }
        buildUploadRequest(payload);
    }

private:
    void addData(const char *message) {
        data.insert(data.end(), message, message + std::strlen(message));
    }

    void addBytes(const std::vector<byte> &bytes) {
        data.insert(data.end(), bytes.begin(), bytes.end());
    }
};

void testUploadFileCopiesFixtureExactly() {
    WiFiClientTmp client;
    client.buildFixtureUploadRequest();
    client.readStringUntil('\n');

    WebServerManager webServerManager;
    SDCardManagerTmp sdCardManager;
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

void setUp(void) {}

void tearDown(void) {}

int runUnitTests() {
    UNITY_BEGIN();
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
