// SPDX-License-Identifier: GPL-2.0-or-later
#include "catalog.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSaveFile>

namespace GraphicPacks {
namespace {
QNetworkRequest Request(const QUrl& url) {
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Citron-Graphic-Packs/1.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(30000);
    return request;
}
}

QStringList Catalog::Repositories() {
    return {QStringLiteral("cucholix/NX-IPS-romfs-compilation"),
            QStringLiteral("ChanseyIsTheBest/NX-60FPS-RES-GFX-Cheats"),
            QStringLiteral("theboy181/switch-ptchtxt-mods"),
            QStringLiteral("StevensND/switch-port-mods"),
            QStringLiteral("yuzu-mirror/yuzu-mod-archive")};
}

Catalog::Catalog(QString cache_directory_, QObject* parent)
    : QObject(parent), cache_directory(std::move(cache_directory_)), network(this) {
    QDir().mkpath(cache_directory);
    QFile seed(QStringLiteral(":/graphic_packs/catalog_seed.json"));
    if (seed.open(QIODevice::ReadOnly)) {
        const auto repositories = QJsonDocument::fromJson(seed.readAll()).object().value(QStringLiteral("repositories")).toArray();
        for (const auto& value : repositories) {
            const auto object = value.toObject();
            trees.insert(object.value(QStringLiteral("repository")).toString(), object);
        }
    }
    for (const auto& repository : Repositories()) {
        QFile file(cache_directory + QLatin1Char('/') + StableId(repository, QStringLiteral("tree")) + QStringLiteral(".json"));
        if (!file.open(QIODevice::ReadOnly)) continue;
        const auto object = QJsonDocument::fromJson(file.readAll()).object();
        if (object.value(QStringLiteral("tree")).isArray() && !object.value(QStringLiteral("truncated")).toBool())
            trees.insert(repository, object);
    }
    Rebuild();
}

void Catalog::Rebuild() {
    packs.clear();
    for (const auto& repository : Repositories()) packs += ParseRepository(repository, trees.value(repository));
    emit Changed();
}

void Catalog::Refresh() {
    if (IsRefreshing()) return;
    const auto repositories = Repositories();
    pending = repositories.size();
    successful = 0;
    errors.clear();
    for (const auto& repository : repositories) {
        const auto url = QUrl(QStringLiteral("https://api.github.com/repos/%1/git/trees/main?recursive=1").arg(repository));
        auto* reply = network.get(Request(url));
        connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
            if (received > 16 * 1024 * 1024) reply->abort();
        });
        connect(reply, &QNetworkReply::finished, this, [this, repository, reply, total = repositories.size()] {
            const QByteArray data = reply->readAll();
            const auto object = QJsonDocument::fromJson(data).object();
            if (reply->error() == QNetworkReply::NoError && object.value(QStringLiteral("tree")).isArray() &&
                !object.value(QStringLiteral("truncated")).toBool()) {
                QSaveFile file(cache_directory + QLatin1Char('/') + StableId(repository, QStringLiteral("tree")) + QStringLiteral(".json"));
                if (file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit()) {
                    trees.insert(repository, object);
                    ++successful;
                } else {
                    errors << tr("Could not cache %1").arg(repository);
                }
            } else {
                const auto detail = object.value(QStringLiteral("message")).toString(reply->errorString());
                errors << repository + QStringLiteral(": ") + detail;
            }
            reply->deleteLater();
            --pending;
            emit Progress(tr("Updated %1/%2 sources...").arg(total - pending).arg(total));
            if (pending == 0) {
                Rebuild();
                QString status = tr("Catalogue updated: %1 packs from %2 sources. Files download when you install a pack.")
                                     .arg(packs.size()).arg(successful);
                if (!errors.isEmpty()) status += QStringLiteral("\n") + errors.join(QLatin1Char('\n'));
                emit RefreshFinished(status);
            }
        });
    }
    emit Progress(tr("Downloading the community pack catalogue..."));
}

void Catalog::RequestNotes(const Pack& pack) {
    if (pack.description_path.isEmpty() || pack.repository.isEmpty()) return;
    const QString key = RawUrl(pack.repository, pack.description_path);
    if (notes.contains(key)) {
        emit NotesReady(pack.id, notes.value(key));
        return;
    }
    auto* reply = network.get(Request(QUrl(key)));
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 received, qint64) {
        if (received > 1024 * 1024) reply->abort();
    });
    connect(reply, &QNetworkReply::finished, this, [this, id = pack.id, key, reply] {
        if (reply->error() == QNetworkReply::NoError) {
            const QString text = QString::fromUtf8(reply->readAll()).remove(QLatin1Char('\r'));
            notes.insert(key, text);
            emit NotesReady(id, text);
        }
        reply->deleteLater();
    });
}
} // namespace GraphicPacks
