#include "debug.h"
#include "app_platform.h"
#include "ui/common.h"
#include "ui/components/card.h"
#include "fira_sans.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <algorithm>
#include <cstring>
#include <format>
#include <vector>

#ifdef ESP_PLATFORM
#include "esp_heap_caps.h"
#endif

static const char *TAG = "ui_debug";
static constexpr int32_t CARD_PAD = 16;
static constexpr uint8_t TASK_COL_NAME_GROW = 40;
static constexpr uint8_t TASK_COL_STATE_GROW = 16;
static constexpr uint8_t TASK_COL_PRIO_GROW = 10;
static constexpr uint8_t TASK_COL_STACK_GROW = 34;

// ===== Helpers =====

lv_obj_t *DebugScreen::build_card(lv_obj_t *parent, const std::string &title)
{
    lv_obj_t *card = ui_create_card(parent, CARD_PAD, 6, 8);

    lv_obj_t *title_label = lv_label_create(card);
    lv_label_set_text(title_label, title.c_str());
    lv_obj_set_style_text_color(title_label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_font(title_label, FONT_BODY, 0);

    return card;
}

static lv_obj_t *create_info_label(lv_obj_t *parent, const std::string &text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text.c_str());
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(label, FONT_MEDIUM, 0);
    return label;
}

// ===== Memory helpers =====

struct HeapRegionInfo {
    size_t total;
    size_t free;
    size_t min_free;
    size_t largest_block;
    bool available;
};

struct HeapInfo {
    HeapRegionInfo overall;
    HeapRegionInfo internal;
    HeapRegionInfo spiram;
};

static HeapInfo get_heap_info()
{
    HeapInfo info = {};
#ifdef ESP_PLATFORM
    info.overall.total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT);
    info.overall.free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT);
    info.overall.min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_DEFAULT);
    info.overall.largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT);
    info.overall.available = info.overall.total > 0;

    info.internal.total = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    info.internal.free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    info.internal.min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    info.internal.largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    info.internal.available = info.internal.total > 0;

    info.spiram.total = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    info.spiram.free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    info.spiram.min_free = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    info.spiram.largest_block = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    info.spiram.available = info.spiram.total > 0;
#else
    if (AppPlatform::is_ui_test_mode()) {
        info.overall = {8 * 1024 * 1024, 4 * 1024 * 1024 + 100 * 1024, 3 * 1024 * 1024 + 900 * 1024, 4 * 1024 * 1024,
                        true};
        info.internal = info.overall;
        info.spiram = {};
        info.spiram.available = false;
        return info;
    }
    info.overall.total = configTOTAL_HEAP_SIZE;
    info.overall.free = xPortGetFreeHeapSize();
    info.overall.min_free = xPortGetMinimumEverFreeHeapSize();
    info.overall.largest_block = info.overall.free; // No equivalent in POSIX port
    info.overall.available = true;

    info.internal = info.overall;
    info.internal.available = true;
    info.spiram = {};
    info.spiram.available = false;
#endif
    return info;
}

static const char *task_state_to_string(eTaskState state);

static uint32_t get_uptime_secs()
{
#ifndef ESP_PLATFORM
    if (AppPlatform::is_ui_test_mode()) {
        return 5;
    }
#endif
    return xTaskGetTickCount() / configTICK_RATE_HZ;
}

struct TaskSnapshot {
    std::string name;
    std::string state;
    unsigned long prio;
    size_t stack_free_bytes;
};

static std::vector<TaskSnapshot> get_task_snapshot()
{
#ifndef ESP_PLATFORM
    if (AppPlatform::is_ui_test_mode()) {
        return {
            {"IDLE", "Ready", 0, 512},
            {"http_fetcher", "Blocked", 5, 2048},
            {"main_task", "Running", 5, 8192},
        };
    }
#endif
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    if (task_count == 0) {
        return {};
    }
    UBaseType_t task_capacity = task_count + 8;
    auto *raw = new TaskStatus_t[task_capacity];
    UBaseType_t actual = uxTaskGetSystemState(raw, task_capacity, nullptr);

    std::sort(raw, raw + actual, [](const TaskStatus_t &a, const TaskStatus_t &b) {
        const char *a_name = a.pcTaskName ? a.pcTaskName : "";
        const char *b_name = b.pcTaskName ? b.pcTaskName : "";
        int cmp = strcmp(a_name, b_name);
        if (cmp != 0)
            return cmp < 0;
        if (a.xTaskNumber != b.xTaskNumber)
            return a.xTaskNumber < b.xTaskNumber;
        return reinterpret_cast<uintptr_t>(a.xHandle) < reinterpret_cast<uintptr_t>(b.xHandle);
    });

    std::vector<TaskSnapshot> result;
    result.reserve(actual);
    for (UBaseType_t i = 0; i < actual; i++) {
        result.push_back(TaskSnapshot{raw[i].pcTaskName, task_state_to_string(raw[i].eCurrentState),
                                      (unsigned long)raw[i].uxCurrentPriority,
                                      (size_t)raw[i].usStackHighWaterMark * sizeof(StackType_t)});
    }
    delete[] raw;
    return result;
}

static std::string format_bytes(size_t bytes)
{
    if (bytes >= 1024 * 1024) {
        return std::format("{:.1f} MB", (double)bytes / (1024.0 * 1024.0));
    } else if (bytes >= 1024) {
        return std::format("{:.1f} KB", (double)bytes / 1024.0);
    } else {
        return std::format("{} B", bytes);
    }
}

static lv_obj_t *create_task_header_cell(lv_obj_t *parent, const std::string &text, uint8_t grow)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text.c_str());
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(label, 0);
    lv_obj_set_flex_grow(label, grow);
    lv_obj_set_style_text_font(label, FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT, 0);
    lv_obj_set_style_pad_left(label, 8, 0);
    lv_obj_set_style_pad_right(label, 8, 0);
    lv_obj_set_style_pad_top(label, 6, 0);
    lv_obj_set_style_pad_bottom(label, 6, 0);
    return label;
}

static lv_obj_t *create_task_row_cell(lv_obj_t *parent, const std::string &text, uint8_t grow)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text.c_str());
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(label, 0);
    lv_obj_set_flex_grow(label, grow);
    lv_obj_set_style_text_font(label, FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(label, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_pad_left(label, 8, 0);
    lv_obj_set_style_pad_right(label, 8, 0);
    lv_obj_set_style_pad_top(label, 6, 0);
    lv_obj_set_style_pad_bottom(label, 6, 0);
    return label;
}

static lv_obj_t *create_task_row(lv_obj_t *parent)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, LV_PCT(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 0, 0);
    lv_obj_set_style_bg_color(row, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_border_color(row, UI_COLOR_BORDER, 0);
    lv_obj_set_style_border_side(row, LV_BORDER_SIDE_FULL, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    return row;
}

static const char *task_state_to_string(eTaskState state)
{
    switch (state) {
    case eRunning:
        return "Running";
    case eReady:
        return "Ready";
    case eBlocked:
        return "Blocked";
    case eSuspended:
        return "Suspended";
    case eDeleted:
        return "Deleted";
    default:
        return "?";
    }
}

// ===== Constructor =====

DebugScreen::DebugScreen(std::function<void()> on_back) : m_on_back(std::move(on_back))
{
    m_screen = lv_obj_create(NULL);
    ui_style_screen(m_screen);
    lv_obj_set_flex_flow(m_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_screen, UI_PADDING, 0);
    lv_obj_set_style_pad_gap(m_screen, 16, 0);

    // Header
    lv_obj_t *header = lv_obj_create(m_screen);
    lv_obj_remove_style_all(header);
    lv_obj_set_width(header, LV_PCT(100));
    lv_obj_set_height(header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_gap(header, 12, 0);

    ui_create_button(header, LV_SYMBOL_LEFT " Back", ScreenBase::generic_callback_cb, &m_on_back);

    lv_obj_t *title = lv_label_create(header);
    lv_label_set_text(title, "Debug");
    lv_obj_set_style_text_color(title, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(title, FONT_TITLE, 0);

    // ===== Memory Card =====
    lv_obj_t *mem_card = build_card(m_screen, "Heap Memory");

    m_mem_total_label = create_info_label(mem_card, "Total: --");
    m_mem_free_label = create_info_label(mem_card, "Free: --");
    m_mem_internal_label = create_info_label(mem_card, "Internal free: --");
    m_mem_spiram_label = create_info_label(mem_card, "SPIRAM free: --");
    m_mem_min_label = create_info_label(mem_card, "Min free: --");
    m_mem_largest_label = create_info_label(mem_card, "Largest block: --");

    // ===== FreeRTOS Tasks Card =====
    lv_obj_t *task_card = build_card(m_screen, "FreeRTOS Tasks");

    lv_obj_t *task_header = lv_obj_create(task_card);
    lv_obj_remove_style_all(task_header);
    lv_obj_set_width(task_header, LV_PCT(100));
    lv_obj_set_height(task_header, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(task_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(task_header, 0, 0);
    lv_obj_set_style_bg_color(task_header, UI_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(task_header, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(task_header, 6, 0);

    create_task_header_cell(task_header, "Name", TASK_COL_NAME_GROW);
    create_task_header_cell(task_header, "State", TASK_COL_STATE_GROW);
    create_task_header_cell(task_header, "Prio", TASK_COL_PRIO_GROW);
    create_task_header_cell(task_header, "Stack", TASK_COL_STACK_GROW);

    m_task_table = lv_obj_create(task_card);
    lv_obj_remove_style_all(m_task_table);
    lv_obj_set_width(m_task_table, LV_PCT(100));
    lv_obj_set_height(m_task_table, 220);
    lv_obj_set_flex_flow(m_task_table, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(m_task_table, 0, 0);
    lv_obj_set_style_pad_gap(m_task_table, 0, 0);
    lv_obj_set_scroll_dir(m_task_table, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(m_task_table, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_radius(m_task_table, 6, 0);
    lv_obj_set_style_bg_color(m_task_table, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(m_task_table, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(m_task_table, 1, 0);
    lv_obj_set_style_border_color(m_task_table, UI_COLOR_BORDER, 0);
    lv_obj_set_style_text_font(m_task_table, FONT_MEDIUM, 0);
    lv_obj_set_style_text_color(m_task_table, UI_COLOR_TEXT_DARK, 0);
    lv_obj_set_style_text_align(m_task_table, LV_TEXT_ALIGN_LEFT, 0);

    // ===== System Info Card =====
    lv_obj_t *sys_card = build_card(m_screen, "System Info");

    create_info_label(sys_card, "LVGL " LVGL_VERSION_INFO);
    create_info_label(sys_card, std::format("Build: {}", AppPlatform::get_version_string()));
    m_uptime_label = create_info_label(sys_card, "Uptime: --");

    ui_set_test_id(m_screen, "debug.screen");

    // Initial data population (avoid calling virtual update() from ctor)
    update_memory_stats();
    update_task_list();
    update_uptime();

    ESP_LOGI(TAG, "Debug screen created");
}

DebugScreen::~DebugScreen()
{
    if (m_screen) {
        lv_obj_delete(m_screen);
        m_screen = nullptr;
    }
}

// ===== Update =====

void DebugScreen::update_memory_stats()
{
    HeapInfo heap = get_heap_info();

    int pct_free = heap.overall.total > 0 ? (int)(heap.overall.free * 100 / heap.overall.total) : 0;
    lv_label_set_text(m_mem_total_label, std::format("Total: {}", format_bytes(heap.overall.total)).c_str());
    lv_label_set_text(m_mem_free_label,
                      std::format("Free: {} ({}%)", format_bytes(heap.overall.free), pct_free).c_str());

    if (heap.internal.available) {
        int internal_pct = heap.internal.total > 0 ? (int)(heap.internal.free * 100 / heap.internal.total) : 0;
        lv_label_set_text(m_mem_internal_label,
                          std::format("Internal free: {} / {} ({}%)", format_bytes(heap.internal.free),
                                      format_bytes(heap.internal.total), internal_pct)
                              .c_str());
    } else {
        lv_label_set_text(m_mem_internal_label, "Internal free: --");
    }

    if (heap.spiram.available) {
        int spiram_pct = heap.spiram.total > 0 ? (int)(heap.spiram.free * 100 / heap.spiram.total) : 0;
        lv_label_set_text(m_mem_spiram_label, std::format("SPIRAM free: {} / {} ({}%)", format_bytes(heap.spiram.free),
                                                          format_bytes(heap.spiram.total), spiram_pct)
                                                  .c_str());
    } else {
        lv_label_set_text(m_mem_spiram_label, "SPIRAM free: not available");
    }

    lv_label_set_text(m_mem_min_label, std::format("Min free ever: {}", format_bytes(heap.overall.min_free)).c_str());
    lv_label_set_text(m_mem_largest_label,
                      std::format("Largest block: {}", format_bytes(heap.overall.largest_block)).c_str());
}

void DebugScreen::update_task_list()
{
    int32_t scroll_y = lv_obj_get_scroll_y(m_task_table);

    lv_obj_clean(m_task_table);
    std::vector<TaskSnapshot> tasks = get_task_snapshot();
    for (const auto &t : tasks) {
        lv_obj_t *row = create_task_row(m_task_table);
        create_task_row_cell(row, t.name, TASK_COL_NAME_GROW);
        create_task_row_cell(row, t.state, TASK_COL_STATE_GROW);
        create_task_row_cell(row, std::format("{}", t.prio), TASK_COL_PRIO_GROW);
        create_task_row_cell(row, std::format("{}", t.stack_free_bytes), TASK_COL_STACK_GROW);
    }

    lv_obj_update_layout(m_task_table);
    lv_obj_scroll_to_y(m_task_table, scroll_y, LV_ANIM_OFF);
}

void DebugScreen::update_uptime()
{
    uint32_t secs = get_uptime_secs();
    uint32_t mins = secs / 60;
    uint32_t hours = mins / 60;
    lv_label_set_text(m_uptime_label, std::format("Uptime: {}h {}m {}s", hours, mins % 60, secs % 60).c_str());
}

void DebugScreen::update()
{
    update_memory_stats();
    update_task_list();
    update_uptime();
}
