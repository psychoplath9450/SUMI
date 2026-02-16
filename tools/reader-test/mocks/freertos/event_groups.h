#pragma once

#include "FreeRTOS.h"

struct MockEventGroup {
  EventBits_t bits = 0;
};

inline EventGroupHandle_t xEventGroupCreate() { return new MockEventGroup(); }

inline void vEventGroupDelete(EventGroupHandle_t xEventGroup) { delete static_cast<MockEventGroup*>(xEventGroup); }

inline EventBits_t xEventGroupSetBits(EventGroupHandle_t xEventGroup, EventBits_t uxBitsToSet) {
  auto* eg = static_cast<MockEventGroup*>(xEventGroup);
  if (!eg) return 0;
  eg->bits |= uxBitsToSet;
  return eg->bits;
}

inline EventBits_t xEventGroupClearBits(EventGroupHandle_t xEventGroup, EventBits_t uxBitsToClear) {
  auto* eg = static_cast<MockEventGroup*>(xEventGroup);
  if (!eg) return 0;
  EventBits_t old = eg->bits;
  eg->bits &= ~uxBitsToClear;
  return old;
}

inline EventBits_t xEventGroupWaitBits(EventGroupHandle_t xEventGroup, EventBits_t uxBitsToWaitFor, BaseType_t, BaseType_t,
                                       TickType_t) {
  auto* eg = static_cast<MockEventGroup*>(xEventGroup);
  return eg ? eg->bits : 0;
}

inline EventBits_t xEventGroupGetBits(EventGroupHandle_t xEventGroup) {
  auto* eg = static_cast<MockEventGroup*>(xEventGroup);
  return eg ? eg->bits : 0;
}
