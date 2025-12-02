# Migration Changes - Quick Visual Reference

## At a Glance: What Changed

### 📁 Files Modified (3 files)

```
✏️ src/main.cpp                - WiFi and BootUI initialization
✏️ src/WebUIManager.cpp         - Include and type declaration
✏️ include/modular-ui.h         - Include and type declaration
```

### 📁 Files Unchanged (kept as-is, commented out in usage)

```
⚪ include/WiFiManager.h        - Old built-in version
⚪ src/WiFiManager.cpp           - Old built-in version
⚪ include/BootUI.h              - Old built-in version
⚪ src/ui/BootUI.cpp             - Old built-in version
```

---

## Side-by-Side Comparison

### Include Statements

| File | Before | After |
|------|--------|-------|
| **main.cpp** | `#include "WiFiManager.h"`<br>`BootUI* g_bootUI = nullptr;` | `// OLD: #include "WiFiManager.h"`<br>`#include "library/src/WiFiSetupManager.h"`<br>`#include "library/src/WiFiSetupBootUI.h"` |
| **WebUIManager.cpp** | `#include "WiFiManager.h"` | `// OLD: #include "WiFiManager.h"`<br>`#include "library/src/WiFiSetupManager.h"` |
| **modular-ui.h** | `#include "BootUI.h"` | `// OLD: #include "BootUI.h"`<br>`#include "../src/library/src/WiFiSetupBootUI.h"` |

### Global Variable Declarations

| File | Before | After |
|------|--------|-------|
| **main.cpp** | `extern WiFiManager* g_wifiManager;`<br>`BootUI* g_bootUI = nullptr;` | `// OLD: extern WiFiManager* g_wifiManager;`<br>`WiFiSetupManager* g_wifiManager = nullptr;`<br><br>`// OLD: BootUI* g_bootUI = nullptr;`<br>`WiFiSetupBootUI* g_bootUI = nullptr;` |
| **WebUIManager.cpp** | `extern WiFiManager* g_wifiManager;` | `// OLD: extern WiFiManager* g_wifiManager;`<br>`extern WiFiSetupManager* g_wifiManager;` |
| **modular-ui.h** | `extern BootUI* g_bootUI;` | `// OLD: extern BootUI* g_bootUI;`<br>`extern WiFiSetupBootUI* g_bootUI;` |

### Initialization Code (main.cpp only)

**Before:**
```cpp
// Initialize BootUI
g_bootUI = new BootUI();
if (g_bootUI && g_bootUI->initialize()) {
  g_bootUI->addText("ModularUI Controller Starting...\r\n");
}

// Initialize WiFi Manager
if (!g_wifiManager) {
  g_wifiManager = new WiFiManager();
}
if (g_wifiManager) {
  g_wifiManager->initialize();
}
```

**After:**
```cpp
// OLD: Initialize BootUI (built-in version)
// g_bootUI = new BootUI();
// if (g_bootUI && g_bootUI->initialize()) {
//   g_bootUI->addText("ModularUI Controller Starting...\r\n");
// }

// NEW: Initialize WiFiSetupBootUI (library version)
g_bootUI = new WiFiSetupBootUI();
if (g_bootUI && g_bootUI->initialize("MODULAR UI CONTROLLER")) {
  g_bootUI->addText("ModularUI Controller Starting...\r\n");
}

// OLD: Initialize WiFi Manager (built-in version)
// if (!g_wifiManager) {
//   g_wifiManager = new WiFiManager();
// }
// if (g_wifiManager) {
//   g_wifiManager->initialize();
// }

// NEW: Initialize WiFi Setup Manager (library version with callback)
if (!g_wifiManager) {
  WiFiSetupConfig config;
  config.defaultAPName = "ModularUI-Setup";
  config.defaultAPPassword = "modularui123";
  config.statusCallback = g_bootUI; // Use boot UI as callback
  g_wifiManager = new WiFiSetupManager(config);
}
if (g_wifiManager) {
  g_wifiManager->begin();
}
```

### Cleanup Function (main.cpp only)

**Before:**
```cpp
void cleanupBootUI() {
  if (g_bootUI) {
    delete g_bootUI;
    g_bootUI = nullptr;
  }
}
```

**After:**
```cpp
// OLD: Cleanup for built-in BootUI
// void cleanupBootUI() {
//   if (g_bootUI) {
//     delete g_bootUI;
//     g_bootUI = nullptr;
//   }
// }

// NEW: Cleanup for library WiFiSetupBootUI
void cleanupBootUI() {
  if (g_bootUI) {
    g_bootUI->cleanup(); // Call cleanup method
    delete g_bootUI;
    g_bootUI = nullptr;
  }
}
```

---

## Change Summary by Impact

### 🟢 Low Risk - Type Changes Only
- **WebUIManager.cpp** - Only changed `extern WiFiManager*` to `extern WiFiSetupManager*`
- **modular-ui.h** - Only changed includes and type declarations
- **Reason:** API-compatible, same method names work

### 🟡 Medium Risk - Logic Changes
- **main.cpp** - Changed initialization to use config and callback
- **Reason:** New pattern but well-tested in library examples

### 🔵 No Risk - Preserved in Comments
- All old code commented out, not deleted
- Easy rollback if needed
- Can compare old vs new side-by-side

---

## Key API Differences

### Method Calls - Compatible ✅
These work the same way:

```cpp
g_wifiManager->getScannedNetworks()  // ✅ Same
g_wifiManager->isInSetupMode()       // ✅ Same
g_wifiManager->isConnected()         // ✅ Same
g_wifiManager->update()              // ✅ Same
g_bootUI->addText("...")             // ✅ Same
g_bootUI->cleanup()                  // ✅ Same
```

### Initialization - Different ❗
**Old way:**
```cpp
g_wifiManager = new WiFiManager();
g_wifiManager->initialize();
```

**New way:**
```cpp
WiFiSetupConfig config;
config.defaultAPName = "ModularUI-Setup";
config.statusCallback = g_bootUI;
g_wifiManager = new WiFiSetupManager(config);
g_wifiManager->begin();
```

### New Feature - Callbacks 🆕
The library version adds callback support:

```cpp
class WiFiStatusCallback {
  virtual void onScanStart() = 0;
  virtual void onConnecting(const String& ssid) = 0;
  virtual void onConnected(IPAddress ip) = 0;
  // ... etc
};
```

The `WiFiSetupBootUI` class implements this interface, so it automatically receives status updates!

---

## Testing Strategy

### Step 1: Compile
```bash
pio run
```
**Expected:** Clean compile, no errors

### Step 2: Upload and Monitor
```bash
pio run --target upload
pio device monitor
```
**Expected:** Serial output shows WiFi scan and connection

### Step 3: First Boot (No Config)
**Expected:**
- AP mode starts
- Boot UI shows: "Scanning WiFi networks..."
- Boot UI shows: "AP Mode Started"
- Boot UI shows: "Network: ModularUI-Setup"

### Step 4: Web Interface
**Navigate to:** http://192.168.4.1/setup
**Expected:**
- Network dropdown populated
- Form fields work
- Can save settings
- Device restarts

### Step 5: Subsequent Boot
**Expected:**
- Auto-connects to saved WiFi
- Boot UI shows connection progress
- Main UI loads
- LEDs initialize

---

## Rollback Plan

If something goes wrong:

1. **Find all "OLD:" comments** in modified files
2. **Uncomment the OLD code**
3. **Comment out the NEW code**
4. **Recompile and upload**

Example:
```cpp
// Change from this:
#include "library/src/WiFiSetupManager.h"
// OLD: #include "WiFiManager.h"

// Back to this:
// #include "library/src/WiFiSetupManager.h"
#include "WiFiManager.h"
```

---

## What's Next?

✅ **Compile and test** the changes
✅ **Verify WiFi setup works** end-to-end
✅ **Test factory reset** functionality
✅ **Monitor for any issues** in serial output

If everything works:
- Can delete old WiFiManager and BootUI files
- Can package library separately
- Can share library with community

---

## Quick Reference Card

| Task | Command |
|------|---------|
| **Build** | `pio run` |
| **Upload** | `pio run --target upload` |
| **Monitor** | `pio device monitor` |
| **Clean** | `pio run --target clean` |
| **Setup Page** | http://192.168.4.1/setup |
| **Factory Reset** | http://192.168.4.1/factory-reset |

| Old Class | New Class |
|-----------|-----------|
| `WiFiManager` | `WiFiSetupManager` |
| `BootUI` | `WiFiSetupBootUI` |

| Old Include | New Include |
|-------------|-------------|
| `#include "WiFiManager.h"` | `#include "library/src/WiFiSetupManager.h"` |
| `#include "BootUI.h"` | `#include "library/src/WiFiSetupBootUI.h"` |
