#pragma once

#include <stdint.h>

/**
 * @brief UI mutation commands posted from non-UI tasks to the UI owner.
 *
 * The WebSocket handlers run on the AsyncTCP task, but every UI mutation must
 * happen on the single task that owns LVGL (today the Arduino loop; later the
 * render task). Instead of calling into LVGL-touching code directly — a data
 * race with LV_OS_NONE and no lock — handlers post a POD command onto a
 * FreeRTOS queue that the UI owner drains in UIManager::update().
 *
 * Keep this a trivially-copyable POD: it is copied by value through the queue.
 */
enum class UiCommandType : uint8_t {
    None = 0,
    SetVu,          ///< boolValue: VU mode on/off
    SetWhite,       ///< boolValue: white mode on/off
    SetBrightness,  ///< intValue: 0–255 brightness
    SetAnimation,   ///< boolValue: run/stop; intValue: animation index (when running)
    SetColour,      ///< colour: "#RRGGBB" hex string
};

struct UiCommand {
    UiCommandType type = UiCommandType::None;
    bool boolValue = false;
    int intValue = 0;
    char colour[8] = {0};  ///< "#RRGGBB" + null terminator
};
