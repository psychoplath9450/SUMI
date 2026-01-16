/**
 * @file SafeResources.h
 * @brief RAII wrappers for safe resource management
 * @version 2.1.16
 *
 * Provides RAII (Resource Acquisition Is Initialization) wrappers
 * to ensure resources are properly cleaned up even when errors occur.
 * 
 * While exceptions are disabled on ESP32, these wrappers still help with:
 * - Early returns from functions
 * - Complex control flow
 * - Preventing resource leaks
 * - Making code more readable
 * 
 * USAGE EXAMPLES:
 * ===============
 * 
 * // File handling
 * {
 *     SafeFile file("/path/to/file", FILE_READ);
 *     if (!file) return false;
 *     
 *     // Use file...
 *     // File automatically closes when leaving scope
 * }
 * 
 * // Memory allocation
 * {
 *     SafeBuffer<uint8_t> buffer(1024);
 *     if (!buffer) return false;
 *     
 *     // Use buffer.get()...
 *     // Memory automatically freed when leaving scope
 * }
 * 
 * // Mutex locking
 * {
 *     SafeLock lock(myMutex);
 *     // Mutex automatically released when leaving scope
 * }
 */

#ifndef SUMI_SAFE_RESOURCES_H
#define SUMI_SAFE_RESOURCES_H

#include <Arduino.h>
#include <SD.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

// =============================================================================
// SafeFile - RAII wrapper for SD card file operations
// =============================================================================

/**
 * @brief RAII wrapper for File objects
 * 
 * Automatically closes the file when the SafeFile goes out of scope.
 */
class SafeFile {
public:
    /**
     * @brief Open a file
     * @param path File path
     * @param mode FILE_READ or FILE_WRITE
     */
    SafeFile(const char* path, const char* mode = FILE_READ) {
        _file = SD.open(path, mode);
        _valid = (bool)_file;
        if (_valid) {
            Serial.printf("[SAFE] Opened: %s\n", path);
        }
    }
    
    /**
     * @brief Move constructor
     */
    SafeFile(SafeFile&& other) noexcept {
        _file = other._file;
        _valid = other._valid;
        other._valid = false;
    }
    
    /**
     * @brief Destructor - automatically closes file
     */
    ~SafeFile() {
        close();
    }
    
    // Prevent copying
    SafeFile(const SafeFile&) = delete;
    SafeFile& operator=(const SafeFile&) = delete;
    
    /**
     * @brief Check if file is valid
     */
    explicit operator bool() const { return _valid && _file; }
    
    /**
     * @brief Get the underlying File object
     */
    File& get() { return _file; }
    const File& get() const { return _file; }
    
    /**
     * @brief Arrow operator for direct File access
     */
    File* operator->() { return &_file; }
    const File* operator->() const { return &_file; }
    
    /**
     * @brief Explicitly close the file
     */
    void close() {
        if (_valid && _file) {
            _file.close();
            _valid = false;
        }
    }
    
    /**
     * @brief Read entire file into string
     * @param maxSize Maximum bytes to read (0 = no limit)
     * @return File contents or empty string on error
     */
    String readAll(size_t maxSize = 0) {
        if (!_valid) return "";
        
        size_t size = _file.size();
        if (maxSize > 0 && size > maxSize) {
            size = maxSize;
        }
        
        String result;
        result.reserve(size);
        
        while (_file.available() && result.length() < size) {
            result += (char)_file.read();
        }
        
        return result;
    }
    
private:
    File _file;
    bool _valid = false;
};

// =============================================================================
// SafeBuffer - RAII wrapper for dynamic memory allocation
// =============================================================================

/**
 * @brief RAII wrapper for dynamically allocated memory
 * 
 * Automatically frees memory when going out of scope.
 * 
 * @tparam T Type of elements in the buffer
 */
template<typename T>
class SafeBuffer {
public:
    /**
     * @brief Allocate a buffer
     * @param count Number of elements
     */
    explicit SafeBuffer(size_t count) : _size(count) {
        if (count > 0) {
            _data = new(std::nothrow) T[count];
            if (_data) {
                memset(_data, 0, count * sizeof(T));
                Serial.printf("[SAFE] Allocated %d bytes\n", count * sizeof(T));
            } else {
                Serial.printf("[SAFE] Failed to allocate %d bytes\n", count * sizeof(T));
            }
        }
    }
    
    /**
     * @brief Move constructor
     */
    SafeBuffer(SafeBuffer&& other) noexcept {
        _data = other._data;
        _size = other._size;
        other._data = nullptr;
        other._size = 0;
    }
    
    /**
     * @brief Destructor - automatically frees memory
     */
    ~SafeBuffer() {
        free();
    }
    
    // Prevent copying
    SafeBuffer(const SafeBuffer&) = delete;
    SafeBuffer& operator=(const SafeBuffer&) = delete;
    
    /**
     * @brief Check if buffer is valid
     */
    explicit operator bool() const { return _data != nullptr; }
    
    /**
     * @brief Get pointer to buffer
     */
    T* get() { return _data; }
    const T* get() const { return _data; }
    
    /**
     * @brief Get buffer size (in elements)
     */
    size_t size() const { return _size; }
    
    /**
     * @brief Get buffer size (in bytes)
     */
    size_t sizeBytes() const { return _size * sizeof(T); }
    
    /**
     * @brief Array access operator
     */
    T& operator[](size_t index) { return _data[index]; }
    const T& operator[](size_t index) const { return _data[index]; }
    
    /**
     * @brief Explicitly free the buffer
     */
    void free() {
        if (_data) {
            delete[] _data;
            _data = nullptr;
            _size = 0;
        }
    }
    
    /**
     * @brief Release ownership of the buffer
     * @return Pointer to buffer (caller takes ownership)
     */
    T* release() {
        T* ptr = _data;
        _data = nullptr;
        _size = 0;
        return ptr;
    }
    
private:
    T* _data = nullptr;
    size_t _size = 0;
};

// =============================================================================
// SafeLock - RAII wrapper for FreeRTOS mutex
// =============================================================================

/**
 * @brief RAII wrapper for FreeRTOS mutex
 * 
 * Automatically releases the mutex when going out of scope.
 */
class SafeLock {
public:
    /**
     * @brief Acquire a mutex
     * @param mutex The mutex to acquire
     * @param timeoutMs Maximum time to wait (0 = forever)
     */
    explicit SafeLock(SemaphoreHandle_t mutex, uint32_t timeoutMs = portMAX_DELAY) 
        : _mutex(mutex), _locked(false) {
        if (_mutex) {
            TickType_t ticks = (timeoutMs == portMAX_DELAY) 
                             ? portMAX_DELAY 
                             : pdMS_TO_TICKS(timeoutMs);
            _locked = (xSemaphoreTake(_mutex, ticks) == pdTRUE);
        }
    }
    
    /**
     * @brief Destructor - automatically releases mutex
     */
    ~SafeLock() {
        unlock();
    }
    
    // Prevent copying and moving
    SafeLock(const SafeLock&) = delete;
    SafeLock& operator=(const SafeLock&) = delete;
    SafeLock(SafeLock&&) = delete;
    SafeLock& operator=(SafeLock&&) = delete;
    
    /**
     * @brief Check if lock was acquired
     */
    explicit operator bool() const { return _locked; }
    bool isLocked() const { return _locked; }
    
    /**
     * @brief Explicitly release the lock
     */
    void unlock() {
        if (_locked && _mutex) {
            xSemaphoreGive(_mutex);
            _locked = false;
        }
    }
    
private:
    SemaphoreHandle_t _mutex;
    bool _locked;
};

// =============================================================================
// ScopeGuard - Execute cleanup code on scope exit
// =============================================================================

/**
 * @brief Execute a function when leaving scope
 * 
 * Useful for cleanup that doesn't fit RAII patterns.
 * 
 * Usage:
 *     auto cleanup = makeScopeGuard([]() { cleanupFunction(); });
 */
template<typename Func>
class ScopeGuard {
public:
    explicit ScopeGuard(Func func) : _func(func), _active(true) {}
    
    ~ScopeGuard() {
        if (_active) {
            _func();
        }
    }
    
    // Prevent copying
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
    
    // Move constructor
    ScopeGuard(ScopeGuard&& other) noexcept 
        : _func(std::move(other._func)), _active(other._active) {
        other._active = false;
    }
    
    /**
     * @brief Cancel the scope guard (skip cleanup execution)
     */
    void dismiss() { _active = false; }
    
private:
    Func _func;
    bool _active;
};

/**
 * @brief Create a scope guard from a lambda
 */
template<typename Func>
ScopeGuard<Func> makeScopeGuard(Func func) {
    return ScopeGuard<Func>(func);
}

// =============================================================================
// Result - Error handling without exceptions
// =============================================================================

/**
 * @brief Result type for operations that may fail
 * 
 * Use instead of returning bool and using out parameters.
 * 
 * Usage:
 *     Result<String> readConfig() {
 *         SafeFile file("/config.json");
 *         if (!file) return Result<String>::error("File not found");
 *         return Result<String>::ok(file.readAll());
 *     }
 *     
 *     auto result = readConfig();
 *     if (result) {
 *         Serial.println(result.value());
 *     } else {
 *         Serial.println(result.errorMessage());
 *     }
 */
template<typename T>
class Result {
public:
    /**
     * @brief Create a successful result
     */
    static Result ok(T value) {
        Result r;
        r._value = value;
        r._hasValue = true;
        return r;
    }
    
    /**
     * @brief Create an error result
     */
    static Result error(const char* message) {
        Result r;
        r._error = message;
        r._hasValue = false;
        return r;
    }
    
    /**
     * @brief Check if result is successful
     */
    explicit operator bool() const { return _hasValue; }
    bool isOk() const { return _hasValue; }
    bool isError() const { return !_hasValue; }
    
    /**
     * @brief Get the value (only valid if isOk())
     */
    T& value() { return _value; }
    const T& value() const { return _value; }
    
    /**
     * @brief Get the value or a default if error
     */
    T valueOr(T defaultValue) const {
        return _hasValue ? _value : defaultValue;
    }
    
    /**
     * @brief Get the error message (only valid if isError())
     */
    const char* errorMessage() const { 
        return _hasValue ? "" : _error; 
    }
    
private:
    Result() = default;
    
    T _value;
    const char* _error = "";
    bool _hasValue = false;
};

// Specialization for void
template<>
class Result<void> {
public:
    static Result ok() {
        Result r;
        r._success = true;
        return r;
    }
    
    static Result error(const char* message) {
        Result r;
        r._error = message;
        r._success = false;
        return r;
    }
    
    explicit operator bool() const { return _success; }
    bool isOk() const { return _success; }
    bool isError() const { return !_success; }
    
    const char* errorMessage() const {
        return _success ? "" : _error;
    }
    
private:
    Result() = default;
    
    const char* _error = "";
    bool _success = false;
};

// =============================================================================
// Utility Macros
// =============================================================================

/**
 * @brief Return early if result is an error
 * 
 * Usage:
 *     Result<void> doSomething() {
 *         RETURN_IF_ERROR(step1());
 *         RETURN_IF_ERROR(step2());
 *         return Result<void>::ok();
 *     }
 */
#define RETURN_IF_ERROR(result) \
    do { \
        auto _r = (result); \
        if (!_r) return _r; \
    } while (0)

/**
 * @brief Log and return error
 */
#define RETURN_ERROR(msg) \
    do { \
        Serial.printf("[ERROR] %s:%d: %s\n", __FILE__, __LINE__, msg); \
        return Result<void>::error(msg); \
    } while (0)

#endif // SUMI_SAFE_RESOURCES_H
