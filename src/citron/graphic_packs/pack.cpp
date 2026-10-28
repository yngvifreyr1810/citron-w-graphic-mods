// SPDX-License-Identifier: GPL-2.0-or-later
#include "pack.h"

#include <algorithm>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QVersionNumber>

namespace GraphicPacks {
namespace {
QString ReadVersion(const QString& text) {
    const auto match = QRegularExpression(QStringLiteral("(?:^|[^0-9])v?(\\d+\\.\\d+(?:\\.\\d+)?)(?:[^0-9]|$)"))
                           .match(text);
    return match.captured(1);
}

struct Range {
    quint64 start;
    quint64 end;
};
struct Footprint {
    QSet<QString> files;
    QVector<Range> code;
};

Footprint ReadFootprint(const QString& root) {
    Footprint result;
    QDirIterator it(root, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString relative = QDir(root).relativeFilePath(path).toLower();
        if (relative.startsWith(QStringLiteral("romfs/")) ||
            relative.startsWith(QStringLiteral("romfslite/"))) {
            result.files.insert(relative);
        } else if (relative.startsWith(QStringLiteral("exefs/"))) {
            QFile file(path);
            if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024) {
                continue;
            }
            const QByteArray bytes = file.readAll();
            if (path.endsWith(QStringLiteral(".pchtxt"), Qt::CaseInsensitive)) {
                qint64 shift = 0;
                bool enabled = false;
                for (const auto& line : QString::fromUtf8(bytes).split(QLatin1Char('\n'))) {
                    const QString text = line.trimmed();
                    if (text.startsWith(QStringLiteral("@flag offset_shift "))) {
                        shift = text.mid(19).trimmed().toLongLong(nullptr, 0);
                    } else if (text == QStringLiteral("@enabled")) {
                        enabled = true;
                    } else if (text == QStringLiteral("@disabled") || text == QStringLiteral("@stop")) {
                        enabled = false;
                    } else if (enabled) {
                        const auto match = QRegularExpression(QStringLiteral("^([0-9a-fA-F]{8,16})\\s+([0-9a-fA-F]+)"))
                                               .match(text);
                        if (match.hasMatch()) {
                            const quint64 offset = match.captured(1).toULongLong(nullptr, 16) + shift;
                            result.code.push_back({offset, offset + quint64(match.captured(2).size() / 2)});
                        }
                    }
                }
            } else if (bytes.startsWith("PATCH") || bytes.startsWith("IPS32")) {
                const int width = bytes.startsWith("IPS32") ? 4 : 3;
                qsizetype pos = 5;
                while (pos + width + 2 <= bytes.size()) {
                    if (bytes.mid(pos, width) == (width == 4 ? QByteArray("EEOF") : QByteArray("EOF"))) {
                        break;
                    }
                    quint64 offset = 0;
                    for (int i = 0; i < width; ++i) {
                        offset = (offset << 8) | quint8(bytes[pos++]);
                    }
                    quint64 length = (quint8(bytes[pos]) << 8) | quint8(bytes[pos + 1]);
                    pos += 2;
                    if (length == 0) {
                        if (pos + 3 > bytes.size()) break;
                        length = (quint8(bytes[pos]) << 8) | quint8(bytes[pos + 1]);
                        pos += 3;
                    } else {
                        pos += length;
                    }
                    // IPS uses NSO file offsets; IPSwitch uses the same coordinate system.
                    result.code.push_back({offset, offset + length});
                }
            } else {
                result.files.insert(relative);
            }
        }
    }
    return result;
}
} // namespace

QJsonObject Pack::ToJson() const {
    return {{QStringLiteral("id"), id}, {QStringLiteral("title_id"), title_id},
            {QStringLiteral("game"), game}, {QStringLiteral("name"), name},
            {QStringLiteral("category"), category}, {QStringLiteral("version"), version},
            {QStringLiteral("build_ids"), QJsonArray::fromStringList(build_ids)},
            {QStringLiteral("repository"), repository}, {QStringLiteral("path"), path},
            {QStringLiteral("blob_sha"), blob_sha}, {QStringLiteral("description_path"), description_path},
            {QStringLiteral("description"), description}, {QStringLiteral("variant"), variant},
            {QStringLiteral("option_name"), option_name}};
}

Pack Pack::FromJson(const QJsonObject& object) {
    Pack pack;
    pack.id = object.value(QStringLiteral("id")).toString();
    pack.title_id = object.value(QStringLiteral("title_id")).toString().toUpper();
    pack.game = object.value(QStringLiteral("game")).toString();
    pack.name = object.value(QStringLiteral("name")).toString();
    pack.category = object.value(QStringLiteral("category")).toString();
    pack.version = object.value(QStringLiteral("version")).toString();
    for (const auto& id : object.value(QStringLiteral("build_ids")).toArray()) pack.build_ids << id.toString();
    pack.repository = object.value(QStringLiteral("repository")).toString();
    pack.path = object.value(QStringLiteral("path")).toString();
    pack.blob_sha = object.value(QStringLiteral("blob_sha")).toString();
    pack.description_path = object.value(QStringLiteral("description_path")).toString();
    pack.description = object.value(QStringLiteral("description")).toString();
    pack.variant = object.value(QStringLiteral("variant")).toString();
    pack.option_name = object.value(QStringLiteral("option_name")).toString();
    return pack;
}

QString NormalizeName(QString name) {
    return name.toCaseFolded().remove(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N}]")));
}

QString CategoryForName(const QString& name) {
    if (name.contains(QRegularExpression(QStringLiteral("fps|frame.?rate|dynamic.?fps"), QRegularExpression::CaseInsensitiveOption)))
        return QStringLiteral("FPS");
    if (name.contains(QRegularExpression(QStringLiteral("fix|workaround"), QRegularExpression::CaseInsensitiveOption)))
        return QStringLiteral("Fixes");
    return QStringLiteral("Graphics");
}

QString StableId(const QString& repository, const QString& path) {
    return QString::fromLatin1(QCryptographicHash::hash((repository + QLatin1Char('/') + path).toUtf8(),
                                                       QCryptographicHash::Sha256).toHex().left(20));
}

QString ModDirectoryName(const Pack& pack) {
    QString name = pack.name;
    name.replace(QRegularExpression(QStringLiteral("[^\\p{L}\\p{N} ._-]")), QStringLiteral("_"));
    return QStringLiteral("GraphicPack_%1_%2").arg(name.left(65).trimmed(), pack.id.left(12));
}

bool InvalidatePatchCache(const QString& directory, const QString& title_id) {
    if (!QRegularExpression(QStringLiteral("^[0-9A-Fa-f]{16}$")).match(title_id).hasMatch()) return false;
    QDir cache(directory);
    bool success = true;
    // Game-list cache names are versioned (e.g. pv_v610_marquee.txt).
    for (const auto& name : cache.entryList({title_id.toUpper() + QStringLiteral(".pv*.txt")}, QDir::Files | QDir::NoSymLinks))
        if (!cache.remove(name)) success = false;
    return success;
}

QString RawUrl(const QString& repository, const QString& path) {
    return QStringLiteral("https://raw.githubusercontent.com/%1/main/%2")
        .arg(repository, QString::fromLatin1(QUrl::toPercentEncoding(path, "/")));
}

QString SourceUrl(const Pack& pack) {
    return QStringLiteral("https://github.com/%1/blob/main/%2")
        .arg(pack.repository, QString::fromLatin1(QUrl::toPercentEncoding(pack.path, "/")));
}

bool IsSafeRelativePath(const QString& input) {
    QString path = input;
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    if (path.isEmpty() || path.startsWith(QLatin1Char('/')) || path.contains(QLatin1Char(':')) ||
        path.contains(QChar::Null)) return false;
    for (const auto& part : path.split(QLatin1Char('/'))) {
        if (part == QStringLiteral("..") || part.endsWith(QLatin1Char('.')) || part.endsWith(QLatin1Char(' ')))
            return false;
    }
    return true;
}

bool BuildIdsMatch(const QString& first, const QString& second) {
    // Cheats use a 64-bit prefix; IPSwitch commonly uses 128/160 bits of the NSO build ID.
    if (first.size() < 16 || second.size() < 16) return false;
    return first.startsWith(second, Qt::CaseInsensitive) || second.startsWith(first, Qt::CaseInsensitive);
}

Compatibility CheckCompatibility(const Pack& pack, const Game& game) {
    if (!pack.title_id.isEmpty() && pack.title_id.compare(game.title_id, Qt::CaseInsensitive) != 0)
        return Compatibility::Mismatch;
    if (!pack.build_ids.isEmpty() && !game.build_id.isEmpty()) {
        for (const auto& id : pack.build_ids) if (BuildIdsMatch(id, game.build_id)) return Compatibility::Match;
        return Compatibility::Mismatch;
    }
    if (!pack.version.isEmpty() && !game.version.isEmpty())
        return pack.version == game.version ? Compatibility::Match : Compatibility::Mismatch;
    return Compatibility::Unknown;
}

QVector<Pack> ParseRepository(const QString& repository, const QJsonObject& tree) {
    QVector<Pack> result;
    const QJsonArray entries = tree.value(QStringLiteral("tree")).toArray();
    QMap<QString, QString> names;
    if (repository.startsWith(QStringLiteral("ChanseyIsTheBest/"))) {
        for (const auto& entry : entries) {
            const QString path = entry.toObject().value(QStringLiteral("path")).toString();
            const auto parts = path.split(QLatin1Char('/'));
            if (parts.size() == 3 && parts[0] == QStringLiteral("titles") && path.endsWith(QStringLiteral(".txt")))
                names[parts[1]] = QFileInfo(path).completeBaseName();
        }
    }
    for (const auto& entry : entries) {
        const auto object = entry.toObject();
        if (object.value(QStringLiteral("type")).toString(QStringLiteral("blob")) != QStringLiteral("blob")) continue;
        const QString path = object.value(QStringLiteral("path")).toString();
        if (!IsSafeRelativePath(path)) continue;
        const QString ext = QFileInfo(path).suffix().toLower();
        const auto parts = path.split(QLatin1Char('/'));
        Pack pack;
        pack.repository = repository;
        pack.path = path;
        pack.id = StableId(repository, path);
        pack.blob_sha = object.value(QStringLiteral("sha")).toString();
        if (repository.startsWith(QStringLiteral("ChanseyIsTheBest/"))) {
            if (parts.size() != 4 || parts[0] != QStringLiteral("titles") ||
                parts[2] != QStringLiteral("cheats") || ext != QStringLiteral("txt")) continue;
            pack.title_id = parts[1].toUpper();
            // This database primarily targets hardware. Only expose reviewed,
            // individual emulator effects, rather than opaque per-build bundles.
            if (pack.title_id != QStringLiteral("0100152000022000") ||
                QFileInfo(path).completeBaseName() != QStringLiteral("6A85262F21B90364")) continue;
            pack.game = names.value(parts[1], parts[1]);
            pack.build_ids << QFileInfo(path).completeBaseName().toUpper();
            pack.name = QStringLiteral("60 FPS in split screen");
            pack.option_name = QStringLiteral("3-4 player 60 FPS");
            pack.version = QStringLiteral("3.0.3");
            pack.category = QStringLiteral("FPS");
            pack.description_path = QStringLiteral("titles/%1/%2.txt").arg(parts[1], pack.game);
            pack.description = QStringLiteral("Raises 3–4 player local races to 60 FPS. One- and two-player races already use 60 FPS. Requires enough CPU/GPU performance. Based on theboy181's patch, updated for 3.0.3 by Craftyawesome. Only the 60 FPS effect is installed; the hardware resolution cheats are excluded.");
        } else if (repository.startsWith(QStringLiteral("cucholix/"))) {
            // Whole overhaul packs from this hardware-oriented source are kept
            // out until individual emulator effects have been reviewed.
            continue;
        } else if (repository == QStringLiteral("yuzu-mirror/yuzu-mod-archive")) {
            if (parts.size() != 3 || parts[0] != QStringLiteral("files") ||
                parts[1] != QStringLiteral("Super Mario Odyssey") || ext != QStringLiteral("zip")) continue;
            const QString base = QFileInfo(path).completeBaseName();
            if (base != QStringLiteral("Disable_FXAA") && base != QStringLiteral("21.9 Ultrawide") &&
                base != QStringLiteral("32.9 Super Ultrawide") && base != QStringLiteral("16.10 Aspect Ratio") &&
                base != QStringLiteral("4.3 Aspect Ratio")) continue;
            pack.title_id = QStringLiteral("0100000000010000");
            pack.game = QStringLiteral("Super Mario Odyssey");
            pack.name = base;
            pack.version = QStringLiteral("1.3.0");
            pack.category = QStringLiteral("Graphics");
            pack.description = base == QStringLiteral("Disable_FXAA")
                ? QStringLiteral("Removes the game's built-in FXAA blur. Useful when rendering above the native resolution. Some edges may look more jagged. Patch by theboy181, archived for emulator use.")
                : QStringLiteral("Changes the game's aspect ratio for your display. Set Citron's aspect ratio to Stretch to Window when using an aspect-ratio patch. By victoria4dx and theboy181.");
        } else {
            if (ext != QStringLiteral("rar") && ext != QStringLiteral("zip") && ext != QStringLiteral("pchtxt")) continue;
            if (parts.size() < 4) continue;
            const auto tid = QRegularExpression(QStringLiteral("(?:^|/|\\[)(010[0-9A-Fa-f]{13})(?:\\]|/|$)")).match(path);
            if (!tid.hasMatch()) continue;
            pack.title_id = tid.captured(1).toUpper();
            pack.game = parts[0];
            pack.version = ReadVersion(parts.mid(2).join(QLatin1Char('/')));
            if (pack.version.isEmpty()) continue;
            pack.name = QFileInfo(path).completeBaseName();
            const auto graphics = QRegularExpression(QStringLiteral("fps|resolution|dynamic|shadow|lod|level.?of.?detail|fxaa|taa|fsr|bloom|dof|depth|wide|21.?9|60|30|120|quality|graphic|visual|filter|lens|anisotrop|render|fix|mods"), QRegularExpression::CaseInsensitiveOption);
            if (!pack.name.contains(graphics) || pack.name.contains(QStringLiteral("Mods"), Qt::CaseInsensitive)) continue;
            if (path.contains(QStringLiteral("OLD STUFF"), Qt::CaseInsensitive) ||
                path.contains(QStringLiteral("DEPRECATED"), Qt::CaseInsensitive)) continue;
            pack.category = CategoryForName(pack.name);
            pack.description = QStringLiteral("Community graphics patch. Requires the indicated game version and matching Build ID. The installer will let you choose a single mod if the archive contains alternatives.");
        }
        result.push_back(std::move(pack));
    }
    return result;
}

QVector<Pack> CuratePacks(QVector<Pack> packs, const QVector<Game>& games) {
    QMap<QString, Pack> selected;
    for (auto& pack : packs) {
        const QString original = pack.name;
        if (original.contains(QRegularExpression(QStringLiteral("dynamic[ _-]*res|\\bDRS\\b|\\bRRS\\b|handheld|overclock|fpslocker|saltynx|reversenx|sys.?clk"), QRegularExpression::CaseInsensitiveOption))) continue;
        if (pack.local && !original.contains(QRegularExpression(QStringLiteral("ultracam|optimizer|fps|frame|fxaa|taa|fsr|bloom|dof|depth|shadow|lens|lod|level.?of.?detail|resolution|wide|draw|graphic|visual|black.?screen"), QRegularExpression::CaseInsensitiveOption))) continue;
        QString key_name = NormalizeName(original);
        QString summary;
        if (key_name.contains(QStringLiteral("disablefxaa")) || key_name.contains(QStringLiteral("forcefxaaoff"))) {
            pack.name = QStringLiteral("FXAA: disabled");
            summary = QStringLiteral("Removes the game's FXAA blur. The image can look sharper when upscaled, with more visible jagged edges.");
        } else if (key_name.contains(QStringLiteral("forcefxaaon"))) {
            pack.name = key_name.contains(QStringLiteral("fix")) ? QStringLiteral("FXAA scaling fix") : QStringLiteral("FXAA: enabled");
            summary = QStringLiteral("Enables the game's FXAA to smooth jagged edges, at the cost of a slightly softer image.");
        } else if (key_name.contains(QStringLiteral("disabledof")) || key_name.contains(QStringLiteral("disabletargetingdof"))) {
            pack.name = QStringLiteral("Depth of field: disabled");
            summary = QStringLiteral("Removes background blur applied by the game. This is a visual preference, not an FPS unlock.");
        } else if (key_name.contains(QStringLiteral("levelofdetail")) || key_name.contains(QStringLiteral("lodimprovement"))) {
            pack.name = QStringLiteral("Draw distance");
            summary = QStringLiteral("Keeps more detailed objects visible farther away. May increase GPU load and reduce FPS.");
        } else if (key_name.contains(QStringLiteral("disablefsr"))) {
            pack.name = QStringLiteral("FSR: disabled");
            summary = QStringLiteral("Disables the game's FSR reconstruction. Useful for some native-resolution and emulator-upscaling configurations.");
        } else if (key_name.contains(QStringLiteral("shadowresolution"))) {
            pack.name = QStringLiteral("Shadow quality");
            summary = QStringLiteral("Changes shadow resolution. Higher quality costs GPU performance and video memory.");
        } else if (key_name.contains(QStringLiteral("lensflare"))) {
            pack.name = QStringLiteral("Lens flare: disabled");
            summary = QStringLiteral("Removes bright camera lens-flare effects. This does not change the game's lighting.");
        } else if (key_name.contains(QStringLiteral("60fpsinsplitscreen"))) {
            pack.name = QStringLiteral("60 FPS in split screen");
        } else {
            pack.name.remove(QRegularExpression(QStringLiteral("[\\[\\]]|\\bv?\\d+\\.\\d+\\.\\d+\\b")));
            pack.name.replace(QLatin1Char('_'), QLatin1Char(' '));
            pack.name = pack.name.simplified();
        }
        if (!summary.isEmpty()) pack.description = summary + QStringLiteral("\n\n") + pack.description;
        const Game* installed = nullptr;
        for (const auto& game : games) if (game.title_id == pack.title_id ||
            (pack.title_id.isEmpty() && NormalizeName(game.name) == NormalizeName(pack.game))) { installed = &game; break; }
        const QString game_key = pack.title_id.isEmpty() ? NormalizeName(pack.game) : pack.title_id;
        const QString key = game_key + QLatin1Char('/') + NormalizeName(pack.name);
        if (!selected.contains(key)) { selected.insert(key, pack); continue; }
        const auto& current = selected[key];
        auto rank = [installed](const Pack& candidate) {
            int value = 0;
            if (installed) {
                const auto state = CheckCompatibility(candidate, *installed);
                value = state == Compatibility::Match ? 100 : state == Compatibility::Unknown ? 50 : 0;
            }
            if (candidate.local) value += 10;
            return value;
        };
        if (rank(pack) > rank(current) || (rank(pack) == rank(current) &&
            QVersionNumber::fromString(pack.version) > QVersionNumber::fromString(current.version))) selected[key] = pack;
    }
    QVector<Pack> result;
    for (const auto& pack : selected) result << pack;
    return result;
}

void InspectPatch(Pack& pack, const QByteArray& content) {
    const QString text = QString::fromUtf8(content);
    const auto bid = QRegularExpression(QStringLiteral("@nsobid-([0-9a-fA-F]{16,64})")).match(text);
    if (bid.hasMatch()) pack.build_ids = {bid.captured(1).toUpper()};
    const auto tid = QRegularExpression(QStringLiteral("\\[(010[0-9a-fA-F]{13})\\]")).match(text);
    if (tid.hasMatch()) {
        // An update title ID ends in 0800, while the load directory uses the base ID.
        const quint64 id = tid.captured(1).toULongLong(nullptr, 16) & ~quint64(0xFFF);
        pack.title_id = QStringLiteral("%1").arg(id, 16, 16, QLatin1Char('0')).toUpper();
    }
}

QVector<CheatOption> SplitCheats(const QByteArray& content) {
    QVector<CheatOption> result;
    QByteArray master;
    QString name;
    QByteArray block;
    bool is_master = false;
    auto finish = [&] {
        if (name.isEmpty() || block.trimmed().isEmpty()) return;
        if (is_master) master += block;
        else result.push_back({name, block});
    };
    for (const auto& line : content.split('\n')) {
        const QString text = QString::fromUtf8(line).trimmed();
        const auto header = QRegularExpression(QStringLiteral("^([\\[\\{])(.+)[\\]\\}]$")).match(text);
        if (header.hasMatch()) {
            finish();
            name = header.captured(2);
            is_master = header.captured(1) == QStringLiteral("{") || name.contains(QStringLiteral("master"), Qt::CaseInsensitive);
            block = line + '\n';
        } else if (!name.isEmpty()) {
            block += line + '\n';
        }
    }
    finish();
    for (auto& option : result) option.content.prepend(master);
    return result;
}

QStringList PayloadBuildIds(const QString& directory) {
    QStringList ids;
    QDirIterator it(directory, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QFileInfo info(path);
        const QString parent = info.dir().dirName().toLower();
        if (parent == QStringLiteral("exefs") && info.suffix().compare(QStringLiteral("pchtxt"), Qt::CaseInsensitive) == 0) {
            QFile file(path);
            if (file.open(QIODevice::ReadOnly) && file.size() < 16 * 1024 * 1024) {
                Pack pack;
                InspectPatch(pack, file.readAll());
                ids.append(pack.build_ids);
            }
        } else if ((parent == QStringLiteral("exefs") || parent == QStringLiteral("cheats")) &&
                   info.completeBaseName().contains(QRegularExpression(QStringLiteral("^[0-9a-fA-F]{16,64}$")))) {
            ids << info.completeBaseName().toUpper();
        }
    }
    ids.removeDuplicates();
    return ids;
}

QStringList FindPayloads(const QString& directory, const QString& title_id) {
    QStringList roots;
    QDirIterator it(directory, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString path = it.next();
        const QString name = it.fileInfo().fileName().toLower();
        if (name != QStringLiteral("exefs") && name != QStringLiteral("romfs") &&
            name != QStringLiteral("romfslite") && name != QStringLiteral("cheats")) continue;
        const QString relative = QDir(directory).relativeFilePath(path);
        const auto tid = QRegularExpression(QStringLiteral("(?:^|/)(010[0-9a-fA-F]{13})(?:/|$)")).match(relative);
        if (tid.hasMatch() && tid.captured(1).compare(title_id, Qt::CaseInsensitive) != 0) continue;
        roots << QFileInfo(path).absolutePath();
    }
    roots.removeDuplicates();
    std::sort(roots.begin(), roots.end());
    // Payloads nested inside an actual RomFS are game data, not independent mods.
    const auto all = roots;
    roots.erase(std::remove_if(roots.begin(), roots.end(), [&](const QString& path) {
        for (const auto& parent : all) {
            if (path.startsWith(parent + QStringLiteral("/romfs/"), Qt::CaseInsensitive) ||
                path.startsWith(parent + QStringLiteral("/romfslite/"), Qt::CaseInsensitive)) return true;
        }
        return false;
    }), roots.end());
    return roots;
}

QStringList ConflictingMods(const QString& candidate, const QString& title_root,
                            const QStringList& disabled_mods, const QString& exclude) {
    const auto target = ReadFootprint(candidate);
    QStringList conflicts;
    for (const auto& root : FindPayloads(title_root, QString())) {
        const QString name = QDir(title_root).relativeFilePath(root);
        if (name == exclude || disabled_mods.contains(name) ||
            disabled_mods.contains(QFileInfo(root).fileName())) continue;
        const auto other = ReadFootprint(root);
        bool overlap = target.files.intersects(other.files);
        if (!overlap) for (const auto& first : target.code) {
            for (const auto& second : other.code) if (first.start < second.end && second.start < first.end) {
                overlap = true;
                break;
            }
            if (overlap) break;
        }
        if (overlap) conflicts << name;
    }
    return conflicts;
}
} // namespace GraphicPacks
