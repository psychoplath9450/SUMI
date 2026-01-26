#ifndef SUMI_PLUGIN_CUBE3D_H
#define SUMI_PLUGIN_CUBE3D_H

/**
 * @file Cube3D.h
 * @brief Demo/Development Testing Plugin
 * @version 1.4.0
 *
 * A collection of visual demos and development tests for e-paper display.
 * Menu-based with multiple demo modes:
 * 
 * 1. 3D Shapes - Rotating wireframe shapes (cube, pyramid, diamond, heart)
 *    - LEFT/RIGHT: Change size
 *    - UP/DOWN: Change rotation speed
 * 
 * 2. Matrix Rain - Falling characters effect
 *    - LEFT/RIGHT: Change density
 *    - UP/DOWN: Change speed
 */

#include "config.h"
#if FEATURE_GAMES

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <math.h>
#include "core/PluginHelpers.h"
#include "core/SettingsManager.h"

extern GxEPD2_BW<GxEPD2_426_GDEQ0426T82, DISPLAY_BUFFER_HEIGHT> display;
extern SettingsManager settingsManager;

// 3D Point structure
struct Point3D {
    float x, y, z;
};

// 2D Point for projection
struct Point2D {
    int x, y;
};

class Cube3DApp {
public:
    // Application states
    enum AppState {
        STATE_MENU,
        STATE_SHAPES,
        STATE_MATRIX
    };
    
    // Shape types for 3D demo
    enum ShapeType {
        SHAPE_CUBE = 0,
        SHAPE_PYRAMID,
        SHAPE_DIAMOND,
        SHAPE_HEART,
        SHAPE_COUNT
    };
    
    Cube3DApp() : 
        screenW(800), screenH(480),
        running(true), paused(false),
        appState(STATE_MENU), menuCursor(0),
        lastFrameTime(0), frameDelay(400),
        // Shape settings
        currentShape(SHAPE_CUBE),
        angleX(0), angleY(0), angleZ(0),
        rotateSpeed(2), shapeSize(2),  // 1-5 scale
        // Matrix settings
        matrixDensity(2), matrixSpeed(2) {
    }
    
    void init(int w, int h) {
        screenW = w;
        screenH = h;
        lastFrameTime = millis();
        
        angleX = 0.3f;
        angleY = 0.5f;
        angleZ = 0.0f;
        
        initMatrix();
        
        Serial.println("[DEMO] Demo plugin initialized");
    }
    
    bool handleInput(Button btn) {
        switch (appState) {
            case STATE_MENU:
                return handleMenuInput(btn);
            case STATE_SHAPES:
                return handleShapesInput(btn);
            case STATE_MATRIX:
                return handleMatrixInput(btn);
        }
        return false;
    }
    
    void draw() {
        switch (appState) {
            case STATE_MENU:
                // Menu is static, drawn once
                break;
            case STATE_SHAPES:
                drawShapeFrame();
                break;
            case STATE_MATRIX:
                drawMatrixFrame();
                break;
        }
    }
    
    void drawFullScreen() {
        if (appState == STATE_MENU) {
            drawMenu();
        } else {
            drawDemoScreen();
        }
    }
    
    bool isRunning() const { return running; }
    
private:
    int screenW, screenH;
    bool running, paused;
    AppState appState;
    int menuCursor;
    uint32_t lastFrameTime;
    uint32_t frameDelay;
    
    // Menu items
    static const int MENU_ITEMS = 2;
    const char* menuLabels[MENU_ITEMS] = { "3D Shapes", "Matrix Rain" };
    
    // =========================================================================
    // MENU
    // =========================================================================
    
    bool handleMenuInput(Button btn) {
        switch (btn) {
            case BTN_BACK:
                running = false;
                return false;
                
            case BTN_UP:
                if (menuCursor > 0) {
                    menuCursor--;
                    drawMenu();
                }
                return true;
                
            case BTN_DOWN:
                if (menuCursor < MENU_ITEMS - 1) {
                    menuCursor++;
                    drawMenu();
                }
                return true;
                
            case BTN_CONFIRM:
                if (menuCursor == 0) {
                    appState = STATE_SHAPES;
                    paused = false;
                } else {
                    appState = STATE_MATRIX;
                    paused = false;
                    initMatrix();
                }
                drawDemoScreen();
                return true;
                
            default:
                return false;
        }
    }
    
    void drawMenu() {
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            
            // Header
            display.setFont(&FreeSansBold12pt7b);
            display.setTextColor(GxEPD_BLACK);
            display.setCursor(20, 35);
            display.print("Demo");
            
            display.setFont(&FreeSans9pt7b);
            display.setCursor(100, 35);
            display.print("- Development & Testing");
            
            display.drawLine(0, 50, screenW, 50, GxEPD_BLACK);
            
            // Menu items
            int itemH = 60;
            int startY = 80;
            int itemW = screenW - 80;
            int itemX = 40;
            
            for (int i = 0; i < MENU_ITEMS; i++) {
                int y = startY + i * (itemH + 15);
                
                if (i == menuCursor) {
                    display.fillRoundRect(itemX, y, itemW, itemH, 8, GxEPD_BLACK);
                    display.setTextColor(GxEPD_WHITE);
                } else {
                    display.drawRoundRect(itemX, y, itemW, itemH, 8, GxEPD_BLACK);
                    display.setTextColor(GxEPD_BLACK);
                }
                
                display.setFont(&FreeSansBold12pt7b);
                display.setCursor(itemX + 20, y + 38);
                display.print(menuLabels[i]);
                
                // Descriptions
                display.setFont(&FreeSans9pt7b);
                if (i == 0) {
                    display.setCursor(itemX + 200, y + 38);
                    display.print("Cube, Pyramid, Diamond, Heart");
                } else {
                    display.setCursor(itemX + 200, y + 38);
                    display.print("Falling characters effect");
                }
            }
            
            // Footer
            display.setTextColor(GxEPD_BLACK);
            display.drawLine(0, screenH - 45, screenW, screenH - 45, GxEPD_BLACK);
            display.setFont(&FreeSans9pt7b);
            display.setCursor(20, screenH - 18);
            display.print("UP/DOWN: Select | OK: Enter | BACK: Exit");
            
        } while (display.nextPage());
    }
    
    // =========================================================================
    // 3D SHAPES
    // =========================================================================
    ShapeType currentShape;
    float angleX, angleY, angleZ;
    int rotateSpeed;  // 1-5
    int shapeSize;    // 1-5
    
    bool handleShapesInput(Button btn) {
        switch (btn) {
            case BTN_BACK:
                appState = STATE_MENU;
                drawMenu();
                return true;
                
            case BTN_CONFIRM:
                // Cycle through shapes
                currentShape = (ShapeType)((currentShape + 1) % SHAPE_COUNT);
                drawDemoHeader();
                return true;
                
            case BTN_LEFT:
                if (shapeSize > 1) {
                    shapeSize--;
                    drawDemoHeader();
                }
                return true;
                
            case BTN_RIGHT:
                if (shapeSize < 5) {
                    shapeSize++;
                    drawDemoHeader();
                }
                return true;
                
            case BTN_UP:
                if (rotateSpeed < 5) {
                    rotateSpeed++;
                    drawDemoHeader();
                }
                return true;
                
            case BTN_DOWN:
                if (rotateSpeed > 1) {
                    rotateSpeed--;
                    drawDemoHeader();
                }
                return true;
                
            default:
                return false;
        }
    }
    
    Point3D rotatePoint(Point3D p, float ax, float ay, float az) {
        float cosX = cos(ax), sinX = sin(ax);
        float y1 = p.y * cosX - p.z * sinX;
        float z1 = p.y * sinX + p.z * cosX;
        
        float cosY = cos(ay), sinY = sin(ay);
        float x2 = p.x * cosY + z1 * sinY;
        float z2 = -p.x * sinY + z1 * cosY;
        
        float cosZ = cos(az), sinZ = sin(az);
        float x3 = x2 * cosZ - y1 * sinZ;
        float y3 = x2 * sinZ + y1 * cosZ;
        
        return {x3, y3, z2};
    }
    
    Point2D project(Point3D p, int cx, int cy) {
        float scale = 300.0f / (300.0f + p.z);
        return {
            cx + (int)(p.x * scale),
            cy + (int)(p.y * scale)
        };
    }
    
    void drawShapeFrame() {
        uint32_t now = millis();
        int delay = 600 - rotateSpeed * 100;  // 500ms to 100ms
        if (now - lastFrameTime < (uint32_t)delay) {
            return;
        }
        lastFrameTime = now;
        
        // Update rotation
        float speed = rotateSpeed * 0.05f;
        angleX += speed * 0.7f;
        angleY += speed;
        angleZ += speed * 0.3f;
        
        int baseSize = 40 + shapeSize * 20;  // 60 to 140
        int cx = screenW / 2;
        int cy = screenH / 2;
        
        // Calculate window
        int margin = baseSize + 60;
        int wx = cx - margin;
        int wy = 55;
        int ww = margin * 2;
        int wh = screenH - 100;
        
        if (wx < 0) { ww += wx; wx = 0; }
        if (wx + ww > screenW) ww = screenW - wx;
        
        display.setPartialWindow(wx, wy, ww, wh);
        display.firstPage();
        do {
            display.fillRect(wx, wy, ww, wh, GxEPD_WHITE);
            
            switch (currentShape) {
                case SHAPE_CUBE:
                    drawCube(baseSize, cx, cy);
                    break;
                case SHAPE_PYRAMID:
                    drawPyramid(baseSize, cx, cy);
                    break;
                case SHAPE_DIAMOND:
                    drawDiamond(baseSize, cx, cy);
                    break;
                case SHAPE_HEART:
                    drawHeart(baseSize, cx, cy);
                    break;
                default:
                    drawCube(baseSize, cx, cy);
            }
            
        } while (display.nextPage());
    }
    
    void drawCube(int s, int cx, int cy) {
        Point3D vertices[8] = {
            {(float)-s, (float)-s, (float)-s}, {(float)s, (float)-s, (float)-s}, 
            {(float)s, (float)s, (float)-s}, {(float)-s, (float)s, (float)-s},
            {(float)-s, (float)-s, (float)s}, {(float)s, (float)-s, (float)s}, 
            {(float)s, (float)s, (float)s}, {(float)-s, (float)s, (float)s}
        };
        
        Point2D projected[8];
        for (int i = 0; i < 8; i++) {
            Point3D r = rotatePoint(vertices[i], angleX, angleY, angleZ);
            projected[i] = project(r, cx, cy);
        }
        
        int edges[12][2] = {
            {0,1}, {1,2}, {2,3}, {3,0},
            {4,5}, {5,6}, {6,7}, {7,4},
            {0,4}, {1,5}, {2,6}, {3,7}
        };
        
        for (int i = 0; i < 12; i++) {
            display.drawLine(projected[edges[i][0]].x, projected[edges[i][0]].y,
                           projected[edges[i][1]].x, projected[edges[i][1]].y, GxEPD_BLACK);
        }
        
        for (int i = 0; i < 8; i++) {
            display.fillCircle(projected[i].x, projected[i].y, 3, GxEPD_BLACK);
        }
    }
    
    void drawPyramid(int s, int cx, int cy) {
        // 4 base vertices + 1 apex
        Point3D vertices[5] = {
            {(float)-s, (float)s, (float)-s}, {(float)s, (float)s, (float)-s},
            {(float)s, (float)s, (float)s}, {(float)-s, (float)s, (float)s},
            {0, (float)-s, 0}  // Apex
        };
        
        Point2D projected[5];
        for (int i = 0; i < 5; i++) {
            Point3D r = rotatePoint(vertices[i], angleX, angleY, angleZ);
            projected[i] = project(r, cx, cy);
        }
        
        // Base edges
        for (int i = 0; i < 4; i++) {
            display.drawLine(projected[i].x, projected[i].y,
                           projected[(i+1)%4].x, projected[(i+1)%4].y, GxEPD_BLACK);
        }
        // Apex edges
        for (int i = 0; i < 4; i++) {
            display.drawLine(projected[i].x, projected[i].y,
                           projected[4].x, projected[4].y, GxEPD_BLACK);
        }
        
        for (int i = 0; i < 5; i++) {
            display.fillCircle(projected[i].x, projected[i].y, 3, GxEPD_BLACK);
        }
    }
    
    void drawDiamond(int s, int cx, int cy) {
        // Octahedron - 6 vertices
        Point3D vertices[6] = {
            {0, (float)-s*1.5f, 0},  // Top
            {(float)-s, 0, 0}, {(float)s, 0, 0},
            {0, 0, (float)-s}, {0, 0, (float)s},
            {0, (float)s*1.5f, 0}   // Bottom
        };
        
        Point2D projected[6];
        for (int i = 0; i < 6; i++) {
            Point3D r = rotatePoint(vertices[i], angleX, angleY, angleZ);
            projected[i] = project(r, cx, cy);
        }
        
        // Top half edges
        int topEdges[8][2] = {{0,1},{0,2},{0,3},{0,4},{1,3},{3,2},{2,4},{4,1}};
        for (int i = 0; i < 8; i++) {
            display.drawLine(projected[topEdges[i][0]].x, projected[topEdges[i][0]].y,
                           projected[topEdges[i][1]].x, projected[topEdges[i][1]].y, GxEPD_BLACK);
        }
        // Bottom half edges
        int botEdges[4][2] = {{5,1},{5,2},{5,3},{5,4}};
        for (int i = 0; i < 4; i++) {
            display.drawLine(projected[botEdges[i][0]].x, projected[botEdges[i][0]].y,
                           projected[botEdges[i][1]].x, projected[botEdges[i][1]].y, GxEPD_BLACK);
        }
        
        for (int i = 0; i < 6; i++) {
            display.fillCircle(projected[i].x, projected[i].y, 3, GxEPD_BLACK);
        }
    }
    
    void drawHeart(int s, int cx, int cy) {
        // Heart made of connected points
        const int numPoints = 20;
        Point3D vertices[numPoints];
        
        // Generate heart shape using parametric equation
        for (int i = 0; i < numPoints; i++) {
            float t = (float)i / numPoints * 2 * PI;
            float x = 16 * pow(sin(t), 3);
            float y = -(13 * cos(t) - 5 * cos(2*t) - 2 * cos(3*t) - cos(4*t));
            vertices[i] = {x * s / 20.0f, y * s / 20.0f, 0};
        }
        
        Point2D projected[numPoints];
        for (int i = 0; i < numPoints; i++) {
            Point3D r = rotatePoint(vertices[i], angleX, angleY, angleZ);
            projected[i] = project(r, cx, cy);
        }
        
        // Connect the dots
        for (int i = 0; i < numPoints; i++) {
            int next = (i + 1) % numPoints;
            display.drawLine(projected[i].x, projected[i].y,
                           projected[next].x, projected[next].y, GxEPD_BLACK);
        }
        
        for (int i = 0; i < numPoints; i++) {
            display.fillCircle(projected[i].x, projected[i].y, 2, GxEPD_BLACK);
        }
    }
    
    const char* getShapeName() {
        switch (currentShape) {
            case SHAPE_CUBE: return "Cube";
            case SHAPE_PYRAMID: return "Pyramid";
            case SHAPE_DIAMOND: return "Diamond";
            case SHAPE_HEART: return "Heart";
            default: return "Shape";
        }
    }
    
    // =========================================================================
    // MATRIX RAIN
    // =========================================================================
    static const int MATRIX_MAX_COLS = 50;
    static const int MATRIX_MAX_ROWS = 25;
    int matrixDrops[MATRIX_MAX_COLS];
    int matrixSpeeds[MATRIX_MAX_COLS];
    char matrixChars[MATRIX_MAX_COLS][MATRIX_MAX_ROWS];
    int matrixDensity;  // 1-5
    int matrixSpeed;    // 1-5
    int matrixCols, matrixRows;
    
    bool handleMatrixInput(Button btn) {
        switch (btn) {
            case BTN_BACK:
                appState = STATE_MENU;
                drawMenu();
                return true;
                
            case BTN_CONFIRM:
                paused = !paused;
                return true;
                
            case BTN_LEFT:
                if (matrixDensity > 1) {
                    matrixDensity--;
                    updateMatrixDimensions();
                    initMatrix();
                    drawDemoHeader();
                }
                return true;
                
            case BTN_RIGHT:
                if (matrixDensity < 5) {
                    matrixDensity++;
                    updateMatrixDimensions();
                    initMatrix();
                    drawDemoHeader();
                }
                return true;
                
            case BTN_UP:
                if (matrixSpeed < 5) {
                    matrixSpeed++;
                    drawDemoHeader();
                }
                return true;
                
            case BTN_DOWN:
                if (matrixSpeed > 1) {
                    matrixSpeed--;
                    drawDemoHeader();
                }
                return true;
                
            default:
                return false;
        }
    }
    
    void initMatrix() {
        updateMatrixDimensions();
        
        for (int i = 0; i < matrixCols; i++) {
            matrixDrops[i] = random(0, matrixRows);
            matrixSpeeds[i] = random(1, 4);
            for (int j = 0; j < matrixRows; j++) {
                matrixChars[i][j] = getRandomMatrixChar();
            }
        }
    }
    
    void updateMatrixDimensions() {
        int baseCols = 15 + matrixDensity * 7;  // 22 to 50 columns
        matrixCols = min(baseCols, MATRIX_MAX_COLS);
        matrixRows = min((screenH - 100) / 18, MATRIX_MAX_ROWS);
    }
    
    char getRandomMatrixChar() {
        int type = random(3);
        if (type == 0) {
            return '0' + random(10);
        } else if (type == 1) {
            return 'A' + random(26);
        } else {
            const char symbols[] = "!@#$%^&*+=<>?/|\\~";
            return symbols[random(sizeof(symbols) - 1)];
        }
    }
    
    void drawMatrixFrame() {
        if (paused) return;
        
        uint32_t now = millis();
        int delay = 600 - matrixSpeed * 100;  // 500ms to 100ms
        if (now - lastFrameTime < (uint32_t)delay) {
            return;
        }
        lastFrameTime = now;
        
        int cellW = (screenW - 40) / matrixCols;
        int cellH = 18;
        int offsetX = 20;
        int offsetY = 55;
        
        // Update drops
        for (int i = 0; i < matrixCols; i++) {
            matrixDrops[i] += matrixSpeeds[i];
            if (matrixDrops[i] > matrixRows + 8) {
                matrixDrops[i] = 0;
                matrixSpeeds[i] = random(1, 4);
            }
            if (random(10) < 2) {
                int row = random(0, matrixRows);
                matrixChars[i][row] = getRandomMatrixChar();
            }
        }
        
        int drawH = matrixRows * cellH;
        display.setPartialWindow(offsetX - 5, offsetY - 5, 
                                 screenW - 40 + 10, drawH + 10);
        display.firstPage();
        do {
            display.fillRect(offsetX - 5, offsetY - 5, 
                            screenW - 40 + 10, drawH + 10, GxEPD_BLACK);
            
            display.setTextColor(GxEPD_WHITE);
            display.setFont(nullptr);
            
            for (int col = 0; col < matrixCols; col++) {
                int dropY = matrixDrops[col];
                
                for (int row = 0; row < matrixRows; row++) {
                    int dist = dropY - row;
                    
                    if (dist >= 0 && dist < 10) {
                        int x = offsetX + col * cellW + cellW/2 - 3;
                        int y = offsetY + row * cellH + cellH - 2;
                        
                        if (dist < 6) {
                            display.setCursor(x, y);
                            display.print((char)matrixChars[col][row]);
                        }
                    }
                }
            }
            
        } while (display.nextPage());
    }
    
    // =========================================================================
    // COMMON UI
    // =========================================================================
    
    void drawDemoScreen() {
        display.setFullWindow();
        display.firstPage();
        do {
            display.fillScreen(GxEPD_WHITE);
            drawDemoHeaderContent();
            drawDemoFooter();
        } while (display.nextPage());
    }
    
    void drawDemoHeader() {
        display.setPartialWindow(0, 0, screenW, 50);
        display.firstPage();
        do {
            display.fillRect(0, 0, screenW, 50, GxEPD_WHITE);
            drawDemoHeaderContent();
        } while (display.nextPage());
    }
    
    void drawDemoHeaderContent() {
        display.setFont(&FreeSansBold12pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(20, 35);
        
        if (appState == STATE_SHAPES) {
            display.print("3D Shapes");
            display.setFont(&FreeSans9pt7b);
            display.setCursor(150, 35);
            display.printf("Shape: %s", getShapeName());
            display.setCursor(screenW - 250, 35);
            display.printf("Size: %d  Speed: %d", shapeSize, rotateSpeed);
        } else {
            display.print("Matrix Rain");
            display.setFont(&FreeSans9pt7b);
            display.setCursor(screenW - 250, 35);
            display.printf("Density: %d  Speed: %d", matrixDensity, matrixSpeed);
        }
        
        display.drawLine(0, 48, screenW, 48, GxEPD_BLACK);
    }
    
    void drawDemoFooter() {
        display.drawLine(0, screenH - 40, screenW, screenH - 40, GxEPD_BLACK);
        display.setFont(&FreeSans9pt7b);
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(20, screenH - 15);
        
        if (appState == STATE_SHAPES) {
            display.print("L/R: Size | U/D: Speed | OK: Next Shape | BACK: Menu");
        } else {
            display.print("L/R: Density | U/D: Speed | OK: Pause | BACK: Menu");
        }
    }
};


#endif // FEATURE_GAMES
#endif // SUMI_PLUGIN_CUBE3D_H
