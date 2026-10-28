// SPDX-License-Identifier: GPL-2.0-or-later
#include "installer.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDirIterator>
#include <QFile>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>
#include <QtConcurrent/QtConcurrentRun>

namespace GraphicPacks {
namespace {
bool WriteFile(const QString& path, const QByteArray& bytes) {
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

bool CopyTree(const QString& source, const QString& destination, QString& error, bool merge = false) {
    QDir().mkpath(destination);
    QDirIterator it(source, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    int count = 0;
    qint64 total = 0;
    while (it.hasNext()) {
        const QString path = it.next();
        const auto info = it.fileInfo();
        const QString relative = QDir(source).relativeFilePath(path);
        total += info.isFile() ? info.size() : 0;
        if (info.isSymLink() || !IsSafeRelativePath(relative) || ++count > 20000 || total > 1024LL * 1024 * 1024) {
            error = QStringLiteral("The archive has an unsafe path, link or oversized payload.");
            return false;
        }
        const QString target = destination + QLatin1Char('/') + relative;
        if (merge && info.isFile() && QFileInfo::exists(target)) {
            QFile first(path), second(target);
            QCryptographicHash a(QCryptographicHash::Sha256), b(QCryptographicHash::Sha256);
            if (!first.open(QIODevice::ReadOnly) || !second.open(QIODevice::ReadOnly) ||
                !a.addData(&first) || !b.addData(&second) || a.result() != b.result()) {
                error = QStringLiteral("The author package contains conflicting alternatives at %1. Nothing was installed.").arg(relative);
                return false;
            }
            continue;
        }
        if (info.isDir()) {
            if (!QDir().mkpath(target)) {
                error = QStringLiteral("Could not create the mod directory.");
                return false;
            }
        } else if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(path, target)) {
            error = QStringLiteral("Could not copy %1").arg(relative);
            return false;
        }
    }
    return true;
}

QString Extractor() {
    for (const auto& name : {QStringLiteral("7z"), QStringLiteral("7za"), QStringLiteral("7zz")}) {
        const QString found = QStandardPaths::findExecutable(name);
        if (!found.isEmpty()) return found;
    }
#ifdef _WIN32
    for (const auto& base : {qEnvironmentVariable("ProgramFiles"), qEnvironmentVariable("ProgramFiles(x86)")}) {
        const QString found = base + QStringLiteral("/7-Zip/7z.exe");
        if (QFileInfo::exists(found)) return found;
    }
#endif
    return {};
}

bool Run7z(const QString& executable, const QStringList& args, QByteArray& output) {
    QProcess process;
#ifdef _WIN32
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args_) {
        args_->flags |= 0x08000000; // CREATE_NO_WINDOW
    });
#endif
    process.start(executable, args);
    if (!process.waitForStarted(5000) || !process.waitForFinished(120000)) {
        process.kill();
        process.waitForFinished(5000);
        return false;
    }
    output = process.readAllStandardOutput();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}

PreparedData PrepareFiles(Pack pack, const QString& download, const QString& temporary) {
    PreparedData result;
    result.pack = std::move(pack);
    QFile file(download);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Could not read the downloaded pack.");
        return result;
    }
    const QByteArray bytes = file.readAll();
    file.close();
    if (!result.pack.blob_sha.isEmpty()) {
        const QByteArray header = QByteArray("blob ") + QByteArray::number(bytes.size()) + '\0';
        const QByteArray blob = header + bytes;
        const auto hash = QString::fromLatin1(QCryptographicHash::hash(blob, QCryptographicHash::Sha1).toHex());
        if (hash != result.pack.blob_sha) {
            result.error = QStringLiteral("This source file changed since the catalogue was downloaded. Update the catalogue and try again.");
            return result;
        }
    }
    const QString suffix = QFileInfo(download).suffix().toLower();
    // Apply archive-level atomicity to every source, including locally imported
    // archives whose metadata predates whole_package.
    if (suffix == QStringLiteral("zip") || suffix == QStringLiteral("rar")) result.pack.whole_package = true;
    if (suffix == QStringLiteral("pchtxt")) {
        InspectPatch(result.pack, bytes);
        const QString root = temporary + QStringLiteral("/payload");
        if (!WriteFile(root + QStringLiteral("/exefs/graphics.pchtxt"), bytes)) {
            result.error = QStringLiteral("Could not stage the patch.");
            return result;
        }
        result.directories << root;
        result.labels << QStringLiteral("Graphics patches");
    } else if (suffix == QStringLiteral("txt")) {
        const auto options = SplitCheats(bytes);
        for (qsizetype index = 0; index < options.size(); ++index) {
            if (!result.pack.option_name.isEmpty() && options[index].name != result.pack.option_name) continue;
            const QString root = temporary + QStringLiteral("/option-%1").arg(index);
            if (result.pack.build_ids.isEmpty() || !WriteFile(root + QStringLiteral("/cheats/%1.txt").arg(result.pack.build_ids.front()), options[index].content)) {
                result.error = QStringLiteral("Could not stage this cheat option.");
                return result;
            }
            result.directories << root;
            result.labels << options[index].name;
        }
    } else {
        const QString extractor = Extractor();
        if (extractor.isEmpty()) {
            result.error = QStringLiteral("Install 7-Zip to extract ZIP/RAR packs, or use an IPSwitch pack instead.");
            return result;
        }
        QByteArray listing;
        if (!Run7z(extractor, {QStringLiteral("l"), QStringLiteral("-slt"), QStringLiteral("-ba"), QStringLiteral("-sccUTF-8"), QStringLiteral("--"), download}, listing)) {
            result.error = QStringLiteral("Could not inspect the archive with 7-Zip.");
            return result;
        }
        qint64 total = 0;
        int count = 0;
        for (const auto& raw : listing.split('\n')) {
            const QString line = QString::fromUtf8(raw).trimmed();
            if (line.startsWith(QStringLiteral("Path = ")) && (!IsSafeRelativePath(line.mid(7)) || ++count > 20000)) {
                result.error = QStringLiteral("The archive contains an unsafe path or too many entries.");
                return result;
            }
            if (line.startsWith(QStringLiteral("Size = "))) total += line.mid(7).toLongLong();
            if (total > 1024LL * 1024 * 1024 || line.startsWith(QStringLiteral("Symbolic Link = ")) ||
                line.startsWith(QStringLiteral("Hard Link = "))) {
                result.error = QStringLiteral("Archives containing links or more than 1 GiB of files are not supported.");
                return result;
            }
        }
        const QString extracted = temporary + QStringLiteral("/extracted");
        QByteArray output;
        if (!Run7z(extractor, {QStringLiteral("x"), QStringLiteral("-y"), QStringLiteral("-o") + extracted, QStringLiteral("--"), download}, output)) {
            result.error = QStringLiteral("The archive could not be extracted.");
            return result;
        }
        // Validate extracted files before following or copying anything.
        QDirIterator validator(extracted, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
        const QString canonical_root = QDir(extracted).canonicalPath() + QLatin1Char('/');
        while (validator.hasNext()) {
            validator.next();
            if (validator.fileInfo().isSymLink() || !validator.fileInfo().canonicalFilePath().startsWith(canonical_root)) {
                result.error = QStringLiteral("The extracted archive contains a link outside its payload.");
                return result;
            }
        }
        if (result.pack.whole_package) {
            const QString root = temporary + QStringLiteral("/author-bundle");
            if (!AssembleAuthorBundle(extracted, result.pack.title_id, root, result.error)) return result;
            result.directories = {root};
            result.labels = {QStringLiteral("Complete author package")};
            return result;
        }
        result.directories = FindPayloads(extracted, result.pack.title_id);
        // Atmosphere graphics bundles often put IPS files in exefs_patches separately from RomFS.
        QStringList standalone_ips;
        QDirIterator patches(extracted, {QStringLiteral("*.ips"), QStringLiteral("*.pchtxt")}, QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (patches.hasNext()) {
            const QString path = patches.next();
            if (path.contains(QStringLiteral("/exefs_patches/"), Qt::CaseInsensitive)) standalone_ips << path;
        }
        if (!standalone_ips.isEmpty() && result.directories.size() <= 1) {
            const QString root = temporary + QStringLiteral("/combined");
            if (!result.directories.isEmpty() && !CopyTree(result.directories.front(), root, result.error)) return result;
            QDir().mkpath(root + QStringLiteral("/exefs"));
            for (const auto& path : standalone_ips) {
                if (!QFile::copy(path, root + QStringLiteral("/exefs/") + QFileInfo(path).fileName())) {
                    result.error = QStringLiteral("Could not combine the IPS and RomFS payloads.");
                    return result;
                }
            }
            result.directories = {root};
        }
        for (const auto& path : result.directories) {
            result.labels << QDir(extracted).relativeFilePath(path);
        }
    }
    if (result.directories.isEmpty()) result.error = QStringLiteral("No compatible ExeFS, RomFS or cheat payload was found in this pack.");
    return result;
}
} // namespace

bool AssembleAuthorBundle(const QString& extracted, const QString& title_id,
                          const QString& destination, QString& error) {
    const auto roots = FindPayloads(extracted, title_id);
    bool found = false;
    for (const auto& root : roots) {
        if (!CopyTree(root, destination, error, true)) return false;
        found = true;
    }
    // Atmosphere places the matching IPS beside contents/<TID>/romfs.
    // Preserve each IPS byte-for-byte; changing the wrapper is sufficient.
    QDirIterator patches(extracted, {QStringLiteral("*.ips"), QStringLiteral("*.pchtxt")},
                         QDir::Files | QDir::NoSymLinks, QDirIterator::Subdirectories);
    while (patches.hasNext()) {
        const QString path = patches.next();
        if (!path.contains(QStringLiteral("/exefs_patches/"), Qt::CaseInsensitive)) continue;
        const QString target = destination + QStringLiteral("/exefs/") + QFileInfo(path).fileName();
        if (QFileInfo::exists(target)) {
            QFile first(path), second(target);
            QCryptographicHash a(QCryptographicHash::Sha256), b(QCryptographicHash::Sha256);
            if (!first.open(QIODevice::ReadOnly) || !second.open(QIODevice::ReadOnly) ||
                !a.addData(&first) || !b.addData(&second) || a.result() != b.result()) {
                error = QStringLiteral("Conflicting IPS files share the name %1. Nothing was installed.").arg(QFileInfo(path).fileName());
                return false;
            }
        } else if (!QDir().mkpath(QFileInfo(target).absolutePath()) || !QFile::copy(path, target)) {
            error = QStringLiteral("Could not stage the complete IPS bundle.");
            return false;
        }
        found = true;
    }
    if (!found) error = QStringLiteral("No payload for the selected game was found in this author package.");
    return found;
}

Installer::Installer(QString cache_directory_, QObject* parent)
    : QObject(parent), cache_directory(std::move(cache_directory_)), network(this) {
    QDir().mkpath(cache_directory);
}

void Installer::Prepare(const Pack& pack) {
    if (busy) return;
    busy = true;
    prepared = {};
    staging = std::make_shared<QTemporaryDir>(cache_directory + QStringLiteral("/stage-XXXXXX"));
    if (!staging->isValid()) {
        busy = false;
        emit Failed(tr("Could not create a temporary download directory."));
        return;
    }
    const QString download = staging->path() + QStringLiteral("/download.") + QFileInfo(pack.path).suffix();
    auto output = std::make_shared<QFile>(download);
    if (!output->open(QIODevice::WriteOnly)) {
        busy = false;
        emit Failed(tr("Could not create the download file."));
        return;
    }
    QNetworkRequest request{QUrl(RawUrl(pack.repository, pack.path))};
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Citron-Graphic-Packs/1.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(60000);
    auto* reply = network.get(request);
    connect(reply, &QNetworkReply::readyRead, reply, [reply, output] {
        const QByteArray chunk = reply->readAll();
        if (output->size() + chunk.size() > 256LL * 1024 * 1024 || output->write(chunk) != chunk.size()) reply->abort();
    });
    connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        emit Progress(tr("Downloading pack: %1 / %2 KiB...").arg(received / 1024).arg(total > 0 ? QString::number(total / 1024) : QStringLiteral("?")));
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, output, pack, download, temporary = staging] {
        const QByteArray last = reply->readAll();
        const bool written = output->size() + last.size() <= 256LL * 1024 * 1024 && output->write(last) == last.size();
        output->close();
        if (reply->error() != QNetworkReply::NoError || !written) {
            busy = false;
            emit Failed(tr("Pack download failed: %1").arg(reply->errorString()));
            reply->deleteLater();
            return;
        }
        reply->deleteLater();
        emit Progress(tr("Inspecting and preparing the pack..."));
        auto* watcher = new QFutureWatcher<PreparedData>(this);
        connect(watcher, &QFutureWatcher<PreparedData>::finished, this, [this, watcher] {
            prepared = watcher->result();
            watcher->deleteLater();
            busy = false;
            if (!prepared.error.isEmpty()) emit Failed(prepared.error);
            else emit PreparedReady();
        });
        watcher->setFuture(QtConcurrent::run([pack, download, temporary] {
            return PrepareFiles(pack, download, temporary->path());
        }));
    });
}

void Installer::Commit(int variant_index, const QString& load_directory) {
    if (busy || variant_index < 0 || variant_index >= prepared.directories.size()) return;
    busy = true;
    const auto temporary = staging;
    Pack pack = prepared.pack;
    pack.variant = prepared.labels[variant_index];
    pack.mod_name = ModDirectoryName(pack);
    pack.build_ids = PayloadBuildIds(prepared.directories[variant_index]);
    const QString source = prepared.directories[variant_index];
    emit Progress(tr("Installing the selected pack..."));
    auto* watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher, pack] {
        const auto result = watcher->result();
        watcher->deleteLater();
        busy = false;
        if (!result[0].isEmpty()) emit Failed(result[0]);
        else emit Installed(pack, result[1]);
    });
    watcher->setFuture(QtConcurrent::run([pack, temporary, source, load_directory, cache = cache_directory] {
        QString error;
        QString backup;
        QTemporaryDir commit_staging(temporary->path() + QStringLiteral("/commit-XXXXXX"));
        if (!commit_staging.isValid()) return QStringList{QStringLiteral("Could not create installation staging."), backup};
        const QString staged = commit_staging.path() + QStringLiteral("/payload");
        if (!CopyTree(source, staged, error)) return QStringList{error, backup};
        if (!WriteFile(staged + QStringLiteral("/.graphic-pack.json"), QJsonDocument(pack.ToJson()).toJson()))
            return QStringList{QStringLiteral("Could not write pack metadata."), backup};
        const QString title_root = load_directory + QLatin1Char('/') + pack.title_id;
        QDir().mkpath(title_root);
        const QString destination = title_root + QLatin1Char('/') + pack.mod_name;
        // Move the completed payload onto the same filesystem before the atomic final rename.
        QTemporaryDir incoming(title_root + QStringLiteral("/.graphic-pack-incoming-XXXXXX"));
        if (!incoming.isValid()) return QStringList{QStringLiteral("Could not create the installation staging folder."), backup};
        const QString ready = incoming.path() + QStringLiteral("/payload");
        if (!CopyTree(staged, ready, error)) return QStringList{error, backup};
        if (QFileInfo::exists(destination)) {
            QFile manifest(destination + QStringLiteral("/.graphic-pack.json"));
            if (!manifest.open(QIODevice::ReadOnly) || QJsonDocument::fromJson(manifest.readAll()).object().value(QStringLiteral("id")).toString() != pack.id)
                return QStringList{QStringLiteral("An unmanaged mod already uses this directory. It was left untouched."), backup};
            manifest.close(); // Windows will not rename a directory containing an open file.
            backup = cache + QStringLiteral("/backups/") + QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")) + QLatin1Char('/') + pack.title_id + QLatin1Char('/') + pack.mod_name;
            if (!QDir().mkpath(QFileInfo(backup).absolutePath()) || !QDir().rename(destination, backup))
                return QStringList{QStringLiteral("Could not back up the previous pack. No files were replaced."), QString()};
        }
        if (!QDir().rename(ready, destination)) {
            if (!backup.isEmpty()) QDir().rename(backup, destination);
            return QStringList{QStringLiteral("Could not activate the pack. The previous pack was restored."), backup};
        }
        return QStringList{QString(), backup};
    }));
}

void Installer::PrepareLocal(const Pack& pack, const QString& path) {
    if (busy) return;
    busy = true;
    prepared = {};
    staging = std::make_shared<QTemporaryDir>(cache_directory + QStringLiteral("/stage-XXXXXX"));
    if (!staging->isValid()) {
        busy = false;
        emit Failed(tr("Could not create a staging directory."));
        return;
    }
    auto* watcher = new QFutureWatcher<PreparedData>(this);
    connect(watcher, &QFutureWatcher<PreparedData>::finished, this, [this, watcher] {
        prepared = watcher->result();
        watcher->deleteLater();
        busy = false;
        if (!prepared.error.isEmpty()) emit Failed(prepared.error);
        else emit PreparedReady();
    });
    watcher->setFuture(QtConcurrent::run([pack, path, temporary = staging] {
        return PrepareFiles(pack, path, temporary->path());
    }));
}

void Installer::PrepareDirectory(const Pack& pack, const QString& directory) {
    if (busy) return;
    staging = std::make_shared<QTemporaryDir>(cache_directory + QStringLiteral("/stage-XXXXXX"));
    prepared = {};
    prepared.pack = pack;
    if (!staging->isValid()) {
        emit Failed(tr("Could not create installation staging."));
        return;
    }
    prepared.directories = {directory};
    prepared.labels = {pack.name};
    emit PreparedReady();
}
} // namespace GraphicPacks
