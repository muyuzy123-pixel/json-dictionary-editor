#include "file_store.hpp"
#include "editor_session.hpp"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRandomGenerator>
#include <cerrno>
#include <cstring>
#include <cstdio>
#include <sys/xattr.h>
#include <fcntl.h>
#include <linux/fs.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace jsondict_linux {
namespace {
class Descriptor {
public:
    explicit Descriptor(int fd = -1) : fd_(fd) {}
    ~Descriptor() { if (fd_ >= 0) ::close(fd_); }
    Descriptor(const Descriptor&) = delete;
    Descriptor& operator=(const Descriptor&) = delete;
    int get() const { return fd_; }
private:
    int fd_;
};
struct Target {
    QString directory, basename;
    QByteArray encoded_directory, encoded_basename;
    explicit Target(const QString& path) {
        const QFileInfo info(path);
        directory = info.absolutePath();
        basename = info.fileName();
        if (path.contains(QChar(0)) || basename.isEmpty() || basename == "." || basename == "..")
            throw FileError(FileFailure::NotRegular, QStringLiteral("Invalid file name"));
        encoded_directory = QFile::encodeName(directory);
        encoded_basename = QFile::encodeName(basename);
    }
};
bool stable_stat(const struct stat& a, const struct stat& b) {
    return a.st_dev == b.st_dev && a.st_ino == b.st_ino && a.st_size == b.st_size &&
        a.st_mtim.tv_sec == b.st_mtim.tv_sec && a.st_mtim.tv_nsec == b.st_mtim.tv_nsec &&
        a.st_ctim.tv_sec == b.st_ctim.tv_sec && a.st_ctim.tv_nsec == b.st_ctim.tv_nsec;
}
FileStamp stamp_from(const struct stat& stat, const QByteArray& bytes) {
    return {static_cast<std::uint64_t>(stat.st_dev), static_cast<std::uint64_t>(stat.st_ino),
        stat.st_size, stat.st_mtim.tv_sec, stat.st_mtim.tv_nsec,
        static_cast<unsigned int>(stat.st_mode & 07777), static_cast<unsigned int>(stat.st_uid),
        static_cast<unsigned int>(stat.st_nlink), static_cast<unsigned int>(stat.st_gid),
        QCryptographicHash::hash(bytes, QCryptographicHash::Sha256)};
}
bool has_extended_attributes(int fd) {
    ssize_t size;
    do { size = ::flistxattr(fd, nullptr, 0); } while (size < 0 && errno == EINTR);
    if (size < 0 && (errno == ENOTSUP || errno == EOPNOTSUPP)) return false;
    if (size < 0) throw FileError(FileFailure::IO, QStringLiteral("Inspect extended attributes"), errno);
    return size > 0;
}
void require_plain_metadata(const FileSnapshot& snapshot) {
    if (snapshot.stamp.extended_attributes)
        throw FileError(FileFailure::UnsupportedAtomicSave,
            QStringLiteral("Extended attributes and ACLs require an explicitly supported metadata policy"));
}
std::optional<FileSnapshot> read_at(int directory, const QByteArray& basename) {
    struct stat path_before{};
    if (::fstatat(directory, basename.constData(), &path_before, AT_SYMLINK_NOFOLLOW) < 0) {
        if (errno == ENOENT) return std::nullopt;
        throw FileError(FileFailure::IO, QStringLiteral("Inspect file"), errno);
    }
    if (S_ISLNK(path_before.st_mode))
        throw FileError(FileFailure::Symlink, QStringLiteral("Symbolic-link targets are not supported"));
    if (!S_ISREG(path_before.st_mode))
        throw FileError(FileFailure::NotRegular, QStringLiteral("The target is not a regular file"));
    Descriptor file(::openat(directory, basename.constData(), O_RDONLY | O_CLOEXEC | O_NOFOLLOW));
    if (file.get() < 0) throw FileError(FileFailure::IO, QStringLiteral("Open file"), errno);
    struct stat before{}, after{}, path_after{};
    if (::fstat(file.get(), &before) < 0)
        throw FileError(FileFailure::IO, QStringLiteral("Read file identity"), errno);
    if (!stable_stat(path_before, before))
        throw FileError(FileFailure::ChangedDuringRead, QStringLiteral("File changed before reading"));
    if (before.st_size < 0 || static_cast<std::uint64_t>(before.st_size) > kMaximumFileBytes)
        throw FileError(FileFailure::TooLarge, QStringLiteral("File exceeds 16 MiB"));
    QByteArray bytes;
    bytes.resize(static_cast<qsizetype>(before.st_size));
    qsizetype offset = 0;
    while (offset < bytes.size()) {
        const ssize_t received = ::read(file.get(), bytes.data() + offset,
                                        static_cast<std::size_t>(bytes.size() - offset));
        if (received < 0 && errno == EINTR) continue;
        if (received <= 0) throw FileError(FileFailure::IO, QStringLiteral("Read file"), received < 0 ? errno : EIO);
        offset += received;
    }
    const bool metadata = has_extended_attributes(file.get());
    if (::fstat(file.get(), &after) < 0 ||
        ::fstatat(directory, basename.constData(), &path_after, AT_SYMLINK_NOFOLLOW) < 0)
        throw FileError(FileFailure::ChangedDuringRead, QStringLiteral("File changed during reading"), errno);
    if (!stable_stat(before, after) || !stable_stat(after, path_after))
        throw FileError(FileFailure::ChangedDuringRead, QStringLiteral("File changed during reading"));
    auto stamp = stamp_from(after, bytes);
    stamp.extended_attributes = metadata;
    return FileSnapshot{bytes, std::move(stamp)};
}
Descriptor open_directory(const Target& target) {
    int fd = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) throw FileError(FileFailure::IO, QStringLiteral("Open root directory"), errno);
    for (const auto& component : target.encoded_directory.split('/')) {
        if (component.isEmpty() || component == ".") continue;
        const int next = ::openat(fd, component.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
        const int error = errno;
        ::close(fd);
        if (next < 0) throw FileError(error == ELOOP || error == ENOTDIR ? FileFailure::Symlink : FileFailure::IO,
                                    QStringLiteral("Open containing directory without symbolic links"), error);
        fd = next;
    }
    return Descriptor(fd);
}
bool matches(const std::optional<FileSnapshot>& current, const std::optional<FileStamp>& expected) {
    return expected ? current && current->stamp == *expected : !current;
}
void call_hook(const SaveOptions& options, SaveStep step) { if (options.hook) options.hook(step); }
}  // namespace

bool FileStamp::operator==(const FileStamp& other) const {
    return device == other.device && inode == other.inode && size == other.size &&
        modified_seconds == other.modified_seconds && modified_nanos == other.modified_nanos &&
        mode == other.mode && owner == other.owner && group == other.group && links == other.links &&
        sha256 == other.sha256 && extended_attributes == other.extended_attributes;
}
FileError::FileError(FileFailure type_value, QString operation_value, int error_value,
                     bool committed_value, QString backup_value, QString target_value)
    : std::runtime_error((operation_value + (error_value ?
        QStringLiteral(": ") + QString::fromLocal8Bit(std::strerror(error_value)) : QString{})).toStdString()),
      type(type_value), operation(std::move(operation_value)), system_error(error_value),
      committed(committed_value), backup(std::move(backup_value)), target(std::move(target_value)) {}

FileSnapshot FileStore::read(const QString& path) {
    const Target target(path);
    auto directory = open_directory(target);
    const auto snapshot = read_at(directory.get(), target.encoded_basename);
    if (!snapshot) throw FileError(FileFailure::IO, QStringLiteral("File does not exist"), ENOENT);
    return *snapshot;
}
std::optional<FileStamp> FileStore::probe(const QString& path) {
    const Target target(path);
    auto directory = open_directory(target);
    const auto snapshot = read_at(directory.get(), target.encoded_basename);
    return snapshot ? std::optional<FileStamp>(snapshot->stamp) : std::nullopt;
}

FileStamp FileStore::save(const QString& path, const QByteArray& bytes,
                           const std::optional<FileStamp>& expected, const SaveOptions& options) {
    if (static_cast<std::size_t>(bytes.size()) > kMaximumFileBytes)
        throw FileError(FileFailure::TooLarge, QStringLiteral("Output exceeds 16 MiB"));
    const Target target(path);
    auto directory = open_directory(target);
    struct stat directory_identity{};
    if (::fstat(directory.get(), &directory_identity) < 0)
        throw FileError(FileFailure::IO, QStringLiteral("Inspect containing directory"), errno);
    if (!(directory_identity.st_mode & 0222))
        throw FileError(FileFailure::IO, QStringLiteral("The containing directory is read-only"), EACCES);
    if (::fsync(directory.get()) < 0)
        throw FileError(FileFailure::UnsupportedAtomicSave, QStringLiteral("Directory fsync is unavailable"), errno);
    auto current = read_at(directory.get(), target.encoded_basename);
    if (!matches(current, expected))
        throw FileError(FileFailure::Conflict, QStringLiteral("The target changed outside the editor"));
    if (current && current->stamp.links != 1)
        throw FileError(FileFailure::Hardlink, QStringLiteral("Hard-linked targets are not supported"));
    if (current && current->stamp.owner != static_cast<unsigned int>(::geteuid()))
        throw FileError(FileFailure::UnsupportedAtomicSave, QStringLiteral("Only user-owned targets are supported"));
    if (current && (current->stamp.mode & 07000))
        throw FileError(FileFailure::UnsupportedAtomicSave, QStringLiteral("Special file permissions are not supported"));
    if (current) require_plain_metadata(*current);
    const QByteArray temporary = ".jsondict-backup-" + QByteArray::number(::getpid()) + '-' +
        QByteArray::number(QRandomGenerator::system()->generate64(), 16);
    const QString backup_path = QDir(target.directory).filePath(QFile::decodeName(temporary));
    Descriptor file(::openat(directory.get(), temporary.constData(),
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW, 0600));
    if (file.get() < 0) throw FileError(FileFailure::IO, QStringLiteral("Create same-directory temporary file"), errno);
    bool committed = false;
    const bool backup_present = expected.has_value();
    try {
        struct stat temporary_identity{};
        if (::fstat(file.get(), &temporary_identity) < 0)
            throw FileError(FileFailure::IO, QStringLiteral("Inspect temporary file"), errno);
        if (current && temporary_identity.st_gid != current->stamp.group &&
            ::fchown(file.get(), static_cast<uid_t>(-1), current->stamp.group) < 0)
            throw FileError(FileFailure::IO, QStringLiteral("Preserve file group"), errno);
        if (current && ::fchmod(file.get(), current->stamp.mode & 0777) < 0)
            throw FileError(FileFailure::IO, QStringLiteral("Preserve file permissions"), errno);
        qsizetype offset = 0;
        while (offset < bytes.size()) {
            const ssize_t written = ::write(file.get(), bytes.constData() + offset,
                                            static_cast<std::size_t>(bytes.size() - offset));
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw FileError(FileFailure::IO, QStringLiteral("Write temporary file"), written < 0 ? errno : EIO);
            offset += written;
        }
        if (::fsync(file.get()) < 0)
            throw FileError(FileFailure::Durability, QStringLiteral("Flush temporary file"), errno);
        call_hook(options, SaveStep::TemporaryWritten);
        call_hook(options, SaveStep::BeforeRename);
        current = read_at(directory.get(), target.encoded_basename);
        if (!matches(current, expected))
            throw FileError(FileFailure::Conflict, QStringLiteral("The target changed while saving"));
        const auto prepared = read_at(directory.get(), temporary);
        if (prepared) require_plain_metadata(*prepared);
        if (!prepared || prepared->stamp.inode != static_cast<std::uint64_t>(temporary_identity.st_ino) ||
            prepared->stamp.device != static_cast<std::uint64_t>(temporary_identity.st_dev) || prepared->bytes != bytes)
            throw FileError(FileFailure::Verification, QStringLiteral("Temporary content or identity changed before commit"));
        call_hook(options, SaveStep::AtCommit);
        const unsigned flags = expected ? RENAME_EXCHANGE : RENAME_NOREPLACE;
        if (::renameat2(directory.get(), temporary.constData(),
                      directory.get(), target.encoded_basename.constData(), flags) < 0) {
            const int error = errno;
            throw FileError(error == ENOSYS || error == EOPNOTSUPP || error == EINVAL
                ? FileFailure::UnsupportedAtomicSave : error == EEXIST
                ? FileFailure::Conflict : FileFailure::IO,
                QStringLiteral("Atomic renameat2 commit failed; no direct-write fallback"), error);
        }
        committed = true;
        call_hook(options, SaveStep::AfterRename);
        if (::fsync(directory.get()) < 0)
            throw FileError(FileFailure::Durability, QStringLiteral("Flush containing directory"), errno);
        const auto installed = read_at(directory.get(), target.encoded_basename);
        if (installed) require_plain_metadata(*installed);
        if (!installed || installed->bytes != bytes || installed->stamp.inode != prepared->stamp.inode ||
            installed->stamp.device != prepared->stamp.device || installed->stamp.mode != prepared->stamp.mode ||
            installed->stamp.owner != prepared->stamp.owner || installed->stamp.group != prepared->stamp.group || installed->stamp.links != 1)
            throw FileError(FileFailure::Verification, QStringLiteral("Saved content verification failed"));
        if (expected) {
            const auto replaced = read_at(directory.get(), temporary);
            if (replaced) require_plain_metadata(*replaced);
            if (!replaced || !(replaced->stamp == *expected))
                throw FileError(FileFailure::Conflict, QStringLiteral("The actual replaced file differed from the baseline"));
        }
        struct stat path_directory{};
        if (::stat(target.encoded_directory.constData(), &path_directory) < 0 ||
            path_directory.st_dev != directory_identity.st_dev || path_directory.st_ino != directory_identity.st_ino)
            throw FileError(FileFailure::Verification, QStringLiteral("Containing directory path changed"));
        call_hook(options, SaveStep::BeforeBackupRemoval);
        const auto verified = read_at(directory.get(), target.encoded_basename);
        if (verified) require_plain_metadata(*verified);
        if (!verified || !(verified->stamp == installed->stamp))
            throw FileError(FileFailure::Verification, QStringLiteral("The file changed after saving"));
        if (expected) {
            const auto previous = read_at(directory.get(), temporary);
            if (previous) require_plain_metadata(*previous);
            if (!previous || !(previous->stamp == *expected))
                throw FileError(FileFailure::Conflict, QStringLiteral("The recovery version changed after commit"));
        }
        // Retain the actual replaced version, including after successful Save.
        // It must survive a failed final directory fsync; cleanup is explicit.
        if (::fsync(directory.get()) < 0)
            throw FileError(FileFailure::Durability, QStringLiteral("Finalize containing directory flush"), errno);
        return verified->stamp;
    } catch (const FileError& error) {
        if (!committed) ::unlinkat(directory.get(), temporary.constData(), 0);
        throw FileError(error.type, error.operation, error.system_error, committed,
                        committed && backup_present ? backup_path : QString{}, QFileInfo(path).absoluteFilePath());
    } catch (const std::exception& error) {
        if (!committed) ::unlinkat(directory.get(), temporary.constData(), 0);
        throw FileError(FileFailure::IO, QString::fromUtf8(error.what()), 0, committed,
                        committed && backup_present ? backup_path : QString{}, QFileInfo(path).absoluteFilePath());
    }
}

}  // namespace jsondict_linux
