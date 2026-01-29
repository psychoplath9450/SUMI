#ifndef SUMI_APP_LAUNCHER_H
#define SUMI_APP_LAUNCHER_H

/**
 * @file AppLauncher.h
 * @brief App launching and plugin runner integration
 * @version 1.3.0
 */

#include <Arduino.h>
#include "config.h"
#include "core/HomeItems.h"

// =============================================================================
// App Launcher Functions
// =============================================================================

// Open an app by its home item index (HOME_ITEM_LIBRARY, HOME_ITEM_CHESS, etc.)
void openAppByItemIndex(uint8_t itemIndex);

// Open an app by selection position on current home page
void openApp(int selectionIndex);

// Open Library and immediately resume last book (for book widget)
void openLibraryWithResume();

#endif // SUMI_APP_LAUNCHER_H
