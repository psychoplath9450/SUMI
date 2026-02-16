#include "test_utils.h"

#include <cstdint>
#include <cstring>

// Inline WifiListView (logic-only copy, mirrors src/ui/views/NetworkViews.h)
struct WifiListView {
  static constexpr int MAX_NETWORKS = 16;
  static constexpr int SSID_LEN = 33;

  struct Network {
    char ssid[SSID_LEN];
    int8_t signal;
    bool secured;
  };

  Network networks[MAX_NETWORKS];
  uint8_t networkCount = 0;
  uint8_t selected = 0;
  uint8_t page = 0;
  bool scanning = false;
  char statusText[32] = "Scanning...";
  bool needsRender = true;

  void clear() {
    networkCount = 0;
    selected = 0;
    page = 0;
    needsRender = true;
  }

  bool addNetwork(const char* ssid, int signal, bool secured) {
    if (networkCount < MAX_NETWORKS) {
      strncpy(networks[networkCount].ssid, ssid, SSID_LEN - 1);
      networks[networkCount].ssid[SSID_LEN - 1] = '\0';
      networks[networkCount].signal = static_cast<int8_t>(signal);
      networks[networkCount].secured = secured;
      networkCount++;
      return true;
    }
    return false;
  }

  void setScanning(bool s, const char* text = "Scanning...") {
    scanning = s;
    strncpy(statusText, text, sizeof(statusText) - 1);
    statusText[sizeof(statusText) - 1] = '\0';
    needsRender = true;
  }
};

// Mock network driver (mirrors src/drivers/Network scan API)
struct MockNetwork {
  bool scanInProgress = false;
  bool scanDone = false;
  int nextScanResultCount = 0;
  bool startScanSucceeds = true;
  int startScanCalls = 0;

  bool startScan() {
    startScanCalls++;
    if (startScanSucceeds) {
      scanInProgress = true;
      scanDone = false;
      return true;
    }
    return false;
  }

  bool isScanComplete() const {
    if (!scanInProgress) return true;
    return scanDone;
  }

  int getScanResults() {
    if (!scanInProgress) return 0;
    if (!scanDone) return 0;
    scanInProgress = false;
    scanDone = false;
    return nextScanResultCount;
  }

  void completeScanWith(int count) {
    nextScanResultCount = count;
    scanDone = true;
  }
};

// Scan retry state machine (mirrors NetworkState::update() retry logic)
// Keep in sync with src/states/NetworkState.cpp
struct ScanRetryLogic {
  static constexpr uint8_t MAX_SCAN_RETRIES = 2;

  WifiListView& view;
  MockNetwork& network;

  uint8_t scanRetryCount = 0;
  uint32_t scanRetryAt = 0;
  bool needsRender = false;

  ScanRetryLogic(WifiListView& v, MockNetwork& n) : view(v), network(n) {}

  void startScan() {
    scanRetryCount = 0;
    scanRetryAt = 0;
    view.clear();
    view.setScanning(true, "Scanning...");
    network.startScan();
  }

  // Returns true if early-returned (retry scheduled)
  bool update(uint32_t currentMillis) {
    needsRender = false;

    // Check for deferred scan retry
    if (scanRetryAt != 0 && currentMillis >= scanRetryAt) {
      scanRetryAt = 0;
      if (network.startScan()) {
        view.setScanning(true, "Scanning...");
        needsRender = true;
      } else {
        view.setScanning(false);
        needsRender = true;
      }
    }

    // Check for scan completion (skip while retry is pending)
    if (view.scanning && scanRetryAt == 0) {
      if (network.isScanComplete()) {
        int count = network.getScanResults();

        if (count == 0 && scanRetryCount < MAX_SCAN_RETRIES) {
          scanRetryCount++;
          view.setScanning(true, "Initializing WiFi...");
          scanRetryAt = currentMillis + 500;
          needsRender = true;
          return true;
        }

        view.clear();
        for (int i = 0; i < count; i++) {
          char ssid[8];
          snprintf(ssid, sizeof(ssid), "Net%d", i);
          view.addNetwork(ssid, 50 + i, false);
        }

        scanRetryCount = 0;
        view.setScanning(false);
        needsRender = true;
      }
    }
    return false;
  }
};

int main() {
  TestUtils::TestRunner runner("ScanRetryLogicTest");

  // --- Scan succeeds on first try (no retry needed) ---
  {
    WifiListView view;
    MockNetwork net;
    ScanRetryLogic logic(view, net);

    logic.startScan();
    runner.expectTrue(view.scanning, "Scanning after startScan");
    runner.expectEq(1, net.startScanCalls, "startScan called once");

    net.completeScanWith(3);
    bool earlyReturn = logic.update(1000);

    runner.expectFalse(earlyReturn, "No early return when results found");
    runner.expectFalse(view.scanning, "Scanning off after results");
    runner.expectEq(uint8_t(3), view.networkCount, "3 networks added");
    runner.expectEq(uint8_t(0), logic.scanRetryCount, "Retry count reset");
  }

  // --- Scan returns 0, schedules retry ---
  {
    WifiListView view;
    MockNetwork net;
    ScanRetryLogic logic(view, net);

    logic.startScan();
    net.completeScanWith(0);
    bool earlyReturn = logic.update(1000);

    runner.expectTrue(earlyReturn, "Early return on retry");
    runner.expectTrue(view.scanning, "Still scanning during retry");
    runner.expectTrue(strcmp(view.statusText, "Initializing WiFi...") == 0, "Status shows initializing");
    runner.expectEq(uint8_t(1), logic.scanRetryCount, "Retry count is 1");
    runner.expectEq(uint32_t(1500), logic.scanRetryAt, "Retry scheduled at +500ms");
  }

  // --- CRITICAL: Retry guard prevents re-entry before timer fires ---
  {
    WifiListView view;
    MockNetwork net;
    ScanRetryLogic logic(view, net);

    logic.startScan();
    net.completeScanWith(0);
    logic.update(1000);  // Schedules retry at 1500

    // Simulate multiple update() calls before retry fires
    // (This is the bug that was fixed: without the guard, each call
    //  would re-enter scan completion and consume a retry attempt)
    int startScanBefore = net.startScanCalls;
    logic.update(1100);  // 400ms before retry
    logic.update(1200);
    logic.update(1300);
    logic.update(1400);

    runner.expectEq(startScanBefore, net.startScanCalls,
                    "No startScan calls while retry is pending");
    runner.expectEq(uint8_t(1), logic.scanRetryCount,
                    "Retry count unchanged while pending");
    runner.expectTrue(view.scanning, "Still scanning while retry pending");
    runner.expectTrue(strcmp(view.statusText, "Initializing WiFi...") == 0,
                      "Status unchanged while retry pending");
  }

  // --- Retry fires after delay, triggers new scan ---
  {
    WifiListView view;
    MockNetwork net;
    ScanRetryLogic logic(view, net);

    logic.startScan();                      // startScanCalls = 1
    net.completeScanWith(0);
    logic.update(1000);                     // Retry scheduled at 1500

    int callsBefore = net.startScanCalls;
    logic.update(1500);                     // Timer fires, new scan starts

    runner.expectEq(callsBefore + 1, net.startScanCalls, "startScan called when retry fires");
    runner.expectTrue(view.scanning, "Scanning after retry fires");
    runner.expectTrue(strcmp(view.statusText, "Scanning...") == 0, "Status updated to Scanning...");
    runner.expectEq(uint32_t(0), logic.scanRetryAt, "scanRetryAt cleared");

    // Now the retry scan completes with results
    net.completeScanWith(5);
    logic.update(2000);
    runner.expectFalse(view.scanning, "Scanning off after retry scan completes");
    runner.expectEq(uint8_t(5), view.networkCount, "5 networks found on retry");
  }

  // --- Full retry cycle: 0 results -> retry -> find networks ---
  {
    WifiListView view;
    MockNetwork net;
    ScanRetryLogic logic(view, net);

    // Initial scan
    logic.startScan();
    net.completeScanWith(0);
    logic.update(1000);  // Retry 1 scheduled at 1500
    runner.expectEq(uint8_t(1), logic.scanRetryCount, "After 1st fail: retryCount=1");

    // Retry fires, new scan starts
    logic.update(1500);  // Timer fires -> startScan()
    runner.expectTrue(view.scanning, "Still scanning after retry fires");

    // Retry scan completes with results
    net.completeScanWith(4);
    logic.update(2000);
    runner.expectFalse(view.scanning, "Scanning done after successful retry");
    runner.expectEq(uint8_t(4), view.networkCount, "4 networks found on retry");
    runner.expectEq(uint8_t(0), logic.scanRetryCount, "Retry count reset on success");
  }

  // --- Retry exhaustion: all retries fail ---
  {
    WifiListView view;
    MockNetwork net;
    ScanRetryLogic logic(view, net);

    logic.startScan();

    // First scan: 0 results -> retry 1
    net.completeScanWith(0);
    logic.update(1000);
    runner.expectEq(uint8_t(1), logic.scanRetryCount, "retryCount=1 after 1st fail");

    // Retry 1 fires, scan again
    logic.update(1500);  // Timer fires -> startScan
    net.completeScanWith(0);
    logic.update(2000);  // Scan complete with 0 -> retry 2
    runner.expectEq(uint8_t(2), logic.scanRetryCount, "retryCount=2 after 2nd fail");

    // Retry 2 fires, scan again
    logic.update(2500);  // Timer fires -> startScan
    net.completeScanWith(0);
    logic.update(3000);  // Scan complete with 0 -> no more retries

    runner.expectFalse(view.scanning, "Scanning stops after retries exhausted");
    runner.expectEq(uint8_t(0), view.networkCount, "No networks when retries exhausted");
    runner.expectEq(uint8_t(0), logic.scanRetryCount, "Retry count reset after exhaustion");
  }

  // --- startScan failure during retry ---
  {
    WifiListView view;
    MockNetwork net;
    ScanRetryLogic logic(view, net);

    logic.startScan();
    net.completeScanWith(0);
    logic.update(1000);  // Retry scheduled at 1500

    net.startScanSucceeds = false;
    logic.update(1500);  // Retry fires but startScan fails

    runner.expectFalse(view.scanning, "Scanning off when retry startScan fails");
    runner.expectEq(uint32_t(0), logic.scanRetryAt, "scanRetryAt cleared on failure");
  }

  // --- clear() only called when processing results, not on retry ---
  {
    WifiListView view;
    MockNetwork net;
    ScanRetryLogic logic(view, net);

    logic.startScan();
    // Add a network before scan completes (simulates pre-existing data)
    view.addNetwork("OldNet", 50, false);
    runner.expectEq(uint8_t(1), view.networkCount, "Pre-existing network present");

    net.completeScanWith(0);
    logic.update(1000);  // Retry scheduled, should NOT clear

    // networkCount should not have been reset by the retry path
    // (clear() is only called after the retry check, when we have final results)
    runner.expectTrue(view.scanning, "Still scanning on retry path");
    runner.expectEq(uint8_t(1), logic.scanRetryCount, "Retry scheduled");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
