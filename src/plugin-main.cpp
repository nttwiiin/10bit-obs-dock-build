#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QTimer>

#include "TenBitDockWidget.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("10bit-broadcast-dock", "vi-VN")

static TenBitDockWidget *g_dock = nullptr;
static const char *kDockId = "tenbit-broadcast-dock";

static void dock_into_obs()
{
    auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!mainWindow)
        return;

    auto *dock = mainWindow->findChild<QDockWidget *>(QStringLiteral("tenbit-broadcast-dock"), Qt::FindChildrenRecursively);
    if (!dock)
        return;

    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->setFloating(false);
    dock->toggleViewAction()->setChecked(true);
    dock->show();
    dock->raise();
}

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

    // obs_frontend_add_dock_by_id creates the native dock as a floating,
    // hidden QDockWidget. Defer one event-loop tick, then attach it to the
    // right side of the OBS main window and show it as a real integrated dock.
    QTimer::singleShot(0, dock_into_obs);

    blog(LOG_INFO, "[10BIT Dock] loaded (version %s)", PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    obs_frontend_remove_dock(kDockId);
    g_dock = nullptr;
    blog(LOG_INFO, "[10BIT Dock] unloaded");
}
