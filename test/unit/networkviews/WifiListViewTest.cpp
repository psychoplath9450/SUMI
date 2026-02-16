#include "test_utils.h"

#include <cstdint>
#include <cstring>

// Inline WifiListView to avoid firmware/graphics dependencies
struct WifiListView {
  static constexpr int MAX_NETWORKS = 16;
  static constexpr int SSID_LEN = 33;
  static constexpr int PAGE_SIZE = 10;

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

  int getPageStart() const { return page * PAGE_SIZE; }

  int getPageEnd() const {
    int end = (page + 1) * PAGE_SIZE;
    return end > networkCount ? networkCount : end;
  }

  void moveUp() {
    if (selected > 0) {
      selected--;
      if (selected < getPageStart()) {
        page--;
      }
      needsRender = true;
    }
  }

  void moveDown() {
    if (networkCount > 0 && selected < networkCount - 1) {
      selected++;
      if (selected >= getPageEnd()) {
        page++;
      }
      needsRender = true;
    }
  }
};

int main() {
  TestUtils::TestRunner runner("WifiListViewTest");

  // --- setScanning with default text ---
  {
    WifiListView view;
    view.needsRender = false;
    view.setScanning(true);
    runner.expectTrue(view.scanning, "setScanning(true) sets scanning flag");
    runner.expectTrue(strcmp(view.statusText, "Scanning...") == 0, "setScanning(true) uses default text");
    runner.expectTrue(view.needsRender, "setScanning sets needsRender");
  }

  // --- setScanning with custom text ---
  {
    WifiListView view;
    view.setScanning(true, "Initializing WiFi...");
    runner.expectTrue(view.scanning, "setScanning with custom text sets scanning");
    runner.expectTrue(strcmp(view.statusText, "Initializing WiFi...") == 0, "setScanning stores custom text");
  }

  // --- setScanning(false) ---
  {
    WifiListView view;
    view.setScanning(true, "Scanning...");
    view.setScanning(false);
    runner.expectFalse(view.scanning, "setScanning(false) clears scanning flag");
    runner.expectTrue(strcmp(view.statusText, "Scanning...") == 0, "setScanning(false) resets to default text");
  }

  // --- statusText truncation for long strings ---
  {
    WifiListView view;
    // 40 chars, but statusText is only 32 (31 + null)
    view.setScanning(true, "This is a very long status message!!!!!!");
    runner.expectEq(size_t(31), strlen(view.statusText), "Long status text truncated to 31 chars");
    runner.expectTrue(strncmp(view.statusText, "This is a very long status mess", 31) == 0,
                      "Truncated text preserves prefix");
  }

  // --- Default statusText on construction ---
  {
    WifiListView view;
    runner.expectTrue(strcmp(view.statusText, "Scanning...") == 0, "Default statusText is 'Scanning...'");
    runner.expectFalse(view.scanning, "Default scanning is false");
  }

  // --- addNetwork basic ---
  {
    WifiListView view;
    bool added = view.addNetwork("MyWiFi", 75, true);
    runner.expectTrue(added, "addNetwork returns true");
    runner.expectEq(uint8_t(1), view.networkCount, "networkCount incremented");
    runner.expectTrue(strcmp(view.networks[0].ssid, "MyWiFi") == 0, "SSID stored correctly");
    runner.expectEq(int8_t(75), view.networks[0].signal, "Signal stored correctly");
    runner.expectTrue(view.networks[0].secured, "Secured flag stored correctly");
  }

  // --- addNetwork overflow ---
  {
    WifiListView view;
    for (int i = 0; i < WifiListView::MAX_NETWORKS; i++) {
      char ssid[8];
      snprintf(ssid, sizeof(ssid), "Net%d", i);
      runner.expectTrue(view.addNetwork(ssid, 50, false), "addNetwork succeeds up to MAX");
    }
    runner.expectEq(uint8_t(WifiListView::MAX_NETWORKS), view.networkCount, "networkCount at MAX");
    runner.expectFalse(view.addNetwork("Overflow", 50, false), "addNetwork fails when full");
    runner.expectEq(uint8_t(WifiListView::MAX_NETWORKS), view.networkCount, "networkCount unchanged after overflow");
  }

  // --- addNetwork SSID truncation ---
  {
    WifiListView view;
    // SSID_LEN is 33 (32 chars + null)
    const char* longSsid = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";  // 36 chars
    view.addNetwork(longSsid, 50, false);
    runner.expectEq(size_t(WifiListView::SSID_LEN - 1), strlen(view.networks[0].ssid),
                    "Long SSID truncated to SSID_LEN-1");
  }

  // --- moveDown/moveUp on empty list (underflow guard) ---
  {
    WifiListView view;
    view.needsRender = false;
    view.moveDown();
    runner.expectEq(uint8_t(0), view.selected, "moveDown on empty list is no-op");
    runner.expectEq(uint8_t(0), view.page, "moveDown on empty list doesn't change page");
    runner.expectFalse(view.needsRender, "moveDown on empty list doesn't set needsRender");

    view.moveUp();
    runner.expectEq(uint8_t(0), view.selected, "moveUp on empty list is no-op");
    runner.expectFalse(view.needsRender, "moveUp on empty list doesn't set needsRender");
  }

  // --- clear ---
  {
    WifiListView view;
    view.addNetwork("Net1", 50, false);
    view.addNetwork("Net2", 60, true);
    view.selected = 1;
    view.page = 1;
    view.needsRender = false;
    view.clear();
    runner.expectEq(uint8_t(0), view.networkCount, "clear resets networkCount");
    runner.expectEq(uint8_t(0), view.selected, "clear resets selected");
    runner.expectEq(uint8_t(0), view.page, "clear resets page");
    runner.expectTrue(view.needsRender, "clear sets needsRender");
  }

  // --- Navigation: moveDown/moveUp ---
  {
    WifiListView view;
    view.addNetwork("Net0", 50, false);
    view.addNetwork("Net1", 60, false);
    view.addNetwork("Net2", 70, true);

    runner.expectEq(uint8_t(0), view.selected, "Initial selected is 0");

    view.needsRender = false;
    view.moveDown();
    runner.expectEq(uint8_t(1), view.selected, "moveDown increments selected");
    runner.expectTrue(view.needsRender, "moveDown sets needsRender");

    view.moveDown();
    runner.expectEq(uint8_t(2), view.selected, "moveDown to last item");

    view.needsRender = false;
    view.moveDown();
    runner.expectEq(uint8_t(2), view.selected, "moveDown at last item is no-op");
    runner.expectFalse(view.needsRender, "moveDown at end doesn't set needsRender");

    view.needsRender = false;
    view.moveUp();
    runner.expectEq(uint8_t(1), view.selected, "moveUp decrements selected");
    runner.expectTrue(view.needsRender, "moveUp sets needsRender");

    view.moveUp();
    runner.expectEq(uint8_t(0), view.selected, "moveUp to first item");

    view.needsRender = false;
    view.moveUp();
    runner.expectEq(uint8_t(0), view.selected, "moveUp at first item is no-op");
    runner.expectFalse(view.needsRender, "moveUp at start doesn't set needsRender");
  }

  // --- Pagination ---
  {
    WifiListView view;
    // Add 12 networks (more than PAGE_SIZE of 10)
    for (int i = 0; i < 12; i++) {
      char ssid[8];
      snprintf(ssid, sizeof(ssid), "Net%d", i);
      view.addNetwork(ssid, 50 + i, false);
    }

    runner.expectEq(0, view.getPageStart(), "Page 0 starts at 0");
    runner.expectEq(10, view.getPageEnd(), "Page 0 ends at 10");

    // Navigate to item 9 (last on page 0)
    for (int i = 0; i < 9; i++) view.moveDown();
    runner.expectEq(uint8_t(9), view.selected, "Selected at 9");
    runner.expectEq(uint8_t(0), view.page, "Still on page 0");

    // Move to item 10 should trigger page change
    view.moveDown();
    runner.expectEq(uint8_t(10), view.selected, "Selected at 10");
    runner.expectEq(uint8_t(1), view.page, "Page advanced to 1");
    runner.expectEq(10, view.getPageStart(), "Page 1 starts at 10");
    runner.expectEq(12, view.getPageEnd(), "Page 1 ends at 12 (partial page)");

    // Move back to item 9 should go back to page 0
    view.moveUp();
    runner.expectEq(uint8_t(9), view.selected, "Selected back at 9");
    runner.expectEq(uint8_t(0), view.page, "Page back to 0");
  }

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
