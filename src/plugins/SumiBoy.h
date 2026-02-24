#pragma once
/**
 * @file SumiBoy.h
 * @brief SumiBoy — Game Boy emulator for SUMI
 *
 * Loads .gb ROMs from /games/ on the SD card.
 * If one ROM is present, launches directly.
 * If multiple, shows a ROM picker list.
 *
 * Streams ROMs from SD and emulates onboard.
 */

#include "../config.h"

#if FEATURE_PLUGINS && FEATURE_GAMES

#include "SumiBoyRomPicker.h"

// SumiBoyRomPicker IS the SumiBoy entry point now.
// main.cpp registers it as "SumiBoy" in the plugin list.

#endif  // FEATURE_PLUGINS && FEATURE_GAMES
