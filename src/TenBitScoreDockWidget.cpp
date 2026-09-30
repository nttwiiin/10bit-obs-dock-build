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
    setMinimumWidth(420);

    auto *panel = new QFrame(this);
    panel->setObjectName("tenbitScorePanel");
    panel->setStyleSheet(R"(
        QFrame#tenbitScorePanel {
            background:#07111d;
            border:1px solid #294261;
            border-radius:11px;
        }
        QFrame#statusCard, QFrame#scoreCard, QFrame#winnerCard {
            background:#0d1827;
            border:1px solid #24415f;
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
            font-weight:900;
        }
        QLabel#statusDot {
            font-size:16px;
            font-weight:900;
            padding:0px;
        }
        QLabel#statusDot[state="connected"] { color:#1eea8d; }
        QLabel#statusDot[state="waiting"] { color:#f4c84a; }
        QLabel#statusDot[state="offline"] { color:#ff4169; }
        QLabel#teamName {
            color:#f5f8ff;
            font-size:13px;
            font-weight:900;
        }
        QLabel#scoreValue {
            color:#ffffff;
            font-size:38px;
            font-weight:1000;
        }
        QLabel#dash {
            color:#5f83b2;
            font-size:23px;
            font-weight:900;
        }
        QLabel#call {
            color:#6688b5;
            font-size:10px;
            font-weight:800;
        }
        QLabel#winnerLabel {
            color:#879bb7;
            font-size:10px;
            font-weight:900;
        }
        QLineEdit {
            color:#f4f7ff;
            background:#172437;
            border:1px solid #314d6d;
            border-radius:8px;
            min-height:32px;
            padding:4px 10px;
            font-size:11px;
            font-weight:800;
            selection-background-color:#df1d47;
        }
        QLineEdit:focus {
            background:#1b2a40;
            border:1px solid #6687b0;
        }
        QPushButton {
            color:#f2f6ff;
            background:#142238;
            border:1px solid #3a5f88;
            border-radius:8px;
            min-height:34px;
            padding:5px 9px;
            font-size:10px;
            font-weight:900;
        }
        QPushButton:hover {
            background:#1b2f49;
            border-color:#6c91bd;
        }
        QPushButton:pressed { background:#0e1a2a; }
        QPushButton:disabled {
            color:#56657a;
            background:#0e1723;
            border-color:#24354b;
        }
        QPushButton#saveButton {
            color:#fff;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #ef1642,stop:1 #b41234);
            border:1px solid #ff3e62;
            min-height:38px;
            font-size:11px;
        }
        QPushButton#rallyA {
            color:#ffffff;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #6f5613,stop:1 #2a2b25);
            border:1px solid #ffc61a;
            border-left:5px solid #ffd21d;
            min-height:45px;
            font-size:12px;
        }
        QPushButton#rallyB {
            color:#ffffff;
            background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #5d1730,stop:1 #2b1823);
            border:1px solid #ff2455;
            border-left:5px solid #ff2455;
            min-height:45px;
            font-size:12px;
        }
        QPushButton#clearWinner {
            min-height:31px;
            color:#aebbd0;
        }
        QRadioButton {
            color:#eef4ff;
            min-height:25px;
            font-size:10px;
            font-weight:850;
            spacing:7px;
        }
        QRadioButton::indicator {
            width:16px;
            height:16px;
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
    projectLabel_ = new QLabel("Chưa mở Project");
    projectLabel_->setObjectName("project");
    connectionLabel_ = new QLabel("●");
    connectionLabel_->setObjectName("statusDot");
    connectionLabel_->setAlignment(Qt::AlignCenter);
    connectionLabel_->setFixedWidth(19);
    projectRow->addWidget(projectLabel_, 1);
    projectRow->addWidget(connectionLabel_);
    root->addWidget(statusCard);

    auto *teamRow = new QHBoxLayout();
    teamRow->setSpacing(7);
    teamAEdit_ = new QLineEdit();
    teamBEdit_ = new QLineEdit();
    teamAEdit_->setPlaceholderText("Tên đội / VĐV A");
    teamBEdit_->setPlaceholderText("Tên đội / VĐV B");
    saveTeamsButton_ = new QPushButton("LƯU");
    saveTeamsButton_->setObjectName("saveButton");
    saveTeamsButton_->setMinimumWidth(78);
    saveTeamsButton_->setMaximumWidth(95);
    saveTeamsButton_->setToolTip("Lưu tên hai đội");
    connect(saveTeamsButton_, &QPushButton::clicked, this, &TenBitScoreDockWidget::sendTeamNames);
    teamRow->addWidget(teamAEdit_, 1);
    teamRow->addWidget(teamBEdit_, 1);
    teamRow->addWidget(saveTeamsButton_);
    root->addLayout(teamRow);

    auto *scoreCard = new QFrame(panel);
    scoreCard->setObjectName("scoreCard");
    auto *grid = new QGridLayout(scoreCard);
    grid->setContentsMargins(14, 11, 14, 10);
    grid->setHorizontalSpacing(9);
    grid->setVerticalSpacing(2);
    grid->setColumnStretch(0, 3);
    grid->setColumnStretch(1, 1);
    grid->setColumnStretch(2, 0);
    grid->setColumnStretch(3, 1);
    grid->setColumnStretch(4, 3);

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
    scoreRow->setSpacing(8);
    rallyAButton_ = new QPushButton("ĐỘI A +");
    rallyAButton_->setObjectName("rallyA");
    rallyBButton_ = new QPushButton("ĐỘI B +");
    rallyBButton_->setObjectName("rallyB");
    connect(rallyAButton_, &QPushButton::clicked, this, [this]() { sendCommand("rally_a"); });
    connect(rallyBButton_, &QPushButton::clicked, this, [this]() { sendCommand("rally_b"); });
    scoreRow->addWidget(rallyAButton_, 1);
    scoreRow->addWidget(rallyBButton_, 1);
    root->addLayout(scoreRow);

    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(7);
    undoButton_ = new QPushButton("HOÀN TÁC");
    swapButton_ = new QPushButton("ĐỔI VỊ TRÍ");
    finishGameButton_ = new QPushButton("KẾT THÚC GAME");
    connect(undoButton_, &QPushButton::clicked, this, [this]() { sendCommand("undo"); });
    connect(swapButton_, &QPushButton::clicked, this, [this]() { sendCommand("swap_sides"); });
    connect(finishGameButton_, &QPushButton::clicked, this, [this]() { sendCommand("finish_game"); });
    actionRow->addWidget(undoButton_, 1);
    actionRow->addWidget(swapButton_, 1);
    actionRow->addWidget(finishGameButton_, 1);
    root->addLayout(actionRow);

    auto *winnerCard = new QFrame(panel);
    winnerCard->setObjectName("winnerCard");
    auto *winnerRow = new QHBoxLayout(winnerCard);
    winnerRow->setContentsMargins(11, 7, 9, 7);
    winnerRow->setSpacing(8);
    auto *winnerLabel = new QLabel("THẮNG:");
    winnerLabel->setObjectName("winnerLabel");
    winnerAButton_ = new QRadioButton("ĐỘI A");
    winnerBButton_ = new QRadioButton("ĐỘI B");
    winnerGroup_ = new QButtonGroup(this);
    winnerGroup_->setExclusive(true);
    winnerGroup_->addButton(winnerAButton_);
    winnerGroup_->addButton(winnerBButton_);
    clearWinnerButton_ = new QPushButton("BỎ CHỌN");
    clearWinnerButton_->setObjectName("clearWinner");
    clearWinnerButton_->setMinimumWidth(90);
    clearWinnerButton_->setMaximumWidth(112);
    clearWinnerButton_->setToolTip("Bỏ lựa chọn đội thắng thủ công");
    winnerRow->addWidget(winnerLabel);
    winnerRow->addWidget(winnerAButton_, 1);
    winnerRow->addWidget(winnerBButton_, 1);
    winnerRow->addWidget(clearWinnerButton_);
    root->addWidget(winnerCard);

    connect(winnerAButton_, &QRadioButton::clicked, this, [this]() {
        QJsonObject args; args.insert("side", "A"); sendCommand("set_result_winner", args);
    });
    connect(winnerBButton_, &QRadioButton::clicked, this, [this]() {
        QJsonObject args; args.insert("side", "B"); sendCommand("set_result_winner", args);
    });
    connect(clearWinnerButton_, &QPushButton::clicked, this, [this]() {
        QJsonObject args; args.insert("side", ""); sendCommand("set_result_winner", args);
    });

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
