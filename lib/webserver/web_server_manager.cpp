#include "web_server_manager.h"

#include "views/server-config.index.h"
#include "views/upload-files.index.h"

#include <cstdlib>

#ifndef ARDUINO_ARCH_ESP32
#include <iostream>
#endif

#define BUFFER_SIZE 512

void WebServerManager::setup() { server.reset(new WiFiServer(80)); }

void WebServerManager::run() const {
    server->begin();
    server->setNoDelay(true);
}

void WebServerManager::loop() { checkForConnections(); }

void WebServerManager::checkForConnections() {
    WiFiClient client = server->accept();
    if (client) {
        Serial.println("Client connected");

        const unsigned long connectionTime = millis();
        bool resetFlag = false;

        while (client.connected() &&
               millis() - connectionTime <= CLIENT_TIMEOUT) {
            if (checkResetButton) {
                checkResetButton();
            }
            if (client.available()) {
                String line = client.readStringUntil('\n');
                // Serial.println(line);
                resetFlag = checkEndpoints(&client, line);
            }
        }

        Serial.println("Client disconnected");
        client.stop();

        if (resetFlag && rf) {
            rf();
        }
    }
}

bool WebServerManager::checkEndpoints(WiFiClient *client,
                                      const String &message) {
    Serial.println("-" + message);
    if (message.startsWith("POST /upload")) {
        connectSD();
        handleFileUpload(client);
        disconnectSD();
        return false;
    }
    if (message.startsWith("DELETE /")) {
        connectSD();
        handleFileDeletion(client, message);
        disconnectSD();
    }
    if (message.startsWith("GET /")) {
        handleWebsite(client, message);
    }
    if (message.startsWith("POST /?")) {
        handleUpdateConfig(client, message);
        return true;
    }
    return false;
}

void WebServerManager::handleWebsite(WiFiClient *client,
                                     const String &message) {
    client->print(prepareHeaders(200));
    client->print(flashManager->isSetup ? prepareUploadHtmlPage()
                                        : prepareIndexHtmlPage());
    client->print("\r\n");
    consumeIncomingData(client);
}

void WebServerManager::handleUpdateConfig(WiFiClient *client,
                                          const String &message) const {
    const int index = static_cast<int>(String("POST /").length()) + 1;
    std::map<String, String> params = parseQueryParameters(message, index);
    for (std::map<String, String>::const_iterator parameter = params.begin();
         parameter != params.end(); ++parameter) {
        setParameter(parameter->first, parameter->second);
    }
    flashManager->saveSetup();
    flashManager->loadSetup();
    client->print(prepareHeaders(204));
    consumeIncomingData(client);
}

void WebServerManager::handleFileDeletion(WiFiClient *client,
                                          const String &message) {
    const int index = static_cast<int>(String("DELETE /").length()) + 1;
    std::map<String, String> params = parseQueryParameters(message, index);
    Serial.println("->" + params["file"]);
    if (params.find("file") != params.end()) {
        sdCardManager->removeFile(params["file"], true);
    }
    filesInSD = sdCardManager->listFiles(true);
    consumeIncomingData(client);
    client->print(prepareHeaders(204));
}

void WebServerManager::handleFileUpload(WiFiClient *client) {
    // read headers
    auto headers = consumeHeaders(client);
    if (headers.find("Content-Length") == headers.end() ||
        headers.find("Content-Type") == headers.end()) {
        client->print(prepareHeaders(400));
        return;
    }
    String boundary = "--";
    String contentType = headers["Content-Type"];
    int boundaryIndex = contentType.indexOf("boundary=");
    if (boundaryIndex == -1) {
        client->print(prepareHeaders(400));
        return;
    }
    boundary += contentType.substring(
        boundaryIndex + static_cast<int>(String("boundary=").length()));

    // initial boundary
    String line = client->readStringUntil('\n');
    if (!line.startsWith(boundary)) {
        client->print(prepareHeaders(400));
        return;
    }

    // file info
    headers = consumeHeaders(client);
    if (headers.find("Content-Disposition") == headers.end()) {
        client->print(prepareHeaders(400));
        return;
    }
    String contentDisposition = headers["Content-Disposition"];
    const String fileNameMarker = "filename=\"";
    const int fileNameMarkerIndex = contentDisposition.indexOf(fileNameMarker);
    if (fileNameMarkerIndex == -1) {
        client->print(prepareHeaders(400));
        return;
    }
    const int fileNameIndex =
        fileNameMarkerIndex + static_cast<int>(fileNameMarker.length());
    const int fileNameEndIndex =
        contentDisposition.indexOf("\"", fileNameIndex);
    if (fileNameEndIndex == -1 || fileNameEndIndex == fileNameIndex) {
        client->print(prepareHeaders(400));
        return;
    }
    String fileName =
        contentDisposition.substring(fileNameIndex, fileNameEndIndex);
    if (fileName == "." || fileName == ".." || fileName.indexOf('/') != -1 ||
        fileName.indexOf('\\') != -1) {
        client->print(prepareHeaders(400));
        return;
    }
    sdCardManager->begin();
    sdCardManager->removeFile(fileName, false);

    // file data
    String buffer;
    byte bBuffer[BUFFER_SIZE * 2];
    byte bSubBuffer[BUFFER_SIZE * 2];
    size_t length = 0, subLength = 0;

    sdCardManager->openFile(fileName);
    while (client->connected()) {
        if (checkResetButton) {
            checkResetButton();
        }
        if (client->available()) {
            byte bCh = client->read();
            char ch = static_cast<char>(bCh);
            buffer += ch;
            if (ch == '\r') {
                int prevCh = bCh;
                bCh = client->read();
                ch = static_cast<char>(bCh);
                buffer += ch;
                if (ch == '\n' && buffer.indexOf(boundary) >= 0) {
                    sdCardManager->appendFile(fileName, bSubBuffer,
                                              subLength - 1); // remove \r
                    break;
                }
                copyBytes(bSubBuffer + subLength, bBuffer, length);
                subLength += length;
                bSubBuffer[subLength++] = prevCh;
                buffer = "";
                length = 0;
            }
            bBuffer[length++] = bCh;
            if (subLength + length >= BUFFER_SIZE) {
                copyBytes(bSubBuffer + subLength, bBuffer, length);
                subLength += length;
                sdCardManager->appendFile(fileName, bSubBuffer, subLength);
                buffer = "";
                length = subLength = 0;
            }
        }
    }

    filesInSD = sdCardManager->listFiles(false);
    sdCardManager->closeFile();
    sdCardManager->end();
    client->print(prepareHeaders(204));
}

std::map<String, String>
WebServerManager::consumeHeaders(WiFiClient *client) const {
    std::map<String, String> headers;
    while (client->connected()) {
        if (checkResetButton) {
            checkResetButton();
        }
        if (client->available()) {
            String line = client->readStringUntil('\n');
            if (line.length() <= 2) {
                break;
            }
            String key = line.substring(0, line.indexOf(":"));
            String value =
                line.substring(line.indexOf(":") + 2, line.length() - 1);
#ifndef ARDUINO_ARCH_ESP32
            std::cout << "Header: " << key.c_str() << " -> " << value.c_str()
                      << std::endl;
#endif
            headers[key] = value;
        }
    }
    return headers;
}

String WebServerManager::consumeIncomingData(WiFiClient *client) const {
    while (client->connected()) {
        if (checkResetButton) {
            checkResetButton();
        }
        if (client->available()) {
            String line = client->readStringUntil('\n');
            Serial.println("+" + line);
            if (line.length() <= 2) {
                Serial.println("finish consuming");
                break;
            }
        }
    }
    return "";
}

std::map<String, String>
WebServerManager::parseQueryParameters(const String &query,
                                       int startPoint) const {
    std::map<String, String> params;
    String key = "", value = "";
    bool flag = false;
    for (int index = startPoint;
         index < static_cast<int>(query.length()) && query[index] != ' ';
         index++) {
        if (!flag) {
            if (query[index] == '=') {
                flag = true;
            } else {
                key += query[index];
            }
        } else {
            if (query[index] == '&') {
                flag = false;
                params[urlDecode(key)] = urlDecode(value);
                key = "";
                value = "";
            } else {
                value += query[index];
            }
        }
    }
    if (key != "") {
        params[urlDecode(key)] = urlDecode(value);
    }
    return params;
}

String WebServerManager::urlDecode(const String &src) {
    String ret;
    for (int i = 0; i < static_cast<int>(src.length()); ++i) {
        if (src[i] == '%') {
            if (i + 2 < static_cast<int>(src.length())) {
                char encodedByte[3] = {src[i + 1], src[i + 2], '\0'};
                char *end = nullptr;
                const long decodedByte = std::strtol(encodedByte, &end, 16);
                if (end != encodedByte && *end == '\0') {
                    ret += static_cast<char>(decodedByte);
                    i += 2;
                    continue;
                }
            }
            ret += src[i];
        } else if (src[i] == '+') {
            ret += ' ';
        } else {
            ret += src[i];
        }
    }
    return ret;
}

void WebServerManager::setParameter(const String &key,
                                    const String &value) const {
    flashManager->setVariable(key, value);

    // only on debug
    Serial.print("Parameter: (\"");
    Serial.print(key);
    Serial.print("\": \"");
    Serial.print(value);
    Serial.println("\")");
}

String WebServerManager::prepareHeaders(int code) const {
    return prepareBaseHeaders(code) +
           "Connection: close\r\n" + // the connection will be closed after
                                     // completion of the response
           // "Refresh: 30\r\n" + // refresh the page automatically every 5 sec
           "\r\n";
}

String WebServerManager::prepareBaseHeaders(int code) const {
    switch (code) {
    case 500:
        return String("HTTP/1.1 500 Internal Server Error\r\nContent-Type: "
                      "text/html\r\n");
    case 400:
        return String(
            "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html\r\n");
    case 404:
        return String("HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n");
    case 204:
        return String("HTTP/1.1 204 No Content\r\n");
    case 200:
    default:
        return String("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n");
    }
}

String WebServerManager::prepareIndexHtmlPage() const {
    String result = String(server_config_index_html) + "\r\n";

    // Network
    result.replace("$$SSID$$", flashManager->ssidNetwork);
    result.replace("$$PASSWORD$$", flashManager->passwordNetwork);

    return result;
}

String WebServerManager::prepareUploadHtmlPage() {
    String result = String(upload_files_index_html) + "\r\n";

    String filesList;
    if (filesInSD.empty()) {
        // load files from SD card only if not loaded yet
        connectSD();
        filesInSD = sdCardManager->listFiles(true);
        disconnectSD();
    }
    for (const FileInfo &file : filesInSD) {
        filesList += "<li>" + file.name + " - " + String(file.size) +
                     "<button onclick=\"del('" + file.name +
                     "')\">x</button></li>";
    }

    // Network
    result.replace("$$FILES$$", filesList);

    return result;
}
