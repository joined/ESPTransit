#include "app_manager.h"

static AppManager s_app_manager;

extern "C" void app_main(void)
{
    s_app_manager.start();
}
