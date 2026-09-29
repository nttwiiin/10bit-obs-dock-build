#include <obs-module.h>
#include <obs-frontend-api.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>
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

static void dock_into_obs()
{
    auto *mainWindow = static_cast<QMainWindow *>(obs_frontend_get_main_window());
    if (!mainWindow)
        return;

    auto *controlDock = findDock(mainWindow, kControlDockId);
    auto *scoreDock = findDock(mainWindow, kScoreDockId);
    if (!controlDock || !scoreDock)
        return;

    controlDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    scoreDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    controlDock->setFloating(false);
    scoreDock->setFloating(false);

    // Apply the 10BIT default workspace only once per layout version.
    // Afterwards OBS keeps the operator's own dock arrangement.
    QSettings settings(QStringLiteral("10BIT Media"), QStringLiteral("10BIT Broadcast OBS Dock"));
    const int applied = settings.value(QStringLiteral("defaultLayoutVersion"), 0).toInt();
    constexpr int wanted = 4;

    if (applied < wanted) {
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, controlDock);

        auto *sourcesDock = mainWindow->findChild<QDockWidget *>(QStringLiteral("sourcesDock"), Qt::FindChildrenRecursively);
        auto *scenesDock = mainWindow->findChild<QDockWidget *>(QStringLiteral("scenesDock"), Qt::FindChildrenRecursively);

        mainWindow->addDockWidget(Qt::LeftDockWidgetArea, scoreDock);
        if (sourcesDock) {
            if (scenesDock)
                mainWindow->tabifyDockWidget(sourcesDock, scenesDock);
            mainWindow->tabifyDockWidget(sourcesDock, scoreDock);
        } else if (scenesDock) {
            mainWindow->tabifyDockWidget(scenesDock, scoreDock);
        }

        settings.setValue(QStringLiteral("defaultLayoutVersion"), wanted);
        settings.sync();
    }

    controlDock->toggleViewAction()->setChecked(true);
    scoreDock->toggleViewAction()->setChecked(true);
    controlDock->show();
    scoreDock->show();
    controlDock->raise();
    scoreDock->raise();
}

static void frontend_event(enum obs_frontend_event event, void *)
{
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
        QTimer::singleShot(350, dock_into_obs);
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

    // Do not place docks here. OBS still restores its saved workspace after
    // plugin load. Placement is applied once after FINISHED_LOADING instead.

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
