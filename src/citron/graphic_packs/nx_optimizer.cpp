// SPDX-License-Identifier: GPL-2.0-or-later
#include "nx_optimizer.h"
#include "scroll_safe_combo.h"
#include <algorithm>
#include <QCheckBox>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QVBoxLayout>

namespace GraphicPacks {
namespace {
const QString Ref = QStringLiteral("66c43a178109a6f2bc2bfedd83de0c1d95da44f0");
const QString Prefix = QStringLiteral("src/PatchInfo/Tears Of The Kingdom/");
bool Save(const QString& path, const QByteArray& data) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size() && file.commit();
}
QString JsonText(const QJsonValue& value) {
    if (value.isBool()) return value.toBool() ? QStringLiteral("On") : QStringLiteral("Off");
    if (value.isDouble()) return QString::number(value.toDouble(), 'g', 10);
    return value.toString();
}
}

Pack NxOptimizer::CatalogueEntry() {
    Pack pack;
    pack.id = QStringLiteral("nx-optimizer-totk");
    pack.title_id = QStringLiteral("0100F2C0115B6000");
    pack.game = QStringLiteral("The Legend of Zelda: Tears of the Kingdom");
    pack.name = QStringLiteral("NX Optimizer / UltraCam");
    pack.category = QStringLiteral("Graphics");
    pack.repository = QStringLiteral("MaxLastBreath/nx-optimizer");
    pack.path = QStringLiteral("src/PatchInfo/Tears Of The Kingdom/PatchInfo.json");
    pack.description = QStringLiteral("Configure TOTK graphics and dynamic FPS using MaxLastBreath's UltraCam. NX Optimizer 3.2.0 / UltraCam Calamity 3.0.0. Components download from the upstream release commit when requested. Original assets retain their CC BY-NC 2.0 licence and credits.");
    return pack;
}

NxOptimizer::NxOptimizer(Game game_, QString load_, QString cache_, QString sdmc_, QWidget* parent)
    : QWidget(parent), game(std::move(game_)), load(std::move(load_)),
      cache(std::move(cache_) + QStringLiteral("/nx-optimizer-3.2.0")), sdmc(std::move(sdmc_)),
      network(this), installer(cache, this) {
    layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    info = new QLabel(tr("NX Optimizer 3.2.0 · UltraCam 3.0.0 — stop the game before applying."), this);
    info->setWordWrap(true);
    info->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    load_button = new QPushButton(tr("Load NX Optimizer settings"), this);
    load_button->setObjectName(QStringLiteral("loadNxOptimizer"));
    layout->addWidget(info);
    layout->addWidget(load_button);
    apply = new QPushButton(tr("Apply NX Optimizer"), this);
    apply->setObjectName(QStringLiteral("applyNxOptimizer"));
    apply->hide();
    layout->addWidget(apply);
    connect(load_button, &QPushButton::clicked, this, &NxOptimizer::Load);
    connect(apply, &QPushButton::clicked, this, &NxOptimizer::Apply);
    connect(&installer, &Installer::Progress, this, &NxOptimizer::Progress);
    connect(&installer, &Installer::PreparedReady, this, [this] { installer.Commit(0, load); });
    connect(&installer, &Installer::Failed, this, [this](const QString& error) {
        if (config_written) {
            if (had_config) Save(config_path, old_config);
            else QFile::remove(config_path);
            config_written = false;
        }
        Fail(error);
    });
    connect(&installer, &Installer::Installed, this, [this](const Pack& pack, const QString&) {
        busy = false;
        config_written = false;
        apply->setEnabled(true);
        emit Applied(pack, memory_layout);
        emit Progress(tr("NX Optimizer applied. Restart TOTK. Original assets: MaxLastBreath, CC BY-NC 2.0."));
        emit BusyChanged();
    });
}

void NxOptimizer::Fetch(const QString& path, const QString& destination, const QString& sha,
                       std::function<void(bool)> finished) {
    auto valid = [sha](const QByteArray& bytes) {
        if (sha.isEmpty()) return !bytes.isEmpty();
        const QByteArray blob = QByteArray("blob ") + QByteArray::number(bytes.size()) + '\0' + bytes;
        return QString::fromLatin1(QCryptographicHash::hash(blob, QCryptographicHash::Sha1).toHex()) == sha;
    };
    QFile existing(destination);
    if (existing.open(QIODevice::ReadOnly) && valid(existing.readAll())) { finished(true); return; }
    existing.close();
    const QString raw = QStringLiteral("https://raw.githubusercontent.com/MaxLastBreath/nx-optimizer/%1/%2")
                            .arg(Ref, QString::fromLatin1(QUrl::toPercentEncoding(path, "/")));
    QNetworkRequest request{QUrl(raw)};
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    auto* reply = network.get(request);
    connect(reply, &QNetworkReply::downloadProgress, reply, [reply](qint64 count, qint64) { if (count > 16 * 1024 * 1024) reply->abort(); });
    connect(reply, &QNetworkReply::finished, this, [reply, destination, callback = std::move(finished), valid] {
        const auto data = reply->readAll();
        const bool success = reply->error() == QNetworkReply::NoError && valid(data) && Save(destination, data);
        reply->deleteLater();
        callback(success);
    });
}

void NxOptimizer::Load() {
    if (IsBusy()) return;
    busy = true;
    load_button->setEnabled(false);
    emit BusyChanged();
    emit Progress(tr("Loading NX Optimizer's supported versions and graphics settings..."));
    Fetch(Prefix + QStringLiteral("Options.json"), cache + QStringLiteral("/Options.json"),
          QStringLiteral("c3619956b18747ce9bed5fb4bf4ed838fd34a955"), [this](bool ok) {
        if (!ok) { Fail(tr("Could not download NX Optimizer's settings.")); return; }
        Fetch(Prefix + QStringLiteral("PatchInfo.json"), cache + QStringLiteral("/PatchInfo.json"),
              QStringLiteral("548cd11a93df6c196957f29b4adcaecd89004ade"), [this](bool info_ok) {
            if (!info_ok) { Fail(tr("Could not verify supported TOTK versions.")); return; }
            QFile file(cache + QStringLiteral("/Options.json"));
            file.open(QIODevice::ReadOnly);
            schema = QJsonDocument::fromJson(file.readAll()).object().value(QStringLiteral("Keys")).toObject();
            QFile metadata_file(cache + QStringLiteral("/PatchInfo.json"));
            metadata_file.open(QIODevice::ReadOnly);
            const auto metadata = QJsonDocument::fromJson(metadata_file.readAll()).object();
            if (!metadata.value(QStringLiteral("Versions")).toArray().contains(game.version)) {
                Fail(tr("NX Optimizer 3.2.0 does not list the detected TOTK version (%1). Update or select a supported game update first.").arg(game.version));
                return;
            }
            busy = false;
            load_button->hide();
            BuildControls();
            emit Progress(tr("NX Optimizer settings loaded. Choose your graphics settings, then apply."));
            emit BusyChanged();
        });
    });
}

void NxOptimizer::BuildControls() {
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto* content = new QWidget(scroll);
    auto* form = new QFormLayout(content);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapLongRows);
    form->setVerticalSpacing(10);
    scroll->setObjectName(QStringLiteral("nxSettingsScroll"));
    form->setContentsMargins(0, 0, 0, 0);
    const QStringList keys{QStringLiteral("fps"), QStringLiteral("menu fps"), QStringLiteral("movie fps"),
        QStringLiteral("resolution"), QStringLiteral("aspect"), QStringLiteral("shadow resolution"),
        QStringLiteral("render distance"), QStringLiteral("fxaa2"), QStringLiteral("fsr2")};
    const QString ini_path = sdmc + QStringLiteral("/UltraCam/TOTK/Config/maxlastbreath.ini");
    QSettings previous(ini_path, QSettings::IniFormat);
    for (const auto& key : keys) {
        const auto definition = schema.value(key).toObject();
        if (definition.isEmpty()) continue;
        auto* group = new QWidget(content);
        auto* row = new QVBoxLayout(group);
        row->setContentsMargins(0, 0, 0, 0);
        const auto mapping = definition.value(QStringLiteral("Config_Class")).toArray();
        const QString old_key = mapping[0].toString() + QLatin1Char('/') + mapping[1].toString();
        if (definition.value(QStringLiteral("Class")).toString() == QStringLiteral("dropdown")) {
            auto* combo = new ScrollSafeComboBox(group);
            combo->setObjectName(QStringLiteral("nx_") + key);
            const auto names = definition.value(QStringLiteral("Name_Values")).toArray();
            const auto values = definition.value(QStringLiteral("Values")).toArray();
            for (int index = 0; index < names.size(); ++index) combo->addItem(names[index].toString(), index);
            int selected = definition.value(QStringLiteral("Default")).toInt();
            if (previous.contains(old_key)) for (int index = 0; index < values.size(); ++index) {
                QString first;
                QString second;
                if (values[index].isArray()) {
                    const auto pair = values[index].toArray(); first = JsonText(pair[0]); second = JsonText(pair[1]);
                } else if (mapping.size() == 3) {
                    const auto pair = values[index].toString().split(QLatin1Char('x')); if (pair.size() == 2) { first = pair[0]; second = pair[1]; }
                } else first = JsonText(values[index]);
                if (first == previous.value(old_key).toString() && (mapping.size() != 3 ||
                    second == previous.value(mapping[0].toString() + QLatin1Char('/') + mapping[2].toString()).toString())) selected = index;
            }
            combo->setCurrentIndex(selected);
            controls[key] = [combo] { return QJsonValue(combo->currentIndex()); };
            row->addWidget(combo);
        } else if (definition.value(QStringLiteral("Class")).toString() == QStringLiteral("bool")) {
            auto* combo = new ScrollSafeComboBox(group);
            combo->setObjectName(QStringLiteral("nx_") + key);
            combo->addItems({tr("Disabled"), tr("Enabled")});
            const bool value = previous.contains(old_key) ? previous.value(old_key).toString().compare(QStringLiteral("On"), Qt::CaseInsensitive) == 0 : definition.value(QStringLiteral("Default")).toBool();
            combo->setCurrentIndex(value ? 1 : 0);
            controls[key] = [combo] { return QJsonValue(combo->currentIndex() == 1); };
            row->addWidget(combo);
        } else {
            auto* combo = new ScrollSafeComboBox(group);
            combo->setObjectName(QStringLiteral("nx_") + key);
            const auto limits = definition.value(QStringLiteral("Values")).toArray();
            int initial = previous.contains(old_key) ? qRound(previous.value(old_key).toDouble()) : definition.value(QStringLiteral("Default")).toInt();
            if (initial < limits[0].toInt() || initial > limits[1].toInt()) initial = definition.value(QStringLiteral("Default")).toInt();
            for (const int fps : {20, 30, 40, 60, 90, 120, 144, 165, 240}) if (fps >= limits[0].toInt() && fps <= limits[1].toInt()) combo->addItem(tr("%1 FPS").arg(fps), fps);
            if (combo->findData(initial) < 0) combo->addItem(tr("%1 FPS (saved setting)").arg(initial), initial);
            combo->setCurrentIndex(combo->findData(initial));
            controls[key] = [combo] { return QJsonValue(combo->currentData().toInt()); };
            row->addWidget(combo);
        }
        group->setToolTip(definition.value(QStringLiteral("Description")).toString());
        auto* label = new QLabel(definition.value(QStringLiteral("Name")).toString(), content);
        label->setBuddy(group->findChild<QComboBox*>());
        label->setToolTip(group->toolTip());
        form->addRow(label, group);
    }
    auto* notes = new QLabel(tr("Applying uses 1× emulator scaling to avoid scaling the image twice and enables extended memory when required. Existing settings are backed up. UltraCam also has an in-game menu (F12 with keyboard enabled)."), content);
    notes->setWordWrap(true);
    notes->setToolTip(notes->text());
    notes->setText(tr("1× emulator scaling · settings backed up · UltraCam menu: F12"));
    form->addRow(notes);
    scroll->setWidget(content);
    layout->insertWidget(2, scroll, 1);
    apply->show();
}

QByteArray NxOptimizer::BuildIni(const QJsonObject& options, const QMap<QString, QJsonValue>& choices,
                               const QByteArray& existing) {
    QMap<QString, QMap<QString, QString>> sections;
    QString current_section;
    for (const auto& raw : QString::fromUtf8(existing).split(QLatin1Char('\n'))) {
        const auto line = raw.trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char(';')) || line.startsWith(QLatin1Char('#'))) continue;
        if (line.startsWith(QLatin1Char('[')) && line.endsWith(QLatin1Char(']'))) {
            current_section = line.mid(1, line.size() - 2).trimmed();
        } else if (const auto separator = line.indexOf(QLatin1Char('=')); separator > 0 && !current_section.isEmpty()) {
            sections[current_section][line.left(separator).trimmed()] = line.mid(separator + 1).trimmed();
        }
    }
    for (auto it = options.begin(); it != options.end(); ++it) {
        const auto definition = it.value().toObject();
        const auto mapping = definition.value(QStringLiteral("Config_Class")).toArray();
        if (mapping.size() < 2) continue;
        auto& section = sections[mapping[0].toString()];
        // Applying a graphics setting must preserve existing FOV, camera and other
        // settings that this pane does not expose, including newer unknown keys.
        if (!choices.contains(it.key()) && section.contains(mapping[1].toString()) &&
            (mapping.size() != 3 || section.contains(mapping[2].toString()))) continue;
        const auto type = definition.value(QStringLiteral("Class")).toString();
        QJsonValue value = choices.value(it.key(), definition.value(QStringLiteral("Default")));
        if (type == QStringLiteral("dropdown")) {
            const auto values = definition.value(QStringLiteral("Values")).toArray();
            const int index = value.toInt();
            if (index < 0 || index >= values.size()) continue;
            value = values[index];
        }
        if (mapping.size() == 3) {
            QJsonArray pair;
            if (value.isArray()) pair = value.toArray();
            else { for (const auto& part : value.toString().split(QLatin1Char('x'))) pair.append(part); }
            if (pair.size() == 2) { section[mapping[1].toString()] = JsonText(pair[0]); section[mapping[2].toString()] = JsonText(pair[1]); }
        } else section[mapping[1].toString()] = JsonText(value);
    }
    QByteArray ini = "; Configured through Citron Graphic Packs. UltraCam by MaxLastBreath.\n";
    for (auto section = sections.begin(); section != sections.end(); ++section) {
        ini += '[' + section.key().toUtf8() + "]\n";
        for (auto value = section->begin(); value != section->end(); ++value) ini += value.key().toUtf8() + " = " + value.value().toUtf8() + '\n';
        ini += '\n';
    }
    return ini;
}

void NxOptimizer::Apply() {
    if (IsBusy() || schema.isEmpty()) return;
    if (confirm_apply && !confirm_apply()) return;
    busy = true;
    apply->setEnabled(false);
    emit BusyChanged();
    DownloadAssets(0);
}

void NxOptimizer::DownloadAssets(int index) {
    const QStringList paths{Prefix + QStringLiteral("UltraCam/exefs/main.npdm"), Prefix + QStringLiteral("UltraCam/exefs/subsdk3"), QStringLiteral("LICENSE.md")};
    const QStringList destinations{cache + QStringLiteral("/payload/exefs/main.npdm"), cache + QStringLiteral("/payload/exefs/subsdk3"), cache + QStringLiteral("/payload/NX-Optimizer-LICENSE.md")};
    const QStringList hashes{QStringLiteral("27a5cd21a43fac1e0aa48fbb940ff555f14a3f78"), QStringLiteral("35e76ee8ca38ab220c0874494170ce9e4a07bcb4"), QString()};
    if (index == paths.size()) { Commit(); return; }
    emit Progress(tr("Downloading NX Optimizer component %1/%2...").arg(index + 1).arg(paths.size()));
    Fetch(paths[index], destinations[index], hashes[index], [this, index](bool ok) { if (ok) DownloadAssets(index + 1); else Fail(tr("Could not download or verify an NX Optimizer component.")); });
}

void NxOptimizer::Commit() {
    QMap<QString, QJsonValue> choices;
    for (auto it = controls.begin(); it != controls.end(); ++it) choices[it.key()] = it.value()();
    config_path = sdmc + QStringLiteral("/UltraCam/TOTK/Config/maxlastbreath.ini");
    QFile existing(config_path);
    had_config = existing.exists();
    old_config.clear();
    if (had_config) {
        if (!existing.open(QIODevice::ReadOnly)) { Fail(tr("Could not back up the existing UltraCam settings.")); return; }
        old_config = existing.readAll(); existing.close();
        old_config_path = cache + QStringLiteral("/backups/") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")) + QStringLiteral("/maxlastbreath.ini");
        if (!Save(old_config_path, old_config)) { Fail(tr("Could not save the UltraCam settings backup.")); return; }
    }
    if (!Save(config_path, BuildIni(schema, choices, old_config))) { Fail(tr("Could not write UltraCam settings.")); return; }
    config_written = true;
    const auto resolution = schema.value(QStringLiteral("resolution")).toObject().value(QStringLiteral("Values")).toArray()[choices.value(QStringLiteral("resolution")).toInt()].toString().split(QLatin1Char('x'));
    const auto shadows = schema.value(QStringLiteral("shadow resolution")).toObject().value(QStringLiteral("Values")).toArray()[choices.value(QStringLiteral("shadow resolution")).toInt()].toDouble();
    const double scale = std::max(resolution[0].toDouble() * resolution[1].toDouble() / (1920.0 * 1080.0), shadows / 1024.0);
    memory_layout = scale > 5 ? 2 : scale > 1 ? 1 : 0;
    Pack pack = CatalogueEntry();
    pack.version = game.version;
    Save(cache + QStringLiteral("/payload/README.txt"), "UltraCam Calamity 3.0.0 from NX Optimizer 3.2.0\nCopyright MaxLastBreath\nhttps://github.com/MaxLastBreath/nx-optimizer/releases/tag/manager-3.2.0\nOriginal components retain CC BY-NC 2.0. See NX-Optimizer-LICENSE.md.\n");
    installer.PrepareDirectory(pack, cache + QStringLiteral("/payload"));
}

void NxOptimizer::Fail(const QString& error) {
    busy = false;
    load_button->setEnabled(true);
    apply->setEnabled(true);
    emit Progress(error);
    emit BusyChanged();
    QMessageBox::warning(this, tr("NX Optimizer"), error);
}
} // namespace GraphicPacks
