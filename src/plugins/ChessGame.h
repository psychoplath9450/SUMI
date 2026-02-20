#pragma once

#include "../config.h"

#if FEATURE_PLUGINS && FEATURE_GAMES

#include <Arduino.h>
#include <SDCardManager.h>
#include "PluginHelpers.h"
#include "PluginInterface.h"
#include "PluginRenderer.h"

namespace sumi {

/**
 * @file ChessGame.h
 * @brief Full-featured Chess for Sumi e-reader
 * @version 1.0.0
 *
 * Features:
 * - Main menu with New Game, Load Game, Settings
 * - Play as Black (default) or White
 * - 3 difficulty levels: Beginner, Intermediate, Advanced
 * - Captured pieces display
 * - Move history in algebraic notation
 * - Undo move (toggleable in settings)
 * - Piece animation (sliding)
 * - Board styles: Classic, High Contrast
 * - Game history with win/loss/draw stats
 * - In-game menu
 * - Save/Resume functionality
 */

// =============================================================================
// 32x32 PIECE BITMAPS — AI-generated Staunton chess pieces
// White pieces: bold outline, hollow interior (newspaper diagram style)
// Black pieces: solid filled silhouettes
// Generated from Copilot/DALL-E via tools/sprite_slicer.py
// =============================================================================

// --- WHITE PIECES (outline, 128 bytes each) ---
static const uint8_t BITMAP_W_PAWN[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x03, 0xE0, 0x00, 0x00, 0x07, 0x70, 0x00, 0x00, 0x06, 0x38, 0x00,
    0x00, 0x0C, 0x18, 0x00, 0x00, 0x0C, 0x18, 0x00, 0x00, 0x06, 0x18, 0x00,
    0x00, 0x07, 0x70, 0x00, 0x00, 0x07, 0xF8, 0x00, 0x00, 0x0C, 0x18, 0x00,
    0x00, 0x0C, 0x18, 0x00, 0x00, 0x0F, 0xF8, 0x00, 0x00, 0x06, 0x30, 0x00,
    0x00, 0x06, 0x30, 0x00, 0x00, 0x06, 0x30, 0x00, 0x00, 0x06, 0x30, 0x00,
    0x00, 0x06, 0x30, 0x00, 0x00, 0x04, 0x10, 0x00, 0x00, 0x0F, 0xF8, 0x00,
    0x00, 0x1F, 0xFC, 0x00, 0x00, 0x18, 0x0C, 0x00, 0x00, 0x1F, 0xFC, 0x00,
    0x00, 0x3F, 0xFC, 0x00, 0x00, 0x60, 0x00, 0x00, 0x00, 0x70, 0x04, 0x00,
    0x00, 0x7F, 0xFC, 0x00, 0x00, 0x1F, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t BITMAP_W_ROOK[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x66, 0x00,
    0x00, 0x0F, 0xFF, 0x00, 0x00, 0x0F, 0xDF, 0x00, 0x00, 0x0D, 0xDF, 0x00,
    0x00, 0x0D, 0xDF, 0x00, 0x00, 0x0C, 0x03, 0x00, 0x00, 0x0C, 0x03, 0x00,
    0x00, 0x07, 0xFF, 0x00, 0x00, 0x06, 0x06, 0x00, 0x00, 0x03, 0xFE, 0x00,
    0x00, 0x03, 0xFE, 0x00, 0x00, 0x02, 0x06, 0x00, 0x00, 0x02, 0x06, 0x00,
    0x00, 0x02, 0x06, 0x00, 0x00, 0x02, 0x06, 0x00, 0x00, 0x06, 0x06, 0x00,
    0x00, 0x06, 0x06, 0x00, 0x00, 0x06, 0x02, 0x00, 0x00, 0x06, 0x02, 0x00,
    0x00, 0x0F, 0xFF, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x0C, 0x03, 0x00,
    0x01, 0x1F, 0xFF, 0x80, 0x01, 0x18, 0x01, 0x80, 0x01, 0x10, 0x00, 0x80,
    0x01, 0x1F, 0xFF, 0x80, 0x01, 0x0F, 0xFF, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t BITMAP_W_KNIGHT[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
    0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0C, 0x38, 0x00, 0x00, 0x06, 0x0C, 0x00,
    0x00, 0x06, 0x06, 0x00, 0x00, 0x0C, 0x42, 0x00, 0x00, 0x0A, 0x23, 0x00,
    0x00, 0x18, 0x23, 0x00, 0x00, 0x30, 0x21, 0x00, 0x00, 0x30, 0x21, 0x00,
    0x00, 0x60, 0x41, 0x00, 0x00, 0x67, 0x81, 0x00, 0x00, 0x3C, 0x81, 0x00,
    0x00, 0x19, 0x81, 0x00, 0x00, 0x03, 0x03, 0x00, 0x00, 0x06, 0x03, 0x00,
    0x00, 0x0C, 0x03, 0x00, 0x00, 0x0C, 0x03, 0x00, 0x00, 0x18, 0x02, 0x00,
    0x00, 0x0F, 0xFE, 0x00, 0x00, 0x0F, 0xFE, 0x00, 0x00, 0x08, 0x06, 0x00,
    0x00, 0x1F, 0xFE, 0x00, 0x00, 0x3F, 0xFF, 0x00, 0x00, 0x30, 0x03, 0x00,
    0x00, 0x3F, 0xFF, 0x00, 0x00, 0x1F, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t BITMAP_W_BISHOP[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00,
    0x00, 0x01, 0xC0, 0x00, 0x00, 0x01, 0xC0, 0x00, 0x00, 0x03, 0xC0, 0x00,
    0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0xE0, 0x00, 0x00, 0x0C, 0xF0, 0x00,
    0x00, 0x0C, 0xB0, 0x00, 0x00, 0x0C, 0x30, 0x00, 0x00, 0x0C, 0x30, 0x00,
    0x00, 0x06, 0x60, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0C, 0x30, 0x00,
    0x00, 0x0F, 0xF0, 0x00, 0x00, 0x06, 0x60, 0x00, 0x00, 0x06, 0x60, 0x00,
    0x00, 0x06, 0x60, 0x00, 0x00, 0x04, 0x20, 0x00, 0x00, 0x04, 0x20, 0x00,
    0x00, 0x0C, 0x30, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x18, 0x10, 0x00,
    0x00, 0x18, 0x38, 0x00, 0x00, 0x3F, 0xFC, 0x00, 0x00, 0x30, 0x0C, 0x00,
    0x00, 0x3F, 0xFC, 0x00, 0x00, 0x1F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t BITMAP_W_QUEEN[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
    0x00, 0x46, 0x40, 0x00, 0x00, 0x66, 0x60, 0x00, 0x00, 0x6E, 0xC0, 0x00,
    0x00, 0x3F, 0xC1, 0x00, 0x00, 0x3B, 0xC0, 0x00, 0x00, 0x20, 0xC0, 0x00,
    0x00, 0x20, 0xC0, 0x00, 0x00, 0x3F, 0xC0, 0x00, 0x00, 0x3B, 0xC0, 0x00,
    0x00, 0x3F, 0xC0, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x11, 0x80, 0x00,
    0x00, 0x11, 0x80, 0x00, 0x00, 0x11, 0x80, 0x00, 0x00, 0x30, 0x80, 0x00,
    0x00, 0x30, 0x80, 0x00, 0x00, 0x30, 0x80, 0x00, 0x00, 0x30, 0x80, 0x00,
    0x00, 0x20, 0xC0, 0x00, 0x00, 0x7F, 0xC1, 0x00, 0x00, 0x60, 0x61, 0x00,
    0x00, 0x60, 0x61, 0x00, 0x00, 0xFF, 0xF3, 0x00, 0x00, 0x80, 0x32, 0x00,
    0x00, 0xFF, 0xF3, 0x00, 0x00, 0xFF, 0xE1, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t BITMAP_W_KING[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x06, 0x00, 0x00, 0x00, 0x0F, 0x80, 0x00, 0x00, 0x0F, 0x80, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x1F, 0x80, 0x00, 0x00, 0x1D, 0xC0, 0x00,
    0x00, 0x10, 0x40, 0x00, 0x00, 0x10, 0x40, 0x00, 0x00, 0x18, 0xC0, 0x00,
    0x00, 0x1F, 0x80, 0x00, 0x00, 0x1F, 0xC0, 0x00, 0x00, 0x1F, 0xC0, 0x00,
    0x00, 0x0F, 0x80, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0x08, 0x80, 0x00,
    0x00, 0x08, 0x80, 0x00, 0x00, 0x08, 0x80, 0x00, 0x00, 0x08, 0x80, 0x00,
    0x00, 0x18, 0xC0, 0x00, 0x00, 0x18, 0xC0, 0x00, 0x00, 0x1F, 0xC0, 0x00,
    0x00, 0x10, 0x60, 0x00, 0x00, 0x1F, 0xE0, 0x00, 0x00, 0x00, 0x30, 0x00,
    0x00, 0x1F, 0xF0, 0x00, 0x00, 0x1F, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// --- BLACK PIECES (solid fill, 128 bytes each) ---
static const uint8_t BITMAP_B_PAWN[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x00,
    0x00, 0x03, 0xE0, 0x00, 0x00, 0x07, 0xF0, 0x00, 0x00, 0x07, 0xF8, 0x00,
    0x00, 0x0F, 0xF8, 0x00, 0x00, 0x0F, 0xF8, 0x00, 0x00, 0x07, 0xF8, 0x00,
    0x00, 0x07, 0xF0, 0x00, 0x00, 0x07, 0xF8, 0x00, 0x00, 0x0F, 0xF8, 0x00,
    0x00, 0x0F, 0xF8, 0x00, 0x00, 0x0F, 0xF8, 0x00, 0x00, 0x07, 0xF0, 0x00,
    0x00, 0x07, 0xF0, 0x00, 0x00, 0x07, 0xF0, 0x00, 0x00, 0x07, 0xF0, 0x00,
    0x00, 0x07, 0xF0, 0x00, 0x00, 0x0F, 0xF8, 0x00, 0x00, 0x0F, 0xF8, 0x00,
    0x00, 0x1F, 0xFC, 0x00, 0x00, 0x1F, 0xFC, 0x00, 0x00, 0x1F, 0xFC, 0x00,
    0x00, 0x3F, 0xFC, 0x00, 0x00, 0x7F, 0xFC, 0x00, 0x00, 0x7F, 0xFC, 0x00,
    0x00, 0x7F, 0xFC, 0x00, 0x00, 0x1F, 0xFC, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t BITMAP_B_ROOK[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x06, 0x00,
    0x00, 0x0F, 0xFF, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x0F, 0xFF, 0x00,
    0x00, 0x0F, 0xFF, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x0F, 0xFF, 0x00,
    0x00, 0x07, 0xFF, 0x00, 0x00, 0x07, 0xFE, 0x00, 0x00, 0x03, 0xFE, 0x00,
    0x00, 0x03, 0xFE, 0x00, 0x00, 0x03, 0xFE, 0x00, 0x00, 0x03, 0xFE, 0x00,
    0x00, 0x03, 0xFE, 0x00, 0x00, 0x03, 0xFE, 0x00, 0x00, 0x07, 0xFE, 0x00,
    0x00, 0x07, 0xFE, 0x00, 0x00, 0x07, 0xFE, 0x00, 0x00, 0x07, 0xFE, 0x00,
    0x00, 0x0F, 0xFF, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x0F, 0xFF, 0x00,
    0x01, 0x1F, 0xFF, 0x80, 0x01, 0x1F, 0xFF, 0x80, 0x01, 0x1F, 0xFF, 0x80,
    0x01, 0x1F, 0xFF, 0x80, 0x01, 0x0F, 0xFF, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t BITMAP_B_KNIGHT[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00,
    0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF8, 0x00, 0x00, 0x07, 0xFC, 0x00,
    0x00, 0x07, 0xFE, 0x00, 0x00, 0x0F, 0xFE, 0x00, 0x00, 0x0F, 0xFF, 0x00,
    0x00, 0x1F, 0xFF, 0x00, 0x00, 0x3F, 0xFF, 0x00, 0x00, 0x3F, 0xFF, 0x00,
    0x00, 0x7F, 0xFF, 0x00, 0x00, 0x7F, 0xFF, 0x00, 0x00, 0x3D, 0xFF, 0x00,
    0x00, 0x19, 0xFF, 0x00, 0x00, 0x03, 0xFF, 0x00, 0x00, 0x07, 0xFF, 0x00,
    0x00, 0x0F, 0xFF, 0x00, 0x00, 0x0F, 0xFF, 0x00, 0x00, 0x1F, 0xFE, 0x00,
    0x00, 0x0F, 0xFE, 0x00, 0x00, 0x0F, 0xFE, 0x00, 0x00, 0x0F, 0xFE, 0x00,
    0x00, 0x1F, 0xFE, 0x00, 0x00, 0x3F, 0xFF, 0x00, 0x00, 0x3F, 0xFF, 0x00,
    0x00, 0x3F, 0xFF, 0x00, 0x00, 0x1F, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t BITMAP_B_BISHOP[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x80, 0x00,
    0x00, 0x01, 0xC0, 0x00, 0x00, 0x01, 0xC0, 0x00, 0x00, 0x03, 0xC0, 0x00,
    0x00, 0x07, 0xE0, 0x00, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x0F, 0xF0, 0x00,
    0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00,
    0x00, 0x07, 0xE0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00,
    0x00, 0x0F, 0xF0, 0x00, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x07, 0xE0, 0x00,
    0x00, 0x07, 0xE0, 0x00, 0x00, 0x07, 0xE0, 0x00, 0x00, 0x07, 0xE0, 0x00,
    0x00, 0x0F, 0xF0, 0x00, 0x00, 0x0F, 0xF0, 0x00, 0x00, 0x1F, 0xF0, 0x00,
    0x00, 0x1F, 0xF8, 0x00, 0x00, 0x3F, 0xFC, 0x00, 0x00, 0x3F, 0xFC, 0x00,
    0x00, 0x3F, 0xFC, 0x00, 0x00, 0x1F, 0xF8, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t BITMAP_B_QUEEN[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00,
    0x00, 0x46, 0x40, 0x00, 0x00, 0x66, 0x60, 0x00, 0x00, 0x6E, 0xC0, 0x00,
    0x00, 0x3F, 0xC1, 0x00, 0x00, 0x3F, 0xC0, 0x00, 0x00, 0x3F, 0xC0, 0x00,
    0x00, 0x3F, 0xC0, 0x00, 0x00, 0x3F, 0xC0, 0x00, 0x00, 0x3F, 0xC0, 0x00,
    0x00, 0x3F, 0xC0, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x1F, 0x80, 0x00,
    0x00, 0x1F, 0x80, 0x00, 0x00, 0x1F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00,
    0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00, 0x00, 0x3F, 0x80, 0x00,
    0x00, 0x3F, 0xC0, 0x00, 0x00, 0x7F, 0xC0, 0x00, 0x00, 0x7F, 0xE1, 0x00,
    0x00, 0x7F, 0xE1, 0x00, 0x00, 0xFF, 0xF3, 0x00, 0x00, 0xFF, 0xF3, 0x00,
    0x00, 0xFF, 0xF3, 0x00, 0x00, 0xFF, 0xE3, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};
static const uint8_t BITMAP_B_KING[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x06, 0x00, 0x00, 0x00, 0x0F, 0x80, 0x00, 0x00, 0x0F, 0x80, 0x00,
    0x00, 0x02, 0x00, 0x00, 0x00, 0x1F, 0x80, 0x00, 0x00, 0x1F, 0xC0, 0x00,
    0x00, 0x1F, 0xC0, 0x00, 0x00, 0x1F, 0xC0, 0x00, 0x00, 0x1F, 0xC0, 0x00,
    0x00, 0x1F, 0x80, 0x00, 0x00, 0x1F, 0xC0, 0x00, 0x00, 0x1F, 0xC0, 0x00,
    0x00, 0x0F, 0x80, 0x00, 0x00, 0x0F, 0x80, 0x00, 0x00, 0x0F, 0x80, 0x00,
    0x00, 0x0F, 0x80, 0x00, 0x00, 0x0F, 0x80, 0x00, 0x00, 0x0F, 0x80, 0x00,
    0x00, 0x1F, 0xC0, 0x00, 0x00, 0x1F, 0xC0, 0x00, 0x00, 0x1F, 0xC0, 0x00,
    0x00, 0x1F, 0xE0, 0x00, 0x00, 0x1F, 0xE0, 0x00, 0x00, 0x1F, 0xF0, 0x00,
    0x00, 0x1F, 0xF0, 0x00, 0x00, 0x1F, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

// --- Indexed arrays (index 0=empty, 1=pawn...6=king) ---
static const uint8_t* PIECE_BITMAPS_W[] = {
    nullptr, BITMAP_W_PAWN, BITMAP_W_ROOK, BITMAP_W_KNIGHT,
    BITMAP_W_BISHOP, BITMAP_W_QUEEN, BITMAP_W_KING
};
static const uint8_t* PIECE_BITMAPS_B[] = {
    nullptr, BITMAP_B_PAWN, BITMAP_B_ROOK, BITMAP_B_KNIGHT,
    BITMAP_B_BISHOP, BITMAP_B_QUEEN, BITMAP_B_KING
};

// Piece bitmap size
#define PIECE_BMP_SIZE 32

// =============================================================================
// Constants & Enums
// =============================================================================
enum Piece : int8_t {
    EMPTY = 0,
    W_PAWN = 1, W_ROOK = 2, W_KNIGHT = 3, W_BISHOP = 4, W_QUEEN = 5, W_KING = 6,
    B_PAWN = -1, B_ROOK = -2, B_KNIGHT = -3, B_BISHOP = -4, B_QUEEN = -5, B_KING = -6
};

static const int PIECE_VALUES[] = { 0, 100, 500, 320, 330, 900, 20000 };

enum Difficulty : uint8_t { DIFF_BEGINNER = 1, DIFF_INTERMEDIATE = 2, DIFF_ADVANCED = 3 };
enum BoardStyle : uint8_t { STYLE_CLASSIC = 0, STYLE_HIGH_CONTRAST = 1 };

enum GameScreen : uint8_t {
    CHESS_SCREEN_MAIN_MENU,
    CHESS_SCREEN_NEW_GAME,
    CHESS_SCREEN_PLAYING,
    CHESS_SCREEN_IN_GAME_MENU,
    CHESS_SCREEN_SETTINGS,
    CHESS_SCREEN_GAME_OVER
};

struct Move {
    int8_t fr, fc, tr, tc;
    int8_t captured;
    int8_t special;  // 0=normal, 1=OO, 2=OOO, 3=ep, 4=promo
    int8_t movedPiece;
    Move() : fr(-1), fc(-1), tr(-1), tc(-1), captured(0), special(0), movedPiece(0) {}
    Move(int a, int b, int c, int d) : fr(a), fc(b), tr(c), tc(d), captured(0), special(0), movedPiece(0) {}
    bool valid() const { return fr >= 0; }
};

struct MoveRecord {
    char notation[8];
    Move move;
};

// =============================================================================
// Save Data Structures
// =============================================================================
#define CHESS_SAVE_PATH "/.sumi/chess_save.bin"
#define CHESS_SETTINGS_PATH "/.sumi/chess_settings.bin"
#define CHESS_STATS_PATH "/.sumi/chess_stats.bin"

struct ChessSaveData {
    uint32_t magic = 0x43485332;  // "CHS2"
    uint8_t version = 2;
    int8_t board[8][8];
    bool whiteTurn;
    bool playerIsWhite;
    bool wCastleK, wCastleQ, bCastleK, bCastleQ;
    int8_t epCol;
    int16_t moveNum;
    Move lastMove;
    uint8_t difficulty;
    int8_t whiteCaptured[16];
    int8_t blackCaptured[16];
    uint8_t whiteCapturedCount;
    uint8_t blackCapturedCount;
    MoveRecord moveHistory[50];
    uint8_t moveHistoryCount;
    uint8_t reserved[32];
    
    bool isValid() const { return magic == 0x43485332 && version == 2; }
};

struct ChessSettings {
    uint32_t magic = 0x43485345;  // "CHSE"
    uint8_t difficulty = DIFF_INTERMEDIATE;
    uint8_t boardStyle = STYLE_CLASSIC;
    bool allowUndo = true;
    bool showLegalMoves = true;
    bool showCoordinates = true;
    bool highlightLastMove = true;
    bool animatePieces = false;  // Off by default - too slow on e-ink
    bool playerDefaultBlack = true;
    uint8_t reserved[16];
    
    bool isValid() const { return magic == 0x43485345; }
};

struct ChessStats {
    uint32_t magic = 0x43485354;  // "CHST"
    uint16_t wins = 0;
    uint16_t losses = 0;
    uint16_t draws = 0;
    uint16_t gamesPlayed = 0;
    uint8_t reserved[16];
    
    bool isValid() const { return magic == 0x43485354; }
};

// =============================================================================
// Chess Game Class
// =============================================================================
class ChessGame : public PluginInterface {
public:

  const char* name() const override { return "Chess"; }
  PluginRunMode runMode() const override { return PluginRunMode::WithUpdate; }
    // Screen dimensions
    int screenW, screenH;
    int boardX, boardY, cellSize;
    
    // Current screen/state
    GameScreen currentScreen;
    int menuCursor;
    int settingsScroll;
    
    // Game settings
    ChessSettings settings;
    ChessStats stats;
    
    // New game options
    bool newGamePlayerWhite;
    Difficulty newGameDifficulty;
    
    // Board state
    int8_t board[8][8];
    bool whiteTurn;
    bool playerIsWhite;
    bool wCastleK, wCastleQ, bCastleK, bCastleQ;
    int8_t epCol;
    bool inCheck, gameOver, checkmate, stalemate, resigned;
    
    // UI state
    int curR, curC;
    int selR, selC;
    bool hasSel;
    Move lastMove;
    int moveNum;
    
    // Captured pieces
    int8_t whiteCaptured[16];
    int8_t blackCaptured[16];
    int whiteCapturedCount;
    int blackCapturedCount;
    
    // Move history
    MoveRecord moveHistory[50];
    int moveHistoryCount;
    
    // Undo support
    Move undoMove;
    int8_t undoBoard[8][8];
    bool undoCastleK[2], undoCastleQ[2];
    int8_t undoEpCol;
    int8_t undoCaptured[16];
    int undoCapturedCount;
    bool canUndo;
    
    // Valid moves
    bool validMoves[8][8];
    
    // Animation
    bool animating;
    int animFromX, animFromY, animToX, animToY;
    int8_t animPiece;
    int animFrame;
    static const int ANIM_FRAMES = 6;   // Fewer frames for smoother feel
    static const int ANIM_DELAY = 50;   // ~20Hz for e-ink partial refresh
    unsigned long lastAnimTime;
    
    // Refresh control (needsFullRedraw inherited from PluginInterface)
    bool aiThinking;
    bool aiRenderPending;  // true = show "AI thinking..." first, compute on next tick
    bool hasSavedGame;
    
    // ==========================================================================
    // Constructor & Initialization
    // ==========================================================================
    explicit ChessGame(PluginRenderer& renderer) : d_(renderer) {
        currentScreen = CHESS_SCREEN_MAIN_MENU;
        menuCursor = 0;
        settingsScroll = 0;
        needsFullRedraw = true;
        animating = false;
        aiThinking = false;
        aiRenderPending = false;
        loadSettings();
        loadStats();
        hasSavedGame = SdMan.exists(CHESS_SAVE_PATH);
    }
    
    void init(int w, int h) override {
        // Always reset to portrait mode for chess
        d_.setRotation(3);
        // orientation handled by host
        
        screenW = 480;  // Portrait dimensions
        screenH = 800;
        
        // Calculate board layout - maximize board size and center it
        int headerH = 40;       // Slightly smaller header
        int capturedH = 24;     // Slightly smaller captured row
        int historyH = 24;      // Move history area
        int footerH = 50;       // Footer with buttons
        int coordMargin = 14;   // Space for coordinates
        
        // Calculate available space for board
        int topSpace = headerH + capturedH;
        int bottomSpace = capturedH + historyH + footerH;
        int availH = screenH - topSpace - bottomSpace - 16;  // 16px padding
        int availW = screenW - coordMargin - 8;
        
        // Calculate cell size - allow larger cells
        cellSize = min(availH, availW) / 8;
        cellSize = constrain(cellSize, 32, 48);  // Allow up to 48px cells
        
        int boardSize = cellSize * 8;
        
        // Center board horizontally
        boardX = coordMargin + (screenW - coordMargin - boardSize) / 2;
        
        // Center board vertically in available space
        int totalBoardArea = boardSize + capturedH * 2;  // Board + captured pieces rows
        int verticalSpace = screenH - headerH - historyH - footerH;
        boardY = headerH + (verticalSpace - totalBoardArea) / 2 + capturedH;
        
        Serial.printf("[CHESS] Init: %dx%d, cell=%d, board at (%d,%d)\n",
                      screenW, screenH, cellSize, boardX, boardY);
        
        hasSavedGame = SdMan.exists(CHESS_SAVE_PATH);
        currentScreen = CHESS_SCREEN_MAIN_MENU;
        menuCursor = 0;
        needsFullRedraw = true;
    }
    
    // ==========================================================================
    // Settings & Stats
    // ==========================================================================
    void loadSettings() {
        settings = ChessSettings();
        FsFile f = SdMan.open(CHESS_SETTINGS_PATH, O_RDONLY);
        if (f) {
            f.read((uint8_t*)&settings, sizeof(ChessSettings));
            f.close();
            if (!settings.isValid()) settings = ChessSettings();
        }
    }
    
    void saveSettings() {
        SdMan.mkdir("/.sumi");
        FsFile f = SdMan.open(CHESS_SETTINGS_PATH, (O_WRONLY | O_CREAT | O_TRUNC));
        if (f) {
            f.write((uint8_t*)&settings, sizeof(ChessSettings));
            f.close();
        }
    }
    
    void loadStats() {
        stats = ChessStats();
        FsFile f = SdMan.open(CHESS_STATS_PATH, O_RDONLY);
        if (f) {
            f.read((uint8_t*)&stats, sizeof(ChessStats));
            f.close();
            if (!stats.isValid()) stats = ChessStats();
        }
    }
    
    void saveStats() {
        SdMan.mkdir("/.sumi");
        FsFile f = SdMan.open(CHESS_STATS_PATH, (O_WRONLY | O_CREAT | O_TRUNC));
        if (f) {
            f.write((uint8_t*)&stats, sizeof(ChessStats));
            f.close();
        }
    }
    
    void recordGameResult(int result) {
        stats.gamesPlayed++;
        if (result > 0) stats.wins++;
        else if (result < 0) stats.losses++;
        else stats.draws++;
        saveStats();
    }
    
    // ==========================================================================
    // Game Save/Load
    // ==========================================================================
    bool saveGame() {
        SdMan.mkdir("/.sumi");
        FsFile f = SdMan.open(CHESS_SAVE_PATH, (O_WRONLY | O_CREAT | O_TRUNC));
        if (!f) return false;
        
        ChessSaveData save;
        memcpy(save.board, board, sizeof(board));
        save.whiteTurn = whiteTurn;
        save.playerIsWhite = playerIsWhite;
        save.wCastleK = wCastleK;
        save.wCastleQ = wCastleQ;
        save.bCastleK = bCastleK;
        save.bCastleQ = bCastleQ;
        save.epCol = epCol;
        save.moveNum = moveNum;
        save.lastMove = lastMove;
        save.difficulty = settings.difficulty;
        memcpy(save.whiteCaptured, whiteCaptured, sizeof(whiteCaptured));
        memcpy(save.blackCaptured, blackCaptured, sizeof(blackCaptured));
        save.whiteCapturedCount = whiteCapturedCount;
        save.blackCapturedCount = blackCapturedCount;
        save.moveHistoryCount = min(moveHistoryCount, 50);
        memcpy(save.moveHistory, moveHistory, sizeof(MoveRecord) * save.moveHistoryCount);
        
        f.write((uint8_t*)&save, sizeof(ChessSaveData));
        f.close();
        hasSavedGame = true;
        return true;
    }
    
    bool loadGame() {
        FsFile f = SdMan.open(CHESS_SAVE_PATH, O_RDONLY);
        if (!f) return false;
        
        ChessSaveData save;
        f.read((uint8_t*)&save, sizeof(ChessSaveData));
        f.close();
        
        if (!save.isValid()) {
            deleteSavedGame();
            return false;
        }
        
        memcpy(board, save.board, sizeof(board));
        whiteTurn = save.whiteTurn;
        playerIsWhite = save.playerIsWhite;
        wCastleK = save.wCastleK;
        wCastleQ = save.wCastleQ;
        bCastleK = save.bCastleK;
        bCastleQ = save.bCastleQ;
        epCol = save.epCol;
        moveNum = save.moveNum;
        lastMove = save.lastMove;
        settings.difficulty = (Difficulty)save.difficulty;
        memcpy(whiteCaptured, save.whiteCaptured, sizeof(whiteCaptured));
        memcpy(blackCaptured, save.blackCaptured, sizeof(blackCaptured));
        whiteCapturedCount = save.whiteCapturedCount;
        blackCapturedCount = save.blackCapturedCount;
        moveHistoryCount = save.moveHistoryCount;
        memcpy(moveHistory, save.moveHistory, sizeof(MoveRecord) * moveHistoryCount);
        
        curR = playerIsWhite ? 6 : 1;
        curC = 4;
        selR = selC = -1;
        hasSel = false;
        canUndo = false;
        aiThinking = false;
        aiRenderPending = false;
        animating = false;
        inCheck = gameOver = checkmate = stalemate = resigned = false;
        memset(validMoves, 0, sizeof(validMoves));
        
        updateGameState();
        return true;
    }
    
    void deleteSavedGame() {
        if (SdMan.exists(CHESS_SAVE_PATH)) {
            SdMan.remove(CHESS_SAVE_PATH);
            hasSavedGame = false;
        }
    }
    
    // ==========================================================================
    // New Game Setup
    // ==========================================================================
    void setupNewGame() {
        memset(board, 0, sizeof(board));
        
        board[0][0] = B_ROOK;  board[0][1] = B_KNIGHT; board[0][2] = B_BISHOP;
        board[0][3] = B_QUEEN; board[0][4] = B_KING;   board[0][5] = B_BISHOP;
        board[0][6] = B_KNIGHT; board[0][7] = B_ROOK;
        for (int c = 0; c < 8; c++) board[1][c] = B_PAWN;
        
        board[7][0] = W_ROOK;  board[7][1] = W_KNIGHT; board[7][2] = W_BISHOP;
        board[7][3] = W_QUEEN; board[7][4] = W_KING;   board[7][5] = W_BISHOP;
        board[7][6] = W_KNIGHT; board[7][7] = W_ROOK;
        for (int c = 0; c < 8; c++) board[6][c] = W_PAWN;
        
        whiteTurn = true;
        playerIsWhite = newGamePlayerWhite;
        settings.difficulty = newGameDifficulty;
        wCastleK = wCastleQ = bCastleK = bCastleQ = true;
        epCol = -1;
        inCheck = gameOver = checkmate = stalemate = resigned = false;
        
        curR = playerIsWhite ? 6 : 1;
        curC = 4;
        selR = selC = -1;
        hasSel = false;
        lastMove = Move();
        moveNum = 1;
        aiThinking = false;
        aiRenderPending = false;
        canUndo = false;
        animating = false;
        
        memset(validMoves, 0, sizeof(validMoves));
        memset(whiteCaptured, 0, sizeof(whiteCaptured));
        memset(blackCaptured, 0, sizeof(blackCaptured));
        whiteCapturedCount = blackCapturedCount = 0;
        moveHistoryCount = 0;
        
        deleteSavedGame();
        
        // If player is black, AI moves first — defer computation so
        // "AI thinking..." renders before the blocking minimax search
        if (!playerIsWhite) {
            aiThinking = true;
            aiRenderPending = true;
        }
    }
    
    // ==========================================================================
    // Input Handling
    // ==========================================================================
    bool handleInput(PluginButton btn) override {
        if (animating) return true;
        
        switch (currentScreen) {
            case CHESS_SCREEN_MAIN_MENU: return handleMainMenuInput(btn);
            case CHESS_SCREEN_NEW_GAME: return handleNewGameInput(btn);
            case CHESS_SCREEN_PLAYING: return handlePlayingInput(btn);
            case CHESS_SCREEN_IN_GAME_MENU: return handleInGameMenuInput(btn);
            case CHESS_SCREEN_SETTINGS: return handleSettingsInput(btn);
            case CHESS_SCREEN_GAME_OVER: return handleGameOverInput(btn);
            default: return false;
        }
    }
    
    bool handleMainMenuInput(PluginButton btn) {
        int itemCount = hasSavedGame ? 3 : 2;
        
        switch (btn) {
            case PluginButton::Up:
                if (menuCursor > 0) { menuCursor--; }  // Partial refresh
                return true;
            case PluginButton::Down:
                if (menuCursor < itemCount - 1) { menuCursor++; }  // Partial refresh
                return true;
            case PluginButton::Center: {
                int action = menuCursor;
                if (!hasSavedGame && action >= 1) action++;
                
                if (action == 0) {
                    newGamePlayerWhite = !settings.playerDefaultBlack;
                    newGameDifficulty = (Difficulty)settings.difficulty;
                    menuCursor = 4;  // Default to Start Game button
                    currentScreen = CHESS_SCREEN_NEW_GAME;
                } else if (action == 1) {
                    if (loadGame()) {
                        currentScreen = CHESS_SCREEN_PLAYING;
                    }
                } else if (action == 2) {
                    menuCursor = 0;
                    currentScreen = CHESS_SCREEN_SETTINGS;
                }
                needsFullRedraw = true;  // Screen change needs full refresh
                return true;
            }
            case PluginButton::Back:
                return false;
            default:
                return true;
        }
    }
    
    bool handleNewGameInput(PluginButton btn) {
        // 0: color selection, 1-3: difficulty, 4: start button
        switch (btn) {
            case PluginButton::Up:
                if (menuCursor > 0) { menuCursor--; }  // Partial refresh
                return true;
            case PluginButton::Down:
                if (menuCursor < 4) { menuCursor++; }  // Partial refresh
                return true;
            case PluginButton::Left:
            case PluginButton::Right:
                if (menuCursor == 0) {
                    newGamePlayerWhite = !newGamePlayerWhite;
                }  // Partial refresh
                return true;
            case PluginButton::Center:
                if (menuCursor == 0) {
                    newGamePlayerWhite = !newGamePlayerWhite;
                } else if (menuCursor >= 1 && menuCursor <= 3) {
                    newGameDifficulty = (Difficulty)menuCursor;
                } else if (menuCursor == 4) {
                    setupNewGame();
                    currentScreen = CHESS_SCREEN_PLAYING;
                    needsFullRedraw = true;  // Screen change
                    return true;
                }
                // Partial refresh for settings changes
                return true;
            case PluginButton::Back:
                menuCursor = 0;
                currentScreen = CHESS_SCREEN_MAIN_MENU;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handlePlayingInput(PluginButton btn) {
        if (aiThinking) return true;
        
        if (gameOver) {
            if (btn == PluginButton::Center || btn == PluginButton::Back) {
                currentScreen = CHESS_SCREEN_GAME_OVER;
                menuCursor = 0;
                needsFullRedraw = true;
            }
            return true;
        }
        
        bool isPlayerTurn = (whiteTurn == playerIsWhite);
        if (!isPlayerTurn) return true;
        
        // Movement needs to account for board flip when playing as black
        // When playing as black, visual UP is internal DOWN and vice versa
        switch (btn) {
            case PluginButton::Up:
                if (playerIsWhite) {
                    if (curR > 0) { curR--; }
                } else {
                    if (curR < 7) { curR++; }  // Inverted for black
                }
                return true;
            case PluginButton::Down:
                if (playerIsWhite) {
                    if (curR < 7) { curR++; }
                } else {
                    if (curR > 0) { curR--; }  // Inverted for black
                }
                return true;
            case PluginButton::Left:
                if (playerIsWhite) {
                    if (curC > 0) { curC--; }
                } else {
                    if (curC < 7) { curC++; }  // Inverted for black
                }
                return true;
            case PluginButton::Right:
                if (playerIsWhite) {
                    if (curC < 7) { curC++; }
                } else {
                    if (curC > 0) { curC--; }  // Inverted for black
                }
                return true;
            case PluginButton::Center:
                return handleSelect();
            case PluginButton::Back:
                if (hasSel) {
                    hasSel = false;
                    selR = selC = -1;
                    memset(validMoves, 0, sizeof(validMoves));
                } else {
                    menuCursor = 0;  // Preselect Save & Exit (first option)
                    currentScreen = CHESS_SCREEN_IN_GAME_MENU;
                    needsFullRedraw = true;
                    return true;
                }
                return true;
            default:
                return true;
        }
    }
    
    bool handleInGameMenuInput(PluginButton btn) {
        int itemCount = settings.allowUndo && canUndo ? 4 : 3;
        
        switch (btn) {
            case PluginButton::Up:
                if (menuCursor > 0) { menuCursor--; }
                return true;
            case PluginButton::Down:
                if (menuCursor < itemCount - 1) { menuCursor++; }
                return true;
            case PluginButton::Center: {
                int action = menuCursor;
                // Map visible index to action: Save&Exit(0), Resume(1), Undo(2), Resign(3)
                // When undo unavailable: Save&Exit(0), Resume(1), Resign(2->3)
                if (!(settings.allowUndo && canUndo) && action >= 2) action++;
                
                if (action == 0) {
                    // Save & Exit
                    saveGame();
                    return false;  // Exit plugin
                } else if (action == 1) {
                    // Resume
                    currentScreen = CHESS_SCREEN_PLAYING;
                } else if (action == 2) {
                    // Undo
                    performUndo();
                    currentScreen = CHESS_SCREEN_PLAYING;
                } else if (action == 3) {
                    // Resign
                    gameOver = true;
                    resigned = true;
                    recordGameResult(-1);
                    deleteSavedGame();
                    currentScreen = CHESS_SCREEN_GAME_OVER;
                    menuCursor = 0;
                }
                needsFullRedraw = true;
                return true;
            }
            case PluginButton::Back:
                currentScreen = CHESS_SCREEN_PLAYING;
                needsFullRedraw = true;
                return true;
            default:
                return true;
        }
    }
    
    bool handleSettingsInput(PluginButton btn) {
        const int ITEM_COUNT = 6;
        
        switch (btn) {
            case PluginButton::Up:
                if (menuCursor > 0) { menuCursor--; }  // Partial refresh
                return true;
            case PluginButton::Down:
                if (menuCursor < ITEM_COUNT - 1) { menuCursor++; }  // Partial refresh
                return true;
            case PluginButton::Left:
            case PluginButton::Right:
            case PluginButton::Center:
                if (menuCursor == 0) {
                    settings.difficulty = (settings.difficulty % 3) + 1;
                } else if (menuCursor == 1) {
                    settings.boardStyle = 1 - settings.boardStyle;
                } else if (menuCursor == 2) {
                    settings.allowUndo = !settings.allowUndo;
                } else if (menuCursor == 3) {
                    settings.showLegalMoves = !settings.showLegalMoves;
                } else if (menuCursor == 4) {
                    settings.animatePieces = !settings.animatePieces;
                } else if (menuCursor == 5) {
                    settings.highlightLastMove = !settings.highlightLastMove;
                }
                saveSettings();
                // Partial refresh for settings changes
                return true;
            case PluginButton::Back:
                menuCursor = hasSavedGame ? 2 : 1;
                currentScreen = CHESS_SCREEN_MAIN_MENU;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleGameOverInput(PluginButton btn) {
        switch (btn) {
            case PluginButton::Up:
            case PluginButton::Down:
                menuCursor = 1 - menuCursor;  // Partial refresh
                return true;
            case PluginButton::Center:
                if (menuCursor == 0) {
                    newGamePlayerWhite = !settings.playerDefaultBlack;
                    newGameDifficulty = (Difficulty)settings.difficulty;
                    menuCursor = 4;  // Default to Start Game button
                    currentScreen = CHESS_SCREEN_NEW_GAME;
                } else {
                    menuCursor = 0;
                    currentScreen = CHESS_SCREEN_MAIN_MENU;
                }
                needsFullRedraw = true;  // Screen change
                return true;
            case PluginButton::Back:
                menuCursor = 0;
                currentScreen = CHESS_SCREEN_MAIN_MENU;
                needsFullRedraw = true;  // Screen change
                return true;
            default:
                return true;
        }
    }
    
    bool handleSelect() {
        if (!hasSel) {
            int8_t piece = board[curR][curC];
            if (piece == EMPTY) return true;
            if ((piece > 0) != playerIsWhite) return true;
            
            selR = curR;
            selC = curC;
            hasSel = true;
            calcValidMoves(curR, curC);
            needsFullRedraw = true;
            return true;
        } else {
            if (curR == selR && curC == selC) {
                hasSel = false;
                selR = selC = -1;
                memset(validMoves, 0, sizeof(validMoves));
                needsFullRedraw = true;
                return true;
            }
            
            if (validMoves[curR][curC]) {
                executePlayerMove(selR, selC, curR, curC);
                return true;
            } else {
                int8_t piece = board[curR][curC];
                if (piece != EMPTY && (piece > 0) == playerIsWhite) {
                    selR = curR;
                    selC = curC;
                    calcValidMoves(curR, curC);
                    needsFullRedraw = true;
                }
            }
            return true;
        }
    }
    
    void executePlayerMove(int fr, int fc, int tr, int tc) {
        // Save for undo
        memcpy(undoBoard, board, sizeof(board));
        undoMove = lastMove;
        undoCastleK[0] = wCastleK; undoCastleK[1] = bCastleK;
        undoCastleQ[0] = wCastleQ; undoCastleQ[1] = bCastleQ;
        undoEpCol = epCol;
        undoCapturedCount = playerIsWhite ? whiteCapturedCount : blackCapturedCount;
        canUndo = true;
        
        Move move = makeMove(fr, fc, tr, tc);
        
        if (moveHistoryCount < 50) {
            generateNotation(move, moveHistory[moveHistoryCount].notation);
            moveHistory[moveHistoryCount].move = move;
            moveHistoryCount++;
        }
        
        if (settings.animatePieces) {
            startAnimation(fr, fc, tr, tc, move.movedPiece);
        }
        
        execMove(move);
        lastMove = move;
        
        hasSel = false;
        selR = selC = -1;
        memset(validMoves, 0, sizeof(validMoves));
        
        updateGameState();
        
        if (!gameOver) {
            saveGame();
            aiThinking = true;
            aiRenderPending = true;  // Render "AI thinking..." before blocking
        } else {
            if (checkmate) {
                recordGameResult(whiteTurn ? 1 : -1);
            } else if (stalemate) {
                recordGameResult(0);
            }
            deleteSavedGame();
        }
        
        needsFullRedraw = true;
    }
    
    void performUndo() {
        if (!canUndo) return;
        
        memcpy(board, undoBoard, sizeof(board));
        lastMove = undoMove;
        wCastleK = undoCastleK[0]; bCastleK = undoCastleK[1];
        wCastleQ = undoCastleQ[0]; bCastleQ = undoCastleQ[1];
        epCol = undoEpCol;
        whiteTurn = !whiteTurn;
        if (!whiteTurn) moveNum--;
        
        if (playerIsWhite) {
            whiteCapturedCount = undoCapturedCount;
        } else {
            blackCapturedCount = undoCapturedCount;
        }
        
        if (moveHistoryCount > 0) moveHistoryCount--;
        
        canUndo = false;
        hasSel = false;
        selR = selC = -1;
        memset(validMoves, 0, sizeof(validMoves));
        updateGameState();
    }
    
    // ==========================================================================
    // Update Loop
    // ==========================================================================
    bool update() {
        if (animating) {
            unsigned long now = millis();
            if (now - lastAnimTime >= ANIM_DELAY) {
                animFrame++;
                lastAnimTime = now;
                if (animFrame >= ANIM_FRAMES) {
                    animating = false;
                    needsFullRedraw = true;  // Only full refresh when animation ends
                } else {
                    // Draw animation frame with partial refresh (no black flash)
                    drawAnimationFrame();
                }
            }
            return true;
        }
        
        if (!aiThinking || gameOver) return false;
        
        // First tick after aiThinking starts: just request a render to show
        // "AI thinking..." on screen, defer actual computation to next tick
        if (aiRenderPending) {
            aiRenderPending = false;
            needsFullRedraw = true;
            return true;  // Triggers render before we block
        }
        
        Move aiMove = findBestMove();
        if (aiMove.valid()) {
            if (moveHistoryCount < 50) {
                generateNotation(aiMove, moveHistory[moveHistoryCount].notation);
                moveHistory[moveHistoryCount].move = aiMove;
                moveHistoryCount++;
            }
            
            if (settings.animatePieces) {
                startAnimation(aiMove.fr, aiMove.fc, aiMove.tr, aiMove.tc, board[aiMove.fr][aiMove.fc]);
            }
            
            execMove(aiMove);
            lastMove = aiMove;
            updateGameState();
            aiThinking = false;
            
            if (!gameOver) {
                saveGame();
            } else {
                if (checkmate) {
                    recordGameResult(whiteTurn ? -1 : 1);
                } else if (stalemate) {
                    recordGameResult(0);
                }
                deleteSavedGame();
            }
            
            needsFullRedraw = true;
            return true;
        }
        
        aiThinking = false;
        return false;
    }
    
    // ==========================================================================
    // Animation
    // ==========================================================================
    void startAnimation(int fr, int fc, int tr, int tc, int8_t piece) {
        // Convert board coordinates to display coordinates for animation
        int dispFr = playerIsWhite ? fr : (7 - fr);
        int dispFc = playerIsWhite ? fc : (7 - fc);
        int dispTr = playerIsWhite ? tr : (7 - tr);
        int dispTc = playerIsWhite ? tc : (7 - tc);
        
        animFromX = boardX + dispFc * cellSize + cellSize / 2;
        animFromY = boardY + dispFr * cellSize + cellSize / 2;
        animToX = boardX + dispTc * cellSize + cellSize / 2;
        animToY = boardY + dispTr * cellSize + cellSize / 2;
        animPiece = piece;
        animFrame = 0;
        lastAnimTime = millis();
        animating = true;
    }
    
    float easeInOut(float t) {
        return t < 0.5f ? 2 * t * t : 1 - (-2 * t + 2) * (-2 * t + 2) / 2;
    }
    
    // ==========================================================================
    // Drawing - Main
    // ==========================================================================
    void draw() override {
        d_.setFullWindow();
        d_.firstPage();
        do {
            d_.fillScreen(GxEPD_WHITE);
            
            switch (currentScreen) {
                case CHESS_SCREEN_MAIN_MENU: drawMainMenu(); break;
                case CHESS_SCREEN_NEW_GAME: drawNewGameScreen(); break;
                case CHESS_SCREEN_PLAYING: drawPlayingScreen(); break;
                case CHESS_SCREEN_IN_GAME_MENU:
                    drawPlayingScreen();
                    drawInGameMenu();
                    break;
                case CHESS_SCREEN_SETTINGS: drawSettingsScreen(); break;
                case CHESS_SCREEN_GAME_OVER: drawGameOverScreen(); break;
            }
        } while (d_.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawPartial() {
        // Use partial window for smoother menu updates
        d_.setPartialWindow(0, 0, screenW, screenH);
        d_.firstPage();
        do {
            d_.fillScreen(GxEPD_WHITE);
            
            switch (currentScreen) {
                case CHESS_SCREEN_MAIN_MENU: drawMainMenu(); break;
                case CHESS_SCREEN_NEW_GAME: drawNewGameScreen(); break;
                case CHESS_SCREEN_PLAYING: drawPlayingScreen(); break;
                case CHESS_SCREEN_IN_GAME_MENU:
                    drawPlayingScreen();
                    drawInGameMenu();
                    break;
                case CHESS_SCREEN_SETTINGS: drawSettingsScreen(); break;
                case CHESS_SCREEN_GAME_OVER: drawGameOverScreen(); break;
            }
        } while (d_.nextPage());
        
        needsFullRedraw = false;
    }
    
    void drawFullScreen() {
        needsFullRedraw = true;
        draw();
    }
    
    void drawAnimationFrame() {
        // Partial refresh just the board area for smooth animation
        int boardSize = cellSize * 8;
        d_.setPartialWindow(boardX - 2, boardY - 2, boardSize + 4, boardSize + 4);
        d_.firstPage();
        do {
            // Redraw board background - iterate in display order
            for (int dr = 0; dr < 8; dr++) {
                for (int dc = 0; dc < 8; dc++) {
                    // Convert display coords to board coords
                    int r = playerIsWhite ? dr : (7 - dr);
                    int c = playerIsWhite ? dc : (7 - dc);
                    
                    int x = boardX + dc * cellSize;
                    int y = boardY + dr * cellSize;
                    bool dark = (dr + dc) % 2 == 1;
                    
                    if (dark) {
                        if (settings.boardStyle == STYLE_HIGH_CONTRAST) {
                            d_.fillRect(x, y, cellSize, cellSize, GxEPD_BLACK);
                        } else {
                            // Dithered pattern
                            for (int py = y; py < y + cellSize; py++) {
                                for (int px = x; px < x + cellSize; px++) {
                                    if ((px + py) % 2 == 0) {
                                        d_.drawPixel(px, py, GxEPD_BLACK);
                                    } else {
                                        d_.drawPixel(px, py, GxEPD_WHITE);
                                    }
                                }
                            }
                        }
                    } else {
                        d_.fillRect(x, y, cellSize, cellSize, GxEPD_WHITE);
                    }
                    
                    // Draw pieces (except the animated one at destination)
                    int8_t piece = board[r][c];
                    if (piece != 0) {
                        // Skip if this is the destination during animation
                        if (!(animating && r == lastMove.tr && c == lastMove.tc && animFrame < ANIM_FRAMES)) {
                            drawPiece(x, y, piece, dark, false);  // Never flip pieces
                        }
                    }
                }
            }
            
            // Draw animated piece at interpolated position
            if (animating && animFrame < ANIM_FRAMES) {
                float t = easeInOut((float)animFrame / ANIM_FRAMES);
                int x = animFromX + (animToX - animFromX) * t - cellSize / 2;
                int y = animFromY + (animToY - animFromY) * t - cellSize / 2;
                
                // Determine if on dark square
                int col = (x - boardX) / cellSize;
                int row = (y - boardY) / cellSize;
                bool onDark = (col + row) % 2 == 1;
                
                drawPiece(x, y, animPiece, onDark, false);  // Never flip pieces
            }
            
            // Board border
            d_.drawRect(boardX - 1, boardY - 1, boardSize + 2, boardSize + 2, GxEPD_BLACK);
            
        } while (d_.nextPage());
    }
    
    // ==========================================================================
    // Drawing - Main Menu
    // ==========================================================================
    void drawMainMenu() {
        // Header
        d_.fillRect(0, 0, screenW, 48, GxEPD_WHITE);
        d_.drawFastHLine(4, 47, screenW - 8, GxEPD_BLACK);
        d_.drawFastHLine(4, 46, screenW - 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        centerText("Chess", screenW / 2, 34);
        
        // Mini board
        int previewSize = 200;
        int previewX = (screenW - previewSize) / 2;
        int previewY = 70;
        drawMiniBoard(previewX, previewY, previewSize, nullptr);
        
        // Menu items
        int menuY = previewY + previewSize + 32;
        int itemH = 60;
        
        const char* items[] = { "New Game", "Load Game", "Settings" };
        const char* descs[] = { "Start a fresh game vs AI", "Continue saved game", "Difficulty, options, history" };
        int itemCount = hasSavedGame ? 3 : 2;
        
        for (int i = 0; i < itemCount; i++) {
            int idx = i;
            if (!hasSavedGame && i >= 1) idx = i + 1;
            
            int y = menuY + i * (itemH + 12);
            bool sel = (menuCursor == i);
            
            if (sel) {
                d_.fillRoundRect(20, y, screenW - 40, itemH, 8, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRoundRect(20, y, screenW - 40, itemH, 8, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            d_.setFont(nullptr);
            d_.setCursor(40, y + 26);
            d_.print(items[idx]);
            
            d_.setFont(nullptr);
            d_.setCursor(40, y + 48);
            d_.print(descs[idx]);
            
            // Arrow
            d_.setFont(nullptr);
            d_.setCursor(screenW - 50, y + 38);
            d_.print(">");
        }
    }
    
    // ==========================================================================
    // Drawing - New Game Screen
    // ==========================================================================
    void drawNewGameScreen() {
        // Header
        d_.fillRect(0, 0, screenW, 48, GxEPD_WHITE);
        d_.drawFastHLine(4, 47, screenW - 8, GxEPD_BLACK);
        d_.drawFastHLine(4, 46, screenW - 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        centerText("New Game", screenW / 2, 34);
        
        int y = 64;
        
        // PLAY AS section
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        d_.setCursor(24, y);
        d_.print("PLAY AS");
        y += 16;
        
        int boxW = (screenW - 60) / 2;
        int boxH = 100;
        
        for (int i = 0; i < 2; i++) {
            int x = 24 + i * (boxW + 12);
            bool isBlack = (i == 0);
            bool sel = isBlack ? !newGamePlayerWhite : newGamePlayerWhite;
            bool isCursor = (menuCursor == 0);
            
            if (sel) {
                d_.fillRoundRect(x, y, boxW, boxH, 8, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRoundRect(x, y, boxW, boxH, 8, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            // Piece icon
            int px = x + (boxW - 32) / 2;
            int py = y + 14;
            drawPieceLarge(px, py, isBlack ? B_KING : W_KING, sel);
            
            // Label
            d_.setFont(nullptr);
            const char* label = isBlack ? "Black" : "White";
            int16_t tx, ty; uint16_t tw, th;
            d_.getTextBounds(label, 0, 0, &tx, &ty, &tw, &th);
            d_.setCursor(x + (boxW - tw) / 2, y + 72);
            d_.print(label);
            
            // Sub-label
            d_.setFont(nullptr);
            const char* sub = isBlack ? "AI moves first" : "You move first";
            d_.getTextBounds(sub, 0, 0, &tx, &ty, &tw, &th);
            d_.setCursor(x + (boxW - tw) / 2, y + 90);
            d_.print(sub);
            
            // Cursor
            if (isCursor) {
                for (int j = 0; j < 3; j++) {
                    d_.drawRoundRect(x - 2 - j, y - 2 - j, boxW + 4 + j*2, boxH + 4 + j*2, 10, GxEPD_BLACK);
                }
            }
        }
        
        y += boxH + 20;
        
        // DIFFICULTY section
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        d_.setCursor(24, y);
        d_.print("DIFFICULTY");
        y += 16;
        
        const char* diffNames[] = { "Beginner", "Intermediate", "Advanced" };
        const char* diffDescs[] = { "Makes mistakes, good for learning", "Balanced, thoughtful play", "Strong tactical play, challenging" };
        
        for (int i = 0; i < 3; i++) {
            int itemY = y + i * 54;
            bool sel = ((int)newGameDifficulty == i + 1);
            bool isCursor = (menuCursor == i + 1);
            
            if (sel) {
                d_.fillRoundRect(24, itemY, screenW - 48, 48, 6, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRoundRect(24, itemY, screenW - 48, 48, 6, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            // Radio button
            int radioX = 42;
            int radioY = itemY + 24;
            d_.drawCircle(radioX, radioY, 8, sel ? GxEPD_WHITE : GxEPD_BLACK);
            if (sel) {
                d_.fillCircle(radioX, radioY, 4, GxEPD_WHITE);
            }
            
            d_.setFont(nullptr);
            d_.setCursor(60, itemY + 20);
            d_.print(diffNames[i]);
            
            d_.setFont(nullptr);
            d_.setCursor(60, itemY + 38);
            d_.print(diffDescs[i]);
            
            if (isCursor && !sel) {
                d_.drawRoundRect(22, itemY - 2, screenW - 44, 52, 8, GxEPD_BLACK);
                d_.drawRoundRect(21, itemY - 3, screenW - 42, 54, 9, GxEPD_BLACK);
            }
        }
        
        y += 3 * 54 + 16;
        
        // Start button
        bool startSel = (menuCursor == 4);
        if (startSel) {
            d_.fillRoundRect(24, y, screenW - 48, 56, 8, GxEPD_BLACK);
            d_.setTextColor(GxEPD_WHITE);
        } else {
            d_.drawRoundRect(24, y, screenW - 48, 56, 8, GxEPD_BLACK);
            d_.setTextColor(GxEPD_BLACK);
        }
        d_.setFont(nullptr);
        centerText("Start Game", screenW / 2, y + 36);
    }
    
    // ==========================================================================
    // Drawing - Playing Screen
    // ==========================================================================
    void drawPlayingScreen() {
        // Header (40px to match layout calculation)
        d_.fillRect(0, 0, screenW, 40, GxEPD_WHITE);
        d_.drawFastHLine(4, 39, screenW - 8, GxEPD_BLACK);
        d_.drawFastHLine(4, 38, screenW - 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        
        // AI label (left)
        d_.setFont(nullptr);
        d_.setCursor(16, 26);
        d_.print("AI");
        
        // Move number (center)
        d_.setFont(nullptr);
        char moveStr[16];
        snprintf(moveStr, sizeof(moveStr), "Move %d", moveNum);
        centerText(moveStr, screenW / 2, 28);
        
        // Player label (right)
        d_.setFont(nullptr);
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds("You", 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(screenW - tw - 16, 26);
        d_.print("You");
        
        // AI captured pieces (above board)
        int captH = 24;
        int aiCaptY = boardY - captH - 2;
        d_.fillRect(0, aiCaptY, screenW, captH, GxEPD_WHITE);
        d_.drawFastHLine(0, aiCaptY + captH - 1, screenW, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        d_.setCursor(8, aiCaptY + 17);
        d_.print("AI:");
        drawCapturedPieces(42, aiCaptY + 4, 
            playerIsWhite ? blackCaptured : whiteCaptured,
            playerIsWhite ? blackCapturedCount : whiteCapturedCount);
        
        // Board
        drawBoard();
        
        // Player captured pieces (below board)
        int yourCaptY = boardY + cellSize * 8 + 2;
        d_.drawFastHLine(0, yourCaptY, screenW, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        d_.setCursor(8, yourCaptY + 17);
        d_.print("You:");
        drawCapturedPieces(48, yourCaptY + 4,
            playerIsWhite ? whiteCaptured : blackCaptured,
            playerIsWhite ? whiteCapturedCount : blackCapturedCount);
        
        // Move history
        int histY = yourCaptY + captH;
        d_.drawFastHLine(0, histY, screenW, GxEPD_BLACK);
        d_.setFont(nullptr);
        d_.setCursor(8, histY + 17);
        
        int startIdx = max(0, moveHistoryCount - 8);
        for (int i = startIdx; i < moveHistoryCount; i++) {
            if (d_.getCursorX() > screenW - 50) break;
            if (i % 2 == 0) {
                char numStr[8];
                snprintf(numStr, sizeof(numStr), "%d.", (i / 2) + 1);
                d_.print(numStr);
            }
            d_.print(moveHistory[i].notation);
            d_.print(" ");
        }
        
        // Footer
        int footerY = screenH - 54;
        d_.fillRect(0, footerY, screenW, 54, GxEPD_WHITE);
        d_.drawFastHLine(0, footerY, screenW, GxEPD_BLACK);
        d_.drawFastHLine(0, footerY + 1, screenW, GxEPD_BLACK);
        
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        d_.setCursor(12, footerY + 22);
        
        if (gameOver) {
            if (checkmate) {
                d_.print((whiteTurn == playerIsWhite) ? "Checkmate! You lose" : "Checkmate! You win!");
            } else if (stalemate) {
                d_.print("Stalemate - Draw");
            } else if (resigned) {
                d_.print("You resigned");
            }
        } else if (aiThinking) {
            d_.print("AI thinking...");
        } else {
            d_.print("Your move");
            if (inCheck) d_.print(" - CHECK!");
        }
        
        // Position info
        d_.setFont(nullptr);
        d_.setCursor(12, footerY + 42);
        char posStr[24];
        snprintf(posStr, sizeof(posStr), "%c%d", 'a' + curC, 8 - curR);
        d_.print(posStr);
        if (hasSel) {
            int cnt = 0;
            for (int r = 0; r < 8; r++)
                for (int c = 0; c < 8; c++)
                    if (validMoves[r][c]) cnt++;
            snprintf(posStr, sizeof(posStr), " sel, %d moves", cnt);
            d_.print(posStr);
        }
        
        // Buttons
        if (!gameOver && !aiThinking) {
            if (settings.allowUndo && canUndo) {
                d_.drawRoundRect(screenW - 165, footerY + 10, 72, 34, 4, GxEPD_BLACK);
                d_.setCursor(screenW - 153, footerY + 32);
                d_.print("Undo");
            }
            
            d_.drawRoundRect(screenW - 85, footerY + 10, 72, 34, 4, GxEPD_BLACK);
            d_.setCursor(screenW - 70, footerY + 32);
            d_.print("Menu");
        }
    }
    
    // ==========================================================================
    // Drawing - Board
    // ==========================================================================
    
    // Helper to flip row for display when player is black
    int displayRow(int r) const {
        return playerIsWhite ? r : (7 - r);
    }
    int displayCol(int c) const {
        return playerIsWhite ? c : (7 - c);
    }
    
    void drawBoard() {
        // Board border
        d_.drawRect(boardX - 1, boardY - 1, cellSize * 8 + 2, cellSize * 8 + 2, GxEPD_BLACK);
        
        // Squares - iterate in display order
        for (int dr = 0; dr < 8; dr++) {
            for (int dc = 0; dc < 8; dc++) {
                int r = displayRow(dr);
                int c = displayCol(dc);
                drawSquareAt(dr, dc, r, c);
            }
        }
        
        // Cursor
        if (!gameOver && !aiThinking && currentScreen == CHESS_SCREEN_PLAYING) {
            drawCursor();
        }
        
        // Animation overlay
        if (animating) {
            float t = easeInOut((float)animFrame / ANIM_FRAMES);
            int x = animFromX + (animToX - animFromX) * t - cellSize / 2;
            int y = animFromY + (animToY - animFromY) * t - cellSize / 2;
            drawPiece(x, y, animPiece, false, false);  // Never flip pieces
        }
        
        // Coordinates (flipped when player is black)
        if (settings.showCoordinates) {
            d_.setFont(NULL);
            d_.setTextSize(1);
            d_.setTextColor(GxEPD_BLACK);
            
            for (int dc = 0; dc < 8; dc++) {
                int c = displayCol(dc);
                d_.setCursor(boardX + dc * cellSize + cellSize / 2 - 2, boardY + 8 * cellSize + 4);
                d_.print((char)('a' + c));
            }
            
            for (int dr = 0; dr < 8; dr++) {
                int r = displayRow(dr);
                d_.setCursor(boardX - 10, boardY + dr * cellSize + cellSize / 2 + 3);
                d_.print((char)('8' - r));
            }
        }
    }
    
    // Draw a square at display position (dr, dc) with board position (r, c)
    void drawSquareAt(int dr, int dc, int r, int c) {
        int x = boardX + dc * cellSize;
        int y = boardY + dr * cellSize;
        bool dark = (r + c) % 2 == 1;
        
        // Background
        if (settings.boardStyle == STYLE_HIGH_CONTRAST) {
            if (dark) {
                d_.fillRect(x, y, cellSize, cellSize, GxEPD_BLACK);
            } else {
                d_.fillRect(x, y, cellSize, cellSize, GxEPD_WHITE);
            }
        } else {
            // Classic - dithered gray
            if (dark) {
                for (int py = 0; py < cellSize; py++) {
                    for (int px = 0; px < cellSize; px++) {
                        if ((px + py) % 2 == 0) {
                            d_.drawPixel(x + px, y + py, GxEPD_BLACK);
                        }
                    }
                }
            } else {
                d_.fillRect(x, y, cellSize, cellSize, GxEPD_WHITE);
            }
        }
        
        // Last move highlight
        if (settings.highlightLastMove && lastMove.valid()) {
            if ((r == lastMove.fr && c == lastMove.fc) || (r == lastMove.tr && c == lastMove.tc)) {
                uint16_t col = dark ? GxEPD_WHITE : GxEPD_BLACK;
                int m = 2, len = cellSize / 4;
                d_.drawFastHLine(x + m, y + m, len, col);
                d_.drawFastVLine(x + m, y + m, len, col);
                d_.drawFastHLine(x + cellSize - m - len, y + m, len, col);
                d_.drawFastVLine(x + cellSize - m - 1, y + m, len, col);
                d_.drawFastHLine(x + m, y + cellSize - m - 1, len, col);
                d_.drawFastVLine(x + m, y + cellSize - m - len, len, col);
                d_.drawFastHLine(x + cellSize - m - len, y + cellSize - m - 1, len, col);
                d_.drawFastVLine(x + cellSize - m - 1, y + cellSize - m - len, len, col);
            }
        }
        
        // Selection
        if (hasSel && r == selR && c == selC) {
            uint16_t col = dark ? GxEPD_WHITE : GxEPD_BLACK;
            for (int i = 2; i <= 4; i++) {
                d_.drawRect(x + i, y + i, cellSize - i*2, cellSize - i*2, col);
            }
        }
        
        // Valid move indicator
        if (settings.showLegalMoves && validMoves[r][c]) {
            int cx = x + cellSize / 2;
            int cy = y + cellSize / 2;
            int dotR = max(4, cellSize / 8);
            uint16_t col = dark ? GxEPD_WHITE : GxEPD_BLACK;
            
            if (board[r][c] != EMPTY) {
                d_.drawCircle(cx, cy, dotR + 2, col);
                d_.drawCircle(cx, cy, dotR + 3, col);
            } else {
                d_.fillCircle(cx, cy, dotR, col);
            }
        }
        
        // Piece (skip if animating from this square)
        int8_t piece = board[r][c];
        if (piece != EMPTY) {
            if (!(animating && r == lastMove.tr && c == lastMove.tc && animFrame < ANIM_FRAMES)) {
                drawPiece(x, y, piece, dark, false);  // Never flip pieces
            }
        }
    }
    
    void drawCursor() {
        // Convert board coordinates to display coordinates
        int dc = displayCol(curC);
        int dr = displayRow(curR);
        int x = boardX + dc * cellSize;
        int y = boardY + dr * cellSize;
        
        d_.drawRect(x - 3, y - 3, cellSize + 6, cellSize + 6, GxEPD_BLACK);
        d_.drawRect(x - 2, y - 2, cellSize + 4, cellSize + 4, GxEPD_WHITE);
        d_.drawRect(x - 1, y - 1, cellSize + 2, cellSize + 2, GxEPD_BLACK);
    }
    
    void drawPiece(int x, int y, int8_t piece, bool onDark, bool flipVertical = false) {
        bool isWhite = piece > 0;
        int type = abs(piece);
        if (type < 1 || type > 6) return;
        
        const uint8_t* bmp = isWhite ? PIECE_BITMAPS_W[type] : PIECE_BITMAPS_B[type];
        if (!bmp) return;
        
        // Center 32×32 bitmap in cell
        int px = x + (cellSize - PIECE_BMP_SIZE) / 2;
        int py = y + (cellSize - PIECE_BMP_SIZE) / 2;
        
        if (onDark) {
            if (settings.boardStyle == STYLE_HIGH_CONTRAST) {
                // High contrast: solid black square — draw piece in white
                // White pieces: white outline on black bg (outline visible, interior = black = matches bg)
                // Black pieces: need white outline for visibility, so use W bitmap (outline) in white
                if (isWhite) {
                    d_.drawBitmap(px, py, bmp, PIECE_BMP_SIZE, PIECE_BMP_SIZE, GxEPD_WHITE);
                } else {
                    // Draw white outline first (using W bitmap for shape), then solid black fill
                    d_.drawBitmap(px, py, PIECE_BITMAPS_W[type], PIECE_BMP_SIZE, PIECE_BMP_SIZE, GxEPD_WHITE);
                    d_.drawBitmap(px, py, bmp, PIECE_BMP_SIZE, PIECE_BMP_SIZE, GxEPD_BLACK);
                }
            } else {
                // Dithered dark square — clear a white rect behind the piece for visibility
                d_.fillRect(px, py, PIECE_BMP_SIZE, PIECE_BMP_SIZE, GxEPD_WHITE);
                d_.drawBitmap(px, py, bmp, PIECE_BMP_SIZE, PIECE_BMP_SIZE, GxEPD_BLACK);
            }
        } else {
            // Light square: just draw on white background
            d_.drawBitmap(px, py, bmp, PIECE_BMP_SIZE, PIECE_BMP_SIZE, GxEPD_BLACK);
        }
    }
    
    void drawPieceLarge(int x, int y, int8_t piece, bool inverted) {
        int type = abs(piece);
        if (type < 1 || type > 6) return;
        bool isWhite = piece > 0;
        
        const uint8_t* bmp = isWhite ? PIECE_BITMAPS_W[type] : PIECE_BITMAPS_B[type];
        if (!bmp) return;
        
        uint16_t fg = inverted ? GxEPD_WHITE : GxEPD_BLACK;
        d_.drawBitmap(x, y, bmp, PIECE_BMP_SIZE, PIECE_BMP_SIZE, fg);
    }
    
    void drawCapturedPieces(int x, int y, int8_t* captured, int count) {
        int px = x;
        const int displaySize = 10;  // Display captured pieces at 10×10
        const int step = PIECE_BMP_SIZE / displaySize;  // Sample every 3rd pixel
        
        for (int i = 0; i < count && i < 16; i++) {
            bool isWhite = captured[i] > 0;
            int type = abs(captured[i]);
            if (type < 1 || type > 6) continue;
            
            // Use solid (B) bitmaps for captured — clearer at small size
            const uint8_t* bmp = PIECE_BITMAPS_B[type];
            if (!bmp) continue;
            
            // Downsample 32×32 → 10×10 by sampling
            for (int dy = 0; dy < displaySize; dy++) {
                int srcY = dy * step + step / 2;
                if (srcY >= PIECE_BMP_SIZE) srcY = PIECE_BMP_SIZE - 1;
                for (int dx = 0; dx < displaySize; dx++) {
                    int srcX = dx * step + step / 2;
                    if (srcX >= PIECE_BMP_SIZE) srcX = PIECE_BMP_SIZE - 1;
                    int byteIdx = (srcY * PIECE_BMP_SIZE + srcX) / 8;
                    int bitIdx = 7 - (srcX % 8);
                    if (pgm_read_byte(&bmp[srcY * (PIECE_BMP_SIZE / 8) + srcX / 8]) & (1 << (7 - (srcX % 8)))) {
                        d_.drawPixel(px + dx, y + dy, GxEPD_BLACK);
                    }
                }
            }
            px += displaySize + 2;
        }
    }
    
    void drawMiniBoard(int x, int y, int size, int8_t (*brd)[8]) {
        int cs = size / 8;
        
        int8_t startBoard[8][8] = {
            {B_ROOK, B_KNIGHT, B_BISHOP, B_QUEEN, B_KING, B_BISHOP, B_KNIGHT, B_ROOK},
            {B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN, B_PAWN},
            {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0}, {0,0,0,0,0,0,0,0},
            {W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN, W_PAWN},
            {W_ROOK, W_KNIGHT, W_BISHOP, W_QUEEN, W_KING, W_BISHOP, W_KNIGHT, W_ROOK}
        };
        
        int8_t (*useBoard)[8] = brd ? brd : startBoard;
        
        d_.drawRect(x, y, size, size, GxEPD_BLACK);
        
        int pieceSize = cs - 4;  // Leave 2px padding in cell
        if (pieceSize < 4) pieceSize = 4;
        
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                int sx = x + c * cs;
                int sy = y + r * cs;
                bool dark = (r + c) % 2 == 1;
                
                if (dark) {
                    for (int py = 0; py < cs; py++) {
                        for (int px = 0; px < cs; px++) {
                            if ((px + py) % 2 == 0) {
                                d_.drawPixel(sx + px, sy + py, GxEPD_BLACK);
                            }
                        }
                    }
                }
                
                int8_t piece = useBoard[r][c];
                if (piece != 0) {
                    int type = abs(piece);
                    // Use solid silhouettes for mini board (clearest at small size)
                    const uint8_t* bmp = PIECE_BITMAPS_B[type];
                    if (!bmp) continue;
                    
                    int ppx = sx + (cs - pieceSize) / 2;
                    int ppy = sy + (cs - pieceSize) / 2;
                    int bytesPerRow = PIECE_BMP_SIZE / 8;  // 4 bytes per row
                    
                    // Downsample 32×32 → pieceSize×pieceSize
                    for (int dy = 0; dy < pieceSize; dy++) {
                        int srcY = dy * PIECE_BMP_SIZE / pieceSize;
                        for (int dx = 0; dx < pieceSize; dx++) {
                            int srcX = dx * PIECE_BMP_SIZE / pieceSize;
                            if (pgm_read_byte(&bmp[srcY * bytesPerRow + srcX / 8]) & (1 << (7 - (srcX % 8)))) {
                                d_.drawPixel(ppx + dx, ppy + dy, GxEPD_BLACK);
                            }
                        }
                    }
                }
            }
        }
    }
    
    // ==========================================================================
    // Drawing - In-Game Menu
    // ==========================================================================
    void drawInGameMenu() {
        int dialogW = 300;
        int dialogH = (settings.allowUndo && canUndo) ? 280 : 230;
        int dialogX = (screenW - dialogW) / 2;
        int dialogY = (screenH - dialogH) / 2;
        
        d_.fillRect(dialogX, dialogY, dialogW, dialogH, GxEPD_WHITE);
        for (int i = 0; i < 3; i++) {
            d_.drawRect(dialogX + i, dialogY + i, dialogW - i*2, dialogH - i*2, GxEPD_BLACK);
        }
        
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        centerText("Game Menu", dialogX + dialogW / 2, dialogY + 36);
        
        const char* allItems[] = { "Save & Exit", "Resume Game", "Undo Last Move", "Resign" };
        int itemIdx[4];
        int itemCount;
        
        if (settings.allowUndo && canUndo) {
            itemIdx[0] = 0; itemIdx[1] = 1; itemIdx[2] = 2; itemIdx[3] = 3;
            itemCount = 4;
        } else {
            itemIdx[0] = 0; itemIdx[1] = 1; itemIdx[2] = 3;
            itemCount = 3;
        }
        
        int itemY = dialogY + 55;
        int itemH = 44;
        
        for (int i = 0; i < itemCount; i++) {
            int iy = itemY + i * (itemH + 8);
            bool sel = (menuCursor == i);
            
            if (sel) {
                d_.fillRoundRect(dialogX + 16, iy, dialogW - 32, itemH, 6, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRoundRect(dialogX + 16, iy, dialogW - 32, itemH, 6, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            d_.setFont(nullptr);
            centerText(allItems[itemIdx[i]], dialogX + dialogW / 2, iy + 28);
        }
    }
    
    // ==========================================================================
    // Drawing - Settings Screen
    // ==========================================================================
    void drawSettingsScreen() {
        d_.fillRect(0, 0, screenW, 48, GxEPD_WHITE);
        d_.drawFastHLine(4, 47, screenW - 8, GxEPD_BLACK);
        d_.drawFastHLine(4, 46, screenW - 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        centerText("Settings", screenW / 2, 34);
        
        int y = 60;
        int itemH = 50;
        
        // Difficulty
        drawSettingsItem(y, 0, "Difficulty", getDiffName(settings.difficulty));
        y += itemH + 6;
        
        // Board Style
        drawSettingsItem(y, 1, "Board Style", settings.boardStyle == STYLE_CLASSIC ? "Classic" : "High Contrast");
        y += itemH + 6;
        
        // Toggles
        drawSettingsToggle(y, 2, "Allow Undo", settings.allowUndo);
        y += itemH + 6;
        
        drawSettingsToggle(y, 3, "Show Legal Moves", settings.showLegalMoves);
        y += itemH + 6;
        
        drawSettingsToggle(y, 4, "Animate Pieces", settings.animatePieces);
        y += itemH + 6;
        
        drawSettingsToggle(y, 5, "Highlight Last Move", settings.highlightLastMove);
        y += itemH + 16;
        
        // Game History
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        d_.setCursor(20, y);
        d_.print("GAME HISTORY");
        y += 20;
        
        int boxW = (screenW - 60) / 3;
        const char* labels[] = { "Wins", "Losses", "Draws" };
        int values[] = { stats.wins, stats.losses, stats.draws };
        
        for (int i = 0; i < 3; i++) {
            int bx = 20 + i * (boxW + 10);
            d_.drawRoundRect(bx, y, boxW, 60, 6, GxEPD_BLACK);
            
            d_.setFont(nullptr);
            char numStr[8];
            snprintf(numStr, sizeof(numStr), "%d", values[i]);
            centerText(numStr, bx + boxW / 2, y + 30);
            
            d_.setFont(nullptr);
            centerText(labels[i], bx + boxW / 2, y + 50);
        }
    }
    
    void drawSettingsItem(int y, int index, const char* label, const char* value) {
        bool sel = (menuCursor == index);
        
        d_.drawRoundRect(20, y, screenW - 40, 50, 6, GxEPD_BLACK);
        if (sel) {
            d_.drawRoundRect(18, y - 2, screenW - 36, 54, 8, GxEPD_BLACK);
            d_.drawRoundRect(17, y - 3, screenW - 34, 56, 9, GxEPD_BLACK);
        }
        
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        d_.setCursor(35, y + 32);
        d_.print(label);
        
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds(value, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(screenW - 60 - tw, y + 32);
        d_.print(value);
        d_.print(" <>");
    }
    
    void drawSettingsToggle(int y, int index, const char* label, bool enabled) {
        bool sel = (menuCursor == index);
        
        d_.drawRoundRect(20, y, screenW - 40, 50, 6, GxEPD_BLACK);
        if (sel) {
            d_.drawRoundRect(18, y - 2, screenW - 36, 54, 8, GxEPD_BLACK);
            d_.drawRoundRect(17, y - 3, screenW - 34, 56, 9, GxEPD_BLACK);
        }
        
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        d_.setCursor(35, y + 32);
        d_.print(label);
        
        int sw = 44, sh = 24;
        int sx = screenW - 70;
        int sy = y + 13;
        
        if (enabled) {
            d_.fillRoundRect(sx, sy, sw, sh, sh/2, GxEPD_BLACK);
            d_.fillCircle(sx + sw - sh/2, sy + sh/2, 8, GxEPD_WHITE);
        } else {
            d_.drawRoundRect(sx, sy, sw, sh, sh/2, GxEPD_BLACK);
            d_.fillCircle(sx + sh/2, sy + sh/2, 8, GxEPD_BLACK);
        }
    }
    
    // ==========================================================================
    // Drawing - Game Over Screen
    // ==========================================================================
    void drawGameOverScreen() {
        d_.fillRect(0, 0, screenW, 48, GxEPD_WHITE);
        d_.drawFastHLine(4, 47, screenW - 8, GxEPD_BLACK);
        d_.drawFastHLine(4, 46, screenW - 8, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        d_.setFont(nullptr);
        centerText("Game Over", screenW / 2, 34);
        
        // Result
        d_.fillRect(0, 56, screenW, 80, GxEPD_WHITE);
        d_.drawFastHLine(0, 56, screenW, GxEPD_BLACK);
        d_.drawFastHLine(0, 135, screenW, GxEPD_BLACK);
        
        const char* result;
        const char* subtext;
        
        if (checkmate) {
            result = (whiteTurn == playerIsWhite) ? "You Lose" : "You Win!";
            subtext = "Checkmate";
        } else if (stalemate) {
            result = "Draw";
            subtext = "Stalemate";
        } else if (resigned) {
            result = "You Lose";
            subtext = "Resigned";
        } else {
            result = "Game Over";
            subtext = "";
        }
        
        d_.setFont(nullptr);
        d_.setTextColor(GxEPD_BLACK);
        centerText(result, screenW / 2, 95);
        
        d_.setFont(nullptr);
        centerText(subtext, screenW / 2, 120);
        
        // Mini board
        int boardSize = 200;
        int bx = (screenW - boardSize) / 2;
        int by = 150;
        drawMiniBoard(bx, by, boardSize, board);
        
        // Stats
        int statsY = by + boardSize + 16;
        int statW = (screenW - 60) / 3;
        
        d_.setFont(nullptr);
        
        char val[16];
        snprintf(val, sizeof(val), "%d", moveNum);
        drawStatBox(20, statsY, statW, "Moves", val);
        
        snprintf(val, sizeof(val), "%d-%d", stats.wins, stats.losses);
        drawStatBox(20 + statW + 10, statsY, statW, "Record", val);
        
        snprintf(val, sizeof(val), "%d", stats.gamesPlayed);
        drawStatBox(20 + (statW + 10) * 2, statsY, statW, "Games", val);
        
        // Buttons
        int btnY = screenH - 70;
        int btnW = (screenW - 60) / 2;
        
        for (int i = 0; i < 2; i++) {
            int bx = 20 + i * (btnW + 20);
            bool sel = (menuCursor == i);
            
            if (sel) {
                d_.fillRoundRect(bx, btnY, btnW, 50, 8, GxEPD_BLACK);
                d_.setTextColor(GxEPD_WHITE);
            } else {
                d_.drawRoundRect(bx, btnY, btnW, 50, 8, GxEPD_BLACK);
                d_.setTextColor(GxEPD_BLACK);
            }
            
            d_.setFont(nullptr);
            centerText(i == 0 ? "Play Again" : "Exit", bx + btnW / 2, btnY + 32);
        }
    }
    
    void drawStatBox(int x, int y, int w, const char* label, const char* value) {
        d_.drawRoundRect(x, y, w, 50, 6, GxEPD_BLACK);
        d_.setTextColor(GxEPD_BLACK);
        
        d_.setFont(nullptr);
        centerText(value, x + w / 2, y + 24);
        
        d_.setFont(nullptr);
        centerText(label, x + w / 2, y + 42);
    }
    
    // ==========================================================================
    // Helpers
    // ==========================================================================
    void centerText(const char* text, int x, int y) {
        int16_t tx, ty; uint16_t tw, th;
        d_.getTextBounds(text, 0, 0, &tx, &ty, &tw, &th);
        d_.setCursor(x - tw / 2, y);
        d_.print(text);
    }
    
    const char* getDiffName(int diff) {
        switch (diff) {
            case DIFF_BEGINNER: return "Beginner";
            case DIFF_INTERMEDIATE: return "Intermediate";
            case DIFF_ADVANCED: return "Advanced";
            default: return "Unknown";
        }
    }
    
    // ==========================================================================
    // Move Generation & Notation
    // ==========================================================================
    void generateNotation(Move& move, char* buf) {
        int8_t piece = move.movedPiece;
        int type = abs(piece);
        
        if (move.special == 1) { strcpy(buf, "O-O"); return; }
        if (move.special == 2) { strcpy(buf, "O-O-O"); return; }
        
        int idx = 0;
        if (type == 2) buf[idx++] = 'R';
        else if (type == 3) buf[idx++] = 'N';
        else if (type == 4) buf[idx++] = 'B';
        else if (type == 5) buf[idx++] = 'Q';
        else if (type == 6) buf[idx++] = 'K';
        
        if (move.captured != 0 || move.special == 3) {
            if (type == 1) buf[idx++] = 'a' + move.fc;
            buf[idx++] = 'x';
        }
        
        buf[idx++] = 'a' + move.tc;
        buf[idx++] = '8' - move.tr;
        
        if (move.special == 4) {
            buf[idx++] = '=';
            buf[idx++] = 'Q';
        }
        
        buf[idx] = '\0';
    }
    
    Move makeMove(int fr, int fc, int tr, int tc) {
        Move move(fr, fc, tr, tc);
        move.movedPiece = board[fr][fc];
        move.captured = board[tr][tc];
        
        int8_t piece = board[fr][fc];
        int type = abs(piece);
        bool isWhite = piece > 0;
        
        if (type == 6 && abs(tc - fc) == 2) {
            move.special = (tc > fc) ? 1 : 2;
        } else if (type == 1 && tc == epCol && board[tr][tc] == EMPTY) {
            move.special = 3;
            move.captured = board[fr][tc];
        } else if (type == 1 && (tr == 0 || tr == 7)) {
            move.special = 4;
        }
        
        if (move.captured != 0) {
            if (isWhite) {
                if (whiteCapturedCount < 16) whiteCaptured[whiteCapturedCount++] = move.captured;
            } else {
                if (blackCapturedCount < 16) blackCaptured[blackCapturedCount++] = move.captured;
            }
        }
        
        return move;
    }
    
    void execMove(Move& move) {
        int8_t piece = board[move.fr][move.fc];
        bool isWhite = piece > 0;
        int type = abs(piece);
        
        if (move.special == 1) {
            board[move.fr][6] = piece;
            board[move.fr][4] = EMPTY;
            board[move.fr][5] = isWhite ? W_ROOK : B_ROOK;
            board[move.fr][7] = EMPTY;
        } else if (move.special == 2) {
            board[move.fr][2] = piece;
            board[move.fr][4] = EMPTY;
            board[move.fr][3] = isWhite ? W_ROOK : B_ROOK;
            board[move.fr][0] = EMPTY;
        } else if (move.special == 3) {
            board[move.tr][move.tc] = piece;
            board[move.fr][move.fc] = EMPTY;
            board[move.fr][move.tc] = EMPTY;
        } else {
            board[move.tr][move.tc] = piece;
            board[move.fr][move.fc] = EMPTY;
            if (move.special == 4) {
                board[move.tr][move.tc] = isWhite ? W_QUEEN : B_QUEEN;
            }
        }
        
        if (type == 6) {
            if (isWhite) { wCastleK = wCastleQ = false; }
            else { bCastleK = bCastleQ = false; }
        }
        // Revoke castling rights when a rook moves from its home square
        if (type == 2) {
            if (move.fr == 7 && move.fc == 7) wCastleK = false;
            if (move.fr == 7 && move.fc == 0) wCastleQ = false;
            if (move.fr == 0 && move.fc == 7) bCastleK = false;
            if (move.fr == 0 && move.fc == 0) bCastleQ = false;
        }
        // Revoke castling rights when a rook is captured on its home square
        if (move.captured != 0) {
            if (move.tr == 7 && move.tc == 7) wCastleK = false;
            if (move.tr == 7 && move.tc == 0) wCastleQ = false;
            if (move.tr == 0 && move.tc == 7) bCastleK = false;
            if (move.tr == 0 && move.tc == 0) bCastleQ = false;
        }
        
        epCol = -1;
        if (type == 1 && abs(move.tr - move.fr) == 2) {
            epCol = move.fc;
        }
        
        whiteTurn = !whiteTurn;
        if (whiteTurn) moveNum++;
    }
    
    // ==========================================================================
    // Move Validation
    // ==========================================================================
    void calcValidMoves(int r, int c) {
        memset(validMoves, 0, sizeof(validMoves));
        
        int8_t piece = board[r][c];
        if (piece == EMPTY) return;
        
        bool isWhite = piece > 0;
        int type = abs(piece);
        
        switch (type) {
            case 1: calcPawnMoves(r, c, isWhite); break;
            case 2: calcSlidingMoves(r, c, true, false); break;
            case 3: calcKnightMoves(r, c, isWhite); break;
            case 4: calcSlidingMoves(r, c, false, true); break;
            case 5: calcSlidingMoves(r, c, true, true); break;
            case 6: calcKingMoves(r, c, isWhite); break;
        }
        
        // Filter illegal moves
        for (int tr = 0; tr < 8; tr++) {
            for (int tc = 0; tc < 8; tc++) {
                if (!validMoves[tr][tc]) continue;
                
                int8_t saved = board[tr][tc];
                int8_t savedEp = EMPTY;
                board[tr][tc] = board[r][c];
                board[r][c] = EMPTY;
                
                if (type == 1 && tc == epCol && saved == EMPTY) {
                    savedEp = board[r][tc];
                    board[r][tc] = EMPTY;
                }
                
                if (kingInCheck(isWhite)) {
                    validMoves[tr][tc] = false;
                }
                
                board[r][c] = board[tr][tc];
                board[tr][tc] = saved;
                if (savedEp != EMPTY) board[r][tc] = savedEp;
            }
        }
    }
    
    void calcPawnMoves(int r, int c, bool isWhite) {
        int dir = isWhite ? -1 : 1;
        int startRow = isWhite ? 6 : 1;
        
        if (r + dir >= 0 && r + dir < 8 && board[r + dir][c] == EMPTY) {
            validMoves[r + dir][c] = true;
            if (r == startRow && board[r + dir * 2][c] == EMPTY) {
                validMoves[r + dir * 2][c] = true;
            }
        }
        
        for (int dc = -1; dc <= 1; dc += 2) {
            int nc = c + dc;
            if (nc < 0 || nc > 7 || r + dir < 0 || r + dir > 7) continue;
            
            int8_t target = board[r + dir][nc];
            if (target != EMPTY && (target > 0) != isWhite) {
                validMoves[r + dir][nc] = true;
            }
            
            if (nc == epCol && r == (isWhite ? 3 : 4)) {
                validMoves[r + dir][nc] = true;
            }
        }
    }
    
    void calcKnightMoves(int r, int c, bool isWhite) {
        static const int dr[] = {-2, -2, -1, -1, 1, 1, 2, 2};
        static const int dc[] = {-1, 1, -2, 2, -2, 2, -1, 1};
        
        for (int i = 0; i < 8; i++) {
            int nr = r + dr[i];
            int nc = c + dc[i];
            if (nr < 0 || nr > 7 || nc < 0 || nc > 7) continue;
            
            int8_t target = board[nr][nc];
            if (target == EMPTY || (target > 0) != isWhite) {
                validMoves[nr][nc] = true;
            }
        }
    }
    
    void calcSlidingMoves(int r, int c, bool rook, bool bishop) {
        static const int dr[] = {-1, 1, 0, 0, -1, -1, 1, 1};
        static const int dc[] = {0, 0, -1, 1, -1, 1, -1, 1};
        
        int8_t piece = board[r][c];
        bool isWhite = piece > 0;
        
        int start = rook ? 0 : 4;
        int end = bishop ? 8 : 4;
        
        for (int d = start; d < end; d++) {
            for (int dist = 1; dist < 8; dist++) {
                int nr = r + dr[d] * dist;
                int nc = c + dc[d] * dist;
                if (nr < 0 || nr > 7 || nc < 0 || nc > 7) break;
                
                int8_t target = board[nr][nc];
                if (target == EMPTY) {
                    validMoves[nr][nc] = true;
                } else {
                    if ((target > 0) != isWhite) {
                        validMoves[nr][nc] = true;
                    }
                    break;
                }
            }
        }
    }
    
    void calcKingMoves(int r, int c, bool isWhite) {
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = r + dr;
                int nc = c + dc;
                if (nr < 0 || nr > 7 || nc < 0 || nc > 7) continue;
                
                int8_t target = board[nr][nc];
                if (target == EMPTY || (target > 0) != isWhite) {
                    validMoves[nr][nc] = true;
                }
            }
        }
        
        // Castling
        if (isWhite && r == 7 && c == 4 && !kingInCheck(true)) {
            if (wCastleK && board[7][5] == EMPTY && board[7][6] == EMPTY &&
                !squareAttacked(7, 5, false) && !squareAttacked(7, 6, false)) {
                validMoves[7][6] = true;
            }
            if (wCastleQ && board[7][3] == EMPTY && board[7][2] == EMPTY && board[7][1] == EMPTY &&
                !squareAttacked(7, 3, false) && !squareAttacked(7, 2, false)) {
                validMoves[7][2] = true;
            }
        }
        if (!isWhite && r == 0 && c == 4 && !kingInCheck(false)) {
            if (bCastleK && board[0][5] == EMPTY && board[0][6] == EMPTY &&
                !squareAttacked(0, 5, true) && !squareAttacked(0, 6, true)) {
                validMoves[0][6] = true;
            }
            if (bCastleQ && board[0][3] == EMPTY && board[0][2] == EMPTY && board[0][1] == EMPTY &&
                !squareAttacked(0, 3, true) && !squareAttacked(0, 2, true)) {
                validMoves[0][2] = true;
            }
        }
    }
    
    bool kingInCheck(bool whiteKing) {
        int kr = -1, kc = -1;
        int8_t kingPiece = whiteKing ? W_KING : B_KING;
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                if (board[r][c] == kingPiece) {
                    kr = r; kc = c;
                    break;
                }
            }
            if (kr >= 0) break;
        }
        
        if (kr < 0) return false;
        return squareAttacked(kr, kc, !whiteKing);
    }
    
    bool squareAttacked(int r, int c, bool byWhite) {
        int pawnDir = byWhite ? 1 : -1;
        for (int dc = -1; dc <= 1; dc += 2) {
            int pr = r + pawnDir;
            int pc = c + dc;
            if (pr >= 0 && pr < 8 && pc >= 0 && pc < 8) {
                if (board[pr][pc] == (byWhite ? W_PAWN : B_PAWN)) return true;
            }
        }
        
        static const int kdr[] = {-2, -2, -1, -1, 1, 1, 2, 2};
        static const int kdc[] = {-1, 1, -2, 2, -2, 2, -1, 1};
        for (int i = 0; i < 8; i++) {
            int nr = r + kdr[i];
            int nc = c + kdc[i];
            if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                if (board[nr][nc] == (byWhite ? W_KNIGHT : B_KNIGHT)) return true;
            }
        }
        
        for (int dr = -1; dr <= 1; dr++) {
            for (int dc = -1; dc <= 1; dc++) {
                if (dr == 0 && dc == 0) continue;
                int nr = r + dr;
                int nc = c + dc;
                if (nr >= 0 && nr < 8 && nc >= 0 && nc < 8) {
                    if (board[nr][nc] == (byWhite ? W_KING : B_KING)) return true;
                }
            }
        }
        
        static const int sdr[] = {-1, 1, 0, 0, -1, -1, 1, 1};
        static const int sdc[] = {0, 0, -1, 1, -1, 1, -1, 1};
        
        for (int d = 0; d < 8; d++) {
            for (int dist = 1; dist < 8; dist++) {
                int nr = r + sdr[d] * dist;
                int nc = c + sdc[d] * dist;
                if (nr < 0 || nr > 7 || nc < 0 || nc > 7) break;
                
                int8_t piece = board[nr][nc];
                if (piece == EMPTY) continue;
                
                bool isEnemy = (piece > 0) == byWhite;
                if (!isEnemy) break;
                
                int type = abs(piece);
                bool isRookDir = (d < 4);
                
                if (type == 5) return true;
                if (type == 2 && isRookDir) return true;
                if (type == 4 && !isRookDir) return true;
                
                break;
            }
        }
        
        return false;
    }
    
    void updateGameState() {
        inCheck = kingInCheck(whiteTurn);
        
        bool hasLegal = false;
        for (int r = 0; r < 8 && !hasLegal; r++) {
            for (int c = 0; c < 8 && !hasLegal; c++) {
                int8_t piece = board[r][c];
                if (piece == EMPTY || (piece > 0) != whiteTurn) continue;
                
                calcValidMoves(r, c);
                for (int tr = 0; tr < 8 && !hasLegal; tr++) {
                    for (int tc = 0; tc < 8; tc++) {
                        if (validMoves[tr][tc]) {
                            hasLegal = true;
                            break;
                        }
                    }
                }
            }
        }
        
        memset(validMoves, 0, sizeof(validMoves));
        
        if (!hasLegal) {
            gameOver = true;
            checkmate = inCheck;
            stalemate = !inCheck;
        }
    }
    
    // ==========================================================================
    // AI
    // ==========================================================================
    Move findBestMove() {
        Move best;
        int bestScore = -100000;
        // Cap depth at 2 — depth 3 blocks ESP32-C3 for 30+ seconds
        int depth = min((int)settings.difficulty, 2);
        int randomFactor = (depth == 1) ? 50 : (settings.difficulty >= 3) ? 3 : 20;
        
        bool aiIsWhite = !playerIsWhite;
        
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                int8_t piece = board[r][c];
                if (piece == EMPTY || (piece > 0) != aiIsWhite) continue;
                
                bool origTurn = whiteTurn;
                whiteTurn = aiIsWhite;
                calcValidMoves(r, c);
                whiteTurn = origTurn;
                
                for (int tr = 0; tr < 8; tr++) {
                    for (int tc = 0; tc < 8; tc++) {
                        if (!validMoves[tr][tc]) continue;
                        
                        int8_t saved = board[tr][tc];
                        int8_t moving = board[r][c];
                        board[tr][tc] = moving;
                        board[r][c] = EMPTY;
                        
                        int8_t epSaved = EMPTY;
                        if (abs(moving) == 1 && tc == epCol && saved == EMPTY) {
                            epSaved = board[r][tc];
                            board[r][tc] = EMPTY;
                        }
                        
                        int score = minimax(depth - 1, -100000, 100000, false, aiIsWhite);
                        score += random(-randomFactor, randomFactor + 1);
                        
                        board[r][c] = moving;
                        board[tr][tc] = saved;
                        if (epSaved != EMPTY) board[r][tc] = epSaved;
                        
                        if (score > bestScore) {
                            bestScore = score;
                            best = Move(r, c, tr, tc);
                            best.movedPiece = moving;
                            best.captured = saved;
                        }
                    }
                }
            }
        }
        
        memset(validMoves, 0, sizeof(validMoves));
        return best;
    }
    
    int minimax(int depth, int alpha, int beta, bool maximizing, bool aiIsWhite) {
        if (depth == 0) return evaluate(aiIsWhite);
        
        if (maximizing) {
            int maxEval = -100000;
            for (int r = 0; r < 8; r++) {
                yield();  // Feed watchdog during long search
                for (int c = 0; c < 8; c++) {
                    int8_t piece = board[r][c];
                    if (piece == EMPTY || (piece > 0) != aiIsWhite) continue;
                    
                    for (int tr = 0; tr < 8; tr++) {
                        for (int tc = 0; tc < 8; tc++) {
                            if (!quickValid(r, c, tr, tc, aiIsWhite)) continue;
                            
                            int8_t saved = board[tr][tc];
                            board[tr][tc] = board[r][c];
                            board[r][c] = EMPTY;
                            
                            int eval = minimax(depth - 1, alpha, beta, false, aiIsWhite);
                            
                            board[r][c] = board[tr][tc];
                            board[tr][tc] = saved;
                            
                            maxEval = max(maxEval, eval);
                            alpha = max(alpha, eval);
                            if (beta <= alpha) return maxEval;
                        }
                    }
                }
            }
            return maxEval;
        } else {
            int minEval = 100000;
            for (int r = 0; r < 8; r++) {
                yield();  // Feed watchdog during long search
                for (int c = 0; c < 8; c++) {
                    int8_t piece = board[r][c];
                    if (piece == EMPTY || (piece > 0) == aiIsWhite) continue;
                    
                    for (int tr = 0; tr < 8; tr++) {
                        for (int tc = 0; tc < 8; tc++) {
                            if (!quickValid(r, c, tr, tc, !aiIsWhite)) continue;
                            
                            int8_t saved = board[tr][tc];
                            board[tr][tc] = board[r][c];
                            board[r][c] = EMPTY;
                            
                            int eval = minimax(depth - 1, alpha, beta, true, aiIsWhite);
                            
                            board[r][c] = board[tr][tc];
                            board[tr][tc] = saved;
                            
                            minEval = min(minEval, eval);
                            beta = min(beta, eval);
                            if (beta <= alpha) return minEval;
                        }
                    }
                }
            }
            return minEval;
        }
    }
    
    bool quickValid(int fr, int fc, int tr, int tc, bool isWhite) {
        int8_t piece = board[fr][fc];
        int8_t target = board[tr][tc];
        if (target != EMPTY && (target > 0) == isWhite) return false;
        
        int type = abs(piece);
        int dr = tr - fr, dc = tc - fc;
        int adr = abs(dr), adc = abs(dc);
        
        switch (type) {
            case 1: {
                int dir = isWhite ? -1 : 1;
                if (dc == 0 && target == EMPTY) {
                    if (dr == dir) return true;
                    if (dr == dir * 2 && fr == (isWhite ? 6 : 1) && board[fr + dir][fc] == EMPTY) return true;
                }
                if (adc == 1 && dr == dir && target != EMPTY) return true;
                return false;
            }
            case 2: return (dr == 0 || dc == 0) && pathClear(fr, fc, tr, tc);
            case 3: return (adr == 2 && adc == 1) || (adr == 1 && adc == 2);
            case 4: return adr == adc && pathClear(fr, fc, tr, tc);
            case 5: return ((dr == 0 || dc == 0) || adr == adc) && pathClear(fr, fc, tr, tc);
            case 6: return adr <= 1 && adc <= 1;
        }
        return false;
    }
    
    bool pathClear(int fr, int fc, int tr, int tc) {
        int dr = (tr > fr) ? 1 : (tr < fr) ? -1 : 0;
        int dc = (tc > fc) ? 1 : (tc < fc) ? -1 : 0;
        if (dr == 0 && dc == 0) return true;

        int r = fr + dr, c = fc + dc;
        while (r != tr || c != tc) {
            if (r < 0 || r > 7 || c < 0 || c > 7) return false;
            if (board[r][c] != EMPTY) return false;
            r += dr;
            c += dc;
        }
        return true;
    }
    
    int evaluate(bool aiIsWhite) {
        int score = 0;
        
        for (int r = 0; r < 8; r++) {
            for (int c = 0; c < 8; c++) {
                int8_t piece = board[r][c];
                if (piece == EMPTY) continue;
                
                bool pieceIsWhite = piece > 0;
                int val = PIECE_VALUES[abs(piece)];
                
                if (abs(piece) == 1) {
                    val += pieceIsWhite ? (6 - r) * 10 : (r - 1) * 10;
                }
                
                if (r >= 3 && r <= 4 && c >= 3 && c <= 4) val += 15;
                else if (r >= 2 && r <= 5 && c >= 2 && c <= 5) val += 5;
                
                if (pieceIsWhite == aiIsWhite) {
                    score += val;
                } else {
                    score -= val;
                }
            }
        }
        
        return score;
    }
  PluginRenderer& d_;
};

}  // namespace sumi

#endif  // FEATURE_PLUGINS
