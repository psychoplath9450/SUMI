/**
 * @file test_main.cpp
 * @brief Unit tests for Sumi Firmware
 * @version 1.3.0
 *
 * Run tests with: pio test -e esp32-c3-devkitm-1
 *
 * Tests require the device to be connected.
 */

#include <unity.h>
#include <cstring>
#include <cstdint>

// =============================================================================
// Mock Arduino Types for Native Testing
// =============================================================================
#ifndef ARDUINO
    typedef const char* String;
    #define PROGMEM
#endif

// =============================================================================
// Include Headers Under Test
// =============================================================================
// Pure logic headers only (no hardware dependencies)

// =============================================================================
// TEST: Button Mapping
// =============================================================================

// Button enum (copy from config.h for native testing)
enum Button {
    BTN_NONE = 0,
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_CONFIRM,
    BTN_BACK,
    BTN_POWER
};

// Orientation-aware button mapping (copy from PluginHelpers.h)
Button remapButtonForOrientation(Button btn, bool landscape) {
    if (landscape) return btn;
    
    switch (btn) {
        case BTN_UP:    return BTN_LEFT;
        case BTN_DOWN:  return BTN_RIGHT;
        case BTN_LEFT:  return BTN_DOWN;
        case BTN_RIGHT: return BTN_UP;
        default:        return btn;
    }
}

void test_button_mapping_landscape() {
    // In landscape mode, buttons should not be remapped
    TEST_ASSERT_EQUAL(BTN_UP, remapButtonForOrientation(BTN_UP, true));
    TEST_ASSERT_EQUAL(BTN_DOWN, remapButtonForOrientation(BTN_DOWN, true));
    TEST_ASSERT_EQUAL(BTN_LEFT, remapButtonForOrientation(BTN_LEFT, true));
    TEST_ASSERT_EQUAL(BTN_RIGHT, remapButtonForOrientation(BTN_RIGHT, true));
    TEST_ASSERT_EQUAL(BTN_CONFIRM, remapButtonForOrientation(BTN_CONFIRM, true));
    TEST_ASSERT_EQUAL(BTN_BACK, remapButtonForOrientation(BTN_BACK, true));
}

void test_button_mapping_portrait() {
    // In portrait mode, directional buttons should be rotated 90 degrees
    TEST_ASSERT_EQUAL(BTN_LEFT, remapButtonForOrientation(BTN_UP, false));
    TEST_ASSERT_EQUAL(BTN_RIGHT, remapButtonForOrientation(BTN_DOWN, false));
    TEST_ASSERT_EQUAL(BTN_DOWN, remapButtonForOrientation(BTN_LEFT, false));
    TEST_ASSERT_EQUAL(BTN_UP, remapButtonForOrientation(BTN_RIGHT, false));
    // Non-directional buttons should not change
    TEST_ASSERT_EQUAL(BTN_CONFIRM, remapButtonForOrientation(BTN_CONFIRM, false));
    TEST_ASSERT_EQUAL(BTN_BACK, remapButtonForOrientation(BTN_BACK, false));
}

// =============================================================================
// TEST: Value Clamping
// =============================================================================

template<typename T>
T clampValue(T val, T minVal, T maxVal) {
    if (val < minVal) return minVal;
    if (val > maxVal) return maxVal;
    return val;
}

void test_clamp_within_range() {
    TEST_ASSERT_EQUAL(50, clampValue(50, 0, 100));
    TEST_ASSERT_EQUAL(0, clampValue(0, 0, 100));
    TEST_ASSERT_EQUAL(100, clampValue(100, 0, 100));
}

void test_clamp_below_range() {
    TEST_ASSERT_EQUAL(0, clampValue(-10, 0, 100));
    TEST_ASSERT_EQUAL(10, clampValue(5, 10, 100));
}

void test_clamp_above_range() {
    TEST_ASSERT_EQUAL(100, clampValue(150, 0, 100));
    TEST_ASSERT_EQUAL(50, clampValue(100, 0, 50));
}

// =============================================================================
// TEST: Grid Layout Calculator
// =============================================================================

struct GridLayout {
    int cellSize;
    int offsetX;
    int offsetY;
    int cols;
    int rows;
    int gridW;
    int gridH;
    bool landscape;
};

#define PLUGIN_HEADER_H 28
#define PLUGIN_FOOTER_H 20
#define PLUGIN_MARGIN 6

bool isLandscapeMode(int screenW, int screenH) {
    return screenW > screenH;
}

GridLayout calculateGrid(int screenW, int screenH, int cols, int rows,
                         bool hasHeader = true, bool hasFooter = true) {
    GridLayout g;
    g.cols = cols;
    g.rows = rows;
    g.landscape = isLandscapeMode(screenW, screenH);
    
    int availW = screenW - 2 * PLUGIN_MARGIN;
    int availH = screenH - 2 * PLUGIN_MARGIN;
    
    int topSpace = hasHeader ? PLUGIN_HEADER_H : 0;
    int bottomSpace = hasFooter ? PLUGIN_FOOTER_H : 0;
    availH -= (topSpace + bottomSpace);
    
    int cellW = availW / cols;
    int cellH = availH / rows;
    g.cellSize = (cellW < cellH) ? cellW : cellH;
    
    g.gridW = cols * g.cellSize;
    g.gridH = rows * g.cellSize;
    
    g.offsetX = (screenW - g.gridW) / 2;
    g.offsetY = topSpace + (availH - g.gridH) / 2 + PLUGIN_MARGIN;
    
    return g;
}

void test_grid_layout_landscape() {
    // Standard landscape: 800x480
    GridLayout g = calculateGrid(800, 480, 8, 8, true, true);
    
    TEST_ASSERT_TRUE(g.landscape);
    TEST_ASSERT_EQUAL(8, g.cols);
    TEST_ASSERT_EQUAL(8, g.rows);
    TEST_ASSERT_TRUE(g.cellSize > 0);
    TEST_ASSERT_TRUE(g.gridW <= 800);
    TEST_ASSERT_TRUE(g.gridH <= 480);
}

void test_grid_layout_portrait() {
    // Standard portrait: 480x800
    GridLayout g = calculateGrid(480, 800, 8, 8, true, true);
    
    TEST_ASSERT_FALSE(g.landscape);
    TEST_ASSERT_EQUAL(8, g.cols);
    TEST_ASSERT_EQUAL(8, g.rows);
}

void test_grid_layout_centered() {
    // Grid should be centered
    GridLayout g = calculateGrid(800, 480, 4, 4, false, false);
    
    int expectedCenterX = 800 / 2;
    int actualCenterX = g.offsetX + g.gridW / 2;
    
    // Allow 1 pixel tolerance for rounding
    TEST_ASSERT_INT_WITHIN(1, expectedCenterX, actualCenterX);
}

// =============================================================================
// TEST: Hash Function (for cover cache)
// =============================================================================

uint32_t hashPath(const char* path) {
    uint32_t hash = 0;
    for (const char* p = path; *p; p++) {
        hash = hash * 31 + *p;
    }
    return hash;
}

void test_hash_consistency() {
    // Same input should always produce same hash
    const char* path = "/books/test.epub";
    uint32_t hash1 = hashPath(path);
    uint32_t hash2 = hashPath(path);
    TEST_ASSERT_EQUAL_UINT32(hash1, hash2);
}

void test_hash_uniqueness() {
    // Different inputs should produce different hashes (with high probability)
    uint32_t hash1 = hashPath("/books/book1.epub");
    uint32_t hash2 = hashPath("/books/book2.epub");
    TEST_ASSERT_NOT_EQUAL(hash1, hash2);
}

void test_hash_deterministic() {
    // Known input should produce known output
    uint32_t hash = hashPath("test");
    // 't' = 116, 'e' = 101, 's' = 115, 't' = 116
    // hash = 0*31 + 116 = 116
    // hash = 116*31 + 101 = 3697
    // hash = 3697*31 + 115 = 114722
    // hash = 114722*31 + 116 = 3556498
    TEST_ASSERT_EQUAL_UINT32(3556498, hash);
}

// =============================================================================
// TEST: Bitmap Operations (for home items)
// =============================================================================

#define HOME_ITEMS_BYTES 4

bool isItemEnabled(const uint8_t* bitmap, uint8_t idx) {
    if (idx >= HOME_ITEMS_BYTES * 8) return false;
    return (bitmap[idx / 8] & (1 << (idx % 8))) != 0;
}

void setItemEnabled(uint8_t* bitmap, uint8_t idx, bool enabled) {
    if (idx >= HOME_ITEMS_BYTES * 8) return;
    if (enabled) {
        bitmap[idx / 8] |= (1 << (idx % 8));
    } else {
        bitmap[idx / 8] &= ~(1 << (idx % 8));
    }
}

void test_bitmap_default_disabled() {
    uint8_t bitmap[HOME_ITEMS_BYTES] = {0};
    
    for (int i = 0; i < HOME_ITEMS_BYTES * 8; i++) {
        TEST_ASSERT_FALSE(isItemEnabled(bitmap, i));
    }
}

void test_bitmap_set_get() {
    uint8_t bitmap[HOME_ITEMS_BYTES] = {0};
    
    setItemEnabled(bitmap, 0, true);
    setItemEnabled(bitmap, 7, true);
    setItemEnabled(bitmap, 15, true);
    
    TEST_ASSERT_TRUE(isItemEnabled(bitmap, 0));
    TEST_ASSERT_TRUE(isItemEnabled(bitmap, 7));
    TEST_ASSERT_TRUE(isItemEnabled(bitmap, 15));
    TEST_ASSERT_FALSE(isItemEnabled(bitmap, 1));
    TEST_ASSERT_FALSE(isItemEnabled(bitmap, 8));
}

void test_bitmap_toggle() {
    uint8_t bitmap[HOME_ITEMS_BYTES] = {0};
    
    setItemEnabled(bitmap, 5, true);
    TEST_ASSERT_TRUE(isItemEnabled(bitmap, 5));
    
    setItemEnabled(bitmap, 5, false);
    TEST_ASSERT_FALSE(isItemEnabled(bitmap, 5));
}

// =============================================================================
// TEST: String Trimming
// =============================================================================

void trimString(char* str) {
    char* start = str;
    while (*start == ' ' || *start == '\t' || *start == '\r' || *start == '\n') start++;
    
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    
    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' || 
                       str[len-1] == '\r' || str[len-1] == '\n')) {
        str[--len] = '\0';
    }
}

void test_trim_leading() {
    char str[] = "   hello";
    trimString(str);
    TEST_ASSERT_EQUAL_STRING("hello", str);
}

void test_trim_trailing() {
    char str[] = "hello   ";
    trimString(str);
    TEST_ASSERT_EQUAL_STRING("hello", str);
}

void test_trim_both() {
    char str[] = "  hello world  ";
    trimString(str);
    TEST_ASSERT_EQUAL_STRING("hello world", str);
}

void test_trim_empty() {
    char str[] = "   ";
    trimString(str);
    TEST_ASSERT_EQUAL_STRING("", str);
}

void test_trim_none_needed() {
    char str[] = "hello";
    trimString(str);
    TEST_ASSERT_EQUAL_STRING("hello", str);
}

// =============================================================================
// TEST: ADC Button Reading Simulation
// =============================================================================

#define BTN_THRESHOLD    100
#define BTN_RIGHT_VAL    3
#define BTN_LEFT_VAL     1470
#define BTN_CONFIRM_VAL  2655
#define BTN_BACK_VAL     3470
#define BTN_DOWN_VAL     3
#define BTN_UP_VAL       2205

Button simulateAdcButtonRead(int adc1Value, int adc2Value) {
    // GPIO2 (ADC2) handles UP/DOWN
    if (adc2Value < BTN_THRESHOLD) return BTN_DOWN;
    if (adc2Value > BTN_UP_VAL - BTN_THRESHOLD && adc2Value < BTN_UP_VAL + BTN_THRESHOLD) return BTN_UP;
    
    // GPIO1 (ADC1) handles LEFT/RIGHT/CONFIRM/BACK
    if (adc1Value < BTN_THRESHOLD) return BTN_RIGHT;
    if (adc1Value > BTN_LEFT_VAL - BTN_THRESHOLD && adc1Value < BTN_LEFT_VAL + BTN_THRESHOLD) return BTN_LEFT;
    if (adc1Value > BTN_CONFIRM_VAL - BTN_THRESHOLD && adc1Value < BTN_CONFIRM_VAL + BTN_THRESHOLD) return BTN_CONFIRM;
    if (adc1Value > BTN_BACK_VAL - BTN_THRESHOLD && adc1Value < BTN_BACK_VAL + BTN_THRESHOLD) return BTN_BACK;
    
    return BTN_NONE;
}

void test_adc_button_none() {
    TEST_ASSERT_EQUAL(BTN_NONE, simulateAdcButtonRead(2000, 1500));
}

void test_adc_button_right() {
    TEST_ASSERT_EQUAL(BTN_RIGHT, simulateAdcButtonRead(BTN_RIGHT_VAL, 2000));
}

void test_adc_button_left() {
    TEST_ASSERT_EQUAL(BTN_LEFT, simulateAdcButtonRead(BTN_LEFT_VAL, 2000));
}

void test_adc_button_confirm() {
    TEST_ASSERT_EQUAL(BTN_CONFIRM, simulateAdcButtonRead(BTN_CONFIRM_VAL, 2000));
}

void test_adc_button_back() {
    TEST_ASSERT_EQUAL(BTN_BACK, simulateAdcButtonRead(BTN_BACK_VAL, 2000));
}

void test_adc_button_up() {
    TEST_ASSERT_EQUAL(BTN_UP, simulateAdcButtonRead(2000, BTN_UP_VAL));
}

void test_adc_button_down() {
    TEST_ASSERT_EQUAL(BTN_DOWN, simulateAdcButtonRead(2000, BTN_DOWN_VAL));
}

// =============================================================================
// TEST RUNNER
// =============================================================================

void setUp(void) {
    // Called before each test
}

void tearDown(void) {
    // Called after each test
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    
    // Button mapping tests
    RUN_TEST(test_button_mapping_landscape);
    RUN_TEST(test_button_mapping_portrait);
    
    // Value clamping tests
    RUN_TEST(test_clamp_within_range);
    RUN_TEST(test_clamp_below_range);
    RUN_TEST(test_clamp_above_range);
    
    // Grid layout tests
    RUN_TEST(test_grid_layout_landscape);
    RUN_TEST(test_grid_layout_portrait);
    RUN_TEST(test_grid_layout_centered);
    
    // Hash function tests
    RUN_TEST(test_hash_consistency);
    RUN_TEST(test_hash_uniqueness);
    RUN_TEST(test_hash_deterministic);
    
    // Bitmap tests
    RUN_TEST(test_bitmap_default_disabled);
    RUN_TEST(test_bitmap_set_get);
    RUN_TEST(test_bitmap_toggle);
    
    // String trimming tests
    RUN_TEST(test_trim_leading);
    RUN_TEST(test_trim_trailing);
    RUN_TEST(test_trim_both);
    RUN_TEST(test_trim_empty);
    RUN_TEST(test_trim_none_needed);
    
    // ADC button reading tests
    RUN_TEST(test_adc_button_none);
    RUN_TEST(test_adc_button_right);
    RUN_TEST(test_adc_button_left);
    RUN_TEST(test_adc_button_confirm);
    RUN_TEST(test_adc_button_back);
    RUN_TEST(test_adc_button_up);
    RUN_TEST(test_adc_button_down);
    
    return UNITY_END();
}
