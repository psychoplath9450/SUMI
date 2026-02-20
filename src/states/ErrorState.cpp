#include "ErrorState.h"

#include <Arduino.h>
#include <GfxRenderer.h>

#include <cstring>

#include "../core/Core.h"
#include "../core/BootMode.h"
#include "ThemeManager.h"

namespace sumi {

ErrorState::ErrorState(GfxRenderer& renderer) : renderer_(renderer), needsRender_(true) {}

void ErrorState::setError(Error err, const char* message) {
  error_ = err;
  if (message) {
    strncpy(message_, message, sizeof(message_) - 1);
    message_[sizeof(message_) - 1] = '\0';
  } else {
    strncpy(message_, errorToString(err), sizeof(message_) - 1);
    message_[sizeof(message_) - 1] = '\0';
  }
  needsRender_ = true;
}

void ErrorState::enter(Core& core) {
  // Check for error message from shared buffer (e.g., from ReaderState)
  if (core.buf.text[0] != '\0') {
    strncpy(message_, core.buf.text, sizeof(message_) - 1);
    message_[sizeof(message_) - 1] = '\0';
    core.buf.text[0] = '\0';  // Clear after reading
  }
  Serial.printf("[STATE] ErrorState::enter - %s\n", message_);
  needsRender_ = true;
}

void ErrorState::exit(Core& core) { Serial.println("[STATE] ErrorState::exit"); }

StateTransition ErrorState::update(Core& core) {
  // Process events
  Event e;
  while (core.events.pop(e)) {
    if (e.type == EventType::ButtonPress) {
      // Try to go to FileList; if we're in READER boot mode it won't be registered,
      // so the state machine will silently fail. In that case, restart into full UI mode.
      // We save a UI transition so the next boot goes to Home screen.
      sumi::saveTransition(sumi::BootMode::UI, nullptr, sumi::ReturnTo::HOME);
      vTaskDelay(50 / portTICK_PERIOD_MS);
      ESP.restart();
    }
  }

  return StateTransition::stay(StateId::Error);
}

void ErrorState::render(Core& core) {
  if (!needsRender_) {
    return;
  }

  const Theme& theme = THEME_MANAGER.current();

  renderer_.clearScreen(theme.backgroundColor);

  // Error title
  renderer_.drawCenteredText(theme.readerFontId, 100, "Error", theme.primaryTextBlack, BOLD);

  // Error message
  renderer_.drawCenteredText(theme.uiFontId, 200, message_, theme.primaryTextBlack, REGULAR);

  // Instructions
  renderer_.drawCenteredText(theme.uiFontId, 350, "Press any button to continue", theme.primaryTextBlack, REGULAR);

  renderer_.displayBuffer();
  needsRender_ = false;
  core.display.markDirty();
}

}  // namespace sumi
