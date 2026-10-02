#include "file_store.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

using namespace jsondict_linux;
int count = 0;
void check(bool value, const char* message) { ++count; if (!value) throw std::runtime_error(message); }
void external_write(const QString& path, const QByteArray& data) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write(data) != data.size())
        throw std::runtime_error("test external writer failed");
}
template<class F> FileError fails(F action) {
    try { action(); } catch (const FileError& error) { ++count; return error; }
    throw std::runtime_error("unsafe file operation unexpectedly succeeded");
}
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    try {
        check(directory.isValid(), "temporary directory unavailable");
        const QString path = directory.filePath(QStringLiteral("Unicode 文件 🙂.json"));
        const QByteArray original = "{\"precise\":1.2300e+04}\n";
        auto first = FileStore::save(path, original, std::nullopt);
        check(FileStore::read(path).bytes == original, "new-file round trip failed");
        const QByteArray replacement = "{\"changed\":true}\n";
        auto second = FileStore::save(path, replacement, first);
        check(FileStore::read(path).bytes == replacement, "replacement round trip failed");
        fails([&] { FileStore::save(path, original, first); });
        check(FileStore::read(path).bytes == replacement, "stale baseline overwrote file");
        fails([&] { FileStore::save(path, original, std::nullopt); });
        check(FileStore::read(path).bytes == replacement, "absent baseline overwrote existing file");

        const QString symlink = directory.filePath("link.json");
        check(::symlink(QFile::encodeName(path).constData(), QFile::encodeName(symlink).constData()) == 0,
              "cannot create test symlink");
        check(fails([&] { FileStore::save(symlink, original, second); }).type == FileFailure::Symlink,
              "symlink save was not rejected");
        check(FileStore::read(path).bytes == replacement, "symlink rejection changed target");
        const QString hardlink = directory.filePath("hard.json");
        check(::link(QFile::encodeName(path).constData(), QFile::encodeName(hardlink).constData()) == 0,
              "cannot create test hard link");
        auto linked = FileStore::read(path).stamp;
        check(fails([&] { FileStore::save(path, original, linked); }).type == FileFailure::Hardlink,
              "hard-linked target was not rejected");
        QFile::remove(hardlink);
        second = FileStore::read(path).stamp;
        const QByteArray external = "{\"external\":true}\n";
        auto race = fails([&] { FileStore::save(path, original, second, {[&](SaveStep step) {
            if (step == SaveStep::BeforeRename) external_write(path, external);
        }}); });
        check(!race.committed && FileStore::read(path).bytes == external,
              "precommit external write was overwritten");
        second = FileStore::read(path).stamp;
        auto after = fails([&] { FileStore::save(path, original, second, {[](SaveStep step) {
            if (step == SaveStep::AfterRename) throw std::runtime_error("injected post-commit failure");
        }}); });
        check(after.committed && !after.backup.isEmpty(), "postcommit failure lost backup");
        check(FileStore::read(after.backup).bytes == external, "retained backup is not actual replaced version");
        check(FileStore::read(path).bytes == original, "postcommit failure corrupted new content");
        second = FileStore::read(path).stamp;
        auto changed_after = fails([&] { FileStore::save(path, replacement, second, {[&](SaveStep step) {
            if (step == SaveStep::BeforeBackupRemoval) external_write(path, external);
        }}); });
        check(changed_after.committed && QFileInfo::exists(changed_after.backup),
              "post-save external write discarded original backup");
        check(FileStore::read(path).bytes == external, "post-save verification overwrote external data");
        const QString readonly = directory.filePath("readonly");
        check(QDir().mkdir(readonly), "cannot create readonly fixture");
        const QString locked = readonly + "/data.json";
        external_write(locked, original);
        auto locked_stamp = FileStore::read(locked).stamp;
        ::chmod(QFile::encodeName(readonly).constData(), 0555);
        if (::geteuid() != 0) {
            fails([&] { FileStore::save(locked, replacement, locked_stamp); });
            check(FileStore::read(locked).bytes == original, "readonly directory caused direct overwrite");
        }
        ::chmod(QFile::encodeName(readonly).constData(), 0755);
        std::cout << "LINUX_FILE_OK: " << count << " assertions; renameat2, SHA-256, conflicts, backups, links, permissions\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "LINUX_FILE_FAILED: " << error.what() << '\n';
        return 1;
    }
}
