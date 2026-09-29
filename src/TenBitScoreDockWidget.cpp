#include "TenBitScoreDockWidget.hpp"

#include <QButtonGroup>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
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

    pollTimer_.setInterval(450);
    connect(&pollTimer_, &QTimer::timeout, this, &TenBitScoreDockWidget::pollStatus);

    ensureConnected();
}

void TenBitScoreDockWidget::buildUI()
{
    setMinimumWidth(360);

    auto *panel = new QFrame(this);
    panel->setObjectName("tenbitScorePanel");
    panel->setStyleSheet(R"(
        QFrame#tenbitScorePanel { background: transparent; }
        QLabel#statusDot { font-size:16px; font-weight:900; padding:0px; }
        QLabel#statusDot[state="connected"] { color:#43e58a; }
        QLabel#statusDot[state="waiting"] { color:#f2c94c; }
        QLabel#statusDot[state="offline"] { color:#ff5a6d; }
        QFrame#scoreCard { background:palette(base); border:1px solid palette(mid); border-radius:6px; }
        QLabel#teamName { font-size:9px; font-weight:800; }
        QLabel#scoreValue { font-size:26px; font-weight:1000; }
        QLabel#dash { color:palette(mid); font-size:18px; font-weight:900; }
        QLabel#call { color:palette(mid); font-size:9px; }
        QLabel#winnerLabel { color:palette(mid); font-size:9px; font-weight:900; }
        QLineEdit { min-height:25px; padding:3px 7px; }
        QPushButton { min-height:27px; }
        QRadioButton { min-height:24px; font-size:9px; font-weight:850; }
    )");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(panel);

    auto *root = new QVBoxLayout(panel);
    root->setContentsMargins(7, 5, 7, 6);
    root->setSpacing(4);

    auto *projectRow = new QHBoxLayout();
    projectLabel_ = new QLabel("Chưa mở Project");
    QFont projectFont = projectLabel_->font();
    projectFont.setPointSizeF(qMax(8.0, projectFont.pointSizeF() - 1.0));
    projectLabel_->setFont(projectFont);
    connectionLabel_ = new QLabel("●");
    connectionLabel_->setObjectName("statusDot");
    connectionLabel_->setAlignment(Qt::AlignCenter);
    connectionLabel_->setFixedWidth(18);
    projectRow->addWidget(projectLabel_, 1);
    projectRow->addWidget(connectionLabel_);
    root->addLayout(projectRow);

    auto *teamRow = new QHBoxLayout();
    teamRow->setSpacing(5);
    teamAEdit_ = new QLineEdit();
    teamBEdit_ = new QLineEdit();
    teamAEdit_->setPlaceholderText("Đội A");
    teamBEdit_->setPlaceholderText("Đội B");
    saveTeamsButton_ = new QPushButton("LƯU");
    saveTeamsButton_->setFixedWidth(50);
    saveTeamsButton_->setToolTip("Lưu tên hai đội");
    connect(saveTeamsButton_, &QPushButton::clicked, this, &TenBitScoreDockWidget::sendTeamNames);
    teamRow->addWidget(teamAEdit_, 1);
    teamRow->addWidget(teamBEdit_, 1);
    teamRow->addWidget(saveTeamsButton_);
    root->addLayout(teamRow);

    auto *scoreCard = new QFrame();
    scoreCard->setObjectName("scoreCard");
    auto *grid = new QGridLayout(scoreCard);
    grid->setContentsMargins(10, 5, 10, 5);
    grid->setHorizontalSpacing(7);
    grid->setVerticalSpacing(0);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(4, 1);

    teamALabel_ = new QLabel("ĐỘI A");
    teamALabel_->setObjectName("teamName");
    teamALabel_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    teamBLabel_ = new QLabel("ĐỘI B");
    teamBLabel_->setObjectName("teamName");
    teamBLabel_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
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

    grid->addWidget(teamALabel_, 0, 0);
    grid->addWidget(scoreAValue_, 0, 1);
    grid->addWidget(dash, 0, 2);
    grid->addWidget(scoreBValue_, 0, 3);
    grid->addWidget(teamBLabel_, 0, 4);
    grid->addWidget(callLabel_, 1, 0, 1, 5);
    root->addWidget(scoreCard);

    auto *scoreRow = new QHBoxLayout();
    scoreRow->setSpacing(5);
    rallyAButton_ = new QPushButton("ĐỘI A +");
    rallyBButton_ = new QPushButton("ĐỘI B +");
    QFont actionFont = rallyAButton_->font();
    actionFont.setBold(true);
    rallyAButton_->setFont(actionFont);
    rallyBButton_->setFont(actionFont);
    connect(rallyAButton_, &QPushButton::clicked, this, [this]() { sendCommand("rally_a"); });
    connect(rallyBButton_, &QPushButton::clicked, this, [this]() { sendCommand("rally_b"); });
    rallyAButton_->setStyleSheet("border-bottom:2px solid #d7b400;");
    rallyBButton_->setStyleSheet("border-bottom:2px solid #d91f49;");
    scoreRow->addWidget(rallyAButton_);
    scoreRow->addWidget(rallyBButton_);
    root->addLayout(scoreRow);

    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(5);
    undoButton_ = new QPushButton("↶ HOÀN TÁC");
    swapButton_ = new QPushButton("⇄ ĐỔI VỊ TRÍ");
    finishGameButton_ = new QPushButton("KẾT THÚC GAME");
    connect(undoButton_, &QPushButton::clicked, this, [this]() { sendCommand("undo"); });
    connect(swapButton_, &QPushButton::clicked, this, [this]() { sendCommand("swap_sides"); });
    connect(finishGameButton_, &QPushButton::clicked, this, [this]() { sendCommand("finish_game"); });
    actionRow->addWidget(undoButton_);
    actionRow->addWidget(swapButton_);
    actionRow->addWidget(finishGameButton_);
    root->addLayout(actionRow);

    auto *winnerRow = new QHBoxLayout();
    winnerRow->setSpacing(7);
    auto *winnerLabel = new QLabel("THẮNG:");
    winnerLabel->setObjectName("winnerLabel");
    winnerAButton_ = new QRadioButton("ĐỘI A");
    winnerBButton_ = new QRadioButton("ĐỘI B");
    winnerGroup_ = new QButtonGroup(this);
    winnerGroup_->setExclusive(true);
    winnerGroup_->addButton(winnerAButton_);
    winnerGroup_->addButton(winnerBButton_);
    clearWinnerButton_ = new QPushButton("BỎ CHỌN");
    clearWinnerButton_->setFixedWidth(82);
    clearWinnerButton_->setMinimumHeight(26);
    clearWinnerButton_->setToolTip("Bỏ lựa chọn đội thắng thủ công");
    winnerRow->addWidget(winnerLabel);
    winnerRow->addWidget(winnerAButton_, 1);
    winnerRow->addWidget(winnerBButton_, 1);
    winnerRow->addWidget(clearWinnerButton_);
    root->addLayout(winnerRow);

    connect(winnerAButton_, &QRadioButton::clicked, this, [this]() {
        QJsonObject args; args.insert("side", "A"); sendCommand("set_result_winner", args);
    });
    connect(winnerBButton_, &QRadioButton::clicked, this, [this]() {
        QJsonObject args; args.insert("side", "B"); sendCommand("set_result_winner", args);
    });
    connect(clearWinnerButton_, &QPushButton::clicked, this, [this]() {
        QJsonObject args; args.insert("side", ""); sendCommand("set_result_winner", args);
    });

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

void TenBitScoreDockWidget::sendCommand(const QString &command, const QJsonObject &args)
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

void TenBitScoreDockWidget::sendTeamNames()
{
    QJsonObject args;
    args.insert("teamA", teamAEdit_->text().trimmed());
    args.insert("teamB", teamBEdit_->text().trimmed());
    sendCommand("set_team_names", args);
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
    const bool pickleball = state.value("sportModule").toString() == "pickleball";
    projectLabel_->setText(state.value("projectName").toString("Chưa mở Project") + (pickleball ? "  •  Pickleball" : ""));
    const auto match = state.value("match").toObject();
    const QString teamA = teamName(match, "TeamA", "ĐỘI A");
    const QString teamB = teamName(match, "TeamB", "ĐỘI B");
    const int scoreA = match.value("ScoreA").toInt();
    const int scoreB = match.value("ScoreB").toInt();
    const int gamesA = match.value("GamesA").toInt();
    const int gamesB = match.value("GamesB").toInt();

    updatingUI_ = true;
    if (!teamAEdit_->hasFocus())
        teamAEdit_->setText(teamA);
    if (!teamBEdit_->hasFocus())
        teamBEdit_->setText(teamB);
    updatingUI_ = false;

    teamALabel_->setText(teamA);
    teamBLabel_->setText(teamB);
    scoreAValue_->setText(QString::number(scoreA));
    scoreBValue_->setText(QString::number(scoreB));
    const QString call = state.value("scoreCall").toString().trimmed();
    callLabel_->setText(QString("Game %1–%2%3").arg(gamesA).arg(gamesB).arg(call.isEmpty() ? QString() : "  •  " + call));

    rallyAButton_->setText(teamA + "  +");
    rallyBButton_->setText(teamB + "  +");
    const bool gameDone = !match.value("GameWinner").toString().isEmpty();
    const bool matchDone = !match.value("MatchWinner").toString().isEmpty();
    rallyAButton_->setEnabled(pickleball && !gameDone && !matchDone);
    rallyBButton_->setEnabled(pickleball && !gameDone && !matchDone);
    undoButton_->setEnabled(pickleball && state.value("canUndo").toBool());
    swapButton_->setEnabled(pickleball);
    finishGameButton_->setEnabled(pickleball && gameDone);
    saveTeamsButton_->setEnabled(pickleball);

    winnerAButton_->setText(teamA);
    winnerBButton_->setText(teamB);
    // Winner controls are an explicit operator override. Do not fall back to
    // MatchWinner here; otherwise clearing the manual choice appears to fail.
    const QString selected = match.value("ResultWinner").toString();
    winnerGroup_->setExclusive(false);
    winnerAButton_->setChecked(selected == "A");
    winnerBButton_->setChecked(selected == "B");
    winnerGroup_->setExclusive(true);
    winnerAButton_->setEnabled(pickleball);
    winnerBButton_->setEnabled(pickleball);
    clearWinnerButton_->setEnabled(pickleball && !selected.isEmpty());
}

void TenBitScoreDockWidget::setConnectionState(const QString &state, const QString &tooltip)
{
    connectionLabel_->setText("●");
    connectionLabel_->setProperty("state", state);
    connectionLabel_->setToolTip(tooltip);
    connectionLabel_->style()->unpolish(connectionLabel_);
    connectionLabel_->style()->polish(connectionLabel_);
}
