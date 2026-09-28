#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QTcpSocket>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;

class TenBitScoreDockWidget final : public QWidget {
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
    void setConnectionText(const QString &text, bool online);

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
    QLabel *scoreLabel_ = nullptr;
    QLabel *callLabel_ = nullptr;
    QPushButton *rallyAButton_ = nullptr;
    QPushButton *rallyBButton_ = nullptr;
    QPushButton *undoButton_ = nullptr;
};
