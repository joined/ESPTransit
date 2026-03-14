#pragma once

#include "lvgl.h"
#include "bsp/esp-bsp.h"
#include <functional>

// Abstract base class for all screens
class ScreenBase {
  public:
    virtual ~ScreenBase() = default;

    // Get the LVGL screen object
    virtual lv_obj_t *get_screen() const = 0;

    // Periodic update hook - override in subclasses (called under LVGL lock)
    virtual void update() {};

    // Lock display, call update(), unlock. Convenience for timer callbacks.
    void locked_update()
    {
        bsp_display_lock(0);
        update();
        bsp_display_unlock();
    }

    // Generic callback for simple button clicks - extracts std::function from user_data and calls it
    static void generic_callback_cb(lv_event_t *e)
    {
        auto *callback = static_cast<std::function<void()> *>(lv_event_get_user_data(e));
        if (callback) {
            (*callback)();
        }
    }

  protected:
    ScreenBase() = default;

    // Non-copyable, non-movable
    ScreenBase(const ScreenBase &) = delete;
    ScreenBase &operator=(const ScreenBase &) = delete;
    ScreenBase(ScreenBase &&) = delete;
    ScreenBase &operator=(ScreenBase &&) = delete;
};
