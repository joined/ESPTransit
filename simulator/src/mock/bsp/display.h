#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Display initialization
lv_display_t *bsp_display_start(void);

// Display locking (for LVGL thread safety)
void bsp_display_lock(uint32_t timeout_ms);
void bsp_display_unlock(void);

// Display brightness control
void bsp_display_brightness_set(uint8_t brightness);

// Display rotation (matches ESP BSP signature)
void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation);

// True when the panel's native 0° orientation is landscape (width > height).
bool bsp_display_is_native_landscape(void);

// Zoom control (call before bsp_display_start)
void sim_set_initial_zoom(float zoom);

// Set base display resolution before display start (native 0° orientation).
// For portrait-native boards width < height; for landscape-native boards width > height.
// Returns false for invalid values or after display initialization.
bool sim_set_base_resolution(int32_t width, int32_t height);

// Capture current window framebuffer to a PNG file.
bool sim_capture_screenshot(const char *file_path);

// Click result codes for sim_click_object_by_name.
#define SIM_CLICK_OK 0
#define SIM_CLICK_NOT_FOUND -1
#define SIM_CLICK_NOT_VISIBLE -2

// UI test helpers for object-name based interaction.
bool sim_object_exists_by_name(const char *name);
int sim_click_object_by_name(const char *name);
bool sim_scroll_to_view_by_name(const char *name);
bool sim_set_dropdown_selected_by_name(const char *name, uint16_t selected_index);
bool sim_type_text_with_test_keys(const char *text);

#ifdef __cplusplus
}
#endif
