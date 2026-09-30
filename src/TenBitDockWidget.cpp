#include "TenBitDockWidget.hpp"

#include <obs-frontend-api.h>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QPushButton>
#include <QStandardPaths>
#include <QSettings>
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

QLabel *sectionLabel(const QString &text)
{
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setBold(true);
    font.setPointSizeF(qMax(8.0, font.pointSizeF() - 1.0));
    label->setFont(font);
    return label;
}

QString matchTeam(const QJsonObject &match, const char *key, const QString &fallback)
{
    const QString value = match.value(QString::fromLatin1(key)).toString().trimmed();
    return value.isEmpty() ? fallback : value;
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

    pollTimer_.setInterval(600);
    connect(&pollTimer_, &QTimer::timeout, this, &TenBitDockWidget::pollStatus);

    ensureConnected();
}

void TenBitDockWidget::buildUI()
{
    setMinimumWidth(318);

    auto *panel = new QFrame(this);
    panel->setObjectName("tenbitPanel");
    panel->setStyleSheet(R"(
        QFrame#tenbitPanel {
            background:#07111d;
            border:1px solid #24364d;
            border-radius:11px;
        }
        QFrame#statusCard, QFrame#card {
            background:#0d1827;
            border:1px solid #20334a;
            border-radius:10px;
        }
        QLabel {
            color:#eef4ff;
            background:transparent;
            border:0;
        }
        QLabel#project {
            color:#f4f7ff;
            font-size:13px;
            font-weight:800;
        }
        QLabel#statusDot {
            font-size:16px;
            font-weight:900;
            padding:0px;
        }
        QLabel#statusDot[state="connected"] { color:#25e58c; }
        QLabel#statusDot[state="waiting"] { color:#f4c84a; }
        QLabel#statusDot[state="offline"] { color:#ff4169; }
        QLabel#sectionTitle {
            color:#f2f6ff;
            font-size:13px;
            font-weight:900;
            padding:0px 0px 6px 12px;
            border-left:4px solid #ff274f;
            border-bottom:1px solid #20334a;
        }
        QLabel#fieldLabel {
            color:#9eacc2;
            font-size:9px;
            font-weight:800;
            padding-top:2px;
        }
        QLabel#hint {
            color:#8f9db2;
            font-size:9px;
            padding:8px 10px;
            background:#111e2f;
            border:1px solid #21364f;
            border-radius:7px;
        }
        QLineEdit, QComboBox {
            color:#f4f7ff;
            background:#172437;
            border:1px solid #31455f;
            border-radius:7px;
            min-height:31px;
            padding:4px 9px;
            font-size:11px;
            font-weight:700;
            selection-background-color:#e11f47;
        }
        QLineEdit:focus, QComboBox:focus {
            border:1px solid #5c7da5;
            background:#1a2940;
        }
        QComboBox::drop-down {
            border:0;
            width:24px;
        }
        QPushButton {
            color:#f1f5fd;
            background:#152235;
            border:1px solid #415672;
            border-radius:8px;
            min-height:39px;
            padding:5px 7px;
            font-size:10px;
            font-weight:900;
        }
        QPushButton:hover {
            background:#1b2c43;
            border-color:#6883a6;
        }
        QPushButton:pressed {
            background:#101b2a;
        }
        QPushButton:disabled {
            color:#5c6879;
            background:#101925;
            border-color:#253244;
        }
        QPushButton[active="true"] {
            color:#ffffff;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #ef1743,stop:1 #a70f31);
            border:1px solid #ff3c62;
        }
        QPushButton#replayPrimary {
            min-height:45px;
            font-size:12px;
        }
        QPushButton#replaySettings {
            min-height:45px;
            max-width:46px;
            font-size:16px;
        }
        QPushButton#recordReplay {
            min-height:39px;
        }
    )");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(panel);

    auto *root = new QVBoxLayout(panel);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(8);

    auto *statusCard = new QFrame(panel);
    statusCard->setObjectName("statusCard");
    auto *projectRow = new QHBoxLayout(statusCard);
    projectRow->setContentsMargins(10, 8, 9, 8);
    projectRow->setSpacing(7);
    projectLabel_ = new QLabel("Chưa mở Project");
    projectLabel_->setObjectName("project");
    connectionLabel_ = new QLabel("●");
    connectionLabel_->setObjectName("statusDot");
    connectionLabel_->setAlignment(Qt::AlignCenter);
    connectionLabel_->setFixedWidth(19);
    projectRow->addWidget(projectLabel_, 1);
    projectRow->addWidget(connectionLabel_);
    root->addWidget(statusCard);

    auto *infoCard = new QFrame(panel);
    infoCard->setObjectName("card");
    auto *info = new QVBoxLayout(infoCard);
    info->setContentsMargins(10, 10, 10, 10);
    info->setSpacing(6);

    auto *infoTitle = new QLabel("THÔNG TIN TRẬN");
    infoTitle->setObjectName("sectionTitle");
    info->addWidget(infoTitle);

    auto *eventLabel = new QLabel("GIẢI ĐẤU");
    eventLabel->setObjectName("fieldLabel");
    info->addWidget(eventLabel);
    eventEdit_ = new QLineEdit();
    eventEdit_->setPlaceholderText("Tên giải");
    info->addWidget(eventEdit_);

    auto *roundCourtLabels = new QHBoxLayout();
    roundCourtLabels->setSpacing(6);
    auto *roundLabel = new QLabel("VÒNG ĐẤU");
    auto *courtLabel = new QLabel("SÂN");
    roundLabel->setObjectName("fieldLabel");
    courtLabel->setObjectName("fieldLabel");
    roundCourtLabels->addWidget(roundLabel, 1);
    roundCourtLabels->addWidget(courtLabel, 1);
    info->addLayout(roundCourtLabels);

    auto *roundCourt = new QHBoxLayout();
    roundCourt->setSpacing(6);
    roundEdit_ = new QLineEdit();
    roundEdit_->setPlaceholderText("Vòng đấu");
    courtEdit_ = new QLineEdit();
    courtEdit_->setPlaceholderText("Sân đấu");
    roundCourt->addWidget(roundEdit_, 1);
    roundCourt->addWidget(courtEdit_, 1);
    info->addLayout(roundCourt);

    auto *formatLabels = new QHBoxLayout();
    formatLabels->setSpacing(6);
    auto *formatLabel = new QLabel("THỂ THỨC");
    auto *pointsLabel = new QLabel("ĐIỂM KẾT THÚC");
    formatLabel->setObjectName("fieldLabel");
    pointsLabel->setObjectName("fieldLabel");
    formatLabels->addWidget(formatLabel, 1);
    formatLabels->addWidget(pointsLabel, 1);
    info->addLayout(formatLabels);

    auto *formatRow = new QHBoxLayout();
    formatRow->setSpacing(6);
    formatCombo_ = new QComboBox();
    formatCombo_->addItem("Đơn", "singles");
    formatCombo_->addItem("Đôi", "doubles");
    pointsCombo_ = new QComboBox();
    pointsCombo_->addItem("Chạm 11", 11);
    pointsCombo_->addItem("Chạm 15", 15);
    pointsCombo_->addItem("Chạm 21", 21);
    formatRow->addWidget(formatCombo_, 1);
    formatRow->addWidget(pointsCombo_, 1);
    info->addLayout(formatRow);

    matchStatusLabel_ = new QLabel("--");
    matchStatusLabel_->setObjectName("hint");
    matchStatusLabel_->setWordWrap(true);
    info->addWidget(matchStatusLabel_);
    root->addWidget(infoCard);

    connect(eventEdit_, &QLineEdit::editingFinished, this, [this]() { sendMatchSetup(); });
    connect(roundEdit_, &QLineEdit::editingFinished, this, [this]() { sendMatchSetup(); });
    connect(courtEdit_, &QLineEdit::editingFinished, this, [this]() { sendMatchSetup(); });
    connect(formatCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!updatingUI_)
            sendMatchSetup();
    });
    connect(pointsCombo_, &QComboBox::currentIndexChanged, this, [this](int) {
        if (!updatingUI_)
            sendMatchSetup();
    });

    auto *graphicsCard = new QFrame(panel);
    graphicsCard->setObjectName("card");
    auto *graphicsRoot = new QVBoxLayout(graphicsCard);
    graphicsRoot->setContentsMargins(10, 10, 10, 10);
    graphicsRoot->setSpacing(7);
    auto *graphicsTitle = new QLabel("ĐỒ HỌA");
    graphicsTitle->setObjectName("sectionTitle");
    graphicsRoot->addWidget(graphicsTitle);

    auto *graphics = new QGridLayout();
    graphics->setHorizontalSpacing(7);
    graphics->setVerticalSpacing(7);
    scoreToggle_ = makeButton("BẢNG ĐIỂM", "toggle_score");
    adButton_ = makeButton("QUẢNG CÁO", "toggle_ad");
    teamsButton_ = makeButton("HIỆN TÊN", "toggle_teams");
    introButton_ = makeButton("INTRO", "intro");
    timeoutButton_ = makeButton("TIME OUT", "timeout");
    standbyButton_ = makeButton("STANDBY", "standby");
    breakButton_ = makeButton("BREAK", "break");
    resultsButton_ = makeButton("KẾT QUẢ", "results");
    graphics->addWidget(scoreToggle_, 0, 0);
    graphics->addWidget(adButton_, 0, 1);
    graphics->addWidget(teamsButton_, 1, 0);
    graphics->addWidget(introButton_, 1, 1);
    graphics->addWidget(timeoutButton_, 2, 0);
    graphics->addWidget(standbyButton_, 2, 1);
    graphics->addWidget(breakButton_, 3, 0);
    graphics->addWidget(resultsButton_, 3, 1);
    graphicsRoot->addLayout(graphics);
    root->addWidget(graphicsCard);

    auto *replayCard = new QFrame(panel);
    replayCard->setObjectName("card");
    auto *replayRoot = new QVBoxLayout(replayCard);
    replayRoot->setContentsMargins(10, 10, 10, 10);
    replayRoot->setSpacing(7);
    auto *replayTitle = new QLabel("REPLAY");
    replayTitle->setObjectName("sectionTitle");
    replayRoot->addWidget(replayTitle);

    auto *replayRow = new QHBoxLayout();
    replayRow->setSpacing(7);
    replayButton_ = makeButton("PHÁT REPLAY", "replay");
    replayButton_->setObjectName("replayPrimary");
    replaySettingsButton_ = new QPushButton("⚙");
    replaySettingsButton_->setObjectName("replaySettings");
    replaySettingsButton_->setToolTip("Cài đặt Replay");
    connect(replaySettingsButton_, &QPushButton::clicked, this, [this]() { openReplaySettings(); });
    replayRow->addWidget(replayButton_, 1);
    replayRow->addWidget(replaySettingsButton_);
    replayRoot->addLayout(replayRow);

    recordReplayButton_ = makeButton("GHI REPLAY", "record_replay");
    recordReplayButton_->setObjectName("recordReplay");
    replayRoot->addWidget(recordReplayButton_);
    root->addWidget(replayCard);

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

void TenBitDockWidget::sendMatchSetup()
{
    if (updatingUI_)
        return;
    QJsonObject args;
    args.insert("event", eventEdit_->text().trimmed());
    args.insert("round", roundEdit_->text().trimmed());
    args.insert("court", courtEdit_->text().trimmed());
    args.insert("format", formatCombo_->currentData().toString());
    args.insert("points", pointsCombo_->currentData().toInt());
    sendCommand("set_match_setup", args);
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
    const QString teamA = matchTeam(match, "TeamA", "ĐỘI A");
    const QString teamB = matchTeam(match, "TeamB", "ĐỘI B");
    const int scoreA = match.value("ScoreA").toInt();
    const int scoreB = match.value("ScoreB").toInt();
    const int gamesA = match.value("GamesA").toInt();
    const int gamesB = match.value("GamesB").toInt();

    updatingUI_ = true;
    if (!eventEdit_->hasFocus())
        eventEdit_->setText(match.value("Event").toString());
    if (!roundEdit_->hasFocus())
        roundEdit_->setText(match.value("Round").toString());
    if (!courtEdit_->hasFocus())
        courtEdit_->setText(match.value("Court").toString());
    const int formatIndex = formatCombo_->findData(match.value("Format").toString("doubles"));
    if (formatIndex >= 0)
        formatCombo_->setCurrentIndex(formatIndex);
    const int pointIndex = pointsCombo_->findData(match.value("PointsToWin").toInt(11));
    if (pointIndex >= 0)
        pointsCombo_->setCurrentIndex(pointIndex);
    updatingUI_ = false;

    const QString formatText = match.value("Format").toString() == "singles" ? "Đơn" : "Đôi";
    matchStatusLabel_->setText(QString("%1  %2–%3  %4   •   Game %5–%6   •   Chạm %7")
        .arg(teamA).arg(scoreA).arg(scoreB).arg(teamB).arg(gamesA).arg(gamesB).arg(match.value("PointsToWin").toInt(11))
        + "   •   " + formatText);

    setActive(scoreToggle_, graphics.value("ProgramScore").toBool(), "●  BẢNG ĐIỂM", "BẢNG ĐIỂM");
    setActive(adButton_, state.value("adVisible").toBool(), "●  QUẢNG CÁO", "QUẢNG CÁO");
    setActive(teamsButton_, graphics.value("ProgramTeams").toBool(), "●  HIỆN TÊN", "HIỆN TÊN");

    const QString takeover = graphics.value("ProgramTakeover").toString();
    setActive(introButton_, takeover == "match");
    setActive(timeoutButton_, takeover == "timeout");
    setActive(standbyButton_, takeover == "standby");
    setActive(breakButton_, takeover == "break");
    setActive(resultsButton_, takeover == "results");

    const bool replayAvailable = state.value("replayAvailable").toBool();
    const bool replayBufferActive = state.value("replayBufferActive").toBool();
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
    scoreDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    if (controlDock->isFloating()) {
        mainWindow->addDockWidget(Qt::RightDockWidgetArea, controlDock);
        controlDock->setFloating(false);
    }
    if (scoreDock->isFloating()) {
        auto *controlsDock = mainWindow->findChild<QDockWidget *>(QStringLiteral("controlsDock"), Qt::FindChildrenRecursively);
        if (controlsDock)
            mainWindow->splitDockWidget(controlsDock, scoreDock, Qt::Horizontal);
        else
            mainWindow->addDockWidget(Qt::BottomDockWidgetArea, scoreDock);
        scoreDock->setFloating(false);
    }

    controlDock->toggleViewAction()->setChecked(true);
    scoreDock->toggleViewAction()->setChecked(true);
    controlDock->show();
    scoreDock->show();
    controlDock->raise();
    scoreDock->raise();
    return controlDock->isVisible() && scoreDock->isVisible();
}
