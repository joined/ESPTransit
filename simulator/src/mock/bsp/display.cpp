#include "bsp/display.h"
#include "lvgl.h"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <unistd.h>
#include "sim_log.h"
#include "sim_env_utils.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#if !defined(LV_USE_OBJ_NAME) || (LV_USE_OBJ_NAME != 1)
#error "LV_USE_OBJ_NAME must be set to 1 for simulator object lookup/click."
#endif

static const char *TAG = "bsp";

static lv_display_t *s_disp = NULL;
static lv_indev_t *s_mouse = NULL;
static float s_initial_zoom = 1.0f;
static int32_t s_base_h_res = 0;
static int32_t s_base_v_res = 0;

struct screenshot_request {
    const char *file_path;
    SemaphoreHandle_t done_sem;
    bool success;
};

// Custom mousewheel handler that scrolls content under the pointer
static void handle_mousewheel_scroll(int32_t wheel_y)
{
    if (!s_mouse || !s_disp)
        return;

    // Get the current mouse position
    lv_point_t point;
    lv_indev_get_point(s_mouse, &point);

    // Find the object under the pointer
    lv_obj_t *obj = lv_indev_search_obj(lv_screen_active(), &point);
    if (!obj)
        return;

    // Walk up to find a parent that actually has scrollable content
    while (obj) {
        lv_dir_t dir = lv_obj_get_scroll_dir(obj);
        if (dir & LV_DIR_VER) {
            // Check if there's actually content to scroll
            int32_t scroll_top = lv_obj_get_scroll_top(obj);
            int32_t scroll_bottom = lv_obj_get_scroll_bottom(obj);
            if (scroll_top > 0 || scroll_bottom > 0) {
                lv_obj_scroll_by_bounded(obj, 0, wheel_y * 30, LV_ANIM_OFF);
                return;
            }
        }
        obj = lv_obj_get_parent(obj);
    }
}

// === Zoom Control ===

void sim_set_initial_zoom(float zoom)
{
    s_initial_zoom = zoom;
}

bool sim_set_base_resolution(int32_t width, int32_t height)
{
    if (width <= 0 || height <= 0) {
        return false;
    }
    if (s_disp != nullptr) {
        return false;
    }

    s_base_h_res = width;
    s_base_v_res = height;
    return true;
}

// === LVGL Display Management ===

void bsp_display_lock(uint32_t timeout_ms)
{
    (void)timeout_ms;
    lv_lock();
}

void bsp_display_unlock(void)
{
    lv_unlock();
}

// === Display Initialization ===

// SDL event filter to catch mousewheel events before LVGL's encoder handler
static int sdl_event_filter(void *userdata, SDL_Event *event)
{
    (void)userdata;
    if (event->type == SDL_MOUSEWHEEL) {
        handle_mousewheel_scroll(event->wheel.y);
        // Return 0 to drop the event (prevent encoder behavior)
        return 0;
    }
    return 1; // Keep other events
}

struct lvgl_task_args {
    SemaphoreHandle_t ready_sem;
};

static void lvgl_task(void *arg)
{
    auto *args = static_cast<struct lvgl_task_args *>(arg);

    // SDL init and event loop must be on the same thread
    lv_init();
    sim_log_init_lvgl();

    lv_group_set_default(lv_group_create());

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");

    s_disp = lv_sdl_window_create(s_base_h_res, s_base_v_res);

    s_mouse = lv_sdl_mouse_create();
    lv_indev_set_group(s_mouse, lv_group_get_default());
    lv_indev_set_display(s_mouse, s_disp);
    lv_display_set_default(s_disp);

    if (!env_flag_enabled("UI_TEST") && !env_flag_enabled("SIMULATOR_HIDE_CURSOR")) {
        LV_IMAGE_DECLARE(mouse_cursor_icon);
        lv_obj_t *cursor_obj = lv_image_create(lv_screen_active());
        lv_image_set_src(cursor_obj, &mouse_cursor_icon);
        lv_indev_set_cursor(s_mouse, cursor_obj);
    }

    SDL_SetEventFilter(sdl_event_filter, NULL);

    lv_indev_t *kb = lv_sdl_keyboard_create();
    lv_indev_set_display(kb, s_disp);
    lv_indev_set_group(kb, lv_group_get_default());

#if LV_USE_TEST
    lv_test_indev_create_all();

    lv_indev_t *test_pointer = lv_test_indev_get_indev(LV_INDEV_TYPE_POINTER);
    if (test_pointer) {
        lv_indev_set_display(test_pointer, s_disp);
    }

    lv_indev_t *test_keypad = lv_test_indev_get_indev(LV_INDEV_TYPE_KEYPAD);
    if (test_keypad) {
        lv_indev_set_display(test_keypad, s_disp);
        lv_indev_set_group(test_keypad, lv_group_get_default());
        lv_indev_set_mode(test_keypad, LV_INDEV_MODE_EVENT);
    }

    lv_indev_t *test_encoder = lv_test_indev_get_indev(LV_INDEV_TYPE_ENCODER);
    if (test_encoder) {
        lv_indev_set_display(test_encoder, s_disp);
        lv_indev_set_group(test_encoder, lv_group_get_default());
    }
#endif

    if (s_initial_zoom != 1.0f) {
        lv_sdl_window_set_zoom(s_disp, s_initial_zoom);
    }

    // Signal that display is ready
    xSemaphoreGive(args->ready_sem);

    // Run timer loop (SDL events must be processed on this thread)
    while (true) {
        lv_lock();
        uint32_t time_till_next = lv_timer_handler();
        lv_unlock();

        vTaskDelay(pdMS_TO_TICKS(time_till_next > 0 ? time_till_next : 5));
    }
}

lv_display_t *bsp_display_start(void)
{
    static struct lvgl_task_args args;
    args.ready_sem = xSemaphoreCreateBinary();

    xTaskCreate(lvgl_task, "lvgl", 8192, &args, 1, nullptr);

    // Wait for LVGL/SDL init to complete on the lvgl thread
    xSemaphoreTake(args.ready_sem, portMAX_DELAY);
    vSemaphoreDelete(args.ready_sem);

    return s_disp;
}

// === Brightness Control ===

void bsp_display_brightness_set(uint8_t brightness)
{
    sim_log_print(SIM_LOG_INFO, TAG, "Brightness set to %d%%", brightness);
}

static void async_capture_screenshot_cb(void *param)
{
    auto *request = static_cast<screenshot_request *>(param);
    request->success = false;

    if (!s_disp) {
        sim_log_print(SIM_LOG_ERROR, TAG, "Cannot capture screenshot: display not initialized");
        xSemaphoreGive(request->done_sem);
        return;
    }

    // Ensure latest frame is rendered before reading back pixels.
    lv_refr_now(s_disp);

    // Always use LVGL's test screenshot path to keep capture behavior consistent
    // across SDL backends (including headless SDL_VIDEODRIVER=dummy).
    const int process_id = static_cast<int>(getpid());
    char lvgl_temp_path[64] = {};
    std::snprintf(lvgl_temp_path, sizeof(lvgl_temp_path), "A:sim_capture_tmp_%d.png", process_id);
    std::filesystem::path host_temp_path("sim_capture_tmp_" + std::to_string(process_id) + ".png");
    (void)std::filesystem::remove(host_temp_path);
    if (!lv_test_screenshot_compare(lvgl_temp_path)) {
        sim_log_print(SIM_LOG_ERROR, TAG, "Failed to save screenshot: lv_test_screenshot_compare returned false (%s)",
                      lvgl_temp_path);
        xSemaphoreGive(request->done_sem);
        return;
    }

    if (!std::filesystem::exists(host_temp_path)) {
        sim_log_print(SIM_LOG_ERROR, TAG, "Failed to save screenshot: missing LVGL temp file (%s)", lvgl_temp_path);
        xSemaphoreGive(request->done_sem);
        return;
    }

    std::filesystem::path destination_path(request->file_path);
    if (!destination_path.parent_path().empty()) {
        std::error_code mkdir_error;
        std::filesystem::create_directories(destination_path.parent_path(), mkdir_error);
        if (mkdir_error) {
            sim_log_print(SIM_LOG_ERROR, TAG, "Failed to create screenshot directory %s: %s",
                          destination_path.parent_path().string().c_str(), mkdir_error.message().c_str());
            xSemaphoreGive(request->done_sem);
            return;
        }
    }

    std::error_code move_error;
    std::filesystem::rename(host_temp_path, destination_path, move_error);
    if (move_error) {
        std::error_code copy_error;
        std::filesystem::copy_file(host_temp_path, destination_path, std::filesystem::copy_options::overwrite_existing,
                                   copy_error);
        if (copy_error) {
            sim_log_print(SIM_LOG_ERROR, TAG, "Failed to move screenshot to %s: %s", destination_path.string().c_str(),
                          copy_error.message().c_str());
            xSemaphoreGive(request->done_sem);
            return;
        }
        (void)std::filesystem::remove(host_temp_path);
    }

    if (!std::filesystem::exists(destination_path)) {
        sim_log_print(SIM_LOG_ERROR, TAG, "Screenshot output file missing after save: %s", request->file_path);
        xSemaphoreGive(request->done_sem);
        return;
    }

    request->success = true;
    sim_log_print(SIM_LOG_INFO, TAG, "Saved screenshot: %s", request->file_path);
    xSemaphoreGive(request->done_sem);
}

bool sim_capture_screenshot(const char *file_path)
{
    if (!file_path || file_path[0] == '\0') {
        sim_log_print(SIM_LOG_ERROR, TAG, "Cannot capture screenshot: missing output path");
        return false;
    }

    screenshot_request request = {};
    request.file_path = file_path;
    request.done_sem = xSemaphoreCreateBinary();
    if (!request.done_sem) {
        sim_log_print(SIM_LOG_ERROR, TAG, "Cannot capture screenshot: failed to create semaphore");
        return false;
    }

    lv_result_t call_result = lv_async_call(async_capture_screenshot_cb, &request);
    if (call_result != LV_RESULT_OK) {
        sim_log_print(SIM_LOG_ERROR, TAG, "Cannot capture screenshot: failed to schedule async callback");
        vSemaphoreDelete(request.done_sem);
        return false;
    }

    BaseType_t completed = xSemaphoreTake(request.done_sem, pdMS_TO_TICKS(5000));
    vSemaphoreDelete(request.done_sem);
    if (completed != pdTRUE) {
        sim_log_print(SIM_LOG_ERROR, TAG, "Cannot capture screenshot: timed out waiting for callback");
        return false;
    }

    return request.success;
}

static lv_obj_t *find_object_by_name_locked(const char *name)
{
    if (!name || name[0] == '\0') {
        return nullptr;
    }

    auto object_name_matches = [name](const lv_obj_t *obj) -> bool {
        if (!obj) {
            return false;
        }
        const char *obj_name = lv_obj_get_name(obj);
        return obj_name && strcmp(obj_name, name) == 0;
    };

    lv_obj_t *active_screen = lv_screen_active();
    if (object_name_matches(active_screen)) {
        return active_screen;
    }

    lv_obj_t *found = lv_obj_find_by_name(active_screen, name);
    if (found) {
        return found;
    }

    lv_obj_t *top_layer = lv_layer_top();
    if (object_name_matches(top_layer)) {
        return top_layer;
    }
    if (top_layer) {
        found = lv_obj_find_by_name(top_layer, name);
        if (found) {
            return found;
        }
    }

    lv_obj_t *sys_layer = lv_layer_sys();
    if (object_name_matches(sys_layer)) {
        return sys_layer;
    }
    if (sys_layer) {
        found = lv_obj_find_by_name(sys_layer, name);
        if (found) {
            return found;
        }
    }

    return nullptr;
}

bool sim_object_exists_by_name(const char *name)
{
    if (!name || name[0] == '\0' || !s_disp) {
        return false;
    }

    lv_lock();
    bool found = find_object_by_name_locked(name) != nullptr;
    lv_unlock();
    return found;
}

int sim_click_object_by_name(const char *name)
{
    if (!name || name[0] == '\0' || !s_disp) {
        return SIM_CLICK_NOT_FOUND;
    }

    lv_lock();
    lv_obj_t *target = find_object_by_name_locked(name);
    int result;
    if (!target) {
        result = SIM_CLICK_NOT_FOUND;
    } else {
        // Ensure pending layouts are computed so obj->coords are valid for visibility check
        lv_obj_update_layout(target);
        if (!lv_obj_is_visible(target)) {
            result = SIM_CLICK_NOT_VISIBLE;
        } else {
            lv_obj_send_event(target, LV_EVENT_PRESSED, nullptr);
            lv_obj_send_event(target, LV_EVENT_RELEASED, nullptr);
            lv_obj_send_event(target, LV_EVENT_CLICKED, nullptr);
            result = SIM_CLICK_OK;
        }
    }
    lv_unlock();
    return result;
}

bool sim_scroll_to_view_by_name(const char *name)
{
    if (!name || name[0] == '\0' || !s_disp) {
        return false;
    }

    lv_lock();
    lv_obj_t *target = find_object_by_name_locked(name);
    if (target) {
        lv_obj_scroll_to_view_recursive(target, LV_ANIM_OFF);
    }
    lv_unlock();
    return target != nullptr;
}

bool sim_set_dropdown_selected_by_name(const char *name, uint16_t selected_index)
{
    if (!name || name[0] == '\0' || !s_disp) {
        return false;
    }

    lv_lock();
    bool success = false;
    lv_obj_t *target = find_object_by_name_locked(name);
    if (target && lv_obj_check_type(target, &lv_dropdown_class)) {
        uint32_t option_count = lv_dropdown_get_option_count(target);
        if (selected_index < option_count) {
            lv_dropdown_set_selected(target, selected_index);
            lv_obj_send_event(target, LV_EVENT_VALUE_CHANGED, nullptr);
            success = true;
        }
    }
    lv_unlock();

    return success;
}

bool sim_type_text_with_test_keys(const char *text)
{
#if !LV_USE_TEST
    (void)text;
    return false;
#else
    if (!text || !s_disp) {
        return false;
    }

    if (text[0] == '\0') {
        return true;
    }

    lv_indev_t *test_keypad = nullptr;
    lv_lock();
    test_keypad = lv_test_indev_get_indev(LV_INDEV_TYPE_KEYPAD);
    lv_unlock();
    if (!test_keypad) {
        sim_log_print(SIM_LOG_ERROR, TAG, "Cannot type text: test keypad indev is missing");
        return false;
    }

    for (const unsigned char *it = reinterpret_cast<const unsigned char *>(text); *it != '\0'; ++it) {
        lv_lock();
        lv_test_key_press(*it);
        lv_indev_read(test_keypad);
        lv_test_key_release();
        lv_indev_read(test_keypad);
        lv_unlock();

        vTaskDelay(pdMS_TO_TICKS(30));
    }

    return true;
#endif
}

bool bsp_display_is_native_landscape(void)
{
    return s_base_h_res > s_base_v_res;
}

// === Rotation Management ===

// Async callback to resize display (must run in LVGL timer task)
static void async_resize_cb(void *p)
{
    int32_t *dims = (int32_t *)p;
    // Resolution change automatically preserves zoom (SDL driver uses stored zoom)
    lv_display_set_resolution(s_disp, dims[0], dims[1]);
    lv_free(dims);
}

void bsp_display_rotate(lv_display_t *disp, lv_disp_rotation_t rotation)
{
    (void)disp; // Display parameter not used in simulator (single display)

    sim_log_print(SIM_LOG_INFO, TAG, "Display rotation changed: %d°", rotation * 90);

    // 90°/270° swap axes relative to the native 0° orientation
    bool swap_axes = (rotation == 1 || rotation == 3);

    int32_t *dims = (int32_t *)lv_malloc(2 * sizeof(int32_t));
    dims[0] = swap_axes ? s_base_v_res : s_base_h_res;
    dims[1] = swap_axes ? s_base_h_res : s_base_v_res;

    // Resize must happen in the LVGL timer task
    lv_async_call(async_resize_cb, dims);
}
