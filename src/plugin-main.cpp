#include <obs-module.h>
#include <obs-frontend-api.h>

#include "TenBitDockWidget.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("10bit-broadcast-dock", "vi-VN")

static TenBitDockWidget *g_dock = nullptr;
static const char *kDockId = "tenbit-broadcast-dock";

MODULE_EXPORT const char *obs_module_description(void)
{
    return "10BIT Broadcast native control dock";
}

bool obs_module_load(void)
{
    g_dock = new TenBitDockWidget();
    if (!obs_frontend_add_dock_by_id(kDockId, "10BIT Broadcast", g_dock)) {
        delete g_dock;
        g_dock = nullptr;
        blog(LOG_ERROR, "[10BIT Dock] Could not register dock id");
        return false;
    }
    blog(LOG_INFO, "[10BIT Dock] loaded (version %s)", PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    obs_frontend_remove_dock(kDockId);
    g_dock = nullptr;
    blog(LOG_INFO, "[10BIT Dock] unloaded");
}
