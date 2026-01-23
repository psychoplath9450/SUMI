/**
 * @file ZipReader.cpp
 * @brief ZIP File Reader Implementation
 * @version 2.1.16
 *
 * ZIP file reader with adaptations for Arduino/ESP32 and SD card usage.
 *
 * Key features:
 * - Uses miniz for DEFLATE decompression
 * - Streaming decompression with configurable chunk size
 * - ~32KB dictionary buffer for LZ77 sliding window
 * - Supports both STORE and DEFLATE compression methods
 */

#include <SD.h>
#include "core/ZipReader.h"

// Include miniz
extern "C" {
#include "../../lib/miniz/miniz.h"
}

// =============================================================================
// Pre-allocated Decompression Buffers (avoids heap fragmentation)
// =============================================================================
// TINFL_LZ_DICT_SIZE is 32768 (32KB)
// tinfl_decompressor is ~11KB
// We MUST allocate these at boot before heap fragments
// Without this, cover extraction and EPUB reading will fail
static uint8_t* _sharedDictBuffer = nullptr;
static bool _dictBufferInUse = false;
static tinfl_decompressor* _sharedDecompressor = nullptr;
static bool _decompressorInUse = false;

// Call this EARLY in setup() - before display init - to pre-allocate
void ZipReader_preallocateBuffer() {
    if (_sharedDictBuffer != nullptr && _sharedDecompressor != nullptr) {
        Serial.printf("[ZIP] Buffers already allocated: dict=%p, decomp=%p\n", 
                      _sharedDictBuffer, _sharedDecompressor);
        return;  // Already allocated
    }
    
    int freeHeap = ESP.getFreeHeap();
    Serial.printf("[ZIP] Attempting to pre-allocate buffers (heap=%d)\n", freeHeap);
    
    // Pre-allocate 32KB dictionary buffer
    if (_sharedDictBuffer == nullptr) {
        _sharedDictBuffer = (uint8_t*)malloc(TINFL_LZ_DICT_SIZE);
        if (_sharedDictBuffer) {
            memset(_sharedDictBuffer, 0, TINFL_LZ_DICT_SIZE);  // Initialize to zero
            Serial.printf("[ZIP] Pre-allocated 32KB dictionary buffer at %p\n", _sharedDictBuffer);
        } else {
            Serial.printf("[ZIP] CRITICAL: Failed to pre-allocate dictionary buffer! (heap=%d, need=%d)\n",
                          ESP.getFreeHeap(), TINFL_LZ_DICT_SIZE);
        }
    }
    
    // Pre-allocate ~11KB decompressor
    if (_sharedDecompressor == nullptr) {
        _sharedDecompressor = tinfl_decompressor_alloc();
        if (_sharedDecompressor) {
            Serial.printf("[ZIP] Pre-allocated decompressor (~11KB) at %p\n", _sharedDecompressor);
        } else {
            Serial.println("[ZIP] CRITICAL: Failed to pre-allocate decompressor!");
        }
    }
    
    // Reset in-use flags to ensure clean state
    _dictBufferInUse = false;
    _decompressorInUse = false;
    
    if (!_sharedDictBuffer || !_sharedDecompressor) {
        Serial.println("[ZIP] EPUB reading may fail due to heap fragmentation");
    } else {
        Serial.printf("[ZIP] Pre-allocation complete. Heap: %d -> %d (used ~43KB)\n",
                      freeHeap, ESP.getFreeHeap());
    }
}

// Free ZIP buffers to reclaim ~43KB for portal mode
void ZipReader_freeBuffers() {
    if (_dictBufferInUse || _decompressorInUse) {
        Serial.println("[ZIP] WARNING: Cannot free buffers - in use!");
        return;
    }
    
    if (_sharedDictBuffer) {
        free(_sharedDictBuffer);
        _sharedDictBuffer = nullptr;
        Serial.println("[ZIP] Freed 32KB dictionary buffer");
    }
    
    if (_sharedDecompressor) {
        tinfl_decompressor_free(_sharedDecompressor);
        _sharedDecompressor = nullptr;
        Serial.println("[ZIP] Freed ~11KB decompressor");
    }
    
    Serial.printf("[ZIP] Buffers freed, heap now: %d\n", ESP.getFreeHeap());
}

// Log current buffer status for debugging
void ZipReader_logStatus() {
    Serial.println("[ZIP] ========== Buffer Status ==========");
    Serial.printf("[ZIP] Dictionary: ptr=%p, inUse=%d\n", _sharedDictBuffer, _dictBufferInUse);
    Serial.printf("[ZIP] Decompressor: ptr=%p, inUse=%d\n", _sharedDecompressor, _decompressorInUse);
    Serial.printf("[ZIP] Heap free: %d bytes\n", ESP.getFreeHeap());
    Serial.println("[ZIP] =====================================");
}

// Force reset in-use flags (recovery mechanism)
void ZipReader_resetFlags() {
    if (_dictBufferInUse || _decompressorInUse) {
        Serial.println("[ZIP] WARNING: Force-resetting in-use flags!");
        Serial.printf("[ZIP] Before: dictInUse=%d, decompInUse=%d\n", 
                      _dictBufferInUse, _decompressorInUse);
    }
    _dictBufferInUse = false;
    _decompressorInUse = false;
    Serial.println("[ZIP] In-use flags reset to false");
}

// =============================================================================
// Constructor / Destructor
// =============================================================================

ZipReader::ZipReader() 
    : _isOpen(false), _fileCount(0), _archive(nullptr) {
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
    
    // Check if file exists using SD library
    if (!SD.exists(zipPath)) {
        _error = "File not found: " + zipPath;
        Serial.printf("[ZIP] %s\n", _error.c_str());
        return false;
    }
    
    // Allocate miniz archive structure
    _archive = malloc(sizeof(mz_zip_archive));
    if (!_archive) {
        _error = "Failed to allocate ZIP archive";
        Serial.printf("[ZIP] %s\n", _error.c_str());
        return false;
    }
    memset(_archive, 0, sizeof(mz_zip_archive));
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    
    // Build full path for stdio (miniz uses fopen, which needs /sd prefix)
    _stdioPath = zipPath;
    if (!_stdioPath.startsWith("/sd")) {
        _stdioPath = "/sd" + _stdioPath;
    }
    
    Serial.printf("[ZIP] Opening: %s (stdio path: %s)\n", zipPath.c_str(), _stdioPath.c_str());
    
    // Open ZIP file
    // Note: miniz uses stdio FILE*, which needs full /sd path on ESP32
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
    
    Serial.printf("[ZIP] Opened: %s (%d files)\n", zipPath.c_str(), _fileCount);
    
    // Debug: List first few files to see path format
    Serial.println("[ZIP] First 5 files in archive:");
    for (int i = 0; i < min(5, _fileCount); i++) {
        char filename[256];
        if (mz_zip_reader_get_filename(zip, i, filename, sizeof(filename))) {
            Serial.printf("[ZIP]   [%d] %s\n", i, filename);
        }
    }
    
    return true;
}

void ZipReader::close() {
    if (_archive) {
        if (_isOpen) {
            mz_zip_reader_end((mz_zip_archive*)_archive);
        }
        free(_archive);
        _archive = nullptr;
    }
    _isOpen = false;
    _fileCount = 0;
}

// =============================================================================
// File Information
// =============================================================================

bool ZipReader::hasFile(const String& innerPath) {
    if (!_isOpen || !_archive) return false;
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    mz_uint32 fileIndex = 0;
    
    // Try with path as-is
    if (mz_zip_reader_locate_file_v2(zip, innerPath.c_str(), nullptr, 0, &fileIndex)) {
        return true;
    }
    
    // Try with backslashes
    String altPath = innerPath;
    altPath.replace("/", "\\");
    if (mz_zip_reader_locate_file_v2(zip, altPath.c_str(), nullptr, 0, &fileIndex)) {
        return true;
    }
    
    return false;
}

size_t ZipReader::getFileSize(const String& innerPath) {
    if (!_isOpen || !_archive) return 0;
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    mz_zip_archive_file_stat stat;
    
    if (!loadFileStat(innerPath.c_str(), &stat)) {
        return 0;
    }
    
    return (size_t)stat.m_uncomp_size;
}

String ZipReader::getFileName(int index) {
    if (!_isOpen || !_archive || index < 0 || index >= _fileCount) {
        return "";
    }
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    char filename[256];
    
    if (mz_zip_reader_get_filename(zip, index, filename, sizeof(filename)) == 0) {
        return "";
    }
    
    return String(filename);
}

// =============================================================================
// Internal: Load File Statistics
// =============================================================================

bool ZipReader::loadFileStat(const char* filename, void* statPtr) {
    if (!_archive) return false;
    
    mz_zip_archive* zip = (mz_zip_archive*)_archive;
    mz_zip_archive_file_stat* stat = (mz_zip_archive_file_stat*)statPtr;
    
    // Find file in archive
    mz_uint32 fileIndex = 0;
    
    // First try with the path as-is
    if (mz_zip_reader_locate_file_v2(zip, filename, nullptr, 0, &fileIndex)) {
        // Get file stats
        if (mz_zip_reader_file_stat(zip, fileIndex, stat)) {
            return true;
        }
    }
    
    // Try converting forward slashes to backslashes (Windows EPUBs)
    String altPath = filename;
    altPath.replace("/", "\\");
    if (altPath != filename) {
        if (mz_zip_reader_locate_file_v2(zip, altPath.c_str(), nullptr, 0, &fileIndex)) {
            if (mz_zip_reader_file_stat(zip, fileIndex, stat)) {
                Serial.printf("[ZIP] Found with backslash path: %s\n", altPath.c_str());
                return true;
            }
        }
    }
    
    // Try converting backslashes to forward slashes 
    altPath = filename;
    altPath.replace("\\", "/");
    if (altPath != filename) {
        if (mz_zip_reader_locate_file_v2(zip, altPath.c_str(), nullptr, 0, &fileIndex)) {
            if (mz_zip_reader_file_stat(zip, fileIndex, stat)) {
                Serial.printf("[ZIP] Found with forward slash path: %s\n", altPath.c_str());
                return true;
            }
        }
    }
    
    _error = "File not found in ZIP: ";
    _error += filename;
    Serial.printf("[ZIP] %s\n", _error.c_str());
    return false;
}

// =============================================================================
// Internal: Get Data Offset
// =============================================================================

long ZipReader::getDataOffset(const void* statPtr) {
    const mz_zip_archive_file_stat* stat = (const mz_zip_archive_file_stat*)statPtr;
    
    // Local file header size (fixed part)
    const int localHeaderSize = 30;
    
    // Read local header to get variable-length fields
    uint8_t localHeader[localHeaderSize];
    uint64_t headerOffset = stat->m_local_header_ofs;
    
    FILE* file = fopen(_stdioPath.c_str(), "rb");
    if (!file) {
        Serial.println("[ZIP] Failed to open file for header read");
        return -1;
    }
    
    fseek(file, headerOffset, SEEK_SET);
    size_t read = fread(localHeader, 1, localHeaderSize, file);
    fclose(file);
    
    if (read != localHeaderSize) {
        Serial.println("[ZIP] Failed to read local header");
        return -1;
    }
    
    // Verify signature (0x04034b50)
    uint32_t sig = localHeader[0] | (localHeader[1] << 8) | 
                   (localHeader[2] << 16) | (localHeader[3] << 24);
    if (sig != 0x04034b50) {
        Serial.println("[ZIP] Invalid local file header signature");
        return -1;
    }
    
    // Get variable-length field sizes
    uint16_t filenameLen = localHeader[26] | (localHeader[27] << 8);
    uint16_t extraLen = localHeader[28] | (localHeader[29] << 8);
    
    // Data starts after header + filename + extra
    return headerOffset + localHeaderSize + filenameLen + extraLen;
}

// =============================================================================
// Read Entire File
// =============================================================================

String ZipReader::readFile(const String& innerPath) {
    uint8_t* data = nullptr;
    size_t size = 0;
    
    if (!readFileToBuffer(innerPath, &data, &size)) {
        return "";
    }
    
    // Convert to String
    String result;
    result.reserve(size + 1);
    for (size_t i = 0; i < size; i++) {
        result += (char)data[i];
    }
    
    free(data);
    return result;
}

bool ZipReader::readFileToBuffer(const String& innerPath, uint8_t** outData, size_t* outSize) {
    if (!_isOpen || !_archive) {
        _error = "ZIP not open";
        return false;
    }
    
    *outData = nullptr;
    *outSize = 0;
    
    // Get file stats
    mz_zip_archive_file_stat stat;
    if (!loadFileStat(innerPath.c_str(), &stat)) {
        return false;
    }
    
    // Get data offset
    long dataOffset = getDataOffset(&stat);
    if (dataOffset < 0) {
        return false;
    }
    
    size_t compSize = (size_t)stat.m_comp_size;
    size_t uncompSize = (size_t)stat.m_uncomp_size;
    
    // Size limit for file decompression
    const size_t MAX_FILE_SIZE = 256 * 1024;
    
    if (uncompSize > MAX_FILE_SIZE) {
        Serial.printf("[ZIP] File too large: %s (%d bytes, max %d)\n", 
                      innerPath.c_str(), (int)uncompSize, (int)MAX_FILE_SIZE);
        _error = "File too large: " + innerPath;
        return false;
    }
    
    // Allocate output buffer
    uint8_t* data = (uint8_t*)malloc(uncompSize + 1);
    if (!data) {
        _error = "Failed to allocate output buffer";
        Serial.printf("[ZIP] %s (%d bytes)\n", _error.c_str(), (int)uncompSize);
        return false;
    }
    
    // Open file
    FILE* file = fopen(_stdioPath.c_str(), "rb");
    if (!file) {
        _error = "Failed to open ZIP file";
        free(data);
        return false;
    }
    fseek(file, dataOffset, SEEK_SET);
    
    bool success = false;
    
    if (stat.m_method == 0) {
        // STORE - no compression
        size_t read = fread(data, 1, uncompSize, file);
        success = (read == uncompSize);
        if (!success) {
            _error = "Failed to read uncompressed data";
        }
    } else if (stat.m_method == 8) {
        // DEFLATE - need to decompress
        // Read compressed data
        uint8_t* compData = (uint8_t*)malloc(compSize);
        if (!compData) {
            _error = "Failed to allocate decompression buffer";
            free(data);
            fclose(file);
            return false;
        }
        
        size_t read = fread(compData, 1, compSize, file);
        if (read != compSize) {
            _error = "Failed to read compressed data";
            free(compData);
            free(data);
            fclose(file);
            return false;
        }
        
        // Use pre-allocated decompressor (~11KB - too big for stack or dynamic alloc)
        tinfl_decompressor* inflator = nullptr;
        bool usingSharedDecompressor = false;
        
        if (_sharedDecompressor != nullptr && !_decompressorInUse) {
            inflator = _sharedDecompressor;
            _decompressorInUse = true;
            usingSharedDecompressor = true;
        } else {
            inflator = tinfl_decompressor_alloc();
            if (!inflator) {
                _error = "Failed to allocate decompressor";
                free(compData);
                free(data);
                fclose(file);
                return false;
            }
        }
        tinfl_init(inflator);
        
        size_t inBytes = compSize;
        size_t outBytes = uncompSize;
        
        tinfl_status status = tinfl_decompress(inflator, compData, &inBytes,
                                               data, data, &outBytes,
                                               TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
        
        // Release decompressor
        if (usingSharedDecompressor) {
            _decompressorInUse = false;
        } else {
            tinfl_decompressor_free(inflator);
        }
        free(compData);
        
        success = (status == TINFL_STATUS_DONE);
        if (!success) {
            _error = "Decompression failed with status " + String((int)status);
        }
    } else {
        _error = "Unsupported compression method: " + String(stat.m_method);
    }
    
    fclose(file);
    
    if (success) {
        data[uncompSize] = '\0';  // Null terminate
        *outData = data;
        *outSize = uncompSize;
    } else {
        free(data);
        Serial.printf("[ZIP] %s\n", _error.c_str());
    }
    
    return success;
}

// =============================================================================
// Stream File via Callback
// =============================================================================

bool ZipReader::streamFile(const String& innerPath, ZipStreamCallback callback, size_t chunkSize) {
    if (!_isOpen || !_archive) {
        _error = "ZIP not open";
        return false;
    }
    
    if (!callback) {
        _error = "No callback provided";
        return false;
    }
    
    // Get file stats
    mz_zip_archive_file_stat stat;
    if (!loadFileStat(innerPath.c_str(), &stat)) {
        return false;
    }
    
    // Get data offset
    long dataOffset = getDataOffset(&stat);
    if (dataOffset < 0) {
        return false;
    }
    
    size_t compSize = (size_t)stat.m_comp_size;
    size_t uncompSize = (size_t)stat.m_uncomp_size;
    
    // Open file
    FILE* file = fopen(_stdioPath.c_str(), "rb");
    if (!file) {
        _error = "Failed to open ZIP file";
        return false;
    }
    fseek(file, dataOffset, SEEK_SET);
    
    bool success = false;
    
    if (stat.m_method == 0) {
        // STORE - no compression, stream directly
        uint8_t* buffer = (uint8_t*)malloc(chunkSize);
        if (!buffer) {
            _error = "Failed to allocate stream buffer";
            fclose(file);
            return false;
        }
        
        size_t remaining = uncompSize;
        while (remaining > 0) {
            size_t toRead = (remaining < chunkSize) ? remaining : chunkSize;
            size_t read = fread(buffer, 1, toRead, file);
            
            if (read == 0) {
                _error = "Unexpected end of file";
                break;
            }
            
            callback(buffer, read);
            remaining -= read;
        }
        
        free(buffer);
        success = (remaining == 0);
    } else if (stat.m_method == 8) {
        // DEFLATE - streaming decompression
        success = inflateStream(file, compSize, uncompSize, callback, chunkSize);
    } else {
        _error = "Unsupported compression method: " + String(stat.m_method);
    }
    
    fclose(file);
    
    if (!success) {
        Serial.printf("[ZIP] Stream failed: %s\n", _error.c_str());
    }
    
    return success;
}

// =============================================================================
// Stream File to Print Object
// =============================================================================

bool ZipReader::streamFileTo(const String& innerPath, Print& output, size_t chunkSize) {
    return streamFile(innerPath, [&output](const uint8_t* data, size_t len) {
        output.write(data, len);
    }, chunkSize);
}

// =============================================================================
// Internal: Streaming DEFLATE Decompression
// =============================================================================

bool ZipReader::inflateStream(FILE* file, size_t compSize, size_t uncompSize,
                               ZipStreamCallback callback, size_t chunkSize) {
    // Use pre-allocated decompressor (~11KB - too big for stack or dynamic alloc)
    tinfl_decompressor* inflator = nullptr;
    bool usingSharedDecompressor = false;
    
    if (_sharedDecompressor != nullptr && !_decompressorInUse) {
        inflator = _sharedDecompressor;
        _decompressorInUse = true;
        usingSharedDecompressor = true;
        Serial.println("[ZIP] Using pre-allocated decompressor");
    } else {
        // Fall back to dynamic allocation
        inflator = tinfl_decompressor_alloc();
        if (!inflator) {
            _error = "Failed to allocate decompressor";
            return false;
        }
        Serial.println("[ZIP] Allocated new decompressor");
    }
    tinfl_init(inflator);
    
    // Input buffer (compressed chunks)
    uint8_t* inBuffer = (uint8_t*)malloc(chunkSize);
    if (!inBuffer) {
        _error = "Failed to allocate input buffer";
        if (usingSharedDecompressor) {
            _decompressorInUse = false;
        } else {
            tinfl_decompressor_free(inflator);
        }
        return false;
    }
    
    // Output buffer (LZ77 dictionary - must be TINFL_LZ_DICT_SIZE = 32KB)
    // Try to use pre-allocated buffer first to avoid fragmentation
    uint8_t* outBuffer = nullptr;
    bool usingSharedBuffer = false;
    
    Serial.printf("[ZIP] Dict buffer state: ptr=%p, inUse=%d, heap=%d\n", 
                  _sharedDictBuffer, _dictBufferInUse, ESP.getFreeHeap());
    
    if (_sharedDictBuffer != nullptr && !_dictBufferInUse) {
        outBuffer = _sharedDictBuffer;
        _dictBufferInUse = true;
        usingSharedBuffer = true;
        Serial.println("[ZIP] Using pre-allocated dictionary buffer");
    } else {
        // Log why we're not using pre-allocated buffer
        if (_sharedDictBuffer == nullptr) {
            Serial.println("[ZIP] WARN: No pre-allocated dictionary buffer!");
        } else if (_dictBufferInUse) {
            Serial.println("[ZIP] WARN: Dictionary buffer already in use - attempting re-allocation");
            // Try to force-release if stuck (recovery mechanism)
            // This shouldn't happen but provides resilience
        }
        
        // Fall back to dynamic allocation
        Serial.printf("[ZIP] Attempting malloc(%d) with %d heap free\n", 
                      TINFL_LZ_DICT_SIZE, ESP.getFreeHeap());
        outBuffer = (uint8_t*)malloc(TINFL_LZ_DICT_SIZE);
        if (!outBuffer) {
            _error = "Failed to allocate dictionary buffer";
            Serial.printf("[ZIP] ERROR: %s (heap=%d, need=%d)\n", 
                          _error.c_str(), ESP.getFreeHeap(), TINFL_LZ_DICT_SIZE);
            free(inBuffer);
            if (usingSharedDecompressor) {
                _decompressorInUse = false;
            } else {
                tinfl_decompressor_free(inflator);
            }
            return false;
        }
        Serial.println("[ZIP] Allocated new dictionary buffer");
    }
    memset(outBuffer, 0, TINFL_LZ_DICT_SIZE);
    
    size_t compRemaining = compSize;
    size_t inBufferFilled = 0;
    size_t inBufferPos = 0;
    size_t outPos = 0;  // Position in circular output buffer
    bool success = false;
    
    while (true) {
        // Refill input buffer when needed
        if (inBufferPos >= inBufferFilled) {
            if (compRemaining == 0) {
                break;  // No more compressed data
            }
            
            size_t toRead = (compRemaining < chunkSize) ? compRemaining : chunkSize;
            inBufferFilled = fread(inBuffer, 1, toRead, file);
            compRemaining -= inBufferFilled;
            inBufferPos = 0;
            
            if (inBufferFilled == 0) {
                break;  // Read error or EOF
            }
        }
        
        // Available input bytes
        size_t inAvail = inBufferFilled - inBufferPos;
        // Available output space
        size_t outAvail = TINFL_LZ_DICT_SIZE - outPos;
        
        // Decompress
        tinfl_status status = tinfl_decompress(inflator, 
                                               inBuffer + inBufferPos, &inAvail,
                                               outBuffer, outBuffer + outPos, &outAvail,
                                               (compRemaining > 0) ? TINFL_FLAG_HAS_MORE_INPUT : 0);
        
        // Advance input position
        inBufferPos += inAvail;
        
        // Output decompressed data
        if (outAvail > 0) {
            callback(outBuffer + outPos, outAvail);
            // Wrap output position in circular buffer
            outPos = (outPos + outAvail) & (TINFL_LZ_DICT_SIZE - 1);
        }
        
        if (status < 0) {
            _error = "Decompression error: " + String((int)status);
            break;
        }
        
        if (status == TINFL_STATUS_DONE) {
            Serial.printf("[ZIP] Streamed %d -> %d bytes\n", (int)compSize, (int)uncompSize);
            success = true;
            break;
        }
    }
    
    // Only free if we dynamically allocated (not the shared buffer)
    if (usingSharedBuffer) {
        _dictBufferInUse = false;  // Release shared buffer for reuse
    } else {
        free(outBuffer);
    }
    free(inBuffer);
    
    // Release decompressor
    if (usingSharedDecompressor) {
        _decompressorInUse = false;  // Release shared decompressor for reuse
    } else {
        tinfl_decompressor_free(inflator);
    }
    
    return success;
}

// =============================================================================
// Utility Functions
// =============================================================================

bool isZipFile(const String& path) {
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".zip");
}

bool isEpubZipFile(const String& path) {
    String lower = path;
    lower.toLowerCase();
    return lower.endsWith(".epub");
}
