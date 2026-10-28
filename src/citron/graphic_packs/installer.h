// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <memory>
#include <QFutureWatcher>
#include <QNetworkAccessManager>
#include <QObject>
#include <QTemporaryDir>
#include "pack.h"

namespace GraphicPacks {
struct PreparedData {
    Pack pack;
    QStringList directories;
    QStringList labels;
    QString error;
};

class Installer final : public QObject {
    Q_OBJECT
public:
    explicit Installer(QString cache_directory, QObject* parent = nullptr);
    bool IsBusy() const { return busy; }
    const PreparedData& Prepared() const { return prepared; }
    void Prepare(const Pack& pack);
    void PrepareLocal(const Pack& pack, const QString& file);
    void PrepareDirectory(const Pack& pack, const QString& directory);
    void Commit(int variant_index, const QString& load_directory);

signals:
    void Progress(const QString& text);
    void PreparedReady();
    void Installed(const GraphicPacks::Pack& pack, const QString& backup);
    void Failed(const QString& message);

private:
    QString cache_directory;
    QNetworkAccessManager network;
    std::shared_ptr<QTemporaryDir> staging;
    PreparedData prepared;
    bool busy = false;
};
} // namespace GraphicPacks
