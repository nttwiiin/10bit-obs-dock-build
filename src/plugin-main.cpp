#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QTimer>

#include "TenBitDockWidget.hpp"
#include "TenBitScoreDockWidget.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("10bit-broadcast-dock", "vi-VN")

static TenBitDockWidget *g_controlDock = nullptr;
static TenBitScoreDockWidget *g_scoreDock = nullptr;

static const char *kControlDockId = "tenbit-broadcast-dock";
static const char *kScoreDockId = "tenbit-score-dock";

static QDockWidget *findDock(QMainWindow *mainWindow, const char *id)
{
    if (!mainWindow)
        return nullptr;
    return mainWindow->findChild<QDockWidget *>(QString::fromUtf8(id), Qt::FindChildrenRecursively);
}

static void attach_control_dock(QMainWindow *mainWindow)
{
    auto *dock = findDock(mainWindow, kControlDockId);
    if (!dock)
        return;

    dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, dock);
    dock->setFloating(false);
    dock->toggleViewAction()->setChecked(true);
    dock->show();
    dock->raise();
}

static void attach_score_dock(QMainWindow *mainWindow)
{
    auto *scoreDock = findDock(mainWindow, kScoreDockId);
    if (!scoreDock)
        return;

    scoreDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    scoreDock->setFloating(false);

    // OBS' native Controls dock has objectName "controlsDock".
    // Split horizontally so 10BIT SCORE sits directly to its right.
    auto *controlsDock = mainWindow->findChild<QDockWidget *>(QStringLiteral("controlsDock"), Qt::FindChildrenRecursively);
    if (controlsDock) {
        mainWindow->splitDockWidget(controlsDock, scoreDock, Qt::Horizontal);
    } else {
        mainWindow->addDockWidget(Qt::BottomDockWidgetArea, scoreDock);
    }

    scoreDock->toggleViewAction()->setChecked(true);
    scoreDock->show();
    scoreDock->raise();
}

static void dock_into_obs()
{
    auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!mainWindow)
        return;

    attach_control_dock(mainWindow);
    attach_score_dock(mainWindow);
}

static void frontend_event(enum obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
        QTimer::singleShot(0, dock_into_obs);
}

MODULE_EXPORT const char *obs_module_description(void)
{
    return "10BIT Broadcast native OBS docks: Broadcast Control + Score";
}

bool obs_module_load(void)
{
    g_controlDock = new TenBitDockWidget();
    if (!obs_frontend_add_dock_by_id(kControlDockId, "10BIT Broadcast", g_controlDock)) {
        delete g_controlDock;
        g_controlDock = nullptr;
        blog(LOG_ERROR, "[10BIT Dock] Could not register control dock id");
        return false;
    }

    g_scoreDock = new TenBitScoreDockWidget();
    if (!obs_frontend_add_dock_by_id(kScoreDockId, "10BIT Score", g_scoreDock)) {
        delete g_scoreDock;
        g_scoreDock = nullptr;
        obs_frontend_remove_dock(kControlDockId);
        g_controlDock = nullptr;
        blog(LOG_ERROR, "[10BIT Dock] Could not register score dock id");
        return false;
    }

    obs_frontend_add_event_callback(frontend_event, nullptr);

    // OBS creates add_dock_by_id docks hidden/floating. Attach both immediately
    // and once again after OBS finishes restoring its layout.
    QTimer::singleShot(0, dock_into_obs);

    blog(LOG_INFO, "[10BIT Dock] loaded dual-dock suite (version %s)", PLUGIN_VERSION);
    return true;
}

void obs_module_unload(void)
{
    obs_frontend_remove_event_callback(frontend_event, nullptr);
    obs_frontend_remove_dock(kScoreDockId);
    obs_frontend_remove_dock(kControlDockId);
    g_scoreDock = nullptr;
    g_controlDock = nullptr;
    blog(LOG_INFO, "[10BIT Dock] unloaded dual-dock suite");
}
