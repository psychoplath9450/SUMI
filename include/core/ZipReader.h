/**
 * @file ZipReader.h
 * @brief ZIP File Reader with Streaming Support
 * @version 1.4.2
 *
 * Memory-efficient ZIP reading:
 * - Pre-allocated decompression buffers (43KB total)
 * - Streaming decompression in chunks
 * - No full-file loading to RAM
 *
 * Buffer management:
 * - Call ZipReader_preallocateBuffer() early in setup()
 * - Call ZipReader_freeBuffers() before WiFi operations
 */

#ifndef SUMI_ZIP_READER_H
#define SUMI_ZIP_READER_H

#include <Arduino.h>
#include <SD.h>
#include "config.h"

// =============================================================================
// Buffer Management - Call from main.cpp
// =============================================================================

/**
 * Pre-allocate decompression buffers (43KB)
 * Call EARLY in setup() before heap fragments
 */
void ZipReader_preallocateBuffer();

/**
 * Free buffers to reclaim memory for WiFi
 * Only safe when no ZIP operations in progress
 */
void ZipReader_freeBuffers();

/**
 * Check if buffers are allocated
 */
bool ZipReader_buffersAllocated();

/**
 * Log buffer status for debugging
 */
void ZipReader_logStatus();

/**
 * Force reset in-use flags (recovery)
 */
void ZipReader_resetFlags();

// =============================================================================
// ZipReader Class
// =============================================================================

class ZipReader {
public:
    ZipReader();
    ~ZipReader();
    
    /**
     * Open a ZIP file
     * @param zipPath Path to .zip or .epub file
     * @return true if successful
     */
    bool open(const String& zipPath);
    
    /**
     * Close the ZIP file
     */
    void close();
    
    /**
     * Check if ZIP is open
     */
    bool isOpen() const { return _isOpen; }
    
    /**
     * Get number of files in archive
     */
    int getFileCount() const { return _fileCount; }
    
    /**
     * Get error message
     */
    const String& getError() const { return _error; }
    
    // ==========================================================================
    // File Information
    // ==========================================================================
    
    /**
     * Get file size (uncompressed)
     * @param innerPath Path inside ZIP
     * @param outSize Receives file size
     * @return true if file found
     */
    bool getFileSize(const String& innerPath, size_t& outSize);
    
    /**
     * Check if file exists in archive
     */
    bool fileExists(const String& innerPath);
    
    /**
     * Get filename at index
     */
    bool getFilename(int index, char* buffer, size_t bufferSize);
    
    // ==========================================================================
    // Streaming Read Methods
    // ==========================================================================
    
    /**
     * Read a chunk of a file
     * For streaming processing without loading entire file
     * 
     * @param innerPath Path inside ZIP
     * @param offset Byte offset to start reading
     * @param buffer Output buffer
     * @param length Bytes to read
     * @return Bytes actually read (0 on error)
     */
    size_t readFileChunk(const String& innerPath, size_t offset, uint8_t* buffer, size_t length);
    
    /**
     * Stream entire file to output file
     * Decompresses in chunks, never loads whole file to RAM
     * 
     * @param innerPath Path inside ZIP
     * @param outFile File to write to (must be open for writing)
     * @param chunkSize Chunk size for streaming (default 1KB)
     * @return true if successful
     */
    bool streamFileTo(const String& innerPath, File& outFile, size_t chunkSize = 1024);
    
    /**
     * Stream file through callback
     * @param innerPath Path inside ZIP
     * @param callback Called with each chunk of data
     * @param userData Passed to callback
     * @param chunkSize Chunk size
     * @return true if successful
     */
    typedef bool (*StreamCallback)(const uint8_t* data, size_t len, void* userData);
    bool streamFileCallback(const String& innerPath, StreamCallback callback, void* userData, size_t chunkSize = 1024);
    
    // ==========================================================================
    // Full Read (Use sparingly - only for small files)
    // ==========================================================================
    
    /**
     * Read entire file to String
     * WARNING: Only use for small files (<10KB) like container.xml
     * For large files, use streamFileTo() instead
     */
    String readFileToString(const String& innerPath, size_t maxSize = 32768);
    
private:
    bool _isOpen;
    int _fileCount;
    String _path;
    String _stdioPath;  // Path with /sd prefix for stdio
    String _error;
    
    void* _archive;     // miniz mz_zip_archive*
    
    /**
     * Find file index by path
     * Handles case-insensitive matching and path normalization
     */
    int findFileIndex(const String& innerPath);
    
    /**
     * Normalize path for comparison
     */
    static String normalizePath(const String& path);
};

// =============================================================================
// Global Instance
// =============================================================================
extern ZipReader zipReader;

#endif // SUMI_ZIP_READER_H
