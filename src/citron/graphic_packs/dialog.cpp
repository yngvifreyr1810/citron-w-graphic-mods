// SPDX-License-Identifier: GPL-2.0-or-later
#include "dialog.h"
#include "nx_optimizer.h"

#include <algorithm>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QGroupBox>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QScrollArea>
#include <QSplitter>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace GraphicPacks {
namespace {
constexpr int PackIndexRole = Qt::UserRole + 101;
QString CompatibilityText(Compatibility compatibility) {
    switch (compatibility) {
    case Compatibility::Match: return QStringLiteral("Matches installed version / Build ID");
    case Compatibility::Mismatch: return QStringLiteral("Different version or Build ID — do not enable");
    default: return QStringLiteral("Compatibility unverified — read the source notes");
    }
}
}

Dialog::Dialog(QVector<Game> games_, QString load_directory_, QString cache_directory,
               bool editable_, QWidget* parent)
    : QDialog(parent), games(std::move(games_)), load_directory(std::move(load_directory_)),
      editable(editable_), catalog(cache_directory + QStringLiteral("/catalogue"), this),
      installer(cache_directory, this) {
    setWindowTitle(tr("Graphic packs"));
    setObjectName(QStringLiteral("graphicPacksDialog"));
    resize(1100, 740);
    setMinimumSize(760, 470);

    auto* layout = new QVBoxLayout(this);
    auto* filters = new QHBoxLayout;
    auto* label = new QLabel(tr("&Filter"), this);
    filter = new QLineEdit(this);
    filter->setObjectName(QStringLiteral("packFilter"));
    filter->setClearButtonEnabled(true);
    label->setBuddy(filter);
    installed_only = new QCheckBox(tr("&Installed games"), this);
    installed_only->setObjectName(QStringLiteral("installedGamesOnly"));
    installed_only->setChecked(true);
    show_incompatible = new QCheckBox(tr("Show incompatible versions"), this);
    show_incompatible->setObjectName(QStringLiteral("showIncompatibleVersions"));
    show_incompatible->setToolTip(tr("Enabled installed mods remain visible so you can disable an incompatible selection."));
    filters->addWidget(label);
    filters->addWidget(filter, 1);
    filters->addWidget(installed_only);
    filters->addWidget(show_incompatible);
    layout->addLayout(filters);

    auto* splitter = new QSplitter(this);
    tree = new QTreeWidget(splitter);
    tree->setObjectName(QStringLiteral("packTree"));
    tree->setHeaderHidden(true);
    tree->setUniformRowHeights(true);
    tree->setMinimumWidth(300);
    tree->setSelectionMode(QAbstractItemView::SingleSelection);
    tree->setAccessibleName(tr("Games and graphic packs"));

    auto* detail = new QWidget(splitter);
    auto* details = new QVBoxLayout(detail);
    details->setAlignment(Qt::AlignTop);
    title = new QLabel(tr("Select a graphic pack"), detail);
    QFont heading = title->font();
    heading.setBold(true);
    heading.setPointSizeF(heading.pointSizeF() + 2);
    title->setFont(heading);
    title->setWordWrap(true);
    title->setTextFormat(Qt::PlainText);
    metadata = new QLabel(detail);
    metadata->setWordWrap(true);
    metadata->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    metadata->setTextFormat(Qt::PlainText);
    metadata->setTextInteractionFlags(Qt::TextSelectableByMouse);
    description = new QTextBrowser(detail);
    description->setObjectName(QStringLiteral("packDescription"));
    description->setFrameShape(QFrame::NoFrame);
    description->setOpenExternalLinks(false);
    description->setPlainText(tr("Choose a game, then Graphics, FPS or Fixes.\n\nThe catalogue includes the original community sources. Packs are downloaded individually, and installed packs stay available offline."));
    variant_label = new QLabel(tr("&Option"), detail);
    variants = new QComboBox(detail);
    variants->setObjectName(QStringLiteral("packVariant"));
    variants->setAccessibleName(tr("Pack option"));
    variant_label->setBuddy(variants);
    variants->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    variants->setMinimumContentsLength(15);
    variants->setEnabled(false);
    primary = new QPushButton(tr("Download && enable"), detail);
    primary->setObjectName(QStringLiteral("installPack"));
    primary->setEnabled(false);
    auto* links = new QHBoxLayout;
    source = new QPushButton(tr("View source"), detail);
    folder = new QPushButton(tr("Open mod folder"), detail);
    source->setEnabled(false);
    folder->setEnabled(false);
    links->addWidget(source);
    links->addWidget(folder);
    auto* title_group = new QGroupBox(tr("Graphic pack"), detail);
    title_group->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* title_layout = new QVBoxLayout(title_group);
    title_layout->addWidget(title);
    details->addWidget(title_group);
    details->addWidget(metadata);
    legacy_notice = new QLabel(detail);
    legacy_notice->setObjectName(QStringLiteral("unlistedModsNotice"));
    legacy_notice->setWordWrap(true);
    legacy_notice->setTextFormat(Qt::PlainText);
    legacy_notice->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    legacy_notice->hide();
    details->addWidget(legacy_notice);
    description->setMaximumHeight(130);
    description->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    details->addWidget(description);
    category_panel = new QScrollArea(detail);
    category_panel->setObjectName(QStringLiteral("categorySettings"));
    category_panel->setWidgetResizable(true);
    category_panel->setFrameShape(QFrame::NoFrame);
    category_panel->hide();
    details->addWidget(category_panel, 1);
    author_notes = new QCheckBox(tr("Description, compatibility and credits"), detail);
    details->addWidget(author_notes);
    details->addWidget(variant_label);
    details->addWidget(variants);
    details->addWidget(primary);
    details->addLayout(links);
    Game totk;
    totk.title_id = QStringLiteral("0100F2C0115B6000");
    for (const auto& game : games) if (game.title_id == totk.title_id) totk = game;
    nx = new NxOptimizer(totk, load_directory, cache_directory,
                         QFileInfo(load_directory).dir().filePath(QStringLiteral("sdmc")), detail);
    nx->setEnabled(editable && !totk.version.isEmpty());
    nx->hide();
    details->insertWidget(details->count() - 1, nx, 1);
    nx->SetConfirmApply([this] {
        nx_conflicts.clear();
        for (const auto& pack : InstalledPacks()) {
            if (pack.title_id == QStringLiteral("0100F2C0115B6000") && pack.enabled && pack.id != NxOptimizer::CatalogueEntry().id &&
                pack.name.contains(QRegularExpression(QStringLiteral("fps|dfps|ultracam|optimizer|visualfix|1080p|lod|lens.flare|shadow|island.fix"), QRegularExpression::CaseInsensitiveOption))) nx_conflicts << pack.mod_name;
        }
        if (nx_conflicts.isEmpty()) return true;
        return QMessageBox::question(this, tr("Replace overlapping TOTK graphics mods"),
            tr("NX Optimizer replaces these enabled graphics/FPS alternatives:\n\n%1\n\nApply NX Optimizer and disable these alternatives? Their files will be preserved.")
                .arg(nx_conflicts.join(QLatin1Char('\n'))), QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes;
    });
    connect(nx, &NxOptimizer::Progress, this, [this](const QString& text) { status->setText(text); });
    connect(nx, &NxOptimizer::BusyChanged, this, &Dialog::UpdateBusy);
    connect(nx, &NxOptimizer::Applied, this, [this](const Pack& pack, int memory) {
        for (const auto& installed : InstalledPacks()) if (nx_conflicts.contains(installed.mod_name)) SetEnabled(installed, false);
        SetEnabled(pack, true);
        emit NxSettingsRequired(pack.title_id, memory);
        Rebuild();
        UpdateBusy();
    });
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({490, 490});
    layout->addWidget(splitter, 1);

    status = new QLabel(this);
    status->setObjectName(QStringLiteral("packStatus"));
    status->setTextFormat(Qt::PlainText);
    status->setWordWrap(true);
    layout->addWidget(status);
    auto* footer = new QHBoxLayout;
    auto* reload = new QPushButton(tr("Reload installed packs"), this);
    update = new QPushButton(tr("Download latest community graphic packs"), this);
    update->setObjectName(QStringLiteral("updateCatalogue"));
    update->setToolTip(tr("Refreshes the community sources. Mod files download only when you install a pack."));
    close = new QPushButton(tr("Close"), this);
    close->setAutoDefault(false);
    footer->addWidget(reload);
    footer->addStretch(1);
    footer->addWidget(update);
    footer->addWidget(close);
    layout->addLayout(footer);

    connect(filter, &QLineEdit::textChanged, this, &Dialog::Filter);
    connect(installed_only, &QCheckBox::toggled, this, &Dialog::Filter);
    connect(author_notes, &QCheckBox::toggled, this, &Dialog::ShowDetails);
    connect(show_incompatible, &QCheckBox::toggled, this, &Dialog::Filter);
    connect(tree, &QTreeWidget::itemSelectionChanged, this, &Dialog::ShowDetails);
    connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int) { Toggle(item); });
    connect(close, &QPushButton::clicked, this, &Dialog::accept);
    connect(reload, &QPushButton::clicked, this, [this] { if (!installer.IsBusy()) Rebuild(); });
    connect(update, &QPushButton::clicked, &catalog, &Catalog::Refresh);
    connect(&catalog, &Catalog::Changed, this, &Dialog::Rebuild);
    connect(&catalog, &Catalog::Progress, this, [this](const QString& text) { status->setText(text); UpdateBusy(); });
    connect(&catalog, &Catalog::RefreshFinished, this, [this](const QString& text) { status->setText(text); UpdateBusy(); });
    connect(&catalog, &Catalog::NotesReady, this, [this](const QString& id, const QString& notes) {
        if (id != selected_id) return;
        source_notes = notes;
        if (const auto* pack = Selected(); pack && author_notes->isChecked()) description->setPlainText(pack->description + QStringLiteral("\n\nSource notes / credits:\n") + notes);
    });
    connect(source, &QPushButton::clicked, this, [this] { if (const auto* pack = Selected()) QDesktopServices::openUrl(QUrl(SourceUrl(*pack))); });
    connect(folder, &QPushButton::clicked, this, [this] {
        if (const auto* pack = Selected()) QDesktopServices::openUrl(QUrl::fromLocalFile(load_directory + QLatin1Char('/') + pack->title_id + QLatin1Char('/') + pack->mod_name));
    });
    connect(primary, &QPushButton::clicked, this, [this] {
        if (prepared_id == selected_id) InstallSelected();
        else PrepareSelected();
    });
    connect(&installer, &Installer::Progress, this, [this](const QString& text) { status->setText(text); UpdateBusy(); });
    connect(&installer, &Installer::Failed, this, [this](const QString& text) {
        prepared_id.clear();
        status->setText(text);
        UpdateBusy();
        QMessageBox::warning(this, tr("Graphic pack"), text);
    });
    connect(&installer, &Installer::PreparedReady, this, [this] {
        prepared_id = installer.Prepared().pack.id;
        for (auto& pack : packs) if (pack.id == prepared_id) {
            pack.build_ids = installer.Prepared().pack.build_ids;
            pack.title_id = installer.Prepared().pack.title_id;
        }
        if (prepared_id == selected_id) {
            variants->clear();
            variants->addItems(installer.Prepared().labels);
            variants->setEnabled(true);
            const auto* game = FindGame(installer.Prepared().pack);
            int matched = -1;
            int matches = 0;
            for (int i = 0; game && i < installer.Prepared().directories.size(); ++i) {
                for (const auto& bid : PayloadBuildIds(installer.Prepared().directories[i])) if (BuildIdsMatch(bid, game->build_id)) { matched = i; ++matches; break; }
            }
            if (matches == 1) variants->setCurrentIndex(matched);
            variant_label->setVisible(variants->count() > 1 && matches != 1);
            variants->setVisible(variants->count() > 1 && matches != 1);
            if (auto_install_requested && (variants->count() == 1 || matches == 1)) {
                auto_install_requested = false;
                InstallSelected();
                return;
            }
        }
        status->setText(tr("Pack ready. Choose one option, then Install & enable. Nothing has been changed yet."));
        ShowDetails();
        UpdateBusy();
    });
    connect(&installer, &Installer::Installed, this, [this](const Pack& pack, const QString& backup) {
        prepared_id.clear();
        SetEnabled(pack, true);
        Rebuild();
        status->setText(backup.isEmpty() ? tr("Installed and enabled. Restart the game to apply this pack.")
                                      : tr("Installed and enabled. Previous pack backed up to %1").arg(backup));
        UpdateBusy();
    });
    Rebuild();
    filter->setFocus();
}

const Game* Dialog::FindGame(const Pack& pack) const {
    for (const auto& game : games) {
        if ((!pack.title_id.isEmpty() && pack.title_id.compare(game.title_id, Qt::CaseInsensitive) == 0) ||
            (pack.title_id.isEmpty() && NormalizeName(pack.game) == NormalizeName(game.name))) return &game;
    }
    return nullptr;
}

QVector<Pack> Dialog::InstalledPacks() const {
    QVector<Pack> installed;
    for (const auto& game : games) {
        const QString root = load_directory + QLatin1Char('/') + game.title_id;
        for (const auto& path : FindPayloads(root, game.title_id)) {
            if (QDir(root).absolutePath() == QDir(path).absolutePath()) continue;
            Pack pack;
            QFile metadata_file(path + QStringLiteral("/.graphic-pack.json"));
            if (metadata_file.open(QIODevice::ReadOnly)) pack = Pack::FromJson(QJsonDocument::fromJson(metadata_file.readAll()).object());
            pack.mod_name = QDir(root).relativeFilePath(path);
            if (pack.id.isEmpty()) pack.id = QStringLiteral("local:") + game.title_id + QLatin1Char('/') + pack.mod_name;
            if (pack.name.isEmpty()) pack.name = QFileInfo(path).fileName();
            pack.game = game.name;
            pack.title_id = game.title_id;
            if (pack.category.isEmpty()) pack.category = CategoryForName(pack.name);
            if (pack.version.isEmpty()) {
                const auto match = QRegularExpression(QStringLiteral("\\bv?(\\d+\\.\\d+\\.\\d+)\\b")).match(pack.name);
                pack.version = match.captured(1);
            }
            pack.build_ids = PayloadBuildIds(path);
            pack.local = true;
            pack.enabled = !game.disabled_mods.contains(pack.mod_name) && !game.disabled_mods.contains(QFileInfo(path).fileName());
            if (pack.description.isEmpty()) pack.description = tr("An existing installed mod. Toggling this checkbox uses the same Add-ons setting as the game's Properties window.\n\nFile and code overlap checks can detect common conflicts; they cannot prove that two mods are safe together.");
            installed.push_back(std::move(pack));
        }
    }
    return installed;
}

void Dialog::Rebuild() {
    rebuilding = true;
    const QString keep = selected_id;
    const QSignalBlocker blocker(tree);
    tree->clear();
    packs = catalog.Packs();
    packs << NxOptimizer::CatalogueEntry();
    const auto installed = InstalledPacks();
    QHash<QString, Pack> installed_by_id;
    for (const auto& pack : installed) installed_by_id.insert(pack.id, pack);
    for (auto& pack : packs) {
        if (installed_by_id.contains(pack.id)) {
            pack = installed_by_id.take(pack.id);
        } else if (const auto* game = FindGame(pack)) {
            pack.title_id = game->title_id;
            pack.game = game->name;
        }
    }
    for (const auto& pack : installed_by_id) packs << pack;
    packs = CuratePacks(std::move(packs), games);
    packs.erase(std::remove_if(packs.begin(), packs.end(), [](const Pack& pack) {
        return pack.title_id == QStringLiteral("0100F2C0115B6000") && pack.id != NxOptimizer::CatalogueEntry().id && !pack.whole_package;
    }), packs.end());
    unlisted_enabled_mods.clear();
    for (const auto& old : installed) {
        if (!old.enabled) continue;
        const bool listed = std::any_of(packs.begin(), packs.end(), [&old](const Pack& pack) {
            return pack.local && pack.id == old.id;
        });
        if (!listed) unlisted_enabled_mods[old.title_id] << old.mod_name;
    }
    std::sort(packs.begin(), packs.end(), [](const Pack& first, const Pack& second) {
        return QString::localeAwareCompare(first.game + first.category + first.name + first.version,
                                           second.game + second.category + second.name + second.version) < 0;
    });
    QMap<QString, QTreeWidgetItem*> game_items;
    QMap<QString, QTreeWidgetItem*> categories;
    QTreeWidgetItem* selection = nullptr;
    for (qsizetype i = 0; i < packs.size(); ++i) {
        auto& pack = packs[i];
        const QString key = pack.title_id.isEmpty() ? NormalizeName(pack.game) : pack.title_id;
        if (!game_items.contains(key)) {
            auto* item = new QTreeWidgetItem(tree, {pack.game});
            item->setData(0, PackIndexRole, -1);
            item->setExpanded(false);
            game_items.insert(key, item);
        }
        const QString category_name = pack.category == QStringLiteral("Graphics") ? tr("Graphics") : tr("Mods");
        const QString category_key = key + QLatin1Char('/') + category_name;
        if (!categories.contains(category_key)) {
            auto* item = new QTreeWidgetItem(game_items[key], {category_name});
            item->setData(0, PackIndexRole, -1);
            item->setExpanded(true);
            categories.insert(category_key, item);
        }
        QString text = pack.name;
        if (pack.local) text += tr("  [installed]");
        auto* item = new QTreeWidgetItem(categories[category_key], {text});
        item->setData(0, PackIndexRole, int(i));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, pack.local && pack.enabled ? Qt::Checked : Qt::Unchecked);
        if (const auto* game = FindGame(pack)) item->setToolTip(0, CompatibilityText(CheckCompatibility(pack, *game)));
        else item->setToolTip(0, tr("This game is not installed. Source details remain available."));
        if (pack.id == keep) selection = item;
    }
    rebuilding = false;
    Filter();
    if (selection) tree->setCurrentItem(selection);
    if (selection) {
        selection->parent()->setExpanded(true);
        selection->parent()->parent()->setExpanded(true);
    }
    ShowDetails();
    status->setText(editable ? tr("%1 community and installed packs. Check a downloaded pack to enable it; select a community pack to download it.").arg(packs.size())
                             : tr("Stop emulation before installing or changing graphic packs."));
}

void Dialog::Filter() {
    const QString query = filter->text().trimmed();
    int visible = 0;
    for (int g = 0; g < tree->topLevelItemCount(); ++g) {
        auto* game = tree->topLevelItem(g);
        bool game_visible = false;
        for (int c = 0; c < game->childCount(); ++c) {
            auto* category = game->child(c);
            bool category_visible = false;
            for (int p = 0; p < category->childCount(); ++p) {
                auto* item = category->child(p);
                const auto& pack = packs[item->data(0, PackIndexRole).toInt()];
                const QString searchable = pack.game + QLatin1Char(' ') + pack.name + QLatin1Char(' ') + pack.category + QLatin1Char(' ') + pack.version + QLatin1Char(' ') + pack.repository + QLatin1Char(' ') + pack.title_id + QLatin1Char(' ') + pack.build_ids.join(QLatin1Char(' '));
                const auto* installed_game = FindGame(pack);
                const bool mismatch = installed_game && CheckCompatibility(pack, *installed_game) == Compatibility::Mismatch;
                const bool show = (!installed_only->isChecked() || installed_game) &&
                                  (show_incompatible->isChecked() || !mismatch || (pack.local && pack.enabled)) &&
                                  searchable.contains(query, Qt::CaseInsensitive);
                item->setHidden(!show);
                category_visible |= show;
                visible += show;
            }
            category->setHidden(!category_visible);
            game_visible |= category_visible;
        }
        game->setHidden(!game_visible);
        if (!query.isEmpty() && game_visible) game->setExpanded(true);
    }
    if (visible == 0) status->setText(tr("No packs match this filter. Clear the filter or uncheck Installed games to browse the full catalogue."));
}

const Pack* Dialog::Selected() const {
    const auto* item = tree->currentItem();
    if (!item) return nullptr;
    if (!item->parent() && item->childCount()) item = item->child(0);
    if (item->data(0, PackIndexRole).toInt() < 0 && item->childCount() == 1) {
        const auto* child = item->child(0);
        const int candidate = child->data(0, PackIndexRole).toInt();
        if (candidate >= 0 && candidate < packs.size() && packs[candidate].id == NxOptimizer::CatalogueEntry().id) item = child;
    }
    const int index = item->data(0, PackIndexRole).toInt();
    return index >= 0 && index < packs.size() ? &packs[index] : nullptr;
}

void Dialog::ShowDetails() {
    if (auto* item = tree->currentItem(); item && !item->parent()) {
        item->setExpanded(true);
        for (int i = 0; i < item->childCount(); ++i) {
            if (!item->child(i)->isHidden()) {
                tree->setCurrentItem(item->child(i));
                return;
            }
        }
    }
    const auto* pack = Selected();
    category_panel->hide();
    const bool is_nx = pack && pack->id == NxOptimizer::CatalogueEntry().id;
    const bool author_bundle = pack && pack->whole_package;
    description->setMaximumHeight(author_bundle ? QWIDGETSIZE_MAX : 130);
    description->setSizePolicy(QSizePolicy::Preferred,
                               author_bundle ? QSizePolicy::Expanding : QSizePolicy::Maximum);
    const auto unlisted = pack ? unlisted_enabled_mods.value(pack->title_id) : QStringList{};
    legacy_notice->setVisible(!unlisted.isEmpty());
    legacy_notice->setText(tr("%1 other installed mod(s) remain enabled outside this curated list. Review Properties > Add-ons for details.").arg(unlisted.size()));
    legacy_notice->setToolTip(unlisted.join(QLatin1Char('\n')));
    nx->setVisible(is_nx);
    if (is_nx && isVisible()) nx->EnsureLoaded();
    primary->setVisible(!is_nx);
    description->setVisible(pack && (pack->whole_package || author_notes->isChecked()));
    metadata->setVisible(pack && author_notes->isChecked());
    author_notes->setVisible(pack != nullptr);
    variant_label->setVisible(!is_nx && prepared_id == selected_id && variants->count() > 1);
    variants->setVisible(variant_label->isVisible());
    if (!pack) {
        selected_id.clear();
        title->setText(tr("Select a graphic pack"));
        metadata->clear();
        description->setPlainText(tr("Choose a game, then Graphics or Mods."));
        auto* category = tree->currentItem();
        if (category && !category->parent()) {
            category->setExpanded(true);
            for (int i = 0; i < category->childCount(); ++i) {
                if (!category->child(i)->isHidden()) {
                    tree->setCurrentItem(category->child(i));
                    return;
                }
            }
        }
        if (category && category->parent() && category->childCount()) {
            title->setText(category->parent()->text(0) + QStringLiteral(" — ") + category->text(0));
            auto* content = new QWidget(category_panel);
            auto* rows = new QVBoxLayout(content);
            rows->setContentsMargins(0, 0, 0, 0);
            for (int i = 0; i < category->childCount(); ++i) {
                auto* leaf = category->child(i);
                if (leaf->isHidden()) continue;
                const int index = leaf->data(0, PackIndexRole).toInt();
                const auto& effect = packs[index];
                auto* group = new QGroupBox(content);
                auto* row = new QHBoxLayout(group);
                auto* enabled = new QCheckBox(effect.name, group);
                enabled->setChecked(effect.local && effect.enabled);
                enabled->setEnabled(editable && !installer.IsBusy() && FindGame(effect));
                enabled->setToolTip(leaf->toolTip(0));
                row->addWidget(enabled, 1);
                auto* options = new QPushButton(effect.local ? tr("Details / options") : tr("Download / options"), group);
                row->addWidget(options);
                connect(options, &QPushButton::clicked, this, [this, leaf] { tree->setCurrentItem(leaf); });
                connect(enabled, &QCheckBox::clicked, this, [leaf](bool on) {
                    leaf->setCheckState(0, on ? Qt::Checked : Qt::Unchecked);
                });
                rows->addWidget(group);
            }
            rows->addStretch();
            delete category_panel->takeWidget();
            category_panel->setWidget(content);
            category_panel->show();
        }
        primary->hide();
        primary->setEnabled(false);
        source->setEnabled(false);
        folder->setEnabled(false);
        return;
    }
    if (selected_id != pack->id) {
        selected_id = pack->id;
        source_notes.clear();
        variants->clear();
        variants->setEnabled(false);
        if (prepared_id == selected_id) {
            variants->addItems(installer.Prepared().labels);
            variants->setEnabled(true);
        } else if (!pack->variant.isEmpty()) variants->addItem(pack->variant);
    }
    title->setText(is_nx ? tr("Graphics — NX Optimizer / UltraCam") : pack->name);
    QString info = tr("%1\nSource: %2").arg(pack->game, pack->repository.isEmpty() ? tr("Installed locally") : pack->repository);
    if (const auto* game = FindGame(*pack)) {
        info += tr("\nGame version: %1").arg(game->version.isEmpty() ? tr("Unknown") : game->version);
        if (!is_nx) info += QLatin1Char('\n') + CompatibilityText(CheckCompatibility(*pack, *game));
        if (author_notes->isChecked()) info += tr("\nInstalled Build ID: %1\nPack Build ID: %2").arg(
            game->build_id.isEmpty() ? tr("Unknown") : game->build_id.left(16),
            pack->build_ids.isEmpty() ? tr("Not specified") : pack->build_ids.join(QStringLiteral(", ")));
    }
    metadata->setText(info);
    if (author_notes->isChecked() && !unlisted.isEmpty())
        metadata->setText(info + tr("\nOther enabled mods:\n%1").arg(unlisted.join(QLatin1Char('\n'))));
    description->setPlainText(pack->description + (!author_notes->isChecked() || source_notes.isEmpty() ? QString() : QStringLiteral("\n\nSource notes / credits:\n") + source_notes));
    source->setEnabled(!pack->repository.isEmpty());
    folder->setEnabled(pack->local);
    UpdateBusy();
    if (author_notes->isChecked()) catalog.RequestNotes(*pack);
}

void Dialog::Toggle(QTreeWidgetItem* item) {
    if (rebuilding) return;
    const int index = item->data(0, PackIndexRole).toInt();
    if (index < 0 || index >= packs.size()) return;
    const Pack pack = packs[index];
    const bool enabled = item->checkState(0) == Qt::Checked;
    if (!editable || installer.IsBusy() || !FindGame(pack)) {
        const QSignalBlocker blocker(tree);
        item->setCheckState(0, pack.local && pack.enabled ? Qt::Checked : Qt::Unchecked);
        return;
    }
    tree->setCurrentItem(item);
    if (!pack.local) {
        const QSignalBlocker blocker(tree);
        item->setCheckState(0, Qt::Unchecked);
        if (pack.id == NxOptimizer::CatalogueEntry().id) status->setText(tr("Select your NX Optimizer settings on the right, then Apply."));
        else PrepareSelected();
        return;
    }
    if (enabled && !ConfirmEnable(pack, load_directory + QLatin1Char('/') + pack.title_id + QLatin1Char('/') + pack.mod_name)) {
        const QSignalBlocker blocker(tree);
        item->setCheckState(0, Qt::Unchecked);
        return;
    }
    SetEnabled(pack, enabled);
    packs[index].enabled = enabled;
    status->setText(tr("Saved. Restart the game to apply the new pack selection."));
}

bool Dialog::ConfirmEnable(const Pack& pack, const QString& payload) {
    const auto* game = FindGame(pack);
    if (!game) return false;
    const auto compatibility = CheckCompatibility(pack, *game);
    if (compatibility == Compatibility::Mismatch) {
        QMessageBox::warning(this, tr("Incompatible graphic pack"), tr("This pack targets a different version or Build ID. Select a pack for the installed update instead."));
        return false;
    }
    const QStringList conflicts = ConflictingMods(payload, load_directory + QLatin1Char('/') + game->title_id, game->disabled_mods, pack.mod_name);
    QString question;
    if (compatibility == Compatibility::Unknown) question = tr("Compatibility with this game build is not confirmed. Read the source notes before enabling it.\n\n");
    if (!conflicts.isEmpty()) question += tr("This pack overlaps files or code changed by these enabled mods:\n%1\n\nDisable overlapping alternatives if they are not intended to work together.\n\n").arg(conflicts.join(QLatin1Char('\n')));
    if (question.isEmpty()) return true;
    question += tr("Enable this pack?");
    return QMessageBox::question(this, tr("Graphic pack compatibility"), question, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) == QMessageBox::Yes;
}

void Dialog::SetEnabled(const Pack& pack, bool enabled) {
    for (auto& game : games) {
        if (game.title_id != pack.title_id) continue;
        game.disabled_mods.removeAll(pack.mod_name);
        game.disabled_mods.removeAll(QFileInfo(pack.mod_name).fileName());
        if (!enabled) game.disabled_mods << pack.mod_name;
        break;
    }
    emit EnabledChanged(pack.title_id, pack.mod_name, enabled);
}

void Dialog::PrepareSelected() {
    const auto* pack = Selected();
    if (!pack || !FindGame(*pack) || !editable || installer.IsBusy()) return;
    prepared_id.clear();
    auto_install_requested = true;
    installer.Prepare(*pack);
    UpdateBusy();
}

void Dialog::InstallSelected() {
    if (prepared_id != selected_id || installer.IsBusy() || !editable) return;
    const int index = variants->currentIndex();
    const auto& prepared = installer.Prepared();
    if (index < 0 || index >= prepared.directories.size()) return;
    Pack pack = prepared.pack;
    pack.mod_name = ModDirectoryName(pack);
    const auto ids = PayloadBuildIds(prepared.directories[index]);
    if (!ids.isEmpty()) pack.build_ids = ids;
    if (!ConfirmEnable(pack, prepared.directories[index])) return;
    installer.Commit(index, load_directory);
    UpdateBusy();
}

void Dialog::UpdateBusy() {
    const bool busy = installer.IsBusy() || nx->IsBusy();
    tree->setEnabled(!busy);
    close->setEnabled(!busy);
    update->setEnabled(!busy && !catalog.IsRefreshing());
    const auto* pack = Selected();
    primary->setText(prepared_id == selected_id && !prepared_id.isEmpty() ? tr("Install && enable") : tr("Download && enable"));
    primary->setEnabled(!busy && editable && pack && FindGame(*pack) && !pack->repository.isEmpty());
    variants->setEnabled(!busy && prepared_id == selected_id && variants->count() > 0);
}

void Dialog::closeEvent(QCloseEvent* event) {
    if (installer.IsBusy() || nx->IsBusy()) event->ignore();
    else QDialog::closeEvent(event);
}

void Dialog::reject() {
    if (!installer.IsBusy() && !nx->IsBusy()) QDialog::reject();
}

void Dialog::SetSdmcDirectory(const QString& path) { nx->SetSdmcDirectory(path); }
} // namespace GraphicPacks
