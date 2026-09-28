#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>
#include "TenBitObsDockContent.hpp"

class QLabel;
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
    void applyState(const QJsonObject &state);
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

    QLabel *connectionLabel_ = nullptr;
    QLabel *projectLabel_ = nullptr;
    QLabel *teamALabel_ = nullptr;
    QLabel *teamBLabel_ = nullptr;
    QLabel *scoreAValue_ = nullptr;
    QLabel *scoreBValue_ = nullptr;
    QLabel *serveLabel_ = nullptr;
    QLabel *messageLabel_ = nullptr;

    QPushButton *scoreToggle_ = nullptr;
    QPushButton *lowerToggle_ = nullptr;
    QPushButton *timeoutButton_ = nullptr;
    QPushButton *replayButton_ = nullptr;
    QPushButton *standbyButton_ = nullptr;
    QPushButton *resultsButton_ = nullptr;
    QPushButton *rallyAButton_ = nullptr;
    QPushButton *rallyBButton_ = nullptr;
    QPushButton *undoButton_ = nullptr;
    QPushButton *finishGameButton_ = nullptr;
};
