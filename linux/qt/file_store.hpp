#pragma once
#include <QByteArray>
#include <QString>
#include <cstdint>
#include <functional>
#include <optional>
#include <stdexcept>

namespace jsondict_linux {

struct FileStamp {
    std::uint64_t device = 0, inode = 0;
    std::int64_t size = 0, modified_seconds = 0, modified_nanos = 0;
    unsigned int mode = 0, owner = 0, links = 0, group = 0;
    QByteArray sha256;
    bool operator==(const FileStamp& other) const;
};
struct FileSnapshot { QByteArray bytes; FileStamp stamp; };
enum class FileFailure {
    IO, Conflict, UnsupportedAtomicSave, Symlink, NotRegular, Hardlink,
    ChangedDuringRead, TooLarge, Verification, Durability
};
class FileError : public std::runtime_error {
public:
    FileError(FileFailure type, QString operation, int error = 0,
              bool committed = false, QString backup = {});
    FileFailure type;
    QString operation;
    int system_error;
    bool committed;
    QString backup;
};
enum class SaveStep { TemporaryWritten, BeforeRename, AtCommit, AfterRename, BeforeBackupRemoval };
struct SaveOptions { std::function<void(SaveStep)> hook; };

// The optional baseline means MUST BE ABSENT when empty, never "skip check".
// Only Linux renameat2 is used for commits. There is no direct-write fallback.
// Successful replacements also retain .jsondict-backup-* recovery files.
// Removing the old version before a final fallible fsync is unsafe.
class FileStore {
public:
    static FileSnapshot read(const QString& path);
    static std::optional<FileStamp> probe(const QString& path);
    static FileStamp save(const QString& path, const QByteArray& bytes,
                          const std::optional<FileStamp>& expected,
                          const SaveOptions& options = {});
};

}  // namespace jsondict_linux
