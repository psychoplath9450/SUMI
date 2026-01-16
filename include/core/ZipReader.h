/**
 * @file ZipReader.h
 * @brief ZIP File Reader for Sumi E-Reader
 * @version 2.1.16
 *
 * Features:
 * - Read files from ZIP archives (like .epub)
 * - Streaming decompression for low memory usage
 * - Support for both DEFLATE and STORE methods
 * - Efficient chunk-based reading
 *
 * Usage:
 *   ZipReader zip;
 *   if (zip.open("/books/mybook.epub")) {
 *       String content = zip.readFile("OEBPS/content.opf");
 *       // or stream to a handler:
 *       zip.streamFile("OEBPS/chapter1.xhtml", myHandler, 1024);
 *       zip.close();
 *   }
 */

#ifndef SUMI_ZIP_READER_H
#define SUMI_ZIP_READER_H

#include <Arduino.h>
#include <SD.h>
#include <functional>

// Note: miniz types are only used internally in ZipReader.cpp
// _archive is stored as void* to avoid including miniz.h here

// =============================================================================
// Streaming Callback Type
// =============================================================================
// Called with chunks of decompressed data
using ZipStreamCallback = std::function<void(const uint8_t* data, size_t len)>;

// =============================================================================
// ZIP Reader Class
// =============================================================================
class ZipReader {
public:
    ZipReader();
    ~ZipReader();
    
    // =========================================================================
    // Open / Close
    // =========================================================================
    
    /**
     * Open a ZIP file for reading
     * @param zipPath Full path to ZIP file on SD card
     * @return true if opened successfully
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
    
    // =========================================================================
    // File Information
    // =========================================================================
    
    /**
     * Check if a file exists in the ZIP
     * @param innerPath Path within ZIP (e.g., "META-INF/container.xml")
     */
    bool hasFile(const String& innerPath);
    
    /**
     * Get the uncompressed size of a file
     * @param innerPath Path within ZIP
     * @return Uncompressed size, or 0 if not found
     */
    size_t getFileSize(const String& innerPath);
    
    /**
     * Get number of files in ZIP
     */
    int getFileCount() const { return _fileCount; }
    
    /**
     * Get filename at index
     */
    String getFileName(int index);
    
    // =========================================================================
    // File Reading
    // =========================================================================
    
    /**
     * Read entire file into memory
     * For small files like container.xml, content.opf
     * @param innerPath Path within ZIP
     * @return File contents as String, empty if failed
     */
    String readFile(const String& innerPath);
    
    /**
     * Read file into byte buffer
     * @param innerPath Path within ZIP
     * @param outData Output buffer (caller must free!)
     * @param outSize Output size
     * @return true if successful
     */
    bool readFileToBuffer(const String& innerPath, uint8_t** outData, size_t* outSize);
    
    /**
     * Stream file in chunks via callback
     * For large files like chapter HTML
     * @param innerPath Path within ZIP
     * @param callback Called with each chunk of data
     * @param chunkSize Size of each chunk (default 1KB)
     * @return true if successful
     */
    bool streamFile(const String& innerPath, ZipStreamCallback callback, size_t chunkSize = 1024);
    
    /**
     * Stream file to a Print object (e.g., File, Serial)
     * @param innerPath Path within ZIP
     * @param output Print destination
     * @param chunkSize Size of each chunk
     * @return true if successful
     */
    bool streamFileTo(const String& innerPath, Print& output, size_t chunkSize = 1024);
    
    // =========================================================================
    // Error Handling
    // =========================================================================
    
    /**
     * Get last error message
     */
    const String& getError() const { return _error; }
    
    /**
     * Get ZIP file path
     */
    const String& getPath() const { return _path; }

private:
    String _path;
    String _stdioPath;  // Full path with /sd prefix for stdio operations
    String _error;
    bool _isOpen;
    int _fileCount;
    
    // Opaque pointer to miniz archive
    // (actual type is mz_zip_archive)
    void* _archive;
    
    // Internal helpers
    bool loadFileStat(const char* filename, void* fileStat);
    long getDataOffset(const void* fileStat);
    bool inflateStream(FILE* file, size_t compSize, size_t uncompSize, 
                       ZipStreamCallback callback, size_t chunkSize);
};

// =============================================================================
// Utility Functions
// =============================================================================

/**
 * Check if path is a ZIP file (by extension)
 */
bool isZipFile(const String& path);

/**
 * Check if path is an EPUB file (which is a ZIP)
 */
bool isEpubZipFile(const String& path);

#endif // SUMI_ZIP_READER_H
