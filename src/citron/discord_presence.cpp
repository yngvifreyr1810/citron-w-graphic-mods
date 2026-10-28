// SPDX-License-Identifier: GPL-2.0-or-later
#include "discord_presence.h"
#include <QApplication>
#include <QCheckBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSettings>
#include <QUuid>
#include <QtEndian>

DiscordPresence::DiscordPresence(const QString& settings_path, QObject* parent)
    : QObject(parent), path(settings_path) {
    setObjectName(QStringLiteral("discordPresence"));
    QSettings settings(path, QSettings::IniFormat);
    enabled = settings.value(QStringLiteral("Discord/enabled"), true).toBool();
    application_id = settings.value(QStringLiteral("Discord/application_id"), QStringLiteral("1564930358410219622")).toString();
    retry.setInterval(15000);
    connect(&retry, &QTimer::timeout, this, &DiscordPresence::Connect);
    connect(&socket, &QLocalSocket::connected, this, [this] {
        Send(0, QJsonDocument(QJsonObject{{QStringLiteral("v"), 1},
             {QStringLiteral("client_id"), application_id}}).toJson(QJsonDocument::Compact));
    });
    connect(&socket, &QLocalSocket::readyRead, this, &DiscordPresence::Receive);
    connect(&socket, &QLocalSocket::disconnected, this, [this] {
        ready = false;
        incoming.clear();
    });
    connect(&socket, &QLocalSocket::errorOccurred, this, [this](auto) {
        ready = false;
        incoming.clear();
        status = tr("Discord is unavailable; retrying in the background.");
        pipe = (pipe + 1) % 10;
    });
    retry.start();
    Connect();
}

void DiscordPresence::Connect() {
    if (!enabled || !QRegularExpression(QStringLiteral("^[0-9]{17,20}$")).match(application_id).hasMatch()) return;
    if (socket.state() == QLocalSocket::UnconnectedState) {
        socket.connectToServer(QStringLiteral("discord-ipc-%1").arg(pipe));
    }
}

void DiscordPresence::Send(quint32 opcode, const QByteArray& payload) {
    QByteArray frame(8, '\0');
    qToLittleEndian<quint32>(opcode, frame.data());
    qToLittleEndian<quint32>(quint32(payload.size()), frame.data() + 4);
    frame += payload;
    socket.write(frame);
}

void DiscordPresence::Receive() {
    incoming += socket.readAll();
    while (incoming.size() >= 8) {
        const auto opcode = qFromLittleEndian<quint32>(incoming.constData());
        const auto length = qFromLittleEndian<quint32>(incoming.constData() + 4);
        if (length > 1024 * 1024) { socket.abort(); incoming.clear(); return; }
        if (incoming.size() < qsizetype(length) + 8) return;
        const auto payload = incoming.mid(8, length);
        incoming.remove(0, length + 8);
        if (opcode == 3) { Send(4, payload); continue; }
        if (opcode == 2) { socket.abort(); return; }
        const auto event = QJsonDocument::fromJson(payload).object();
        if (event.value(QStringLiteral("evt")) == QStringLiteral("READY")) {
            ready = true;
            Publish();
        } else if (event.value(QStringLiteral("evt")) == QStringLiteral("ERROR")) {
            status = tr("Discord rejected the activity. Check your Application ID and Discord settings.");
        } else if (event.value(QStringLiteral("cmd")) == QStringLiteral("SET_ACTIVITY")) {
            status = tr("Discord acknowledged the activity.");
        }
    }
}

void DiscordPresence::SetGame(const QString& value) {
    if (game != value) started = QDateTime::currentSecsSinceEpoch();
    game = value.left(128);
    Publish();
}

void DiscordPresence::Publish() {
    if (!ready || !enabled) return;
    QJsonObject activity{{QStringLiteral("details"), game.isEmpty() ? tr("Browsing game library") : game},
                         {QStringLiteral("state"), QStringLiteral("citron-mods-by-yngvifreyr1810 · Beta v0.1")}};
    if (started && !game.isEmpty()) activity.insert(QStringLiteral("timestamps"), QJsonObject{{QStringLiteral("start"), started}});
    Send(1, QJsonDocument(QJsonObject{{QStringLiteral("cmd"), QStringLiteral("SET_ACTIVITY")},
        {QStringLiteral("args"), QJsonObject{{QStringLiteral("pid"), QApplication::applicationPid()}, {QStringLiteral("activity"), activity}}},
        {QStringLiteral("nonce"), QUuid::createUuid().toString(QUuid::WithoutBraces)}}).toJson(QJsonDocument::Compact));
}

void DiscordPresence::Configure(QWidget* parent) {
    QDialog dialog(parent);
    dialog.setWindowTitle(tr("Discord Rich Presence"));
    auto* form = new QFormLayout(&dialog);
    auto* toggle = new QCheckBox(tr("Enable Discord Rich Presence"), &dialog);
    toggle->setChecked(enabled);
    auto* id = new QLineEdit(application_id, &dialog);
    id->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]{17,20}")), id));
    auto* help = new QLabel(tr("Create an application named citron-mods-by-yngvifreyr1810 in Discord Developer Portal, then enter its public Application ID. The version appears in the activity details. No token is needed. Game titles are shared while enabled."), &dialog);
    help->setWordWrap(true);
    form->addRow(help);
    form->addRow(toggle);
    form->addRow(tr("Application ID"), id);
    auto* feedback = new QLabel(status.isEmpty() ? tr("Not connected. An Application ID and the Discord desktop app are required.") : status, &dialog);
    feedback->setWordWrap(true);
    form->addRow(feedback);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&] {
        if (toggle->isChecked() && !id->hasAcceptableInput()) {
            feedback->setText(tr("Enter a valid public Application ID (17–20 digits), or disable Rich Presence."));
            return;
        }
        enabled = toggle->isChecked();
        application_id = id->text();
        socket.abort();
        ready = false;
        incoming.clear();
        QSettings settings(path, QSettings::IniFormat);
        settings.setValue(QStringLiteral("Discord/enabled"), enabled);
        settings.setValue(QStringLiteral("Discord/application_id"), application_id);
        Connect();
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.resize(480, 260);
    dialog.exec();
}
