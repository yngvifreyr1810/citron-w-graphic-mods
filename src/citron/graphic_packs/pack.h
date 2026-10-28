// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace GraphicPacks {

struct Game {
    QString title_id;
    QString name;
    QString version;
    QString build_id;
    QStringList disabled_mods;
};

struct Pack {
    QString id;
    QString title_id;
    QString game;
    QString name;
    QString category;
    QString version;
    QStringList build_ids;
    QString repository;
    QString path;
    QString blob_sha;
    QString description_path;
    QString description;
    QString mod_name;
    QString variant;
    QString option_name;
    bool whole_package = false;
    bool local = false;
    bool enabled = false;

    QJsonObject ToJson() const;
    static Pack FromJson(const QJsonObject& object);
};

enum class Compatibility { Match, Mismatch, Unknown };

QString NormalizeName(QString name);
QString CategoryForName(const QString& name);
QString StableId(const QString& repository, const QString& path);
QString ModDirectoryName(const Pack& pack);
bool InvalidatePatchCache(const QString& directory, const QString& title_id);
QString RawUrl(const QString& repository, const QString& path);
QString SourceUrl(const Pack& pack);
bool IsSafeRelativePath(const QString& path);
bool BuildIdsMatch(const QString& first, const QString& second);
Compatibility CheckCompatibility(const Pack& pack, const Game& game);
QVector<Pack> ParseRepository(const QString& repository, const QJsonObject& tree);
QVector<Pack> CuratePacks(QVector<Pack> packs, const QVector<Game>& games);
void InspectPatch(Pack& pack, const QByteArray& content);

struct CheatOption {
    QString name;
    QByteArray content;
};
QVector<CheatOption> SplitCheats(const QByteArray& content);
QStringList PayloadBuildIds(const QString& directory);
QStringList FindPayloads(const QString& directory, const QString& title_id);
QStringList ConflictingMods(const QString& candidate, const QString& title_root,
                            const QStringList& disabled_mods, const QString& exclude = {});

} // namespace GraphicPacks

Q_DECLARE_METATYPE(GraphicPacks::Pack)
