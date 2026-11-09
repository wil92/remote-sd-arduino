#pragma once

#ifndef PLATFORM_NATIVE
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

#define SD_SCK GPIO_NUM_4
#define SD_MISO GPIO_NUM_5
#define SD_MOSI GPIO_NUM_6
#define SD_CS GPIO_NUM_7
#else
#include "sdcardfake.h"

using namespace sdcardfake;

#define SD_SCK 1
#define SD_MISO 2
#define SD_MOSI 3
#define SD_CS 4
#endif

#include "file_info.h"

#include <memory>
#include <vector>

class SDCardManager {
    std::unique_ptr<File> file_;

public:
    std::vector<FileInfo> listFiles(bool beginConnectionToSD);

    void removeFile(const String &fileName, bool beginConnectionToSD);

    void openFile(const String &fileName);

    void closeFile();

    void appendFile(const String &fileName, const byte *chunk, size_t size);

    void begin();

    void end();
};
