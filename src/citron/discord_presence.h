// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QObject>
#include <QLocalSocket>
#include <QTimer>
#include <QJsonObject>

class QWidget;
class DiscordPresence final : public QObject {
    Q_OBJECT
public:
    explicit DiscordPresence(const QString& settings_path, QObject* parent);
    void SetGame(const QString& game);
    void Configure(QWidget* parent);
private:
    void Connect();
    void Receive();
    void Send(quint32 opcode, const QByteArray& payload);
    void Publish();
    QString path, application_id, game, status;
    bool enabled = true, ready = false;
    int pipe = 0;
    qint64 started = 0;
    QByteArray incoming;
    QLocalSocket socket;
    QTimer retry;
};
