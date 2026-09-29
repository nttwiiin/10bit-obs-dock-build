#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>
#include "TenBitObsDockContent.hpp"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

class TenBitDockWidget final : public TenBitObsDockContent {
    Q_OBJECT
public:
    explicit TenBitDockWidget(QWidget *parent = nullptr);
    ~TenBitDockWidget() override = default;

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
    void sendMatchSetup();
    void applyState(const QJsonObject &state);
    void openReplaySettings();
    bool revealDock();
    void setConnectionState(const QString &state, const QString &tooltip);
    QPushButton *makeButton(const QString &text, const QString &command);
    void setActive(QPushButton *button, bool active, const QString &activeText = {}, const QString &inactiveText = {});

    QTcpSocket socket_;
    QTimer reconnectTimer_;
    QTimer pollTimer_;
    QByteArray readBuffer_;
    QString host_ = "127.0.0.1";
    quint16 port_ = 0;
    QString token_;
    quint64 requestId_ = 0;
    quint64 lastDockShowSeq_ = 0;
    int replayDurationSec_ = 5;
    int replaySpeedPercent_ = 50;
    bool updatingUI_ = false;

    QLabel *connectionLabel_ = nullptr;
    QLabel *projectLabel_ = nullptr;
    QLabel *matchStatusLabel_ = nullptr;
    QLabel *messageLabel_ = nullptr;

    QLineEdit *eventEdit_ = nullptr;
    QLineEdit *roundEdit_ = nullptr;
    QLineEdit *courtEdit_ = nullptr;
    QComboBox *formatCombo_ = nullptr;
    QComboBox *pointsCombo_ = nullptr;

    QPushButton *scoreToggle_ = nullptr;
    QPushButton *adButton_ = nullptr;
    QPushButton *teamsButton_ = nullptr;
    QPushButton *introButton_ = nullptr;
    QPushButton *timeoutButton_ = nullptr;
    QPushButton *standbyButton_ = nullptr;
    QPushButton *breakButton_ = nullptr;
    QPushButton *resultsButton_ = nullptr;
    QPushButton *replayButton_ = nullptr;
    QPushButton *replaySettingsButton_ = nullptr;
    QPushButton *recordReplayButton_ = nullptr;
};
