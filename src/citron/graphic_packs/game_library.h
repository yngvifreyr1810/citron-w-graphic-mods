// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "pack.h"
#include "core/file_sys/vfs/vfs_types.h"

class QStandardItemModel;
namespace Core { class System; }
namespace GraphicPacks {
QVector<Game> ReadGameLibrary(QStandardItemModel& model, Core::System& system,
                            const FileSys::VirtualFilesystem& vfs);
}
