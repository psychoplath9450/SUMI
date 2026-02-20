#pragma once

#include <cstdint>

#include "../config.h"
#include "../ui/views/SettingsViews.h"
#include "State.h"

class GfxRenderer;

namespace sumi {

enum class SettingsScreen : uint8_t {
  Menu,
  HomeArt,
  BleTransfer,  // Wireless file transfer
  Reader,
  Device,
  Cleanup,
  SystemInfo,
  ConfirmDialog,
#if FEATURE_BLUETOOTH
  Bluetooth,
#endif
};

class SettingsState : public State {
 public:
  explicit SettingsState(GfxRenderer& renderer);
  ~SettingsState() override;

  void enter(Core& core) override;
  void exit(Core& core) override;
  StateTransition update(Core& core) override;
  void render(Core& core) override;
  StateId id() const override { return StateId::Settings; }

 private:
  GfxRenderer& renderer_;
  Core* core_;  // Stored for helper methods that don't receive Core&
  SettingsScreen currentScreen_;
  bool needsRender_;
  bool goHome_;
  bool goApps_ = false;
  bool themeWasChanged_;

  // Pending action for confirmation dialog
  // 0=none, 10=Clear Book Cache, 11=Clear Device Storage, 12=Factory Reset
  uint8_t pendingAction_;
  
  // BLE Transfer state
  bool bleTransferEnabled_ = false;
  unsigned long lastBleUpdate_ = 0;
  int lastBleProgress_ = -1;
  bool bleCallbackRegistered_ = false;
  bool bleShowResult_ = false;       // Show result screen after transfer
  bool bleTransferDirty_ = false;    // Files were received, need refresh on exit
  bool bleQueueComplete_ = false;    // Full queue finished

  // Views (all small structs)
  ui::SettingsMenuView menuView_;
  ui::HomeArtSettingsView homeArtView_;
  ui::ReaderSettingsView readerView_;
  ui::DeviceSettingsView deviceView_;
  ui::CleanupMenuView cleanupView_;
  ui::SystemInfoView infoView_;
  ui::ConfirmDialogView confirmView_;

  // Navigation helpers
  void openSelected();
  void goBack(Core& core);
  void handleConfirm(Core& core);
  void handleLeftRight(int delta);

  // Settings binding
  void loadReaderSettings();
  void saveReaderSettings();
  void loadDeviceSettings();
  void saveDeviceSettings();
  void loadHomeArtSettings();
  void saveHomeArtSettings();
  void populateSystemInfo();

  // Actions
  void clearCache(int type, Core& core);
  
  // BLE File Transfer
  void enterBleTransfer();
  void renderBleTransfer();
  void updateBleTransfer();

#if FEATURE_BLUETOOTH
  // Bluetooth screen state
  int8_t btSelected_ = 0;
  bool btScanned_ = false;
  bool btConnecting_ = false;
  void enterBluetooth();
  void renderBluetooth();
#endif
};

}  // namespace sumi
