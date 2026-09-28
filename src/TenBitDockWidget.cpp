#include "TenBitDockWidget.hpp"

#include <obs-frontend-api.h>

#include <QDir>
#include <QDockWidget>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QMainWindow>
#include <QPalette>
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
    QFont font = label->font();
    font.setBold(true);
    font.setPointSizeF(qMax(8.0, font.pointSizeF() - 1.0));
    label->setFont(font);
    return label;
}
}

TenBitDockWidget::TenBitDockWidget(QWidget *parent) : TenBitObsDockContent(parent)
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
    setMinimumWidth(292);

    auto *panel = new QFrame(this);
    panel->setObjectName("tenbitPanel");
    panel->setStyleSheet(R"(
        QFrame#tenbitPanel { background: transparent; }
        QLabel#statusDot {
            font-size: 17px;
            font-weight: 900;
            padding: 0px;
        }
        QLabel#statusDot[state="connected"] { color: #43e58a; }
        QLabel#statusDot[state="waiting"] { color: #f2c94c; }
        QLabel#statusDot[state="offline"] { color: #ff5a6d; }
        QLabel#section {
            color: palette(mid);
            font-size: 10px;
            font-weight: 800;
            padding-top: 2px;
        }
        QFrame#scoreCard {
            background: palette(base);
            border: 1px solid palette(mid);
            border-radius: 5px;
        }
        QLabel#teamName {
            font-size: 10px;
            font-weight: 700;
        }
        QLabel#scoreValue {
            font-size: 28px;
            font-weight: 900;
        }
        QLabel#scoreDash {
            color: palette(mid);
            font-size: 20px;
            font-weight: 800;
        }
        QLabel#call {
            color: palette(mid);
            font-size: 10px;
        }
        QPushButton[active="true"] {
            background: #d91f49;
            border-color: #ff4269;
            color: #ffffff;
        }
    )");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(panel);

    auto *root = new QVBoxLayout(panel);
    root->setContentsMargins(8, 7, 8, 8);
    root->setSpacing(6);

    auto *projectRow = new QHBoxLayout();
    projectRow->setContentsMargins(0, 0, 0, 0);
    projectLabel_ = new QLabel("Chưa mở Project");
    QFont projectFont = projectLabel_->font();
    projectFont.setPointSizeF(qMax(8.0, projectFont.pointSizeF() - 1.0));
    projectLabel_->setFont(projectFont);
    connectionLabel_ = new QLabel("●");
    connectionLabel_->setObjectName("statusDot");
    connectionLabel_->setAlignment(Qt::AlignCenter);
    connectionLabel_->setFixedWidth(20);
    projectRow->addWidget(projectLabel_, 1);
    projectRow->addWidget(connectionLabel_);
    root->addLayout(projectRow);

    auto *scoreCard = new QFrame();
    scoreCard->setObjectName("scoreCard");
    auto *scoreGrid = new QGridLayout(scoreCard);
    scoreGrid->setContentsMargins(10, 8, 10, 7);
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

    auto *graphicsLabel = sectionLabel("ĐỒ HỌA");
    graphicsLabel->setObjectName("section");
    root->addWidget(graphicsLabel);

    auto *graphics = new QGridLayout();
    graphics->setHorizontalSpacing(5);
    graphics->setVerticalSpacing(5);
    scoreToggle_ = makeButton("BẢNG ĐIỂM", "toggle_score");
    lowerToggle_ = makeButton("LOWER THIRD", "toggle_lower");
    timeoutButton_ = makeButton("TIME OUT", "timeout");
    standbyButton_ = makeButton("STANDBY", "standby");
    resultsButton_ = makeButton("KẾT QUẢ", "results");
    graphics->addWidget(scoreToggle_, 0, 0);
    graphics->addWidget(lowerToggle_, 0, 1);
    graphics->addWidget(timeoutButton_, 1, 0);
    graphics->addWidget(standbyButton_, 1, 1);
    graphics->addWidget(resultsButton_, 2, 0, 1, 2);
    root->addLayout(graphics);

    auto *scoreLabel = sectionLabel("ĐIỂM NHANH");
    scoreLabel->setObjectName("section");
    root->addWidget(scoreLabel);

    auto *scoreRow = new QHBoxLayout();
    scoreRow->setSpacing(5);

    auto makeAccentButton = [](QPushButton *button, const QString &color) {
        auto *box = new QWidget();
        auto *layout = new QVBoxLayout(box);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(2);
        layout->addWidget(button);
        auto *line = new QFrame();
        line->setFixedHeight(2);
        line->setStyleSheet(QString("background:%1; border:0;").arg(color));
        layout->addWidget(line);
        return box;
    };

    rallyAButton_ = makeButton("ĐỘI A +", "rally_a");
    rallyBButton_ = makeButton("ĐỘI B +", "rally_b");
    scoreRow->addWidget(makeAccentButton(rallyAButton_, "#d7b400"));
    scoreRow->addWidget(makeAccentButton(rallyBButton_, "#d91f49"));
    root->addLayout(scoreRow);

    undoButton_ = makeButton("↶  HOÀN TÁC PHA", "undo");
    root->addWidget(undoButton_);
    finishGameButton_ = makeButton("KẾT THÚC GAME", "finish_game");
    root->addWidget(finishGameButton_);

    auto *replayLabel = sectionLabel("REPLAY");
    replayLabel->setObjectName("section");
    root->addWidget(replayLabel);

    auto *replayRow = new QHBoxLayout();
    replayRow->setSpacing(5);
    replayButton_ = makeButton("PHÁT REPLAY", "replay");
    replayButton_->setMinimumHeight(38);
    replaySettingsButton_ = new QPushButton(QString::fromUtf8("⚙"));
    replaySettingsButton_->setCursor(Qt::PointingHandCursor);
    replaySettingsButton_->setToolTip("Cài đặt Replay");
    replaySettingsButton_->setFixedWidth(42);
    replaySettingsButton_->setMinimumHeight(38);
    connect(replaySettingsButton_, &QPushButton::clicked, this, [this]() { openReplaySettings(); });
    replayRow->addWidget(replayButton_, 1);
    replayRow->addWidget(replaySettingsButton_);
    root->addLayout(replayRow);

    recordReplayButton_ = makeButton("GHI REPLAY", "record_replay");
    root->addWidget(recordReplayButton_);

    messageLabel_ = new QLabel();
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

void TenBitDockWidget::openReplaySettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle("Cài đặt Replay");
    dialog.setModal(true);
    dialog.setMinimumWidth(290);

    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    auto *duration = new QComboBox(&dialog);
    for (int sec : {3, 5, 8, 10})
        duration->addItem(QString("%1 giây").arg(sec), sec);
    auto *speed = new QComboBox(&dialog);
    for (int pct : {100, 75, 50, 25})
        speed->addItem(QString("%1%  (%2×)").arg(pct).arg(QString::number(pct / 100.0, 'f', pct == 100 ? 0 : 2)), pct);

    int durationIndex = duration->findData(replayDurationSec_);
    if (durationIndex >= 0)
        duration->setCurrentIndex(durationIndex);
    int speedIndex = speed->findData(replaySpeedPercent_);
    if (speedIndex >= 0)
        speed->setCurrentIndex(speedIndex);

    form->addRow("Đoạn nguồn:", duration);
    form->addRow("Tốc độ phát:", speed);
    layout->addLayout(form);

    auto *hint = new QLabel("Ví dụ: 3 giây ở 50% = khoảng 6 giây phát lại.", &dialog);
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted)
        return;
    QJsonObject args;
    args.insert("durationSec", duration->currentData().toInt());
    args.insert("speedPercent", speed->currentData().toInt());
    sendCommand("set_replay_settings", args);
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
            QTimer::singleShot(5000, messageLabel_, [this]() {
                if (messageLabel_)
                    messageLabel_->hide();
            });
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
    setActive(standbyButton_, takeover == "standby");
    setActive(resultsButton_, takeover == "results");

    const bool gameDone = !match.value("GameWinner").toString().isEmpty();
    const bool matchDone = !match.value("MatchWinner").toString().isEmpty();
    rallyAButton_->setText(teamA + "  +");
    rallyBButton_->setText(teamB + "  +");
    rallyAButton_->setEnabled(pickleball && !gameDone && !matchDone);
    rallyBButton_->setEnabled(pickleball && !gameDone && !matchDone);
    undoButton_->setEnabled(pickleball && state.value("canUndo").toBool());
    finishGameButton_->setEnabled(pickleball && gameDone);
    const bool replayAvailable = state.value("replayAvailable").toBool();
    const bool replayPlaying = state.value("replayPlaying").toBool();
    replayDurationSec_ = qMax(1, state.value("replayDurationSec").toInt(5));
    replaySpeedPercent_ = qMax(1, state.value("replaySpeedPercent").toInt(50));
    replayButton_->setEnabled(replayAvailable);
    replaySettingsButton_->setEnabled(replayAvailable);
    recordReplayButton_->setEnabled(replayAvailable);
    setActive(replayButton_, replayPlaying, "●  PHÁT REPLAY", "PHÁT REPLAY");
    setActive(recordReplayButton_, replayBufferActive, "●  GHI REPLAY", "GHI REPLAY");
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
