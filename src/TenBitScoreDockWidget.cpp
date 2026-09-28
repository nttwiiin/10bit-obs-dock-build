#include "TenBitScoreDockWidget.hpp"

#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
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

QString teamName(const QJsonObject &match, const char *key, const QString &fallback)
{
    const QString value = match.value(QString::fromLatin1(key)).toString().trimmed();
    return value.isEmpty() ? fallback : value;
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

TenBitScoreDockWidget::TenBitScoreDockWidget(QWidget *parent) : TenBitObsDockContent(parent)
{
    setObjectName("scoreDockRoot");
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
    setMinimumWidth(300);

    auto *panel = new QFrame(this);
    panel->setObjectName("tenbitScorePanel");
    panel->setStyleSheet(R"(
        QFrame#tenbitScorePanel { background: transparent; }
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
            padding-top: 1px;
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
            font-size: 31px;
            font-weight: 900;
        }
        QLabel#dash {
            color: palette(mid);
            font-size: 20px;
            font-weight: 800;
        }
        QLabel#call {
            color: palette(mid);
            font-size: 10px;
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
    auto *grid = new QGridLayout(scoreCard);
    grid->setContentsMargins(12, 8, 12, 7);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(1);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(2, 1);

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
    dash->setObjectName("dash");
    dash->setAlignment(Qt::AlignCenter);

    callLabel_ = new QLabel("Game 0–0");
    callLabel_->setObjectName("call");
    callLabel_->setAlignment(Qt::AlignCenter);
    callLabel_->setWordWrap(true);

    grid->addWidget(teamALabel_, 0, 0);
    grid->addWidget(teamBLabel_, 0, 2);
    grid->addWidget(scoreAValue_, 1, 0);
    grid->addWidget(dash, 1, 1);
    grid->addWidget(scoreBValue_, 1, 2);
    grid->addWidget(callLabel_, 2, 0, 1, 3);
    root->addWidget(scoreCard);

    auto *section = sectionLabel("CỘNG ĐIỂM");
    section->setObjectName("section");
    root->addWidget(section);

    auto *scoreRow = new QHBoxLayout();
    scoreRow->setSpacing(6);

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

    rallyAButton_ = new QPushButton("ĐỘI A +");
    rallyBButton_ = new QPushButton("ĐỘI B +");
    QFont actionFont = rallyAButton_->font();
    actionFont.setBold(true);
    rallyAButton_->setFont(actionFont);
    rallyBButton_->setFont(actionFont);
    connect(rallyAButton_, &QPushButton::clicked, this, [this]() { sendCommand("rally_a"); });
    connect(rallyBButton_, &QPushButton::clicked, this, [this]() { sendCommand("rally_b"); });
    scoreRow->addWidget(makeAccentButton(rallyAButton_, "#d7b400"));
    scoreRow->addWidget(makeAccentButton(rallyBButton_, "#d91f49"));
    root->addLayout(scoreRow);

    undoButton_ = new QPushButton("↶  HOÀN TÁC");
    connect(undoButton_, &QPushButton::clicked, this, [this]() { sendCommand("undo"); });
    root->addWidget(undoButton_);

    root->addStretch(1);

    setConnectionState("waiting", "Đang chờ 10BIT Broadcast Core");
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
        setConnectionState("waiting", "Đang chờ ứng dụng 10BIT Broadcast");
        return;
    }
    connectToRuntime();
}

void TenBitScoreDockWidget::connectToRuntime()
{
    socket_.abort();
    socket_.connectToHost(host_, port_);
    setConnectionState("waiting", "Đang kết nối 10BIT Broadcast Core");
}

void TenBitScoreDockWidget::onConnected()
{
    setConnectionState("connected", "Đã kết nối 10BIT Broadcast Core");
    pollTimer_.start();
    sendCommand("status");
}

void TenBitScoreDockWidget::onDisconnected()
{
    pollTimer_.stop();
    setConnectionState("offline", "Mất kết nối 10BIT Broadcast Core");
}

void TenBitScoreDockWidget::onSocketError()
{
    pollTimer_.stop();
    setConnectionState("offline", "Không kết nối được 10BIT Broadcast Core");
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

    const QString teamA = teamName(match, "TeamA", "ĐỘI A");
    const QString teamB = teamName(match, "TeamB", "ĐỘI B");
    const int scoreA = match.value("ScoreA").toInt();
    const int scoreB = match.value("ScoreB").toInt();
    const int gamesA = match.value("GamesA").toInt();
    const int gamesB = match.value("GamesB").toInt();

    projectLabel_->setText(state.value("projectName").toString("Chưa mở Project") + (pickleball ? "  •  Pickleball" : ""));

    const QFontMetrics fm(teamALabel_->font());
    teamALabel_->setText(fm.elidedText(teamA, Qt::ElideRight, 125));
    teamBLabel_->setText(fm.elidedText(teamB, Qt::ElideRight, 125));
    teamALabel_->setToolTip(teamA);
    teamBLabel_->setToolTip(teamB);
    scoreAValue_->setText(QString::number(scoreA));
    scoreBValue_->setText(QString::number(scoreB));

    const QString call = state.value("scoreCall").toString().trimmed();
    callLabel_->setText(QString("Game %1–%2%3").arg(gamesA).arg(gamesB)
        .arg(call.isEmpty() ? QString() : "  •  " + call));

    rallyAButton_->setText(teamA + "  +");
    rallyBButton_->setText(teamB + "  +");
    rallyAButton_->setEnabled(pickleball);
    rallyBButton_->setEnabled(pickleball);
    undoButton_->setEnabled(pickleball && state.value("canUndo").toBool());
}

void TenBitScoreDockWidget::setConnectionState(const QString &state, const QString &tooltip)
{
    connectionLabel_->setText("●");
    connectionLabel_->setProperty("state", state);
    connectionLabel_->setToolTip(tooltip);
    connectionLabel_->style()->unpolish(connectionLabel_);
    connectionLabel_->style()->polish(connectionLabel_);
}
