// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QDialog>
#include <QHash>
#include "catalog.h"
#include "installer.h"

class QCheckBox;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTextBrowser;
class QScrollArea;
class QTreeWidget;
class QTreeWidgetItem;

namespace GraphicPacks {
class NxOptimizer;
class Dialog final : public QDialog {
    Q_OBJECT
public:
    Dialog(QVector<Game> games, QString load_directory, QString cache_directory,
           bool editable, QWidget* parent = nullptr);
    void SetSdmcDirectory(const QString& path);

signals:
    void EnabledChanged(const QString& title_id, const QString& mod_name, bool enabled);
    void NxSettingsRequired(const QString& title_id, int memory_layout);

protected:
    void closeEvent(QCloseEvent* event) override;
    void reject() override;

private:
    void Rebuild();
    void Filter();
    void ShowDetails();
    void Toggle(QTreeWidgetItem* item);
    void PrepareSelected();
    void InstallSelected();
    bool ConfirmEnable(const Pack& pack, const QString& payload);
    void SetEnabled(const Pack& pack, bool enabled);
    void UpdateBusy();
    const Game* FindGame(const Pack& pack) const;
    QVector<Pack> InstalledPacks() const;
    const Pack* Selected() const;

    QVector<Game> games;
    QString load_directory;
    bool editable;
    bool rebuilding = false;
    QString selected_id;
    QString prepared_id;
    QString source_notes;
    bool auto_install_requested = false;
    QStringList nx_conflicts;
    NxOptimizer* nx;
    QVector<Pack> packs;
    QHash<QString, QStringList> unlisted_enabled_mods;
    Catalog catalog;
    Installer installer;
    QLineEdit* filter;
    QCheckBox* installed_only;
    QCheckBox* show_incompatible;
    QTreeWidget* tree;
    QScrollArea* category_panel;
    QLabel* title;
    QLabel* metadata;
    QLabel* legacy_notice;
    QTextBrowser* description;
    QComboBox* variants;
    QLabel* variant_label;
    QCheckBox* author_notes;
    QPushButton* primary;
    QPushButton* source;
    QPushButton* folder;
    QPushButton* update;
    QPushButton* close;
    QLabel* status;
};
} // namespace GraphicPacks
