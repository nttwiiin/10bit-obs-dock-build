#include "TenBitDockWidget.hpp"

#include <obs-frontend-api.h>

#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QStandardPaths>
#include <QStyle>
#include <QVBoxLayout>

namespace {
QString runtimeFilePath()
{
#ifdef Q_OS_WIN
    const QString appData = qEnvironmentVariable("APPDATA");
    if (!appData.isEmpty())
        return QDir(appData).filePath("10BIT Broadcast/dock-runtime.json");
#endif
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath("10BIT Broadcast/dock-runtime.json");
}

QString sideTeam(const QJsonObject &match, const QString &side)
{
    const QString value = match.value(side == "A" ? "TeamA" : "TeamB").toString().trimmed();
    return value.isEmpty() ? (side == "A" ? "ĐỘI A" : "ĐỘI B") : value;
}

QLabel *sectionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    label->setObjectName("section");
    return label;
}
}

TenBitDockWidget::TenBitDockWidget(QWidget *parent) : QWidget(parent)
{
    setObjectName("dockRoot");
    buildUI();

    connect(&socket_, &QTcpSocket::connected, this, &TenBitDockWidget::onConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &TenBitDockWidget::onDisconnected);
    connect(&socket_, &QTcpSocket::readyRead, this, &TenBitDockWidget::onReadyRead);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &TenBitDockWidget::onSocketError);

    reconnectTimer_.setInterval(2000);
    connect(&reconnectTimer_, &QTimer::timeout, this, &TenBitDockWidget::ensureConnected);
    reconnectTimer_.start();

    pollTimer_.setInterval(700);
    connect(&pollTimer_, &QTimer::timeout, this, &TenBitDockWidget::pollStatus);

    ensureConnected();
}

void TenBitDockWidget::buildUI()
{
    setMinimumWidth(285);
    setStyleSheet(R"(
        QWidget#dockRoot {
            background: #0c1119;
            color: #f5f7fb;
            font-size: 12px;
        }
        QLabel#title {
            color: #ffffff;
            font-size: 18px;
            font-weight: 800;
            letter-spacing: 0.4px;
        }
        QLabel#statusDot {
            font-size: 18px;
            font-weight: 900;
            padding: 0px;
        }
        QLabel#statusDot[state="connected"] { color: #43e58a; }
        QLabel#statusDot[state="waiting"] { color: #f6c94c; }
        QLabel#statusDot[state="offline"] { color: #ff5568; }
        QLabel#project {
            color: #aab4c4;
            background: #111925;
            border: 1px solid #202c3c;
            border-radius: 6px;
            padding: 6px 8px;
        }
        QLabel#section {
            color: #7f8da2;
            font-size: 10px;
            font-weight: 800;
            letter-spacing: 1px;
            padding-top: 2px;
        }
        QFrame#scoreCard {
            background: #121a25;
            border: 1px solid #253247;
            border-radius: 9px;
        }
        QLabel#teamName {
            color: #b8c3d2;
            font-size: 11px;
            font-weight: 700;
        }
        QLabel#scoreValue {
            color: #ffffff;
            font-size: 30px;
            font-weight: 900;
        }
        QLabel#scoreDash {
            color: #56647a;
            font-size: 22px;
            font-weight: 700;
        }
        QLabel#call {
            color: #8f9cb0;
            font-size: 10px;
        }
        QLabel#notice {
            color: #9aa6b8;
            font-size: 10px;
            padding: 3px 0px;
        }
        QPushButton {
            background: #1b2533;
            color: #f3f6fa;
            border: 1px solid #334157;
            border-radius: 6px;
            min-height: 32px;
            padding: 5px 8px;
            font-weight: 700;
        }
        QPushButton:hover {
            background: #253247;
            border-color: #4b5c77;
        }
        QPushButton:pressed {
            background: #303f56;
        }
        QPushButton[active="true"] {
            background: #d91f49;
            border-color: #ff4269;
            color: #ffffff;
        }
        QPushButton:disabled {
            color: #627086;
            background: #131a24;
            border-color: #242e3d;
        }
        QPushButton#replaySave {
            border-color: #66551d;
            color: #f7d95a;
        }
    )");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 9, 10, 9);
    root->setSpacing(7);

    auto *header = new QHBoxLayout();
    header->setContentsMargins(0, 0, 0, 0);
    auto *title = new QLabel("10BIT BROADCAST");
    title->setObjectName("title");
    connectionLabel_ = new QLabel("●");
    connectionLabel_->setObjectName("statusDot");
    connectionLabel_->setAlignment(Qt::AlignCenter);
    connectionLabel_->setFixedWidth(20);
    header->addWidget(title);
    header->addStretch(1);
    header->addWidget(connectionLabel_);
    root->addLayout(header);

    projectLabel_ = new QLabel("Chưa mở Project");
    projectLabel_->setObjectName("project");
    root->addWidget(projectLabel_);

    auto *scoreCard = new QFrame();
    scoreCard->setObjectName("scoreCard");
    auto *scoreGrid = new QGridLayout(scoreCard);
    scoreGrid->setContentsMargins(10, 7, 10, 7);
    scoreGrid->setHorizontalSpacing(8);
    scoreGrid->setVerticalSpacing(1);
    scoreGrid->setColumnStretch(0, 1);
    scoreGrid->setColumnStretch(2, 1);

    teamALabel_ = new QLabel("ĐỘI A");
    teamALabel_->setObjectName("teamName");
    teamALabel_->setAlignment(Qt::AlignCenter);
    teamBLabel_ = new QLabel("ĐỘI B");
    teamBLabel_->setObjectName("teamName");
    teamBLabel_->setAlignment(Qt::AlignCenter);

    scoreAValue_ = new QLabel("0");
    scoreAValue_->setObjectName("scoreValue");
    scoreAValue_->setAlignment(Qt::AlignCenter);
    scoreBValue_ = new QLabel("0");
    scoreBValue_->setObjectName("scoreValue");
    scoreBValue_->setAlignment(Qt::AlignCenter);

    auto *dash = new QLabel("–");
    dash->setObjectName("scoreDash");
    dash->setAlignment(Qt::AlignCenter);

    serveLabel_ = new QLabel("Game 0–0");
    serveLabel_->setObjectName("call");
    serveLabel_->setAlignment(Qt::AlignCenter);
    serveLabel_->setWordWrap(true);

    scoreGrid->addWidget(teamALabel_, 0, 0);
    scoreGrid->addWidget(teamBLabel_, 0, 2);
    scoreGrid->addWidget(scoreAValue_, 1, 0);
    scoreGrid->addWidget(dash, 1, 1);
    scoreGrid->addWidget(scoreBValue_, 1, 2);
    scoreGrid->addWidget(serveLabel_, 2, 0, 1, 3);
    root->addWidget(scoreCard);

    root->addWidget(sectionLabel("ĐỒ HỌA"));
    auto *graphics = new QGridLayout();
    graphics->setHorizontalSpacing(6);
    graphics->setVerticalSpacing(6);
    scoreToggle_ = makeButton("BẢNG ĐIỂM", "toggle_score");
    lowerToggle_ = makeButton("LOWER THIRD", "toggle_lower");
    timeoutButton_ = makeButton("TIME OUT", "timeout");
    replayButton_ = makeButton("REPLAY", "replay");
    standbyButton_ = makeButton("STANDBY", "standby");
    resultsButton_ = makeButton("KẾT QUẢ", "results");
    graphics->addWidget(scoreToggle_, 0, 0);
    graphics->addWidget(lowerToggle_, 0, 1);
    graphics->addWidget(timeoutButton_, 1, 0);
    graphics->addWidget(replayButton_, 1, 1);
    graphics->addWidget(standbyButton_, 2, 0);
    graphics->addWidget(resultsButton_, 2, 1);
    root->addLayout(graphics);

    root->addWidget(sectionLabel("ĐIỂM NHANH"));
    auto *scoreRow = new QHBoxLayout();
    scoreRow->setSpacing(6);
    rallyAButton_ = makeButton("ĐỘI A +", "rally_a");
    rallyBButton_ = makeButton("ĐỘI B +", "rally_b");
    scoreRow->addWidget(rallyAButton_);
    scoreRow->addWidget(rallyBButton_);
    root->addLayout(scoreRow);

    undoButton_ = makeButton("↶ HOÀN TÁC PHA", "undo");
    root->addWidget(undoButton_);

    root->addWidget(sectionLabel("REPLAY"));
    saveReplayButton_ = makeButton("LƯU REPLAY", "save_replay");
    saveReplayButton_->setObjectName("replaySave");
    root->addWidget(saveReplayButton_);

    messageLabel_ = new QLabel();
    messageLabel_->setObjectName("notice");
    messageLabel_->setWordWrap(true);
    messageLabel_->hide();
    root->addWidget(messageLabel_);
    root->addStretch(1);

    setConnectionState("waiting", "Đang chờ 10BIT Broadcast Core");
}

QPushButton *TenBitDockWidget::makeButton(const QString &text, const QString &command)
{
    auto *button = new QPushButton(text);
    button->setCursor(Qt::PointingHandCursor);
    connect(button, &QPushButton::clicked, this, [this, command]() { sendCommand(command); });
    return button;
}

bool TenBitDockWidget::loadRuntime()
{
    QFile f(runtimeFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;
    const auto doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isObject())
        return false;
    const auto obj = doc.object();
    const int port = obj.value("port").toInt();
    const QString token = obj.value("token").toString();
    const QString host = obj.value("host").toString("127.0.0.1");
    if (port <= 0 || port > 65535 || token.isEmpty())
        return false;
    host_ = host;
    port_ = static_cast<quint16>(port);
    token_ = token;
    return true;
}

void TenBitDockWidget::ensureConnected()
{
    if (socket_.state() == QAbstractSocket::ConnectedState || socket_.state() == QAbstractSocket::ConnectingState)
        return;
    if (!loadRuntime()) {
        setConnectionState("waiting", "Đang chờ ứng dụng 10BIT Broadcast");
        return;
    }
    connectToRuntime();
}

void TenBitDockWidget::connectToRuntime()
{
    socket_.abort();
    socket_.connectToHost(host_, port_);
    setConnectionState("waiting", "Đang kết nối 10BIT Broadcast Core");
}

void TenBitDockWidget::onConnected()
{
    setConnectionState("connected", "Đã kết nối 10BIT Broadcast Core");
    pollTimer_.start();
    sendCommand("status");
}

void TenBitDockWidget::onDisconnected()
{
    pollTimer_.stop();
    setConnectionState("offline", "Mất kết nối 10BIT Broadcast Core");
}

void TenBitDockWidget::onSocketError()
{
    pollTimer_.stop();
    setConnectionState("offline", "Không kết nối được 10BIT Broadcast Core");
}

void TenBitDockWidget::pollStatus()
{
    if (socket_.state() == QAbstractSocket::ConnectedState)
        sendCommand("status");
}

void TenBitDockWidget::sendCommand(const QString &command, const QJsonObject &args)
{
    if (socket_.state() != QAbstractSocket::ConnectedState) {
        ensureConnected();
        return;
    }
    QJsonObject req;
    req.insert("id", QString::number(++requestId_));
    req.insert("token", token_);
    req.insert("command", command);
    if (!args.isEmpty())
        req.insert("args", args);
    socket_.write(QJsonDocument(req).toJson(QJsonDocument::Compact));
    socket_.write("\n");
}

void TenBitDockWidget::onReadyRead()
{
    readBuffer_.append(socket_.readAll());
    while (true) {
        const qsizetype pos = readBuffer_.indexOf('\n');
        if (pos < 0)
            break;
        const QByteArray line = readBuffer_.left(pos);
        readBuffer_.remove(0, pos + 1);
        const auto doc = QJsonDocument::fromJson(line);
        if (!doc.isObject())
            continue;
        const auto obj = doc.object();
        if (!obj.value("ok").toBool()) {
            messageLabel_->setText(obj.value("error").toString("Lỗi điều khiển Dock"));
            messageLabel_->show();
            continue;
        }
        if (obj.value("state").isObject())
            applyState(obj.value("state").toObject());
    }
}

void TenBitDockWidget::applyState(const QJsonObject &state)
{
    const quint64 showSeq = static_cast<quint64>(state.value("dockShowSeq").toDouble());
    if (showSeq > lastDockShowSeq_) {
        lastDockShowSeq_ = showSeq;
        if (revealDock()) {
            QJsonObject ack;
            ack.insert("seq", static_cast<double>(showSeq));
            sendCommand("dock_show_ack", ack);
        }
    }

    const QString sport = state.value("sportModule").toString();
    const bool pickleball = sport == "pickleball";
    projectLabel_->setText(state.value("projectName").toString("Chưa mở Project") + (pickleball ? "  •  Pickleball" : ""));

    const auto match = state.value("match").toObject();
    const auto graphics = state.value("graphics").toObject();
    const QString teamA = sideTeam(match, "A");
    const QString teamB = sideTeam(match, "B");
    const int scoreA = match.value("ScoreA").toInt();
    const int scoreB = match.value("ScoreB").toInt();
    const int gamesA = match.value("GamesA").toInt();
    const int gamesB = match.value("GamesB").toInt();

    const QFontMetrics fm(teamALabel_->font());
    teamALabel_->setText(fm.elidedText(teamA, Qt::ElideRight, 112));
    teamBLabel_->setText(fm.elidedText(teamB, Qt::ElideRight, 112));
    teamALabel_->setToolTip(teamA);
    teamBLabel_->setToolTip(teamB);
    scoreAValue_->setText(QString::number(scoreA));
    scoreBValue_->setText(QString::number(scoreB));

    const QString call = state.value("scoreCall").toString().trimmed();
    serveLabel_->setText(QString("Game %1–%2%3").arg(gamesA).arg(gamesB)
        .arg(call.isEmpty() ? QString() : "  •  " + call));

    setActive(scoreToggle_, graphics.value("ProgramScore").toBool(), "●  BẢNG ĐIỂM", "BẢNG ĐIỂM");
    setActive(lowerToggle_, graphics.value("ProgramLower").toBool(), "●  LOWER THIRD", "LOWER THIRD");
    const QString takeover = graphics.value("ProgramTakeover").toString();
    setActive(timeoutButton_, takeover == "timeout");
    setActive(replayButton_, takeover == "replay");
    setActive(standbyButton_, takeover == "standby");
    setActive(resultsButton_, takeover == "results");

    rallyAButton_->setText(teamA + "  +");
    rallyBButton_->setText(teamB + "  +");
    rallyAButton_->setEnabled(pickleball);
    rallyBButton_->setEnabled(pickleball);
    undoButton_->setEnabled(pickleball && state.value("canUndo").toBool());
    saveReplayButton_->setEnabled(state.value("replayAvailable").toBool());

    if (messageLabel_->isVisible())
        messageLabel_->hide();
}

void TenBitDockWidget::setConnectionState(const QString &state, const QString &tooltip)
{
    connectionLabel_->setText("●");
    connectionLabel_->setProperty("state", state);
    connectionLabel_->setToolTip(tooltip);
    connectionLabel_->style()->unpolish(connectionLabel_);
    connectionLabel_->style()->polish(connectionLabel_);
}

void TenBitDockWidget::setActive(QPushButton *button, bool active, const QString &activeText, const QString &inactiveText)
{
    button->setProperty("active", active);
    button->style()->unpolish(button);
    button->style()->polish(button);
    if (active && !activeText.isEmpty())
        button->setText(activeText);
    else if (!active && !inactiveText.isEmpty())
        button->setText(inactiveText);
}

bool TenBitDockWidget::revealDock()
{
    QWidget *mainWidget = static_cast<QWidget *>(obs_frontend_get_main_window());
    if (!mainWidget)
        return false;

    auto *mainWindow = qobject_cast<QMainWindow *>(mainWidget);
    if (!mainWindow)
        return false;

    auto *controlDock = mainWindow->findChild<QDockWidget *>(QStringLiteral("tenbit-broadcast-dock"), Qt::FindChildrenRecursively);
    auto *scoreDock = mainWindow->findChild<QDockWidget *>(QStringLiteral("tenbit-score-dock"), Qt::FindChildrenRecursively);
    if (!controlDock || !scoreDock)
        return false;

    controlDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, controlDock);
    controlDock->setFloating(false);
    controlDock->toggleViewAction()->setChecked(true);
    controlDock->show();
    controlDock->raise();

    scoreDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    scoreDock->setFloating(false);
    auto *controlsDock = mainWindow->findChild<QDockWidget *>(QStringLiteral("controlsDock"), Qt::FindChildrenRecursively);
    if (controlsDock)
        mainWindow->splitDockWidget(controlsDock, scoreDock, Qt::Horizontal);
    else
        mainWindow->addDockWidget(Qt::BottomDockWidgetArea, scoreDock);
    scoreDock->toggleViewAction()->setChecked(true);
    scoreDock->show();
    scoreDock->raise();

    return controlDock->isVisible() && scoreDock->isVisible() &&
           !controlDock->isFloating() && !scoreDock->isFloating();
}
