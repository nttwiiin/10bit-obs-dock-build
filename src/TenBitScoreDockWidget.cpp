#include "TenBitScoreDockWidget.hpp"

#include <QDir>
#include <QHBoxLayout>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
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

QString teamName(const QJsonObject &match, const char *key, const QString &fallback)
{
    const QString v = match.value(QString::fromLatin1(key)).toString().trimmed();
    return v.isEmpty() ? fallback : v;
}
}

TenBitScoreDockWidget::TenBitScoreDockWidget(QWidget *parent) : QWidget(parent)
{
    buildUI();

    connect(&socket_, &QTcpSocket::connected, this, &TenBitScoreDockWidget::onConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &TenBitScoreDockWidget::onDisconnected);
    connect(&socket_, &QTcpSocket::readyRead, this, &TenBitScoreDockWidget::onReadyRead);
    connect(&socket_, &QTcpSocket::errorOccurred, this, &TenBitScoreDockWidget::onSocketError);

    reconnectTimer_.setInterval(2000);
    connect(&reconnectTimer_, &QTimer::timeout, this, &TenBitScoreDockWidget::ensureConnected);
    reconnectTimer_.start();

    pollTimer_.setInterval(500);
    connect(&pollTimer_, &QTimer::timeout, this, &TenBitScoreDockWidget::pollStatus);

    ensureConnected();
}

void TenBitScoreDockWidget::buildUI()
{
    setMinimumWidth(260);
    setStyleSheet(R"(
        QWidget { background: #111722; color: #f4f6f9; font-size: 12px; }
        QLabel#title { font-size: 16px; font-weight: 800; }
        QLabel#muted { color: #96a1b4; }
        QLabel#score { font-size: 27px; font-weight: 800; padding: 3px; }
        QPushButton { background: #242c39; border: 1px solid #3c4657; padding: 10px 8px; font-weight: 700; }
        QPushButton:hover { background: #303a49; }
        QPushButton:pressed { background: #3a4658; }
        QPushButton:disabled { color: #657084; background: #171d27; }
    )");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(10, 8, 10, 8);
    root->setSpacing(5);

    auto *title = new QLabel("10BIT SCORE");
    title->setObjectName("title");
    root->addWidget(title);

    connectionLabel_ = new QLabel("● Đang kết nối...");
    connectionLabel_->setObjectName("muted");
    root->addWidget(connectionLabel_);

    projectLabel_ = new QLabel("Chưa mở Project");
    projectLabel_->setObjectName("muted");
    root->addWidget(projectLabel_);

    scoreLabel_ = new QLabel("0  –  0");
    scoreLabel_->setObjectName("score");
    scoreLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(scoreLabel_);

    callLabel_ = new QLabel("Game 0–0");
    callLabel_->setObjectName("muted");
    callLabel_->setAlignment(Qt::AlignCenter);
    root->addWidget(callLabel_);

    auto *scoreRow = new QHBoxLayout();
    scoreRow->setSpacing(6);
    rallyAButton_ = new QPushButton("ĐỘI A +");
    rallyBButton_ = new QPushButton("ĐỘI B +");
    connect(rallyAButton_, &QPushButton::clicked, this, [this]() { sendCommand("rally_a"); });
    connect(rallyBButton_, &QPushButton::clicked, this, [this]() { sendCommand("rally_b"); });
    scoreRow->addWidget(rallyAButton_);
    scoreRow->addWidget(rallyBButton_);
    root->addLayout(scoreRow);

    undoButton_ = new QPushButton("↶ HOÀN TÁC");
    connect(undoButton_, &QPushButton::clicked, this, [this]() { sendCommand("undo"); });
    root->addWidget(undoButton_);
}

bool TenBitScoreDockWidget::loadRuntime()
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

void TenBitScoreDockWidget::ensureConnected()
{
    if (socket_.state() == QAbstractSocket::ConnectedState || socket_.state() == QAbstractSocket::ConnectingState)
        return;
    if (!loadRuntime()) {
        setConnectionText("● Mở 10BIT Broadcast", false);
        return;
    }
    connectToRuntime();
}

void TenBitScoreDockWidget::connectToRuntime()
{
    socket_.abort();
    socket_.connectToHost(host_, port_);
    setConnectionText("● Đang kết nối...", false);
}

void TenBitScoreDockWidget::onConnected()
{
    setConnectionText("● CONNECTED", true);
    pollTimer_.start();
    sendCommand("status");
}

void TenBitScoreDockWidget::onDisconnected()
{
    pollTimer_.stop();
    setConnectionText("● Mất kết nối • đang thử lại", false);
}

void TenBitScoreDockWidget::onSocketError()
{
    pollTimer_.stop();
    setConnectionText("● Chưa kết nối Core", false);
}

void TenBitScoreDockWidget::pollStatus()
{
    if (socket_.state() == QAbstractSocket::ConnectedState)
        sendCommand("status");
}

void TenBitScoreDockWidget::sendCommand(const QString &command)
{
    if (socket_.state() != QAbstractSocket::ConnectedState) {
        ensureConnected();
        return;
    }
    QJsonObject req;
    req.insert("id", QString::number(++requestId_));
    req.insert("token", token_);
    req.insert("command", command);
    socket_.write(QJsonDocument(req).toJson(QJsonDocument::Compact));
    socket_.write("\n");
}

void TenBitScoreDockWidget::onReadyRead()
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
        if (!obj.value("ok").toBool())
            continue;
        if (obj.value("state").isObject())
            applyState(obj.value("state").toObject());
    }
}

void TenBitScoreDockWidget::applyState(const QJsonObject &state)
{
    const QString sport = state.value("sportModule").toString();
    const bool pickleball = sport == "pickleball";
    const auto match = state.value("match").toObject();

    const QString a = teamName(match, "TeamA", "ĐỘI A");
    const QString b = teamName(match, "TeamB", "ĐỘI B");
    const int scoreA = match.value("ScoreA").toInt();
    const int scoreB = match.value("ScoreB").toInt();
    const int gamesA = match.value("GamesA").toInt();
    const int gamesB = match.value("GamesB").toInt();

    projectLabel_->setText(state.value("projectName").toString("Chưa mở Project") + (pickleball ? " • Pickleball" : ""));
    scoreLabel_->setText(QString("%1   %2 – %3   %4").arg(a).arg(scoreA).arg(scoreB).arg(b));
    const QString call = state.value("scoreCall").toString();
    callLabel_->setText(QString("Game %1–%2%3").arg(gamesA).arg(gamesB).arg(call.isEmpty() ? QString() : " • " + call));
    rallyAButton_->setText(a + " +");
    rallyBButton_->setText(b + " +");
    rallyAButton_->setEnabled(pickleball);
    rallyBButton_->setEnabled(pickleball);
    undoButton_->setEnabled(pickleball && state.value("canUndo").toBool());
}

void TenBitScoreDockWidget::setConnectionText(const QString &text, bool online)
{
    connectionLabel_->setText(text);
    connectionLabel_->setStyleSheet(online ? "color:#52e28a; font-weight:700;" : "color:#e9b949;");
}
