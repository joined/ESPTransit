#pragma once

#include "screen_base.h"
#include "lvgl.h"
#include <functional>
#include <string>

class DebugScreen : public ScreenBase {
  public:
    explicit DebugScreen(std::function<void()> on_back);
    ~DebugScreen() override;

    lv_obj_t *get_screen() const override { return m_screen; }
    void update() override;

  private:
    DebugScreen(const DebugScreen &) = delete;
    DebugScreen &operator=(const DebugScreen &) = delete;

    lv_obj_t *build_card(lv_obj_t *parent, const std::string &title);
    void update_memory_stats();
    void update_task_list();
    void update_uptime();

    lv_obj_t *m_screen = nullptr;
    std::function<void()> m_on_back;

    // Memory card
    lv_obj_t *m_mem_total_label = nullptr;
    lv_obj_t *m_mem_free_label = nullptr;
    lv_obj_t *m_mem_internal_label = nullptr;
    lv_obj_t *m_mem_spiram_label = nullptr;
    lv_obj_t *m_mem_min_label = nullptr;
    lv_obj_t *m_mem_largest_label = nullptr;

    // Task table
    lv_obj_t *m_task_table = nullptr;

    // System info
    lv_obj_t *m_uptime_label = nullptr;
};
