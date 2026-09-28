#include "TenBitDockWidget.hpp"

#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
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
    return match.value(side == "A" ? "TeamA" : "TeamB").toString(side == "A" ? "ĐỘI A" : "ĐỘI B");
}
}

TenBitDockWidget::TenBitDockWidget(QWidget *parent) : QWidget(parent)
{
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
        QWidget { background: #0f131b; color: #f3f5f8; font-size: 12px; }
        QLabel#title { font-size: 20px; font-weight: 700; }
        QLabel#muted { color: #93a0b4; }
        QLabel#score { font-size: 22px; font-weight: 700; }
        QPushButton { background: #202632; border: 1px solid #394252; padding: 8px 7px; font-weight: 600; }
        QPushButton:hover { background: #2a3240; }
        QPushButton:pressed { background: #343e4f; }
        QPushButton[active="true"] { background: #d51e42; border-color: #ef3157; color: white; }
        QPushButton:disabled { color: #667085; background: #171b23; }
    )");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 12, 12, 12);
    root->setSpacing(8);

    auto *title = new QLabel("10BIT BROADCAST");
    title->setObjectName("title");
    root->addWidget(title);

    connectionLabel_ = new QLabel("● Đang tìm 10BIT Broadcast...");
    connectionLabel_->setObjectName("muted");
    root->addWidget(connectionLabel_);

    projectLabel_ = new QLabel("Chưa mở Project");
    projectLabel_->setObjectName("muted");
    root->addWidget(projectLabel_);

    scoreLabel_ = new QLabel("0   –   0");
    scoreLabel_->setObjectName("score");
    scoreLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(scoreLabel_);

    serveLabel_ = new QLabel("Mở Project Pickleball trong 10BIT Broadcast");
    serveLabel_->setWordWrap(true);
    serveLabel_->setAlignment(Qt::AlignCenter);
    serveLabel_->setObjectName("muted");
    root->addWidget(serveLabel_);

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

    auto *scoreRow = new QHBoxLayout();
    rallyAButton_ = makeButton("ĐỘI A +", "rally_a");
    rallyBButton_ = makeButton("ĐỘI B +", "rally_b");
    scoreRow->addWidget(rallyAButton_);
    scoreRow->addWidget(rallyBButton_);
    root->addLayout(scoreRow);

    undoButton_ = makeButton("↶ HOÀN TÁC PHA", "undo");
    root->addWidget(undoButton_);

    saveReplayButton_ = makeButton("LƯU REPLAY", "save_replay");
    root->addWidget(saveReplayButton_);

    messageLabel_ = new QLabel("Dock native • không Browser Dock • không URL");
    messageLabel_->setObjectName("muted");
    messageLabel_->setWordWrap(true);
    root->addWidget(messageLabel_);
    root->addStretch(1);
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
        setConnectionText("● Mở 10BIT Broadcast để kết nối", false);
        return;
    }
    connectToRuntime();
}

void TenBitDockWidget::connectToRuntime()
{
    socket_.abort();
    socket_.connectToHost(host_, port_);
    setConnectionText("● Đang kết nối 10BIT Broadcast...", false);
}

void TenBitDockWidget::onConnected()
{
    setConnectionText("● CONNECTED", true);
    pollTimer_.start();
    sendCommand("status");
}

void TenBitDockWidget::onDisconnected()
{
    pollTimer_.stop();
    setConnectionText("● Mất kết nối • đang thử lại", false);
}

void TenBitDockWidget::onSocketError()
{
    pollTimer_.stop();
    setConnectionText("● Chưa kết nối 10BIT Broadcast", false);
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
    socket_.write("\\n");
}

void TenBitDockWidget::onReadyRead()
{
    readBuffer_.append(socket_.readAll());
    while (true) {
        const qsizetype pos = readBuffer_.indexOf('\\n');
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
            continue;
        }
        if (obj.value("state").isObject())
            applyState(obj.value("state").toObject());
    }
}

void TenBitDockWidget::applyState(const QJsonObject &state)
{
    const QString sport = state.value("sportModule").toString();
    const bool pickleball = sport == "pickleball";
    projectLabel_->setText(state.value("projectName").toString("Chưa mở Project") + (pickleball ? " • Pickleball" : ""));

    const auto match = state.value("match").toObject();
    const auto graphics = state.value("graphics").toObject();
    const QString teamA = sideTeam(match, "A");
    const QString teamB = sideTeam(match, "B");
    const int scoreA = match.value("ScoreA").toInt();
    const int scoreB = match.value("ScoreB").toInt();
    const int gamesA = match.value("GamesA").toInt();
    const int gamesB = match.value("GamesB").toInt();
    scoreLabel_->setText(QString("%1  %2  –  %3  %4").arg(teamA).arg(scoreA).arg(scoreB).arg(teamB));
    serveLabel_->setText(QString("Game %1–%2 • %3").arg(gamesA).arg(gamesB).arg(state.value("scoreCall").toString()));

    setActive(scoreToggle_, graphics.value("ProgramScore").toBool(), "● BẢNG ĐIỂM", "BẢNG ĐIỂM");
    setActive(lowerToggle_, graphics.value("ProgramLower").toBool(), "● LOWER THIRD", "LOWER THIRD");
    const QString takeover = graphics.value("ProgramTakeover").toString();
    setActive(timeoutButton_, takeover == "timeout");
    setActive(replayButton_, takeover == "replay");
    setActive(standbyButton_, takeover == "standby");
    setActive(resultsButton_, takeover == "results");

    rallyAButton_->setText(teamA + " +");
    rallyBButton_->setText(teamB + " +");
    rallyAButton_->setEnabled(pickleball);
    rallyBButton_->setEnabled(pickleball);
    undoButton_->setEnabled(pickleball && state.value("canUndo").toBool());
    saveReplayButton_->setEnabled(state.value("replayAvailable").toBool());
    messageLabel_->setText(pickleball ? "Điều khiển trực tiếp 10BIT Core" : "Dock scoring hiện hỗ trợ Pickleball");
}

void TenBitDockWidget::setConnectionText(const QString &text, bool online)
{
    connectionLabel_->setText(text);
    connectionLabel_->setStyleSheet(online ? "color:#53e38a; font-weight:700;" : "color:#f0b93b;");
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


void TenBitDockWidget::revealDock()
{
    QWidget *w = this;
    while (w) {
        if (auto *dock = qobject_cast<QDockWidget *>(w)) {
            dock->show();
            dock->raise();
            dock->activateWindow();
            return;
        }
        w = w->parentWidget();
    }
    show();
    raise();
    activateWindow();
}
