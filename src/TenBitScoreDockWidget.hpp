#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>
#include "TenBitObsDockContent.hpp"

class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;

class TenBitScoreDockWidget final : public TenBitObsDockContent {
    Q_OBJECT
public:
    explicit TenBitScoreDockWidget(QWidget *parent = nullptr);
    ~TenBitScoreDockWidget() override = default;

private slots:
    void ensureConnected();
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onSocketError();
    void pollStatus();

private:
    void buildUI();
    bool loadRuntime();
    void connectToRuntime();
    void sendCommand(const QString &command, const QJsonObject &args = {});
    void sendTeamNames();
    void applyState(const QJsonObject &state);
    void setConnectionState(const QString &state, const QString &tooltip);

    QTcpSocket socket_;
    QTimer reconnectTimer_;
    QTimer pollTimer_;
    QByteArray readBuffer_;
    QString host_ = "127.0.0.1";
    quint16 port_ = 0;
    QString token_;
    quint64 requestId_ = 0;
    bool updatingUI_ = false;

    QLabel *connectionLabel_ = nullptr;
    QLabel *projectLabel_ = nullptr;
    QLabel *teamALabel_ = nullptr;
    QLabel *teamBLabel_ = nullptr;
    QLabel *scoreAValue_ = nullptr;
    QLabel *scoreBValue_ = nullptr;
    QLabel *callLabel_ = nullptr;
    QLineEdit *teamAEdit_ = nullptr;
    QLineEdit *teamBEdit_ = nullptr;
    QPushButton *saveTeamsButton_ = nullptr;
    QPushButton *rallyAButton_ = nullptr;
    QPushButton *rallyBButton_ = nullptr;
    QPushButton *undoButton_ = nullptr;
    QPushButton *swapButton_ = nullptr;
    QPushButton *finishGameButton_ = nullptr;
    QButtonGroup *winnerGroup_ = nullptr;
    QRadioButton *winnerAButton_ = nullptr;
    QRadioButton *winnerBButton_ = nullptr;
    QPushButton *clearWinnerButton_ = nullptr;
};
