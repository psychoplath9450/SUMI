#ifndef SUMI_PLUGIN_CUBE3D_H
#define SUMI_PLUGIN_CUBE3D_H

/**
 * @file Cube3D.h
 * @brief Demo/Development Testing Plugin - E-ink Game Template
 * @version 2.0.0
 *
 * A testing sandbox for e-paper game development concepts.
 * Use this to prototype ideas without touching other code.
 * 
 * Demos:
 * 1. 3D World - First-person view with pillars (movement test)
 * 2. Matrix Rain - Classic falling characters effect
 */

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <math.h>
#include "core/PluginHelpers.h"
#include "core/SettingsManager.h"
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeMono9pt7b.h>

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern SettingsManager settingsManager;

// =============================================================================
// 3D World Constants
// =============================================================================
#define WORLD_SIZE 20        // World is 20x20 units
#define MAX_PILLARS 12       // Number of pillars in world
#define PILLAR_RADIUS 0.4f   // Pillar collision radius
#define MOVE_SPEED 1.2f      // Movement per button press (faster!)
#define TURN_SPEED 0.4f      // Radians per turn (faster turning)
#define VIEW_DISTANCE 15.0f  // Max render distance
#define FOV 1.2f             // Field of view in radians (~70 degrees)

// Pillar structure
struct Pillar {
    float x, z;      // Position in world
    float height;    // Height (affects rendered size)
    bool dark;       // Dark or light pillar
};

class Cube3DApp {
public:
    enum AppState {
        STATE_MENU,
        STATE_3D_WORLD,
        STATE_MATRIX
    };
    
    Cube3DApp() : 
        screenW(800), screenH(480),
        running(true), needsRedraw(true),
        appState(STATE_MENU), menuCursor(0),
        // Player state
        playerX(10.0f), playerZ(10.0f), playerAngle(0.0f),
        // Matrix state
        matrixDensity(2), matrixSpeed(2),
        lastFrameTime(0) {
    }
    
    bool needsFullRedraw = true;
    
    void init(int w, int h) {
        screenW = w;
        screenH = h;
        needsFullRedraw = true;
        
        // Initialize pillars in world
        initWorld();
        initMatrix();
        
        Serial.println("[DEMO] Demo plugin initialized");
    }
    
    bool handleInput(Button btn) {
        switch (appState) {
            case STATE_MENU:
                return handleMenuInput(btn);
            case STATE_3D_WORLD:
                return handle3DInput(btn);
            case STATE_MATRIX:
                return handleMatrixInput(btn);
        }
        return false;
    }
    
    bool update() {
        if (appState == STATE_MATRIX && !paused) {
            uint32_t now = millis();
            uint32_t delay = 600 - (matrixSpeed * 100);
            if (now - lastFrameTime >= delay) {
                lastFrameTime = now;
                updateMatrix();
                needsFullRedraw = true;
                return true;
            }
        }
        return false;
    }
    
    void draw() {
        if (!needsFullRedraw) return;
        
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            
            switch (appState) {
                case STATE_MENU:
                    drawMenu();
                    break;
                case STATE_3D_WORLD:
                    draw3DWorld();
                    break;
                case STATE_MATRIX:
                    drawMatrix();
                    break;
            }
        } while (display.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawPartial() {
        draw();
    }
    
    void drawFullScreen() {
        needsFullRedraw = true;
        draw();
    }
    
    bool isRunning() const { return running; }
    
private:
    int screenW, screenH;
    bool running, needsRedraw, paused = false;
    AppState appState;
    int menuCursor;
    
    // Player state for 3D world
    float playerX, playerZ;  // Position
    float playerAngle;       // Facing direction (radians)
    
    // World objects
    Pillar pillars[MAX_PILLARS];
    int pillarCount = 0;
    
    // Matrix rain state
    int matrixDensity, matrixSpeed;
    static const int MATRIX_COLS = 40;
    static const int MATRIX_ROWS = 20;
    char matrixChars[MATRIX_COLS][MATRIX_ROWS];
    int matrixDrops[MATRIX_COLS];
    uint32_t lastFrameTime;
    
    // =========================================================================
    // WORLD INITIALIZATION
    // =========================================================================
    
    void initWorld() {
        // Create some pillars scattered around
        // Avoid center spawn area
        pillarCount = 0;
        
        // Fixed pillar positions for consistent world
        float positions[][3] = {
            {5.0f, 8.0f, 2.0f},     // x, z, height
            {15.0f, 8.0f, 2.5f},
            {8.0f, 5.0f, 1.8f},
            {12.0f, 5.0f, 2.2f},
            {6.0f, 15.0f, 2.0f},
            {14.0f, 15.0f, 1.5f},
            {3.0f, 12.0f, 2.8f},
            {17.0f, 12.0f, 2.0f},
            {10.0f, 3.0f, 3.0f},
            {10.0f, 17.0f, 2.5f},
            {4.0f, 4.0f, 1.8f},
            {16.0f, 16.0f, 2.2f}
        };
        
        for (int i = 0; i < MAX_PILLARS; i++) {
            pillars[i].x = positions[i][0];
            pillars[i].z = positions[i][1];
            pillars[i].height = positions[i][2];
            pillars[i].dark = (i % 2 == 0);
        }
        pillarCount = MAX_PILLARS;
        
        // Spawn player close to a pillar, facing toward it
        // Pillar at (5, 8) - spawn at (5, 6) facing +Z direction
        playerX = 5.0f;
        playerZ = 6.0f;
        playerAngle = 0.0f;  // Face +Z direction (toward pillar at z=8)
    }
    
    // =========================================================================
    // MENU
    // =========================================================================
    
    static const int MENU_ITEMS = 2;
    const char* menuLabels[MENU_ITEMS] = { "3D World", "Matrix Rain" };
    const char* menuDescs[MENU_ITEMS] = { 
        "First-person movement demo", 
        "Classic falling characters" 
    };
    
    bool handleMenuInput(Button btn) {
        switch (btn) {
            case BTN_BACK:
                running = false;
                return false;
                
            case BTN_UP:
                if (menuCursor > 0) {
                    menuCursor--;
                    needsFullRedraw = true;
                }
                return true;
                
            case BTN_DOWN:
                if (menuCursor < MENU_ITEMS - 1) {
                    menuCursor++;
                    needsFullRedraw = true;
                }
                return true;
                
            case BTN_CONFIRM:
                if (menuCursor == 0) {
                    appState = STATE_3D_WORLD;
                    initWorld();
                } else {
                    appState = STATE_MATRIX;
                    initMatrix();
                }
                needsFullRedraw = true;
                return true;
                
            default:
                return false;
        }
    }
    
    void drawMenu() {
        // Header bar
        display.fillRect(0, 0, screenW, 48, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold12pt7b);
        int16_t tx, ty; uint16_t tw, th;
        display.getTextBounds("Demo Lab", 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, 32);
        display.print("Demo Lab");
        
        display.setTextColor(GxEPD_BLACK);
        
        // Subtitle
        display.setFont(&FreeSans9pt7b);
        display.getTextBounds("Development & Testing Sandbox", 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, 75);
        display.print("Development & Testing Sandbox");
        
        // Menu items as cards
        int cardW = screenW - 80;
        int cardH = 70;
        int cardX = 40;
        int startY = 100;
        
        for (int i = 0; i < MENU_ITEMS; i++) {
            int y = startY + i * (cardH + 16);
            bool selected = (i == menuCursor);
            
            if (selected) {
                display.fillRoundRect(cardX, y, cardW, cardH, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_WHITE);
            } else {
                display.drawRoundRect(cardX, y, cardW, cardH, 8, GxEPD_BLACK);
                display.setTextColor(GxEPD_BLACK);
            }
            
            // Icon
            display.setFont(&FreeSansBold12pt7b);
            display.setCursor(cardX + 20, y + 30);
            display.print(i == 0 ? ">" : "#");
            
            // Title
            display.setCursor(cardX + 50, y + 30);
            display.print(menuLabels[i]);
            
            // Description
            display.setFont(&FreeSans9pt7b);
            if (!selected) display.setTextColor(GxEPD_BLACK);
            display.setCursor(cardX + 50, y + 52);
            display.print(menuDescs[i]);
        }
        
        // Footer
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.getTextBounds("UP/DOWN: Select | OK: Launch | BACK: Exit", 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, screenH - 20);
        display.print("UP/DOWN: Select | OK: Launch | BACK: Exit");
    }
    
    // =========================================================================
    // 3D WORLD
    // =========================================================================
    
    bool handle3DInput(Button btn) {
        float dx = 0, dz = 0;
        
        switch (btn) {
            case BTN_BACK:
                appState = STATE_MENU;
                needsFullRedraw = true;
                return true;
                
            case BTN_DOWN:
                // Move forward (toward view direction)
                dx = sin(playerAngle) * MOVE_SPEED;
                dz = cos(playerAngle) * MOVE_SPEED;
                tryMove(dx, dz);
                needsFullRedraw = true;
                return true;
                
            case BTN_UP:
                // Move backward (away from view direction)
                dx = -sin(playerAngle) * MOVE_SPEED;
                dz = -cos(playerAngle) * MOVE_SPEED;
                tryMove(dx, dz);
                needsFullRedraw = true;
                return true;
                
            case BTN_RIGHT:
                // Turn left (counterclockwise when viewed from above)
                playerAngle += TURN_SPEED;
                needsFullRedraw = true;
                return true;
                
            case BTN_LEFT:
                // Turn right (clockwise when viewed from above)
                playerAngle -= TURN_SPEED;
                needsFullRedraw = true;
                return true;
                
            case BTN_CONFIRM:
                // Toggle minimap or action
                needsFullRedraw = true;
                return true;
                
            default:
                return false;
        }
    }
    
    void tryMove(float dx, float dz) {
        float newX = playerX + dx;
        float newZ = playerZ + dz;
        
        // World bounds
        if (newX < 1.0f || newX > WORLD_SIZE - 1) return;
        if (newZ < 1.0f || newZ > WORLD_SIZE - 1) return;
        
        // Collision with pillars
        for (int i = 0; i < pillarCount; i++) {
            float distSq = (newX - pillars[i].x) * (newX - pillars[i].x) +
                          (newZ - pillars[i].z) * (newZ - pillars[i].z);
            if (distSq < (PILLAR_RADIUS + 0.3f) * (PILLAR_RADIUS + 0.3f)) {
                return; // Collision
            }
        }
        
        playerX = newX;
        playerZ = newZ;
    }
    
    void draw3DWorld() {
        int horizonY = screenH / 2 - 20;  // Horizon line
        
        // Sky (white - already filled)
        
        // Ground with dither pattern (gray)
        for (int y = horizonY; y < screenH - 40; y++) {
            for (int x = 0; x < screenW; x++) {
                // Dither pattern for gray ground
                // Denser near horizon, sparser far away
                int ditherThreshold = ((y - horizonY) * 2 + (x % 4) * 3 + (y % 4)) % 8;
                if (ditherThreshold < 3) {
                    display.drawPixel(x, y, GxEPD_BLACK);
                }
            }
        }
        
        // Horizon line
        display.drawLine(0, horizonY, screenW, horizonY, GxEPD_BLACK);
        
        // Render pillars (sorted by distance, far to near)
        // Simple bubble sort for distance
        int order[MAX_PILLARS];
        float distances[MAX_PILLARS];
        
        for (int i = 0; i < pillarCount; i++) {
            order[i] = i;
            float dx = pillars[i].x - playerX;
            float dz = pillars[i].z - playerZ;
            distances[i] = dx * dx + dz * dz;
        }
        
        // Sort far to near
        for (int i = 0; i < pillarCount - 1; i++) {
            for (int j = i + 1; j < pillarCount; j++) {
                if (distances[order[i]] < distances[order[j]]) {
                    int tmp = order[i];
                    order[i] = order[j];
                    order[j] = tmp;
                }
            }
        }
        
        // Render each pillar
        for (int i = 0; i < pillarCount; i++) {
            renderPillar(pillars[order[i]], horizonY);
        }
        
        // UI overlay
        drawWorldUI();
    }
    
    void renderPillar(const Pillar& p, int horizonY) {
        // Get relative position to player
        float dx = p.x - playerX;
        float dz = p.z - playerZ;
        
        // Rotate by player angle to get view-space coords
        float viewX = dx * cos(-playerAngle) - dz * sin(-playerAngle);
        float viewZ = dx * sin(-playerAngle) + dz * cos(-playerAngle);
        
        // Behind player?
        if (viewZ <= 0.1f) return;
        
        // Too far?
        if (viewZ > VIEW_DISTANCE) return;
        
        // Project to screen
        float screenX = (screenW / 2) + (viewX / viewZ) * (screenW / 2) / tan(FOV / 2);
        
        // Calculate pillar size based on distance
        float scale = 1.0f / viewZ;
        int pillarHeight = (int)(p.height * 80 * scale);
        int pillarWidth = (int)(30 * scale);
        
        if (pillarWidth < 2) pillarWidth = 2;
        if (pillarHeight < 4) pillarHeight = 4;
        
        int px = (int)screenX - pillarWidth / 2;
        int py = horizonY - pillarHeight;
        
        // Clip to screen
        if (px + pillarWidth < 0 || px > screenW) return;
        
        // Draw pillar
        if (p.dark) {
            // Dark pillar - filled
            display.fillRect(px, py, pillarWidth, pillarHeight, GxEPD_BLACK);
        } else {
            // Light pillar - outline with cross-hatch
            display.drawRect(px, py, pillarWidth, pillarHeight, GxEPD_BLACK);
            // Cross-hatch fill
            for (int y = py + 2; y < py + pillarHeight - 2; y += 4) {
                for (int x = px + 2; x < px + pillarWidth - 2; x += 4) {
                    display.drawPixel(x, y, GxEPD_BLACK);
                }
            }
        }
        
        // Pillar top (small cap)
        display.fillRect(px - 1, py - 2, pillarWidth + 2, 3, GxEPD_BLACK);
    }
    
    void drawWorldUI() {
        // Header bar
        display.fillRect(0, 0, screenW, 36, GxEPD_BLACK);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(12, 25);
        display.print("3D World Demo");
        
        // Position display
        char posStr[32];
        snprintf(posStr, 32, "X:%.1f Z:%.1f", playerX, playerZ);
        display.setCursor(screenW - 120, 25);
        display.print(posStr);
        
        // Minimap (bottom right)
        int mapSize = 80;
        int mapX = screenW - mapSize - 12;
        int mapY = screenH - mapSize - 50;
        
        // Map background
        display.fillRect(mapX, mapY, mapSize, mapSize, GxEPD_WHITE);
        display.drawRect(mapX, mapY, mapSize, mapSize, GxEPD_BLACK);
        
        // Scale: world to map
        float scale = (float)mapSize / WORLD_SIZE;
        
        // Draw pillars on map
        for (int i = 0; i < pillarCount; i++) {
            int mx = mapX + (int)(pillars[i].x * scale);
            int my = mapY + (int)(pillars[i].z * scale);
            if (pillars[i].dark) {
                display.fillCircle(mx, my, 2, GxEPD_BLACK);
            } else {
                display.drawCircle(mx, my, 2, GxEPD_BLACK);
            }
        }
        
        // Draw player on map
        int playerMapX = mapX + (int)(playerX * scale);
        int playerMapY = mapY + (int)(playerZ * scale);
        
        // Player triangle showing direction
        float triSize = 4;
        float ax = playerMapX + sin(playerAngle) * triSize;
        float ay = playerMapY + cos(playerAngle) * triSize;
        float bx = playerMapX + sin(playerAngle + 2.5f) * triSize * 0.6f;
        float by = playerMapY + cos(playerAngle + 2.5f) * triSize * 0.6f;
        float cx = playerMapX + sin(playerAngle - 2.5f) * triSize * 0.6f;
        float cy = playerMapY + cos(playerAngle - 2.5f) * triSize * 0.6f;
        
        display.fillTriangle((int)ax, (int)ay, (int)bx, (int)by, (int)cx, (int)cy, GxEPD_BLACK);
        
        // Controls footer
        display.fillRect(0, screenH - 40, screenW, 40, GxEPD_WHITE);
        display.drawLine(0, screenH - 40, screenW, screenH - 40, GxEPD_BLACK);
        display.setTextColor(GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        int16_t tx, ty; uint16_t tw, th;
        const char* controls = "UP: Forward | DOWN: Back | LEFT/RIGHT: Turn | BACK: Menu";
        display.getTextBounds(controls, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, screenH - 15);
        display.print(controls);
    }
    
    // =========================================================================
    // MATRIX RAIN
    // =========================================================================
    
    void initMatrix() {
        for (int c = 0; c < MATRIX_COLS; c++) {
            matrixDrops[c] = random(0, MATRIX_ROWS);
            for (int r = 0; r < MATRIX_ROWS; r++) {
                matrixChars[c][r] = getRandomChar();
            }
        }
        lastFrameTime = millis();
        paused = false;
    }
    
    char getRandomChar() {
        int type = random(0, 3);
        if (type == 0) return '0' + random(0, 10);
        if (type == 1) return 'A' + random(0, 26);
        return (char)(0x30 + random(0, 64));
    }
    
    void updateMatrix() {
        for (int c = 0; c < MATRIX_COLS; c++) {
            if (random(0, 5 - matrixDensity) == 0) {
                matrixDrops[c]++;
                if (matrixDrops[c] >= MATRIX_ROWS + 8) {
                    matrixDrops[c] = 0;
                    for (int r = 0; r < MATRIX_ROWS; r++) {
                        matrixChars[c][r] = getRandomChar();
                    }
                }
            }
            // Randomly change some chars
            if (random(0, 10) == 0) {
                int r = random(0, MATRIX_ROWS);
                matrixChars[c][r] = getRandomChar();
            }
        }
    }
    
    bool handleMatrixInput(Button btn) {
        switch (btn) {
            case BTN_BACK:
                appState = STATE_MENU;
                needsFullRedraw = true;
                return true;
                
            case BTN_LEFT:
                if (matrixDensity > 1) {
                    matrixDensity--;
                    needsFullRedraw = true;
                }
                return true;
                
            case BTN_RIGHT:
                if (matrixDensity < 4) {
                    matrixDensity++;
                    needsFullRedraw = true;
                }
                return true;
                
            case BTN_UP:
                if (matrixSpeed < 5) {
                    matrixSpeed++;
                    needsFullRedraw = true;
                }
                return true;
                
            case BTN_DOWN:
                if (matrixSpeed > 1) {
                    matrixSpeed--;
                    needsFullRedraw = true;
                }
                return true;
                
            case BTN_CONFIRM:
                paused = !paused;
                needsFullRedraw = true;
                return true;
                
            default:
                return false;
        }
    }
    
    void drawMatrix() {
        // Black background
        display.fillScreen(GxEPD_BLACK);
        
        // Header
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSansBold9pt7b);
        display.setCursor(12, 25);
        display.print("Matrix Rain");
        
        if (paused) {
            display.setCursor(screenW / 2 - 40, 25);
            display.print("[PAUSED]");
        }
        
        // Settings display
        char setStr[32];
        snprintf(setStr, 32, "Density:%d Speed:%d", matrixDensity, matrixSpeed);
        display.setCursor(screenW - 160, 25);
        display.print(setStr);
        
        // Draw characters
        display.setFont(&FreeMono9pt7b);
        int charW = (screenW - 40) / MATRIX_COLS;
        int charH = (screenH - 80) / MATRIX_ROWS;
        int startX = 20;
        int startY = 45;
        
        for (int c = 0; c < MATRIX_COLS; c++) {
            int drop = matrixDrops[c];
            for (int r = 0; r < MATRIX_ROWS; r++) {
                int brightness = 0;
                
                // Calculate brightness based on distance from drop head
                int dist = drop - r;
                if (dist >= 0 && dist < 8) {
                    brightness = 8 - dist;
                }
                
                if (brightness > 0) {
                    int x = startX + c * charW;
                    int y = startY + r * charH + charH;
                    
                    // Head of drop is brightest (white)
                    if (dist == 0) {
                        display.setTextColor(GxEPD_WHITE);
                        display.setCursor(x, y);
                        display.print(matrixChars[c][r]);
                    }
                    // Tail fades out with dithering
                    else if (brightness > 3) {
                        display.setTextColor(GxEPD_WHITE);
                        display.setCursor(x, y);
                        display.print(matrixChars[c][r]);
                    }
                }
            }
        }
        
        // Controls footer
        display.fillRect(0, screenH - 35, screenW, 35, GxEPD_BLACK);
        display.drawLine(0, screenH - 35, screenW, screenH - 35, GxEPD_WHITE);
        display.setTextColor(GxEPD_WHITE);
        display.setFont(&FreeSans9pt7b);
        int16_t tx, ty; uint16_t tw, th;
        const char* controls = "L/R: Density | U/D: Speed | OK: Pause | BACK: Menu";
        display.getTextBounds(controls, 0, 0, &tx, &ty, &tw, &th);
        display.setCursor((screenW - tw) / 2, screenH - 12);
        display.print(controls);
    }
};

#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_CUBE3D_H
