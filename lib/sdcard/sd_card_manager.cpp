#include "sd_card_manager.h"

void SDCardManager::begin() {
#ifdef ARDUINO_ARCH_ESP32
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
#endif
    if (!SD.begin(SD_CS, SPI)) {
        Serial.println("Card Mount Failed");
    }
}

void SDCardManager::end() {
    SD.end();
    SPI.end();
}

std::vector<FileInfo> SDCardManager::listFiles(bool beginConnectionToSD) {
    if (beginConnectionToSD) {
        begin();
    }

    std::vector<FileInfo> files;

    File root = SD.open("/");
    if (!root) {
        Serial.println("File Not Found");
        return files;
    }
    if (!root.isDirectory()) {
        Serial.println("Not a directory");
        return files;
    }

    File file = root.openNextFile();
    while (file) {
        if (!file.isDirectory()) {
            files.push_back({String(file.name()), file.size()});
        }
        file = root.openNextFile();
    }

    if (beginConnectionToSD) {
        end();
    }

    return files;
}

void SDCardManager::removeFile(const String &fileName,
                               bool beginConnectionToSD) {
    if (beginConnectionToSD) {
        begin();
    }
    if (SD.remove("/" + fileName)) {
        Serial.println("File removed");
    } else {
        Serial.println("File not found");
    }
    if (beginConnectionToSD) {
        end();
    }
}

void SDCardManager::openFile(const String &fileName) {
    file_.reset(new File(SD.open("/" + fileName, FILE_WRITE)));
}

void SDCardManager::closeFile() {
    if (file_) {
        file_->close();
        file_.reset();
    }
}

void SDCardManager::appendFile(const String &fileName, const byte *chunk,
                               const size_t length) {
    if (length > 0) {
        if (!file_) {
            begin();
        }

        File *file;
        if (!file_) {
            file = new File(SD.open("/" + fileName, FILE_APPEND));
        } else {
            file = file_.get();
        }
        if (!file) {
            Serial.println("Error opening file for appending");
        }
        if (file && !file->write(chunk, length)) {
            Serial.println("Failed to write to the file");
        }
        if (!file_ && file) {
            file->close();
        }

        if (!file_) {
            end();
        }
    }
}
