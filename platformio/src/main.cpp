/**
 * BLE Bot — M5 Cardputer authorized BLE lab scout
 *
 * Keyboard-first proximity / discovery UI for networks and devices you own
 * or have written permission to test.
 *
 * Controls: ;/w up · ./s down · Enter detail · r rescan · ` back
 */

#include <M5Cardputer.h>
#include <NimBLEDevice.h>

#include <algorithm>
#include <vector>

#ifndef BLE_SCAN_SECONDS
#define BLE_SCAN_SECONDS 5
#endif
#ifndef BLE_REFRESH_MS
#define BLE_REFRESH_MS 8000
#endif

static const int kVisibleRows = 7;

struct BleEntry {
    String name;
    String addr;
    int rssi = 0;
};

static std::vector<BleEntry> gDevices;
static int gSelected = 0;
static int gScroll = 0;
static bool gDetail = false;
static bool gScanning = false;
static unsigned long gLastScan = 0;
static String gStatus = "Authorized lab only";

struct Buttons {
    bool up = false;
    bool down = false;
    bool ok = false;
    bool back = false;
    bool rescan = false;
};

static Buttons readButtons() {
    Buttons b;
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
        return b;
    }
    auto status = M5Cardputer.Keyboard.keysState();
    for (auto key : status.word) {
        if (key == ';' || key == 'w' || key == 'W') b.up = true;
        if (key == '.' || key == 's' || key == 'S') b.down = true;
        if (key == '\n' || key == ' ') b.ok = true;
        if (key == '`' || key == 27) b.back = true;
        if (key == 'r' || key == 'R') b.rescan = true;
    }
    return b;
}

static void sortDevices() {
    std::sort(gDevices.begin(), gDevices.end(),
              [](const BleEntry& a, const BleEntry& b) { return a.rssi > b.rssi; });
}

static void upsertDevice(const String& name, const String& addr, int rssi) {
    for (auto& d : gDevices) {
        if (d.addr == addr) {
            d.rssi = rssi;
            if (name.length() && (d.name.length() == 0 || d.name == "(unknown)")) {
                d.name = name;
            }
            return;
        }
    }
    BleEntry entry;
    entry.name = name.length() ? name : "(unknown)";
    entry.addr = addr;
    entry.rssi = rssi;
    gDevices.push_back(entry);
}

class BleScanCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        String name = advertisedDevice->haveName() ? advertisedDevice->getName().c_str() : "";
        String addr = advertisedDevice->getAddress().toString().c_str();
        upsertDevice(name, addr, advertisedDevice->getRSSI());
    }
};

static BleScanCallbacks gScanCallbacks;

static void runScan() {
    if (gScanning) return;
    gScanning = true;
    gStatus = "Scanning...";
    gDevices.clear();
    sortDevices();

    NimBLEScan* scan = NimBLEDevice::getScan();
    scan->setActiveScan(true);
    scan->setInterval(97);
    scan->setWindow(67);
    scan->clearResults();
    scan->start(BLE_SCAN_SECONDS, false);
    sortDevices();
    if (gSelected >= static_cast<int>(gDevices.size())) {
        gSelected = gDevices.empty() ? 0 : static_cast<int>(gDevices.size()) - 1;
    }
    if (gScroll > gSelected) gScroll = gSelected;
    gStatus = gDevices.empty() ? "No advertisers" : String(gDevices.size()) + " device(s)";
    gScanning = false;
    gLastScan = millis();
}

static void drawHeader(const char* title) {
    auto& d = M5Cardputer.Display;
    d.fillScreen(TFT_BLACK);
    d.setTextColor(0xB6DF, TFT_BLACK);
    d.setCursor(4, 4);
    d.print(title);
    d.drawFastHLine(0, 18, d.width(), 0x0083);
}

static void drawList() {
    drawHeader("BLE Bot scout");
    auto& d = M5Cardputer.Display;
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(4, 22);
    d.print(gStatus);
    d.setCursor(4, 34);
    d.print(";/. nav  r scan  Enter detail");

    if (gDevices.empty()) {
        d.setTextColor(TFT_WHITE, TFT_BLACK);
        d.setCursor(4, 56);
        d.print("Press r to scan");
        return;
    }

    if (gSelected >= static_cast<int>(gDevices.size())) {
        gSelected = static_cast<int>(gDevices.size()) - 1;
    }
    if (gSelected < gScroll) gScroll = gSelected;
    if (gSelected >= gScroll + kVisibleRows) gScroll = gSelected - kVisibleRows + 1;

    int y = 48;
    for (int i = gScroll; i < static_cast<int>(gDevices.size()) && i < gScroll + kVisibleRows; ++i) {
        const BleEntry& e = gDevices[i];
        bool sel = (i == gSelected);
        d.setTextColor(sel ? TFT_BLACK : TFT_WHITE, sel ? 0xB6DF : TFT_BLACK);
        d.fillRect(0, y - 1, d.width(), 12, sel ? 0xB6DF : TFT_BLACK);
        d.setCursor(4, y);
        String line = String(e.rssi) + " " + e.name;
        if (line.length() > 28) line = line.substring(0, 28);
        d.print(line);
        y += 12;
    }
}

static void drawDetail() {
    if (gDevices.empty()) {
        gDetail = false;
        return;
    }
    const BleEntry& e = gDevices[gSelected];
    drawHeader("Device detail");
    auto& d = M5Cardputer.Display;
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(4, 28);
    d.print("Name:");
    d.setCursor(4, 40);
    d.print(e.name);
    d.setCursor(4, 56);
    d.print("Addr:");
    d.setCursor(4, 68);
    d.print(e.addr);
    d.setCursor(4, 84);
    d.printf("RSSI: %d dBm", e.rssi);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(4, 104);
    d.print("` back");
}

static void handleListInput(const Buttons& b) {
    if (b.rescan) {
        runScan();
        return;
    }
    if (gDevices.empty()) return;
    if (b.up && gSelected > 0) --gSelected;
    if (b.down && gSelected + 1 < static_cast<int>(gDevices.size())) ++gSelected;
    if (b.ok) gDetail = true;
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setCursor(4, 40);
    M5Cardputer.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5Cardputer.Display.print("BLE Bot init...");

    NimBLEDevice::init("BLE-Bot-Cardputer");
    NimBLEDevice::getScan()->setScanCallbacks(&gScanCallbacks, false);
    runScan();
}

void loop() {
    M5Cardputer.update();
    Buttons b = readButtons();

    if (gDetail) {
        if (b.back) gDetail = false;
        drawDetail();
    } else {
        handleListInput(b);
        if (!gScanning && !b.up && !b.down && !b.ok && !b.back &&
            millis() - gLastScan >= static_cast<unsigned long>(BLE_REFRESH_MS)) {
            runScan();
        }
        drawList();
    }
    delay(40);
}
