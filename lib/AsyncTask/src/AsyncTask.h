#pragma once

// Safe FreeRTOS task utilities for ESP32
// Prevents common issues: mutex corruption, force-delete crashes, memory leaks

#include "BackgroundTask.h"
#include "ScopedMutex.h"
