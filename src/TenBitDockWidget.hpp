#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QPointer>
#include <QTcpSocket>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;

class TenBitDockWidget final : public QWidget {
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
    void applyState(const QJsonObject &state);\n    void revealDock();
    void setConnectionText(const QString &text, bool online);
    QPushButton *makeButton(const QString &text, const QString &command);
    void setActive(QPushButton *button, bool active, const QString &activeText = {}, const QString &inactiveText = {});

    QTcpSocket socket_;
    QTimer reconnectTimer_;
    QTimer pollTimer_;
    QByteArray readBuffer_;
    QString host_ = "127.0.0.1";
    quint16 port_ = 0;
    QString token_;
    quint64 requestId_ = 0;\n    quint64 lastDockShowSeq_ = 0;

    QLabel *connectionLabel_ = nullptr;
    QLabel *projectLabel_ = nullptr;
    QLabel *scoreLabel_ = nullptr;
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
    QPushButton *saveReplayButton_ = nullptr;
};
