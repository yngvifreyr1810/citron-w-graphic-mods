// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include "pack.h"

namespace GraphicPacks {
class Catalog final : public QObject {
    Q_OBJECT
public:
    explicit Catalog(QString cache_directory, QObject* parent = nullptr);
    const QVector<Pack>& Packs() const { return packs; }
    bool IsRefreshing() const { return pending != 0; }
    void Refresh();
    void RequestNotes(const Pack& pack);
    static QStringList Repositories();

signals:
    void Changed();
    void Progress(const QString& text);
    void RefreshFinished(const QString& text);
    void NotesReady(const QString& id, const QString& notes);

private:
    void Rebuild();
    QString cache_directory;
    QNetworkAccessManager network;
    QHash<QString, QJsonObject> trees;
    QHash<QString, QString> notes;
    QVector<Pack> packs;
    int pending = 0;
    int successful = 0;
    QStringList errors;
};
} // namespace GraphicPacks
