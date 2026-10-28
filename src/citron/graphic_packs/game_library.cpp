// SPDX-License-Identifier: GPL-2.0-or-later
#include "game_library.h"

#include <algorithm>
#include <functional>
#include <QSet>
#include <QStandardItemModel>
#include "citron/game_list_p.h"
#include "common/hex_util.h"
#include "common/settings.h"
#include "core/core.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/submission_package.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/loader/loader.h"

namespace GraphicPacks {
namespace {
QString ReadBuildId(const FileSys::VirtualDir& exefs) {
    if (!exefs) return {};
    const auto main = exefs->GetFile("main");
    if (!main || main->GetSize() < 0x100) return {};
    std::array<u8, 32> id{};
    if (main->ReadBytes(id.data(), id.size(), 0x40) != id.size()) return {};
    if (std::all_of(id.begin(), id.end(), [](u8 value) { return value == 0; })) return {};
    return QString::fromStdString(Common::HexToString(id, true));
}

void ReadMetadata(Game& game, const QString& path, Core::System& system,
                  const FileSys::VirtualFilesystem& vfs) {
    const u64 title_id = game.title_id.toULongLong(nullptr, 16);
    const auto file = Core::GetGameFileFromPath(vfs, path.toStdString());
    const auto loader = Loader::GetLoader(system, file, title_id);
    const auto& provider = system.GetContentProvider();
    const FileSys::PatchManager manager(title_id, system.GetFileSystemController(), provider);
    const auto control = manager.GetControlMetadata();
    if (control.first) game.version = QString::fromStdString(control.first->GetVersionString());
    else if (loader) {
        FileSys::NACP nacp;
        if (loader->ReadControlData(nacp) == Loader::ResultStatus::Success)
            game.version = QString::fromStdString(nacp.GetVersionString());
    }
    const auto update = manager.GetActiveUpdate();
    if (update.found) {
        std::unique_ptr<FileSys::NCA> nca;
        if (update.is_external && provider.IsUnionProvider()) {
            const auto& united = static_cast<const FileSys::ContentProviderUnion&>(provider);
            const auto raw = united.GetExternalEntryForVersion(FileSys::GetUpdateTitleID(title_id), FileSys::ContentRecordType::Program, update.version);
            if (raw) nca = std::make_unique<FileSys::NCA>(raw);
        } else nca = provider.GetEntry(FileSys::GetUpdateTitleID(title_id), FileSys::ContentRecordType::Program);
        if (nca && nca->GetStatus() == Loader::ResultStatus::Success) game.build_id = ReadBuildId(nca->GetExeFS());
        // Do not compare an update's patches against the base game's Build ID.
    } else if (file && loader) {
        switch (loader->GetFileType()) {
        case Loader::FileType::NSP: {
            FileSys::NSP nsp(file, title_id);
            game.build_id = ReadBuildId(nsp.GetExeFS());
            break;
        }
        case Loader::FileType::XCI: {
            FileSys::XCI xci(file, title_id, 0);
            const auto nca = xci.GetNCAByType(FileSys::NCAContentType::Program);
            if (nca) game.build_id = ReadBuildId(nca->GetExeFS());
            break;
        }
        case Loader::FileType::NCA: {
            FileSys::NCA nca(file);
            game.build_id = ReadBuildId(nca.GetExeFS());
            break;
        }
        case Loader::FileType::DeconstructedRomDirectory:
            game.build_id = ReadBuildId(file->GetContainingDirectory());
            break;
        default: break;
        }
    }
    if (system.IsPoweredOn() && system.GetApplicationProcessProgramID() == title_id)
        game.build_id = QString::fromStdString(Common::HexToString(system.GetApplicationProcessBuildID(), true));
    const auto found = Settings::values.disabled_addons.find(title_id);
    if (found != Settings::values.disabled_addons.end())
        for (const auto& mod : found->second) game.disabled_mods << QString::fromStdString(mod);
}
}

QVector<Game> ReadGameLibrary(QStandardItemModel& model, Core::System& system,
                            const FileSys::VirtualFilesystem& vfs) {
    QVector<Game> games;
    QSet<qulonglong> seen;
    std::function<void(QStandardItem*)> visit = [&](QStandardItem* parent) {
        for (int row = 0; row < parent->rowCount(); ++row) {
            const auto* item = parent->child(row, 0);
            if (!item) continue;
            if (item->data(GameListItem::TypeRole).toInt() == int(GameListItemType::Game)) {
                const auto id = item->data(GameListItemPath::ProgramIdRole).toULongLong();
                if (!id || seen.contains(id)) continue;
                seen.insert(id);
                Game game;
                game.title_id = QStringLiteral("%1").arg(id, 16, 16, QLatin1Char('0')).toUpper();
                game.name = item->data(GameListItemPath::TitleRole).toString();
                ReadMetadata(game, item->data(GameListItemPath::FullPathRole).toString(), system, vfs);
                games << std::move(game);
            } else visit(parent->child(row, 0));
        }
    };
    visit(model.invisibleRootItem());
    return games;
}
} // namespace GraphicPacks
