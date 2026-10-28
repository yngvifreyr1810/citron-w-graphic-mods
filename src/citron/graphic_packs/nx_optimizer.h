// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <functional>
#include <QJsonObject>
#include <QMap>
#include <QNetworkAccessManager>
#include <QWidget>
#include "installer.h"

class QFormLayout;
class QLabel;
class QPushButton;
class QVBoxLayout;

namespace GraphicPacks {
class NxOptimizer final : public QWidget {
    Q_OBJECT
public:
    NxOptimizer(Game game, QString load, QString cache, QString sdmc, QWidget* parent = nullptr);
    bool IsBusy() const { return busy || installer.IsBusy(); }
    void EnsureLoaded() { if (schema.isEmpty() && !IsBusy()) Load(); }
    void SetSdmcDirectory(const QString& path) { sdmc = path; }
    void SetConfirmApply(std::function<bool()> callback) { confirm_apply = std::move(callback); }
    static Pack CatalogueEntry();
    static QByteArray BuildIni(const QJsonObject& options, const QMap<QString, QJsonValue>& values,
                               const QByteArray& existing = {});

signals:
    void Progress(const QString& text);
    void BusyChanged();
    void Applied(const GraphicPacks::Pack& pack, int required_memory_layout);

private:
    void Load();
    void BuildControls();
    void Apply();
    void Fetch(const QString& path, const QString& destination, const QString& sha,
               std::function<void(bool)> finished);
    void DownloadAssets(int index);
    void Commit();
    void Fail(const QString& error);
    Game game;
    QString load;
    QString cache;
    QString sdmc;
    QString config_path;
    QString old_config_path;
    QByteArray old_config;
    bool had_config = false;
    bool config_written = false;
    bool busy = false;
    QJsonObject schema;
    QMap<QString, std::function<QJsonValue()>> controls;
    QVBoxLayout* layout;
    QLabel* info;
    QPushButton* load_button;
    QPushButton* apply;
    QNetworkAccessManager network;
    Installer installer;
    int memory_layout = 0;
    std::function<bool()> confirm_apply;
};
} // namespace GraphicPacks
