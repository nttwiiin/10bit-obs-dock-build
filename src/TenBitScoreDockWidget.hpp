#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>
#include "TenBitObsDockContent.hpp"

class QLabel;
class QPushButton;

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
    void sendCommand(const QString &command);
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

    QLabel *connectionLabel_ = nullptr;
    QLabel *projectLabel_ = nullptr;
    QLabel *teamALabel_ = nullptr;
    QLabel *teamBLabel_ = nullptr;
    QLabel *scoreAValue_ = nullptr;
    QLabel *scoreBValue_ = nullptr;
    QLabel *callLabel_ = nullptr;
    QPushButton *rallyAButton_ = nullptr;
    QPushButton *rallyBButton_ = nullptr;
    QPushButton *undoButton_ = nullptr;
};
