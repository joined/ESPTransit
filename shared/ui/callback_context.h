#pragma once

#include "lvgl.h"
#include <utility>

template <typename T> T *ui_manage_callback_context(lv_obj_t *owner, T *ctx)
{
    if (!ctx) {
        return nullptr;
    }
    if (!owner) {
        delete ctx;
        return nullptr;
    }

    lv_obj_add_event_cb(
        owner,
        [](lv_event_t *e) {
            auto *context = static_cast<T *>(lv_event_get_user_data(e));
            delete context;
        },
        LV_EVENT_DELETE, ctx);

    return ctx;
}

template <typename T, typename... Args> T *ui_make_callback_context(lv_obj_t *owner, Args &&...args)
{
    return ui_manage_callback_context(owner, new T{std::forward<Args>(args)...});
}
