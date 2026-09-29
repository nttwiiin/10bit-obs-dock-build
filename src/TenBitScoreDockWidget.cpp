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
    setMinimumWidth(330);

    auto *panel = new QFrame(this);
    panel->setObjectName("tenbitScorePanel");
    panel->setStyleSheet(R"(
        QFrame#tenbitScorePanel { background: transparent; }
        QLabel#statusDot { font-size:17px; font-weight:900; padding:0px; }
        QLabel#statusDot[state="connected"] { color:#43e58a; }
        QLabel#statusDot[state="waiting"] { color:#f2c94c; }
        QLabel#statusDot[state="offline"] { color:#ff5a6d; }
        QLabel#section { color:palette(mid); font-size:10px; font-weight:800; padding-top:1px; }
        QFrame#scoreCard { background:palette(base); border:1px solid palette(mid); border-radius:5px; }
        QLabel#teamName { font-size:10px; font-weight:700; }
        QLabel#scoreValue { font-size:31px; font-weight:900; }
        QLabel#dash { color:palette(mid); font-size:20px; font-weight:800; }
        QLabel#call { color:palette(mid); font-size:10px; }
        QLineEdit { min-height:27px; }
        QRadioButton { min-height:26px; }
    )");

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(panel);

    auto *root = new QVBoxLayout(panel);
    root->setContentsMargins(8, 7, 8, 8);
    root->setSpacing(6);

    auto *projectRow = new QHBoxLayout();
    projectLabel_ = new QLabel("Chưa mở Project");
    connectionLabel_ = new QLabel("●");
    connectionLabel_->setObjectName("statusDot");
    connectionLabel_->setAlignment(Qt::AlignCenter);
    connectionLabel_->setFixedWidth(20);
    projectRow->addWidget(projectLabel_, 1);
    projectRow->addWidget(connectionLabel_);
    root->addLayout(projectRow);

    auto *teamsSection = sectionLabel("TÊN 2 ĐỘI");
    teamsSection->setObjectName("section");
    root->addWidget(teamsSection);

    auto *teamRow = new QHBoxLayout();
    teamRow->setSpacing(6);
    teamAEdit_ = new QLineEdit();
    teamBEdit_ = new QLineEdit();
    teamAEdit_->setPlaceholderText("Đội A");
    teamBEdit_->setPlaceholderText("Đội B");
    teamRow->addWidget(teamAEdit_);
    teamRow->addWidget(teamBEdit_);
    root->addLayout(teamRow);
    saveTeamsButton_ = new QPushButton("LƯU TÊN 2 ĐỘI");
    connect(saveTeamsButton_, &QPushButton::clicked, this, &TenBitScoreDockWidget::sendTeamNames);
    root->addWidget(saveTeamsButton_);

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

    auto *actionRow = new QHBoxLayout();
    actionRow->setSpacing(6);
    undoButton_ = new QPushButton("↶  HOÀN TÁC");
    swapButton_ = new QPushButton("⇄  ĐỔI VỊ TRÍ");
    connect(undoButton_, &QPushButton::clicked, this, [this]() { sendCommand("undo"); });
    connect(swapButton_, &QPushButton::clicked, this, [this]() { sendCommand("swap_sides"); });
    actionRow->addWidget(undoButton_);
    actionRow->addWidget(swapButton_);
    root->addLayout(actionRow);

    finishGameButton_ = new QPushButton("KẾT THÚC GAME");
    connect(finishGameButton_, &QPushButton::clicked, this, [this]() { sendCommand("finish_game"); });
    root->addWidget(finishGameButton_);

    auto *winnerSection = sectionLabel("ĐỘI THẮNG • DÙNG CHO KẾT QUẢ");
    winnerSection->setObjectName("section");
    root->addWidget(winnerSection);

    auto *winnerRow = new QHBoxLayout();
    winnerRow->setSpacing(6);
    winnerAButton_ = new QRadioButton("ĐỘI A");
    winnerBButton_ = new QRadioButton("ĐỘI B");
    winnerGroup_ = new QButtonGroup(this);
    winnerGroup_->setExclusive(true);
    winnerGroup_->addButton(winnerAButton_);
    winnerGroup_->addButton(winnerBButton_);
    winnerRow->addWidget(winnerAButton_);
    winnerRow->addWidget(winnerBButton_);
    root->addLayout(winnerRow);
    connect(winnerAButton_, &QRadioButton::clicked, this, [this]() {
        QJsonObject args; args.insert("side", "A"); sendCommand("set_result_winner", args);
    });
    connect(winnerBButton_, &QRadioButton::clicked, this, [this]() {
        QJsonObject args; args.insert("side", "B"); sendCommand("set_result_winner", args);
    });
    clearWinnerButton_ = new QPushButton("BỎ CHỌN ĐỘI THẮNG");
    connect(clearWinnerButton_, &QPushButton::clicked, this, [this]() {
        QJsonObject args; args.insert("side", ""); sendCommand("set_result_winner", args);
    });
    root->addWidget(clearWinnerButton_);

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
    QString selected = match.value("ResultWinner").toString();
    if (selected.isEmpty())
        selected = match.value("MatchWinner").toString();
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
