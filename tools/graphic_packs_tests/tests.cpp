// SPDX-License-Identifier: GPL-2.0-or-later
#include <QCheckBox>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLineEdit>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest>
#include "catalog.h"
#include "dialog.h"
#include "installer.h"
#include "pack.h"
#include "nx_optimizer.h"
#include "scroll_safe_combo.h"
#include "../../src/citron/game_list_addons.h"
#include <QLabel>
#include <QScrollArea>
#include <QTextBrowser>

using namespace GraphicPacks;
namespace {
void Write(const QString& path, const QByteArray& bytes) {
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) file.write(bytes);
}
}

class PackTests : public QObject {
    Q_OBJECT
private slots:
    void actualAuthorArchivesStayIntact() {
        const QString fixtures = qEnvironmentVariable("CITRON_BUNDLE_FIXTURES");
        if (fixtures.isEmpty()) QSKIP("Set CITRON_BUNDLE_FIXTURES to independently downloaded upstream archives for the optional integration check.");
        QTemporaryDir temp;
        Catalog catalog(temp.path() + QStringLiteral("/catalog"));
        for (const auto& test : QVector<QStringList>{
                 {QStringLiteral("Super Mario Odyssey"), QStringLiteral("0100000000010000"), QStringLiteral("SMO-release_1.0.3.rar")},
                 {QStringLiteral("The Legend of Zelda Tears of the Kingdom"), QStringLiteral("0100F2C0115B6000"), QStringLiteral("TOTK-release_1.2.1.rar")}}) {
            Pack pack;
            for (const auto& candidate : catalog.Packs()) if (candidate.whole_package && candidate.game == test[0] && candidate.repository.startsWith(QStringLiteral("cucholix/"))) pack = candidate;
            QVERIFY(!pack.id.isEmpty());
            pack.title_id = test[1];
            Installer installer(temp.path() + QStringLiteral("/cache"));
            QSignalSpy ready(&installer, &Installer::PreparedReady);
            QSignalSpy installed(&installer, &Installer::Installed);
            QSignalSpy errors(&installer, &Installer::Failed);
            installer.PrepareLocal(pack, fixtures + QLatin1Char('/') + test[2]);
            QTRY_VERIFY_WITH_TIMEOUT(ready.count() || errors.count(), 15000);
            QVERIFY2(errors.isEmpty(), errors.isEmpty() ? "" : qPrintable(errors[0][0].toString()));
            QCOMPARE(installer.Prepared().directories.size(), 1);
            QCOMPARE(installer.Prepared().labels.size(), 1);
            const QString staged = installer.Prepared().directories.front();
            QVERIFY(QFileInfo::exists(staged + QStringLiteral("/exefs")));
            if (test[1] == QStringLiteral("0100000000010000")) QVERIFY(QFileInfo::exists(staged + QStringLiteral("/romfs/SystemData/GraphicsPreset.szs")));
            installer.Commit(0, temp.path() + QStringLiteral("/load"));
            QTRY_VERIFY_WITH_TIMEOUT(installed.count() || errors.count(), 15000);
            QVERIFY(errors.isEmpty());
            QCOMPARE(installed.count(), 1);
            const QString game_root = temp.path() + QStringLiteral("/load/") + pack.title_id;
            QCOMPARE(FindPayloads(game_root, pack.title_id).size(), 1);
            QFile manifest(FindPayloads(game_root, pack.title_id).front() + QStringLiteral("/.graphic-pack.json"));
            QVERIFY(manifest.open(QIODevice::ReadOnly));
            QVERIFY(Pack::FromJson(QJsonDocument::fromJson(manifest.readAll()).object()).whole_package);
        }
    }
    void wholeAuthorBundlesAcrossGames() {
        QTemporaryDir temp;
        Catalog catalog(temp.path());
        int bundles = 0;
        bool odyssey = false, totk = false;
        for (const auto& pack : catalog.Packs()) {
            if (pack.repository != QStringLiteral("cucholix/NX-IPS-romfs-compilation")) continue;
            ++bundles;
            QVERIFY(pack.whole_package);
            QVERIFY(QFileInfo(pack.path).fileName().startsWith(QStringLiteral("release_")));
            QVERIFY(!pack.path.endsWith(QStringLiteral(".pchtxt")));
            QVERIFY(!pack.description.isEmpty());
            const auto restored = Pack::FromJson(pack.ToJson());
            QVERIFY(restored.whole_package);
            if (pack.game == QStringLiteral("Super Mario Odyssey")) {
                odyssey = true;
                QCOMPARE(pack.version, QStringLiteral("1.3.0"));
                QVERIFY(pack.description.contains(QStringLiteral("interlacing")));
            }
            if (pack.game.contains(QStringLiteral("Tears of the Kingdom"))) totk = true;
        }
        QVERIFY(bundles > 100);
        QVERIFY(odyssey && totk);
    }
    void assemblesIpsAndRomfsAsOnePackage() {
        QTemporaryDir temp;
        const QString extracted = temp.path() + QStringLiteral("/archive");
        const QString output = temp.path() + QStringLiteral("/bundle");
        const QString tid = QStringLiteral("0100000000010000");
        Write(extracted + QStringLiteral("/part-a/exefs/a.ips"), "PATCH-original-ips");
        Write(extracted + QStringLiteral("/atmosphere/exefs_patches/Graphics/b.ips"), "IPS32-original-second");
        Write(extracted + QStringLiteral("/atmosphere/contents/") + tid + QStringLiteral("/romfs/SystemData/test.bin"), "original-romfs");
        Write(extracted + QStringLiteral("/atmosphere/contents/0100152000022000/romfs/other.bin"), "other-game");
        QString error;
        QVERIFY2(AssembleAuthorBundle(extracted, tid, output, error), qPrintable(error));
        QCOMPARE(FindPayloads(output, tid), QStringList{output});
        QVERIFY(QFileInfo::exists(output + QStringLiteral("/exefs/a.ips")));
        QVERIFY(QFileInfo::exists(output + QStringLiteral("/exefs/b.ips")));
        QFile romfs(output + QStringLiteral("/romfs/SystemData/test.bin"));
        QVERIFY(romfs.open(QIODevice::ReadOnly));
        QCOMPARE(romfs.readAll(), QByteArray("original-romfs"));
        QVERIFY(!QFileInfo::exists(output + QStringLiteral("/romfs/other.bin")));
    }
    void authorBundleDoesNotOverwriteConflictingAlternatives() {
        QTemporaryDir temp;
        Write(temp.path() + QStringLiteral("/archive/one/romfs/file.bin"), "option one");
        Write(temp.path() + QStringLiteral("/archive/two/romfs/file.bin"), "option two");
        QString error;
        QVERIFY(!AssembleAuthorBundle(temp.path() + QStringLiteral("/archive"), QStringLiteral("0100000000010000"), temp.path() + QStringLiteral("/output"), error));
        QVERIFY(error.contains(QStringLiteral("conflicting alternatives")));
    }
    void selectingCategoryShowsControlsAndCollapsesDescription() {
        QTemporaryDir temp;
        Game game{QStringLiteral("010028600EBDA000"), QStringLiteral("Super Mario 3D World"), QStringLiteral("1.2.2"), QStringLiteral("B424BE150A8E7D78")};
        Write(temp.path() + QStringLiteral("/load/") + game.title_id + QStringLiteral("/Disable-DOF v1.2.2/exefs/test.pchtxt"),
              "@nsobid-B424BE150A8E7D78\n@enabled\n00001000 AABB\n@stop\n");
        Dialog dialog({game}, temp.path() + QStringLiteral("/load"), temp.path() + QStringLiteral("/cache"), true);
        auto* tree = dialog.findChild<QTreeWidget*>(QStringLiteral("packTree"));
        auto* panel = dialog.findChild<QScrollArea*>(QStringLiteral("categorySettings"));
        auto* description = dialog.findChild<QTextBrowser*>(QStringLiteral("packDescription"));
        QVERIFY(tree && panel && description);
        for (int i = 0; i < tree->topLevelItemCount(); ++i) {
            auto* item = tree->topLevelItem(i);
            if (item->text(0) != game.name) continue;
            tree->setCurrentItem(item);
            QVERIFY(tree->currentItem()->parent() == item);
            QVERIFY(!panel->isHidden());
            QVERIFY(!panel->findChildren<QCheckBox*>().isEmpty());
            QVERIFY(description->isHidden());
            return;
        }
        QFAIL("Installed game category not found");
    }
    void librarySummaryKeepsVersionAndEnabledCounts() {
        const auto text = GameListPresentation::SummarizeAddons(QStringLiteral(
            "NAND Files/Update v1.2.1\n[D] External Files/Update v1.1.0\nDLC 0001\n"
            "GraphicPack_NX (IPSwitch/IPS)\nController UI (LayeredFS)\n[D] Old 60 FPS (IPSwitch/IPS)\n"));
        QCOMPARE(text, QStringLiteral("Update v1.2.1\nDLC (1) · 2 mods"));
        QVERIFY(!text.contains(QStringLiteral("Old 60 FPS")));
        QCOMPARE(GameListPresentation::SummarizeAddons({}), QString());
        QCOMPARE(GameListPresentation::SummarizeAddons(QStringLiteral("[D] Old graphics\n")), QStringLiteral("Mods disabled"));
    }
    void refreshesVersionedAddonCacheOnlyForSelectedGame() {
        QTemporaryDir temp;
        const QString id = QStringLiteral("0100F2C0115B6000");
        Write(temp.path() + QLatin1Char('/') + id + QStringLiteral(".pv.txt"), "old");
        Write(temp.path() + QLatin1Char('/') + id + QStringLiteral(".pv_v610_marquee.txt"), "stale");
        const QString unrelated = temp.path() + QStringLiteral("/0100000000010000.pv_v610_marquee.txt");
        Write(unrelated, "other game");
        QVERIFY(InvalidatePatchCache(temp.path(), id));
        QVERIFY(!QFileInfo::exists(temp.path() + QLatin1Char('/') + id + QStringLiteral(".pv_v610_marquee.txt")));
        QVERIFY(!QFileInfo::exists(temp.path() + QLatin1Char('/') + id + QStringLiteral(".pv.txt")));
        QVERIFY(QFileInfo::exists(unrelated));
        QVERIFY(!InvalidatePatchCache(temp.path(), QStringLiteral("*")));
    }
    void wheelDoesNotChangeClosedSetting() {
        ScrollSafeComboBox combo;
        combo.addItems({QStringLiteral("60 FPS"), QStringLiteral("120 FPS"), QStringLiteral("144 FPS")});
        combo.show();
        combo.setFocus();
        QWheelEvent wheel(QPointF(5, 5), QPointF(combo.mapToGlobal(QPoint(5, 5))),
                          QPoint(), QPoint(0, -120), Qt::NoButton, Qt::NoModifier,
                          Qt::NoScrollPhase, false);
        QApplication::sendEvent(&combo, &wheel);
        QCOMPARE(combo.currentIndex(), 0);
        QVERIFY(!wheel.isAccepted());
        QTest::keyClick(&combo, Qt::Key_Down);
        QCOMPARE(combo.currentIndex(), 1);
    }
    void warnsAboutEnabledUnlistedLegacyMod() {
        QTemporaryDir temp;
        Game game{QStringLiteral("010028600EBDA000"), QStringLiteral("Super Mario 3D World"), QStringLiteral("1.2.2"), QStringLiteral("B424BE150A8E7D78")};
        const QString root = temp.path() + QStringLiteral("/load/") + game.title_id;
        const QByteArray patch("@nsobid-B424BE150A8E7D78\n@enabled\n00001000 AABB\n@stop\n");
        Write(root + QStringLiteral("/Disable-DOF v1.2.2/exefs/test.pchtxt"), patch);
        Write(root + QStringLiteral("/Force 60FPS and Disable Dynamic Resolution v1.2.2/exefs/test.pchtxt"), patch);
        Dialog dialog({game}, temp.path() + QStringLiteral("/load"), temp.path() + QStringLiteral("/cache"), true);
        auto* tree = dialog.findChild<QTreeWidget*>(QStringLiteral("packTree"));
        const auto items = tree->findItems(QStringLiteral("Depth of field"), Qt::MatchContains | Qt::MatchRecursive);
        QTreeWidgetItem* installed = nullptr;
        for (auto* item : items) if (item->text(0).contains(QStringLiteral("[installed]")) &&
            item->parent()->parent()->text(0) == game.name) installed = item;
        QVERIFY(installed);
        tree->setCurrentItem(installed);
        auto* notice = dialog.findChild<QLabel*>(QStringLiteral("unlistedModsNotice"));
        QVERIFY(notice && !notice->isHidden());
        QVERIFY(notice->toolTip().contains(QStringLiteral("Force 60FPS")));
        QVERIFY(tree->findItems(QStringLiteral("Dynamic Resolution"), Qt::MatchContains | Qt::MatchRecursive).isEmpty());
    }
    void paths() {
        QVERIFY(IsSafeRelativePath(QStringLiteral("game/option/exefs/ABC.ips")));
        for (const auto& path : {QStringLiteral("../save"), QStringLiteral("game/../../save"),
                                 QStringLiteral("C:/Windows/file"), QStringLiteral("\\\\host\\file"),
                                 QStringLiteral("game\\..\\save"), QStringLiteral("game/file:stream"),
                                 QStringLiteral("game/.. /file")}) QVERIFY(!IsSafeRelativePath(path));
    }
    void compatibility() {
        Game game{QStringLiteral("0100000000010000"), QStringLiteral("Super Mario Odyssey"),
                  QStringLiteral("1.3.0"), QStringLiteral("B424BE150A8E7D78701CBE7A439D9EBF00000000")};
        Pack pack;
        QCOMPARE(CheckCompatibility(pack, game), Compatibility::Unknown);
        pack.version = QStringLiteral("1.0.0");
        QCOMPARE(CheckCompatibility(pack, game), Compatibility::Mismatch);
        pack.build_ids = {QStringLiteral("b424be150a8e7d78")};
        QCOMPARE(CheckCompatibility(pack, game), Compatibility::Match);
        pack.build_ids = {QStringLiteral("FFFFFFFFFFFFFFFF")};
        pack.version = game.version;
        QCOMPARE(CheckCompatibility(pack, game), Compatibility::Mismatch);
        QVERIFY(!BuildIdsMatch(QStringLiteral("ABC"), QStringLiteral("ABC")));
    }
    void patchMetadata() {
        Pack pack;
        InspectPatch(pack, "@nsobid-B424BE150A8E7D78701CBE7A439D9EBF\n# Super Mario Odyssey [0100000000010800]\n");
        QCOMPARE(pack.title_id, QStringLiteral("0100000000010000"));
        QCOMPARE(pack.build_ids.front(), QStringLiteral("B424BE150A8E7D78701CBE7A439D9EBF"));
        const auto restored = Pack::FromJson(pack.ToJson());
        QCOMPARE(restored.title_id, pack.title_id);
        QCOMPARE(restored.build_ids, pack.build_ids);
    }
    void cheatAlternatives() {
        const auto options = SplitCheats("{Master Code}\n580F0000 00000001\n[60 FPS]\n04000000 00010000 00000001\n[30 FPS]\n04000000 00010000 00000002\n");
        QCOMPARE(options.size(), 2);
        QVERIFY(options[0].content.contains("{Master Code}"));
        QVERIFY(options[1].content.contains("{Master Code}"));
        QVERIFY(!options[0].content.contains("[30 FPS]"));
        QVERIFY(!options[1].content.contains("[60 FPS]"));
    }
    void sourceAdapters() {
        QTemporaryDir temp;
        Catalog catalogue(temp.path());
        QVERIFY(catalogue.Packs().size() > 20);
        QSet<QString> sources;
        bool odyssey = false;
        bool mk8 = false;
        for (const auto& pack : catalogue.Packs()) {
            sources.insert(pack.repository);
            QVERIFY(!pack.id.isEmpty());
            QVERIFY(IsSafeRelativePath(pack.path));
            if (pack.title_id == QStringLiteral("0100000000010000")) odyssey = true;
            if (pack.title_id == QStringLiteral("0100152000022000")) mk8 = true;
        }
        QVERIFY(sources.contains(QStringLiteral("yuzu-mirror/yuzu-mod-archive")));
        QCOMPARE(Catalog::Repositories().size(), 5);
        QVERIFY(odyssey && mk8);
    }
    void latestCompatibleEffect() {
        Game game{QStringLiteral("0100000000010000"), QStringLiteral("Super Mario Odyssey"), QStringLiteral("1.3.0"), QString()};
        Pack old;
        old.title_id = game.title_id;
        old.name = QStringLiteral("Disable_FXAA");
        old.version = QStringLiteral("1.0.0");
        Pack compatible = old; compatible.version = QStringLiteral("1.3.0");
        Pack future = old; future.version = QStringLiteral("1.4.0");
        Pack hardware = old; hardware.name = QStringLiteral("Disable Dynamic Resolution");
        const auto result = CuratePacks({old, compatible, future, hardware}, {game});
        QCOMPARE(result.size(), 1);
        QCOMPARE(result.front().version, QStringLiteral("1.3.0"));
        QCOMPARE(result.front().name, QStringLiteral("FXAA: disabled"));
    }
    void nxConfiguration() {
        const auto options = QJsonDocument::fromJson(R"({"fps":{"Class":"scale","Default":60,"Config_Class":["UltraCamFPS","MaxFPS"]},"resolution":{"Class":"dropdown","Default":0,"Values":["1920x1080","2560x1440"],"Config_Class":["Resolution","Width","Height"]},"aspect":{"Class":"dropdown","Default":0,"Values":[[16,9],[21,9]],"Config_Class":["Aspect","Width","Height"]},"fxaa":{"Class":"bool","Default":false,"Config_Class":["Resolution","FXAA"]}})").object();
        const auto ini = NxOptimizer::BuildIni(options, {{QStringLiteral("fps"),90},{QStringLiteral("resolution"),1},{QStringLiteral("aspect"),1}});
        QVERIFY(ini.contains("MaxFPS = 90"));
        QVERIFY(ini.contains("Width = 2560"));
        QVERIFY(ini.contains("Height = 1440"));
        QVERIFY(ini.contains("Width = 21"));
        QVERIFY(ini.contains("FXAA = Off"));
    }
    void nxPreservesSettingsOutsidePane() {
        const auto options = QJsonDocument::fromJson(R"({"fps":{"Class":"scale","Default":60,"Config_Class":["UltraCamFPS","MaxFPS"]},"fov":{"Class":"scale","Default":50,"Config_Class":["Gameplay","Fov"]},"resolution":{"Class":"dropdown","Default":0,"Values":["1920x1080"],"Config_Class":["Resolution","Width","Height"]}})").object();
        const auto ini = NxOptimizer::BuildIni(options, {{QStringLiteral("fps"), 90}},
            "[Gameplay]\nFov = 75.00\n[UltraCamFPS]\nMaxFPS = 60\n[Resolution]\nWidth = 2560\nHeight = 1440\n[Camera]\nCustomSetting = True\n");
        QVERIFY(ini.contains("MaxFPS = 90"));
        QVERIFY(ini.contains("Fov = 75.00"));
        QVERIFY(ini.contains("Width = 2560"));
        QVERIFY(ini.contains("Height = 1440"));
        QVERIFY(ini.contains("CustomSetting = True"));
    }
    void codeAndFileConflicts() {
        QTemporaryDir temp;
        const QString candidate = temp.path() + QStringLiteral("/candidate");
        const QString mods = temp.path() + QStringLiteral("/mods");
        Write(candidate + QStringLiteral("/exefs/patch.pchtxt"), "@enabled\n00001000 AABBCCDD\n@stop\n");
        Write(mods + QStringLiteral("/overlap/exefs/test.pchtxt"), "@enabled\n00001002 AABB\n@stop\n");
        Write(mods + QStringLiteral("/separate/exefs/test.pchtxt"), "@enabled\n00002000 AABB\n@stop\n");
        QCOMPARE(ConflictingMods(candidate, mods, {}), QStringList{QStringLiteral("overlap")});
        QVERIFY(ConflictingMods(candidate, mods, {QStringLiteral("overlap")}).isEmpty());
        Write(candidate + QStringLiteral("/romfs/config.bin"), "new");
        Write(mods + QStringLiteral("/file-overlap/romfs/config.bin"), "old");
        QVERIFY(ConflictingMods(candidate, mods, {}).contains(QStringLiteral("file-overlap")));
    }
    void installAndBackup() {
        QTemporaryDir temp;
        Installer installer(temp.path() + QStringLiteral("/cache"));
        Pack pack;
        pack.id = QStringLiteral("test-pack");
        pack.title_id = QStringLiteral("0100000000010000");
        pack.name = QStringLiteral("FPS options");
        pack.build_ids = {QStringLiteral("B424BE150A8E7D78")};
        const QString path = temp.path() + QStringLiteral("/sample.txt");
        Write(path, "[60 FPS]\n04000000 00001000 00000001\n[30 FPS]\n04000000 00001000 00000002\n");
        QSignalSpy ready(&installer, &Installer::PreparedReady);
        QSignalSpy errors(&installer, &Installer::Failed);
        QSignalSpy installed(&installer, &Installer::Installed);
        installer.PrepareLocal(pack, path);
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 10000);
        QCOMPARE(installer.Prepared().directories.size(), 2);
        QCOMPARE(errors.count(), 0);
        const QString load = temp.path() + QStringLiteral("/load");
        Write(load + QStringLiteral("/unrelated/save.dat"), "keep");
        installer.Commit(0, load);
        QTRY_COMPARE_WITH_TIMEOUT(installed.count(), 1, 10000);
        const QString output = load + QLatin1Char('/') + pack.title_id + QLatin1Char('/') + ModDirectoryName(pack);
        QFile first(output + QStringLiteral("/cheats/B424BE150A8E7D78.txt"));
        QVERIFY(first.open(QIODevice::ReadOnly));
        QVERIFY(first.readAll().contains("[60 FPS]"));
        first.close();
        installer.Commit(1, load);
        QTRY_COMPARE_WITH_TIMEOUT(installed.count(), 2, 10000);
        const QString backup = installed[1][1].toString();
        QVERIFY(QFileInfo::exists(backup + QStringLiteral("/cheats/B424BE150A8E7D78.txt")));
        QVERIFY(QFileInfo::exists(load + QStringLiteral("/unrelated/save.dat")));
        QCOMPARE(errors.count(), 0);
    }
    void unmanagedDirectoryProtection() {
        QTemporaryDir temp;
        Installer installer(temp.path() + QStringLiteral("/cache"));
        Pack pack;
        pack.id = QStringLiteral("protection");
        pack.title_id = QStringLiteral("0100000000010000");
        pack.name = QStringLiteral("Graphics");
        const QString patch = temp.path() + QStringLiteral("/test.pchtxt");
        Write(patch, "@nsobid-B424BE150A8E7D78\n@enabled\n00001000 AABB\n@stop\n");
        QSignalSpy ready(&installer, &Installer::PreparedReady);
        QSignalSpy errors(&installer, &Installer::Failed);
        installer.PrepareLocal(pack, patch);
        QTRY_COMPARE_WITH_TIMEOUT(ready.count(), 1, 10000);
        const QString load = temp.path() + QStringLiteral("/load");
        const QString existing = load + QLatin1Char('/') + pack.title_id + QLatin1Char('/') + ModDirectoryName(pack);
        Write(existing + QStringLiteral("/romfs/keep.bin"), "original");
        installer.Commit(0, load);
        QTRY_COMPARE_WITH_TIMEOUT(errors.count(), 1, 10000);
        QFile preserved(existing + QStringLiteral("/romfs/keep.bin"));
        QVERIFY(preserved.open(QIODevice::ReadOnly));
        QCOMPARE(preserved.readAll(), QByteArray("original"));
    }
    void filterAndToggle() {
        QTemporaryDir temp;
        Game game{QStringLiteral("0100000000010000"), QStringLiteral("Super Mario Odyssey"), QStringLiteral("1.3.0"), QStringLiteral("B424BE150A8E7D78")};
        const QString load = temp.path() + QStringLiteral("/load");
        Write(load + QLatin1Char('/') + game.title_id + QStringLiteral("/Test graphics/exefs/test.pchtxt"), "@nsobid-B424BE150A8E7D78\n@enabled\n00001000 AABB\n@stop\n");
        Dialog dialog({game}, load, temp.path() + QStringLiteral("/cache"), true);
        QSignalSpy toggled(&dialog, &Dialog::EnabledChanged);
        auto* tree = dialog.findChild<QTreeWidget*>(QStringLiteral("packTree"));
        auto* filter = dialog.findChild<QLineEdit*>(QStringLiteral("packFilter"));
        QVERIFY(tree && filter);
        const auto items = tree->findItems(QStringLiteral("Test graphics"), Qt::MatchContains | Qt::MatchRecursive);
        QCOMPARE(items.size(), 1);
        QCOMPARE(items[0]->checkState(0), Qt::Checked);
        items[0]->setCheckState(0, Qt::Unchecked);
        QCOMPARE(toggled.count(), 1);
        QCOMPARE(toggled[0][2].toBool(), false);
        filter->setText(QStringLiteral("impossible search text"));
        for (int i = 0; i < tree->topLevelItemCount(); ++i) QVERIFY(tree->topLevelItem(i)->isHidden());
    }
};
QTEST_MAIN(PackTests)
#include "tests.moc"
