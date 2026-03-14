#pragma once

#include "screen_base.h"
#include "lvgl.h"

class BootScreen : public ScreenBase {
  public:
    BootScreen();
    ~BootScreen() override = default;

    lv_obj_t *get_screen() const override { return m_screen; }

    // Update the status text below the logo (call under LVGL lock)
    void set_status(const char *text);

  private:
    lv_obj_t *m_screen = nullptr;
    lv_obj_t *m_status_label = nullptr;
};
