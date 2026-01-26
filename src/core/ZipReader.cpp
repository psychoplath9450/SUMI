/**
 * @file ZipReader.cpp
 * @brief ZIP File Reader Implementation
 * @version 1.4.2
 *
 * Uses miniz for DEFLATE decompression with pre-allocated buffers.
 */

#include "core/ZipReader.h"

// Include miniz
extern "C" {
#include "miniz.h"
}

// =============================================================================
// Global Instance
// =============================================================================
ZipReader zipReader;

// =============================================================================
// Pre-allocated Decompression Buffers
// =============================================================================
// TINFL_LZ_DICT_SIZE is 32768 (32KB)
// tinfl_decompressor is ~11KB
// Total: ~43KB - MUST allocate before heap fragments

static uint8_t* _sharedDictBuffer = nullptr;
static bool _dictBufferInUse = false;
static tinfl_decompressor* _sharedDecompressor = nullptr;
static bool _decompressorInUse = false;

void ZipReader_preallocateBuffer() {
    if (_sharedDictBuffer != nullptr && _sharedDecompressor != nullptr) {
        Serial.printf("[ZIP] Buffers already allocated: dict=%p, decomp=%p\n", 
                      _sharedDictBuffer, _sharedDecompressor);
        return;
    }
    
    int freeHeap = ESP.getFreeHeap();
    Serial.printf("[ZIP] Pre-allocating buffers (heap=%d)\n", freeHeap);
    
    // Pre-allocate 32KB dictionary buffer
    if (_sharedDictBuffer == nullptr) {
        _sharedDictBuffer = (uint8_t*)malloc(TINFL_LZ_DICT_SIZE);
        if (_sharedDictBuffer) {
            memset(_sharedDictBuffer, 0, TINFL_LZ_DICT_SIZE);
            Serial.printf("[ZIP] Allocated 32KB dict buffer at %p\n", _sharedDictBuffer);
        } else {
            Serial.printf("[ZIP] CRITICAL: Failed to allocate dict buffer! (heap=%d)\n", ESP.getFreeHeap());
        }
    }
    
    // Pre-allocate decompressor
    if (_sharedDecompressor == nullptr) {
        _sharedDecompressor = tinfl_decompressor_alloc();
        if (_sharedDecompressor) {
            Serial.printf("[ZIP] Allocated decompressor at %p\n", _sharedDecompressor);
        } else {
            Serial.println("[ZIP] CRITICAL: Failed to allocate decompressor!");
        }
    }
    
    _dictBufferInUse = false;
    _decompressorInUse = false;
    
    if (!_sharedDictBuffer || !_sharedDecompressor) {
        Serial.println("[ZIP] WARNING: EPUB reading may fail due to memory issues");
    } else {
        Serial.printf("[ZIP] Pre-allocation complete. Heap: %d -> %d\n",
                      freeHeap, ESP.getFreeHeap());
    }
}

void ZipReader_freeBuffers() {
    if (_dictBufferInUse || _decompressorInUse) {
        Serial.println("[ZIP] WARNING: Cannot free buffers - in use!");
        return;
    }
    
    if (_sharedDictBuffer) {
        free(_sharedDictBuffer);
        _sharedDictBuffer = nullptr;
        Serial.println("[ZIP] Freed dict buffer");
    }
    
    if (_sharedDecompressor) {
        tinfl_decompressor_free(_sharedDecompressor);
        _sharedDecompressor = nullptr;
        Serial.println("[ZIP] Freed decompressor");
    }
    
    Serial.printf("[ZIP] Buffers freed, heap: %d\n", ESP.getFreeHeap());
}

bool ZipReader_buffersAllocated() {
    return _sharedDictBuffer != nullptr && _sharedDecompressor != nullptr;
}

void ZipReader_logStatus() {
    Serial.println("[ZIP] ========== Buffer Status ==========");
    Serial.printf("[ZIP] Dict: ptr=%p, inUse=%d\n", _sharedDictBuffer, _dictBufferInUse);
    Serial.printf("[ZIP] Decomp: ptr=%p, inUse=%d\n", _sharedDecompressor, _decompressorInUse);
    Serial.printf("[ZIP] Heap: %d bytes free\n", ESP.getFreeHeap());
    Serial.println("[ZIP] =====================================");
}

void ZipReader_resetFlags() {
    if (_dictBufferInUse || _decompressorInUse) {
        Serial.println("[ZIP] WARNING: Force-resetting in-use flags!");
    }
    _dictBufferInUse = false;
    _decompressorInUse = false;
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

ZipReader::ZipReader() 
    : _isOpen(false)
    , _fileCount(0)
    , _archive(nullptr) {
}

ZipReader::~ZipReader() {
    close();
}

// =============================================================================
// Open / Close
// =============================================================================

bool ZipReader::open(const String& zipPath) {
    close();
    
    _path = zipPath;
    _error = "";
    
    if (!SD.exists(zipPath)) {
        _error = "File not found: " + zipPath;
        Serial.printf("[ZIP] %s\n", _error.c_str());
        return false;
    }
    
    // Allocate miniz archive
    _archive = malloc(sizeof(mz_zip_archive));
    if (!_archive) {
        _error = "Failed to allocate ZIP archive";
        return false;
    }
    memset(_archive, 0, sizeof(mz_zip_archive));
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    
    // Build stdio path (miniz uses fopen which needs /sd prefix)
    _stdioPath = zipPath;
    if (!_stdioPath.startsWith("/sd")) {
        _stdioPath = "/sd" + _stdioPath;
    }
    
    Serial.printf("[ZIP] Opening: %s\n", zipPath.c_str());
    
    if (!mz_zip_reader_init_file(zip, _stdioPath.c_str(), 0)) {
        _error = "Failed to open ZIP: ";
        _error += mz_zip_get_error_string(zip->m_last_error);
        Serial.printf("[ZIP] %s\n", _error.c_str());
        free(_archive);
        _archive = nullptr;
        return false;
    }
    
    _isOpen = true;
    _fileCount = mz_zip_reader_get_num_files(zip);
    
    Serial.printf("[ZIP] Opened: %d files\n", _fileCount);
    
    return true;
}

void ZipReader::close() {
    if (_archive) {
        mz_zip_archive* zip = (mz_zip_archive*)_archive;
        mz_zip_reader_end(zip);
        free(_archive);
        _archive = nullptr;
    }
    
    _isOpen = false;
    _fileCount = 0;
    _path = "";
    _stdioPath = "";
    _error = "";
}

// =============================================================================
// File Information
// =============================================================================

bool ZipReader::getFileSize(const String& innerPath, size_t& outSize) {
    if (!_isOpen) return false;
    
    int index = findFileIndex(innerPath);
    if (index < 0) return false;
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    mz_zip_archive_file_stat stat;
    
    if (!mz_zip_reader_file_stat(zip, index, &stat)) {
        return false;
    }
    
    outSize = stat.m_uncomp_size;
    return true;
}

bool ZipReader::fileExists(const String& innerPath) {
    if (!_isOpen) return false;
    return findFileIndex(innerPath) >= 0;
}

bool ZipReader::getFilename(int index, char* buffer, size_t bufferSize) {
    if (!_isOpen || index < 0 || index >= _fileCount) return false;
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    return mz_zip_reader_get_filename(zip, index, buffer, bufferSize) > 0;
}

// =============================================================================
// Streaming Read
// =============================================================================

size_t ZipReader::readFileChunk(const String& innerPath, size_t offset, uint8_t* buffer, size_t length) {
    if (!_isOpen || !buffer || length == 0) return 0;
    
    int index = findFileIndex(innerPath);
    if (index < 0) {
        _error = "File not found: " + innerPath;
        return 0;
    }
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    mz_zip_archive_file_stat stat;
    
    if (!mz_zip_reader_file_stat(zip, index, &stat)) {
        _error = "Failed to stat file";
        return 0;
    }
    
    // For uncompressed (STORE) files, we can seek directly
    if (stat.m_method == 0) {
        // Direct read from uncompressed file
        // This is a simplified path - for DEFLATE we need full decompression
        // TODO: Implement partial decompression for DEFLATE
    }
    
    // For now, extract entire file and copy chunk
    // This is not ideal for large files but works for container.xml etc.
    size_t fileSize = stat.m_uncomp_size;
    
    if (offset >= fileSize) return 0;
    
    // Allocate temp buffer for full extraction
    // Only do this for small files
    if (fileSize > 64 * 1024) {
        _error = "File too large for chunk read, use streamFileTo()";
        Serial.printf("[ZIP] %s: %s (%d bytes)\n", _error.c_str(), innerPath.c_str(), (int)fileSize);
        return 0;
    }
    
    uint8_t* tempBuffer = (uint8_t*)malloc(fileSize);
    if (!tempBuffer) {
        _error = "Failed to allocate temp buffer";
        return 0;
    }
    
    if (!mz_zip_reader_extract_to_mem(zip, index, tempBuffer, fileSize, 0)) {
        _error = "Failed to extract file";
        free(tempBuffer);
        return 0;
    }
    
    // Copy requested chunk
    size_t available = fileSize - offset;
    size_t toCopy = (length < available) ? length : available;
    memcpy(buffer, tempBuffer + offset, toCopy);
    
    free(tempBuffer);
    return toCopy;
}

bool ZipReader::streamFileTo(const String& innerPath, File& outFile, size_t chunkSize) {
    if (!_isOpen || !outFile) {
        _error = "Not open or invalid output file";
        return false;
    }
    
    Serial.printf("[ZIP] streamFileTo: '%s'\n", innerPath.c_str());
    
    int index = findFileIndex(innerPath);
    if (index < 0) {
        _error = "File not found: " + innerPath;
        Serial.printf("[ZIP] %s\n", _error.c_str());
        return false;
    }
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    mz_zip_archive_file_stat stat;
    
    if (!mz_zip_reader_file_stat(zip, index, &stat)) {
        _error = "Failed to stat file";
        return false;
    }
    
    size_t fileSize = stat.m_uncomp_size;
    Serial.printf("[ZIP] File index=%d, size=%d\n", index, fileSize);
    
    if (fileSize == 0) {
        // Empty file - write nothing, return success
        return true;
    }
    
    // Check if we can fit in available heap (leave 10KB margin)
    size_t availHeap = ESP.getFreeHeap();
    if (fileSize > availHeap - 10240) {
        _error = "File too large for available memory";
        Serial.printf("[ZIP] %s: need %d, have %d\n", _error.c_str(), fileSize, availHeap);
        return false;
    }
    
    // Extract to memory then write to file
    uint8_t* buffer = (uint8_t*)malloc(fileSize);
    if (!buffer) {
        _error = "Failed to allocate buffer";
        Serial.printf("[ZIP] malloc(%d) failed, heap=%d\n", fileSize, ESP.getFreeHeap());
        return false;
    }
    
    bool ok = mz_zip_reader_extract_to_mem(zip, index, buffer, fileSize, 0);
    if (ok) {
        // Write in chunks
        size_t written = 0;
        while (written < fileSize) {
            size_t toWrite = min(chunkSize, fileSize - written);
            size_t actualWritten = outFile.write(buffer + written, toWrite);
            if (actualWritten != toWrite) {
                Serial.printf("[ZIP] Write failed: wanted %d, wrote %d\n", toWrite, actualWritten);
            }
            written += toWrite;
            yield();
        }
        Serial.printf("[ZIP] Extracted %d bytes OK\n", written);
    } else {
        _error = "Extraction failed";
        Serial.printf("[ZIP] mz_zip_reader_extract_to_mem failed\n");
    }
    
    free(buffer);
    return ok;
}

bool ZipReader::streamFileCallback(const String& innerPath, StreamCallback callback, void* userData, size_t chunkSize) {
    if (!_isOpen || !callback) return false;
    
    int index = findFileIndex(innerPath);
    if (index < 0) {
        _error = "File not found: " + innerPath;
        return false;
    }
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    
    mz_zip_reader_extract_iter_state* iter = mz_zip_reader_extract_iter_new(zip, index, 0);
    if (!iter) {
        _error = "Failed to create extraction iterator";
        return false;
    }
    
    uint8_t* buffer = (uint8_t*)malloc(chunkSize);
    if (!buffer) {
        mz_zip_reader_extract_iter_free(iter);
        _error = "Failed to allocate chunk buffer";
        return false;
    }
    
    bool success = true;
    size_t read;
    
    while ((read = mz_zip_reader_extract_iter_read(iter, buffer, chunkSize)) > 0) {
        if (!callback(buffer, read, userData)) {
            success = false;
            break;
        }
        yield();
    }
    
    free(buffer);
    mz_zip_reader_extract_iter_free(iter);
    
    return success;
}

// =============================================================================
// Full Read (Small Files Only)
// =============================================================================

String ZipReader::readFileToString(const String& innerPath, size_t maxSize) {
    if (!_isOpen) return "";
    
    int index = findFileIndex(innerPath);
    if (index < 0) {
        _error = "File not found: " + innerPath;
        return "";
    }
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    mz_zip_archive_file_stat stat;
    
    if (!mz_zip_reader_file_stat(zip, index, &stat)) {
        _error = "Failed to stat file";
        return "";
    }
    
    if (stat.m_uncomp_size > maxSize) {
        _error = "File too large for string read";
        Serial.printf("[ZIP] %s: %d > %d\n", _error.c_str(), (int)stat.m_uncomp_size, (int)maxSize);
        return "";
    }
    
    size_t size = stat.m_uncomp_size;
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        _error = "Failed to allocate buffer";
        return "";
    }
    
    if (!mz_zip_reader_extract_to_mem(zip, index, buffer, size, 0)) {
        _error = "Failed to extract file";
        free(buffer);
        return "";
    }
    
    buffer[size] = '\0';
    String result = String(buffer);
    free(buffer);
    
    return result;
}

// =============================================================================
// Helpers
// =============================================================================

int ZipReader::findFileIndex(const String& innerPath) {
    if (!_isOpen) return -1;
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    String normalized = normalizePath(innerPath);
    
    // Try exact match first
    int index = mz_zip_reader_locate_file(zip, normalized.c_str(), nullptr, 0);
    if (index >= 0) return index;
    
    // Try case-insensitive
    index = mz_zip_reader_locate_file(zip, normalized.c_str(), nullptr, MZ_ZIP_FLAG_CASE_SENSITIVE);
    if (index >= 0) return index;
    
    // Manual search with normalization
    char filename[256];
    for (int i = 0; i < _fileCount; i++) {
        if (mz_zip_reader_get_filename(zip, i, filename, sizeof(filename))) {
            String archivePath = normalizePath(filename);
            if (archivePath.equalsIgnoreCase(normalized)) {
                return i;
            }
        }
    }
    
    return -1;
}

String ZipReader::normalizePath(const String& path) {
    String result = path;
    
    // Convert backslashes
    result.replace("\\", "/");
    
    // Remove leading slash
    while (result.startsWith("/")) {
        result = result.substring(1);
    }
    
    // Remove ./
    result.replace("/./", "/");
    if (result.startsWith("./")) {
        result = result.substring(2);
    }
    
    // Remove double slashes
    while (result.indexOf("//") >= 0) {
        result.replace("//", "/");
    }
    
    return result;
}
