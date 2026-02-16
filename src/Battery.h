#pragma once
#include <BatteryMonitor.h>

#define BAT_GPIO0 0  // Battery voltage

inline BatteryMonitor& getBatteryMonitor() {
  static BatteryMonitor instance(BAT_GPIO0);
  return instance;
}

#define batteryMonitor getBatteryMonitor()
