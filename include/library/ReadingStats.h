/**
 * @file ReadingStats.h
 * @brief Reading statistics tracking for the Library plugin
 * @version 1.3.0
 */

#ifndef SUMI_LIBRARY_READING_STATS_H
#define SUMI_LIBRARY_READING_STATS_H

#include <Arduino.h>
#include <SD.h>

namespace Library {

/**
 * @brief Tracks reading statistics across sessions
 */
struct ReadingStats {
    static constexpr uint32_t MAGIC = 0x53544154;  // "STAT"
    
    uint32_t magic = MAGIC;
    uint32_t totalPagesRead = 0;
    uint32_t totalMinutesRead = 0;
    uint32_t sessionPagesRead = 0;
    uint32_t sessionStartTime = 0;
    
    void startSession() {
        sessionPagesRead = 0;
        sessionStartTime = millis();
    }
    
    void recordPageTurn() {
        totalPagesRead++;
        sessionPagesRead++;
    }
    
    uint32_t getSessionMinutes() const {
        return (millis() - sessionStartTime) / 60000;
    }
    
    void endSession() {
        totalMinutesRead += getSessionMinutes();
    }
    
    bool save(const char* path) {
        File f = SD.open(path, FILE_WRITE);
        if (!f) return false;
        f.write((uint8_t*)this, sizeof(ReadingStats));
        f.close();
        return true;
    }
    
    bool load(const char* path) {
        File f = SD.open(path, FILE_READ);
        if (!f) return false;
        if (f.size() != sizeof(ReadingStats)) {
            f.close();
            return false;
        }
        f.read((uint8_t*)this, sizeof(ReadingStats));
        f.close();
        return magic == MAGIC;
    }
};

/**
 * @brief Information about the last book being read (for sleep screen)
 */
struct LastBookInfo {
    static constexpr uint32_t MAGIC = 0x4C415354;  // "LAST"
    
    uint32_t magic = MAGIC;
    char title[64] = {0};
    char author[48] = {0};
    char coverPath[96] = {0};
    int chapter = 0;
    int page = 0;
    int totalPages = 0;
    float progress = 0.0f;  // 0.0 - 1.0
    
    bool save(const char* path) {
        File f = SD.open(path, FILE_WRITE);
        if (!f) return false;
        f.write((uint8_t*)this, sizeof(LastBookInfo));
        f.close();
        return true;
    }
    
    bool load(const char* path) {
        File f = SD.open(path, FILE_READ);
        if (!f) return false;
        if (f.size() != sizeof(LastBookInfo)) {
            f.close();
            return false;
        }
        f.read((uint8_t*)this, sizeof(LastBookInfo));
        f.close();
        return magic == MAGIC;
    }
};

} // namespace Library

#endif // SUMI_LIBRARY_READING_STATS_H
