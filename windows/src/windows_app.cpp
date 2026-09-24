#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif

#include <windows.h>
#include <bcrypt.h>
#include <commctrl.h>
#include <commdlg.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <windowsx.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <climits>
#include <cstdio>
#include <cwchar>
#include <exception>
#include <functional>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "json_core.hpp"

namespace {

constexpr wchar_t kAppTitle[] = L"JSON 字典编辑器";
constexpr wchar_t kMainWindowClass[] = L"CodexJsonDictionaryEditorMainWindow";
constexpr wchar_t kRawWindowClass[] = L"CodexJsonDictionaryEditorRawWindow";
constexpr std::uint64_t kMaximumFileBytes = 16ull * 1024ull * 1024ull;
constexpr std::size_t kMaximumRawUtf8Bytes = 4ull * 1024ull * 1024ull;
constexpr std::size_t kMaximumRawCharacters = 4ull * 1024ull * 1024ull;
constexpr std::size_t kMaximumInspectorCharacters = 1ull * 1024ull * 1024ull;
constexpr std::size_t kMaximumKeyCharacters = 65535;
constexpr std::size_t kMaximumDocumentNodes = 50000;
constexpr UINT_PTR kSearchTimerId = 1;
constexpr UINT kRebuildTreeMessage = WM_APP + 1;
constexpr UINT kSearchRefreshMessage = WM_APP + 2;
constexpr int kAppIconResource = 101;

enum CommandId : int {
    ID_FILE_NEW = 100,
    ID_FILE_OPEN,
    ID_FILE_SAVE,
    ID_FILE_SAVE_AS,
    ID_FILE_EXIT,
    ID_EDIT_FIND = 130,
    ID_NODE_ADD_MENU = 199,
    ID_NODE_ADD_STRING = 200,
    ID_NODE_ADD_NUMBER,
    ID_NODE_ADD_BOOLEAN,
    ID_NODE_ADD_NULL,
    ID_NODE_ADD_OBJECT,
    ID_NODE_ADD_ARRAY,
    ID_NODE_DUPLICATE,
    ID_NODE_DELETE,
    ID_NODE_MOVE_UP,
    ID_NODE_MOVE_DOWN,
    ID_NODE_SORT,
    ID_NODE_RAW,
    ID_NODE_RENAME,
    ID_VIEW_EXPAND_ALL = 250,
    ID_VIEW_COLLAPSE_ALL,
    ID_HELP_ABOUT = 280,

    IDC_SEARCH = 1000,
    IDC_TREE,
    IDC_SPLITTER,
    IDC_TREE_HEADER,
    IDC_STATUS,
    IDC_FORMAT,
    IDC_TRAILING_NEWLINE,
    IDC_INS_TITLE,
    IDC_INS_PATH_LABEL,
    IDC_INS_PATH,
    IDC_INS_KEY_LABEL,
    IDC_INS_KEY,
    IDC_INS_KEY_APPLY,
    IDC_INS_TYPE_LABEL,
    IDC_INS_TYPE,
    IDC_INS_VALUE_LABEL,
    IDC_INS_STRING,
    IDC_INS_NUMBER,
    IDC_INS_VALUE_APPLY,
    IDC_INS_BOOLEAN,
    IDC_INS_INFO,
    IDC_INS_VALIDATION,
    IDC_INS_ADD,
    IDC_INS_RAW,

    IDC_RAW_EDIT = 2000,
    IDC_RAW_STATUS,
    IDC_RAW_CHECK,
    IDC_RAW_FORMAT,
    IDC_RAW_APPLY,
    IDC_RAW_CANCEL,
};

class Win32Error : public std::runtime_error {
public:
    Win32Error(std::string operation, DWORD code)
        : std::runtime_error(std::move(operation)), code_(code) {}
    DWORD code() const noexcept { return code_; }

private:
    DWORD code_;
};

class UniqueHandle {
public:
    UniqueHandle() = default;
    explicit UniqueHandle(HANDLE value) : value_(value) {}
    ~UniqueHandle() { reset(); }
    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;
    UniqueHandle(UniqueHandle&& other) noexcept : value_(other.release()) {}
    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) reset(other.release());
        return *this;
    }
    HANDLE get() const noexcept { return value_; }
    explicit operator bool() const noexcept {
        return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
    }
    HANDLE release() noexcept {
        HANDLE value = value_;
        value_ = INVALID_HANDLE_VALUE;
        return value;
    }
    void reset(HANDLE value = INVALID_HANDLE_VALUE) noexcept {
        if (*this) CloseHandle(value_);
        value_ = value;
    }

private:
    HANDLE value_ = INVALID_HANDLE_VALUE;
};

class BoolFlagGuard {
public:
    explicit BoolFlagGuard(bool& flag, bool value = true)
        : flag_(flag), previous_(flag) {
        flag_ = value;
    }
    ~BoolFlagGuard() { flag_ = previous_; }
    BoolFlagGuard(const BoolFlagGuard&) = delete;
    BoolFlagGuard& operator=(const BoolFlagGuard&) = delete;

private:
    bool& flag_;
    bool previous_;
};

class WindowRedrawGuard {
public:
    explicit WindowRedrawGuard(HWND window) : window_(window) {
        if (window_) SendMessageW(window_, WM_SETREDRAW, FALSE, 0);
    }
    ~WindowRedrawGuard() {
        if (window_) {
            SendMessageW(window_, WM_SETREDRAW, TRUE, 0);
            InvalidateRect(window_, nullptr, TRUE);
        }
    }
    WindowRedrawGuard(const WindowRedrawGuard&) = delete;
    WindowRedrawGuard& operator=(const WindowRedrawGuard&) = delete;

private:
    HWND window_;
};

std::wstring Utf8ToWide(std::string_view text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<std::size_t>(INT_MAX)) {
        throw std::runtime_error("UTF-8 text is too large");
    }
    const int length = MultiByteToWideChar(
        CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) throw Win32Error("MultiByteToWideChar", GetLastError());
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), result.data(), length) != length) {
        throw Win32Error("MultiByteToWideChar", GetLastError());
    }
    return result;
}

std::string WideToUtf8(std::wstring_view text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<std::size_t>(INT_MAX)) {
        throw std::runtime_error("UTF-16 text is too large");
    }
    const int length = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
        nullptr, 0, nullptr, nullptr);
    if (length <= 0) throw Win32Error("WideCharToMultiByte", GetLastError());
    std::string result(static_cast<std::size_t>(length), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
                            static_cast<int>(text.size()), result.data(), length,
                            nullptr, nullptr) != length) {
        throw Win32Error("WideCharToMultiByte", GetLastError());
    }
    return result;
}

std::wstring WindowsErrorMessage(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
    std::wstring result = length && buffer ? std::wstring(buffer, length) : L"未知系统错误";
    if (buffer) LocalFree(buffer);
    while (!result.empty() && (result.back() == L'\r' || result.back() == L'\n')) {
        result.pop_back();
    }
    return result;
}

std::wstring ExceptionMessage(const std::exception& error) {
    if (const auto* win32 = dynamic_cast<const Win32Error*>(&error)) {
        return Utf8ToWide(win32->what()) + L"：" + WindowsErrorMessage(win32->code());
    }
    if (const auto* json = dynamic_cast<const jsondict::Error*>(&error)) {
        std::wstring result = Utf8ToWide(json->what());
        if (json->line() != 0) {
            result += L"（第 " + std::to_wstring(json->line()) + L" 行，第 " +
                      std::to_wstring(json->column()) + L" 列）";
        }
        if (!json->path().empty()) result += L"\n路径：" + Utf8ToWide(json->path());
        return result;
    }
    try {
        return Utf8ToWide(error.what());
    } catch (...) {
        return L"未知错误";
    }
}

std::wstring GetControlText(HWND control) {
    const int length = GetWindowTextLengthW(control);
    std::wstring result(static_cast<std::size_t>(std::max(0, length)) + 1, L'\0');
    const int copied = GetWindowTextW(control, result.data(), static_cast<int>(result.size()));
    result.resize(static_cast<std::size_t>(std::max(0, copied)));
    return result;
}

void SetControlText(HWND control, std::wstring_view value) {
    std::wstring terminated(value);
    SetWindowTextW(control, terminated.c_str());
}

std::wstring NormalizeEditNewlines(std::wstring text) {
    std::wstring result;
    result.reserve(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') {
            result.push_back(L'\n');
            ++i;
        } else {
            result.push_back(text[i]);
        }
    }
    return result;
}

std::wstring ToEditNewlines(std::wstring_view text) {
    std::wstring result;
    result.reserve(text.size() + text.size() / 16);
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] == L'\n' && (i == 0 || text[i - 1] != L'\r')) result.push_back(L'\r');
        result.push_back(text[i]);
    }
    return result;
}

std::wstring EscapeForUi(std::wstring_view text, std::size_t limit = 72) {
    std::wstring result;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const wchar_t unit = text[index];
        if (result.size() >= limit) {
            result += L"…";
            break;
        }
        if (unit >= 0xD800 && unit <= 0xDBFF && index + 1 < text.size() &&
            text[index + 1] >= 0xDC00 && text[index + 1] <= 0xDFFF) {
            if (result.size() + 2 > limit) {
                result += L"…";
                break;
            }
            result.push_back(unit);
            result.push_back(text[++index]);
            continue;
        }
        switch (unit) {
            case L'\n': result += L" ↩ "; break;
            case L'\r': break;
            case L'\t': result += L" ⇥ "; break;
            default:
                if (unit < 0x20) {
                    wchar_t escaped[7]{};
                    swprintf_s(escaped, L"\\u%04X", static_cast<unsigned>(unit));
                    result += escaped;
                } else {
                    result.push_back(unit);
                }
                break;
        }
    }
    return result;
}

std::wstring Utf8Preview(std::string_view text, std::size_t maximum_bytes = 256) {
    if (text.size() <= maximum_bytes) return Utf8ToWide(text);
    std::size_t end = maximum_bytes;
    while (end > 0 &&
           (static_cast<unsigned char>(text[end]) & 0xC0u) == 0x80u) {
        --end;
    }
    std::wstring preview = Utf8ToWide(text.substr(0, end));
    preview += L"…";
    return preview;
}

std::wstring KindTitle(jsondict::Kind kind) {
    switch (kind) {
        case jsondict::Kind::String: return L"字符串";
        case jsondict::Kind::Number: return L"数字";
        case jsondict::Kind::Boolean: return L"布尔值";
        case jsondict::Kind::Null: return L"Null";
        case jsondict::Kind::Object: return L"对象";
        case jsondict::Kind::Array: return L"数组";
    }
    return L"未知";
}

std::wstring NodeSummary(const jsondict::Node& node) {
    switch (node.kind()) {
        case jsondict::Kind::String:
            return EscapeForUi(Utf8Preview(node.as_string()));
        case jsondict::Kind::Number: {
            const std::string& text = node.as_number().text;
            std::wstring preview = Utf8ToWide(text.substr(
                0, std::min<std::size_t>(text.size(), 96)));
            if (text.size() > 96) preview += L"…";
            return preview;
        }
        case jsondict::Kind::Boolean:
            return node.as_boolean() ? L"true" : L"false";
        case jsondict::Kind::Null:
            return L"null";
        case jsondict::Kind::Object:
            return node.child_count() == 0 ? L"空对象" :
                std::to_wstring(node.child_count()) + L" 个键";
        case jsondict::Kind::Array:
            return node.child_count() == 0 ? L"空数组" :
                std::to_wstring(node.child_count()) + L" 个元素";
    }
    return {};
}

std::wstring ToLowerInvariant(std::wstring value) {
    if (!value.empty()) CharLowerBuffW(value.data(), static_cast<DWORD>(value.size()));
    return value;
}

bool ContainsInsensitive(std::wstring_view haystack, std::wstring_view needle_lower) {
    if (needle_lower.empty()) return true;
    return ToLowerInvariant(std::wstring(haystack)).find(needle_lower) != std::wstring::npos;
}

bool IsAsciiIdentifierKey(std::string_view key) noexcept {
    if (key.empty()) return false;
    const auto identifier_start = [](unsigned char byte) {
        return (byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') ||
               byte == '_' || byte == '$';
    };
    const auto identifier_continue = [&](unsigned char byte) {
        return identifier_start(byte) || (byte >= '0' && byte <= '9');
    };
    if (!identifier_start(static_cast<unsigned char>(key.front()))) return false;
    return std::all_of(key.begin() + 1, key.end(), [&](char byte) {
        return identifier_continue(static_cast<unsigned char>(byte));
    });
}

std::string SearchPathForKey(std::string_view base, std::string_view key) {
    std::string path(base);
    if (IsAsciiIdentifierKey(key)) {
        path.push_back('.');
        path.append(key.data(), key.size());
    } else {
        path.push_back('[');
        path += jsondict::write(jsondict::Node::string(std::string(key)),
                                jsondict::WriteOptions{
                                    jsondict::Formatting::Compact, false, false});
        path.push_back(']');
    }
    return path;
}

struct FileStamp {
    DWORD volume_serial = 0;
    DWORD file_index_high = 0;
    DWORD file_index_low = 0;
    DWORD last_write_high = 0;
    DWORD last_write_low = 0;
    std::uint64_t size = 0;

    friend bool operator==(const FileStamp& left, const FileStamp& right) noexcept {
        return left.volume_serial == right.volume_serial &&
               left.file_index_high == right.file_index_high &&
               left.file_index_low == right.file_index_low &&
               left.last_write_high == right.last_write_high &&
               left.last_write_low == right.last_write_low &&
               left.size == right.size;
    }
    friend bool operator!=(const FileStamp& left, const FileStamp& right) noexcept {
        return !(left == right);
    }
};

using Sha256Digest = std::array<unsigned char, 32>;

Sha256Digest ComputeSha256(std::string_view bytes) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    try {
        NTSTATUS status = BCryptOpenAlgorithmProvider(
            &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
        if (status < 0) throw std::runtime_error("Windows SHA-256 provider is unavailable");

        ULONG object_size = 0;
        ULONG copied = 0;
        status = BCryptGetProperty(
            algorithm, BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &copied, 0);
        if (status < 0 || copied != sizeof(object_size) || object_size == 0) {
            throw std::runtime_error("Windows SHA-256 provider returned invalid properties");
        }
        std::vector<UCHAR> object(object_size);
        status = BCryptCreateHash(
            algorithm, &hash, object.data(), object_size, nullptr, 0, 0);
        if (status < 0) throw std::runtime_error("Unable to create SHA-256 hash state");

        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const ULONG chunk = static_cast<ULONG>(
                std::min<std::size_t>(bytes.size() - offset, 1u << 20));
            status = BCryptHashData(
                hash,
                reinterpret_cast<PUCHAR>(const_cast<char*>(bytes.data() + offset)),
                chunk, 0);
            if (status < 0) throw std::runtime_error("Unable to update SHA-256 hash state");
            offset += chunk;
        }

        Sha256Digest digest{};
        status = BCryptFinishHash(
            hash, digest.data(), static_cast<ULONG>(digest.size()), 0);
        if (status < 0) throw std::runtime_error("Unable to finish SHA-256 hash");
        BCryptDestroyHash(hash);
        hash = nullptr;
        BCryptCloseAlgorithmProvider(algorithm, 0);
        algorithm = nullptr;
        return digest;
    } catch (...) {
        if (hash) BCryptDestroyHash(hash);
        if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
        throw;
    }
}

struct FileIdentity {
    FileStamp stamp;
    Sha256Digest digest{};

    friend bool operator==(const FileIdentity& left, const FileIdentity& right) noexcept {
        return left.stamp == right.stamp && left.digest == right.digest;
    }
};

enum class ExpectedTargetMode {
    MustBeAbsent,
    MustMatch,
};

struct ExpectedTarget {
    ExpectedTargetMode mode = ExpectedTargetMode::MustBeAbsent;
    FileIdentity identity;

    static ExpectedTarget Absent() { return {}; }
    static ExpectedTarget Match(FileIdentity value) {
        return ExpectedTarget{ExpectedTargetMode::MustMatch, std::move(value)};
    }
};

FileStamp ReadFileStamp(HANDLE file) {
    BY_HANDLE_FILE_INFORMATION information{};
    if (!GetFileInformationByHandle(file, &information)) {
        throw Win32Error("读取文件身份失败", GetLastError());
    }
    return FileStamp{
        information.dwVolumeSerialNumber,
        information.nFileIndexHigh,
        information.nFileIndexLow,
        information.ftLastWriteTime.dwHighDateTime,
        information.ftLastWriteTime.dwLowDateTime,
        (static_cast<std::uint64_t>(information.nFileSizeHigh) << 32) |
            information.nFileSizeLow};
}

std::string ReadBoundedBytes(HANDLE file, std::uint64_t size) {
    if (size > kMaximumFileBytes) {
        throw std::runtime_error("文件超过 16 MiB 的安全编辑上限");
    }
    std::string bytes(static_cast<std::size_t>(size), '\0');
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const DWORD request = static_cast<DWORD>(
            std::min<std::size_t>(bytes.size() - offset, 1u << 20));
        DWORD received = 0;
        if (!ReadFile(file, bytes.data() + offset, request, &received, nullptr)) {
            throw Win32Error("读取文件失败", GetLastError());
        }
        if (received == 0) throw std::runtime_error("读取文件时意外到达末尾");
        offset += received;
    }
    return bytes;
}

std::optional<FileIdentity> TryReadFileIdentity(const std::wstring& path) {
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) {
        const DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND) return std::nullopt;
        throw Win32Error("检查文件外部更改失败", error);
    }
    const FileStamp before = ReadFileStamp(file.get());
    const std::string bytes = ReadBoundedBytes(file.get(), before.size);
    const FileStamp after = ReadFileStamp(file.get());
    if (after != before) {
        throw std::runtime_error("文件在检查过程中被其他程序修改，请重试");
    }
    return FileIdentity{after, ComputeSha256(bytes)};
}

ExpectedTarget SnapshotExpectedTarget(const std::wstring& path) {
    const auto identity = TryReadFileIdentity(path);
    return identity ? ExpectedTarget::Match(*identity) : ExpectedTarget::Absent();
}

bool MatchesExpectedTarget(const std::optional<FileIdentity>& current,
                           const ExpectedTarget& expected) noexcept {
    return (expected.mode == ExpectedTargetMode::MustBeAbsent && !current) ||
           (expected.mode == ExpectedTargetMode::MustMatch && current &&
            *current == expected.identity);
}

struct FileContents {
    std::string utf8;
    bool had_utf8_bom = false;
    FileIdentity identity;
};

FileContents ReadUtf8File(const std::wstring& path) {
    UniqueHandle file(CreateFileW(path.c_str(), GENERIC_READ,
                                  FILE_SHARE_READ,
                                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (!file) throw Win32Error("打开文件失败", GetLastError());
    const FileStamp stamp_before = ReadFileStamp(file.get());
    std::string bytes = ReadBoundedBytes(file.get(), stamp_before.size);

    if (bytes.size() >= 2) {
        const unsigned char first = static_cast<unsigned char>(bytes[0]);
        const unsigned char second = static_cast<unsigned char>(bytes[1]);
        if ((first == 0xFF && second == 0xFE) || (first == 0xFE && second == 0xFF)) {
            throw std::runtime_error("检测到 UTF-16 文件；当前版本仅支持 UTF-8 JSON");
        }
    }

    const FileStamp stamp_after = ReadFileStamp(file.get());
    if (stamp_after != stamp_before) {
        throw std::runtime_error("文件在读取过程中被其他程序修改，请重试");
    }
    FileContents result;
    result.identity = FileIdentity{stamp_after, ComputeSha256(bytes)};
    if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
        static_cast<unsigned char>(bytes[1]) == 0xBB &&
        static_cast<unsigned char>(bytes[2]) == 0xBF) {
        result.had_utf8_bom = true;
        result.utf8.assign(bytes.begin() + 3, bytes.end());
    } else {
        result.utf8 = std::move(bytes);
    }
    return result;
}

std::wstring TemporarySiblingPath(const std::wstring& target, unsigned attempt) {
    return target + L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." +
           std::to_wstring(attempt);
}

void WriteFileAtomically(const std::wstring& target, std::string_view bytes,
                         const ExpectedTarget& expected) {
    const Sha256Digest replacement_digest = ComputeSha256(bytes);
    std::wstring temporary;
    std::wstring safety_backup;
    UniqueHandle file;
    for (unsigned attempt = 1; attempt <= 100; ++attempt) {
        temporary = TemporarySiblingPath(target, attempt);
        file.reset(CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW,
                               FILE_ATTRIBUTE_NORMAL, nullptr));
        if (file) {
            if (expected.mode == ExpectedTargetMode::MustMatch) {
                safety_backup = temporary + L".previous";
                const DWORD attributes = GetFileAttributesW(safety_backup.c_str());
                const DWORD attribute_error = GetLastError();
                if (attributes != INVALID_FILE_ATTRIBUTES ||
                    (attribute_error != ERROR_FILE_NOT_FOUND &&
                     attribute_error != ERROR_PATH_NOT_FOUND)) {
                    file.reset();
                    if (!DeleteFileW(temporary.c_str())) {
                        throw Win32Error("清理临时文件失败", GetLastError());
                    }
                    continue;
                }
            }
            break;
        }
        if (GetLastError() != ERROR_FILE_EXISTS) {
            throw Win32Error("创建安全保存临时文件失败", GetLastError());
        }
    }
    if (!file) throw std::runtime_error("无法为安全保存创建唯一临时文件");

    try {
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            const DWORD request = static_cast<DWORD>(
                std::min<std::size_t>(bytes.size() - offset, 1u << 20));
            DWORD written = 0;
            if (!WriteFile(file.get(), bytes.data() + offset, request, &written, nullptr)) {
                throw Win32Error("写入临时文件失败", GetLastError());
            }
            if (written == 0) throw std::runtime_error("写入临时文件时发生短写");
            offset += written;
        }
        if (!FlushFileBuffers(file.get())) throw Win32Error("刷新临时文件失败", GetLastError());
        file.reset();

        const auto current = TryReadFileIdentity(target);
        const bool target_matches = MatchesExpectedTarget(current, expected);
        if (!target_matches) {
            throw std::runtime_error(
                "目标文件在保存过程中又被其他程序修改；原文件没有被覆盖");
        }

        if (expected.mode == ExpectedTargetMode::MustMatch) {
            if (!ReplaceFileW(target.c_str(), temporary.c_str(), safety_backup.c_str(),
                              0, nullptr, nullptr)) {
                throw Win32Error("原子替换目标文件失败", GetLastError());
            }

            std::optional<FileIdentity> replaced_identity;
            try {
                replaced_identity = TryReadFileIdentity(safety_backup);
            } catch (const std::exception& error) {
                throw std::runtime_error(
                    "无法验证原子替换保留的安全备份；备份仍保留在：" +
                    WideToUtf8(safety_backup) + "\n" + error.what());
            }
            if (!replaced_identity || !(*replaced_identity == expected.identity)) {
                bool restored = false;
                try {
                    const auto installed = TryReadFileIdentity(target);
                    if (installed && installed->digest == replacement_digest) {
                        restored = ReplaceFileW(target.c_str(), safety_backup.c_str(), nullptr,
                                                0, nullptr, nullptr) != FALSE;
                    }
                } catch (...) {
                }
                if (restored) {
                    throw std::runtime_error(
                        "目标文件在最终替换前又被其他程序修改；"
                        "程序已恢复该外部版本，未覆盖它");
                }
                throw std::runtime_error(
                    "目标文件在最终替换前又被其他程序修改。"
                    "为避免数据丢失，被替换的版本已保留在：" +
                    WideToUtf8(safety_backup));
            }
            try {
                const auto installed = TryReadFileIdentity(target);
                if (!installed || installed->digest != replacement_digest) {
                    throw std::runtime_error(
                        "新目标内容与本次写入的 SHA-256 不一致");
                }
            } catch (const std::exception& error) {
                throw std::runtime_error(
                    "原子替换后无法验证新目标内容；原磁盘版本已保留在：" +
                    WideToUtf8(safety_backup) + "\n" + error.what());
            }
            if (!DeleteFileW(safety_backup.c_str())) {
                OutputDebugStringW(
                    (L"无法清理 JSON 安全保存备份：" + safety_backup + L"\n").c_str());
            }
        } else if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH)) {
            throw Win32Error("原子创建目标文件失败", GetLastError());
        }
    } catch (...) {
        file.reset();
        if (!temporary.empty()) DeleteFileW(temporary.c_str());
        throw;
    }
}

std::size_t CountNodes(const jsondict::Node& node) {
    std::size_t count = 1;
    if (node.kind() == jsondict::Kind::Object) {
        for (const auto& member : node.as_object()) count += CountNodes(member.value);
    } else if (node.kind() == jsondict::Kind::Array) {
        for (const auto& child : node.as_array()) count += CountNodes(child);
    }
    return count;
}

struct AppState;
void ShowRawEditor(AppState& app, jsondict::NodeId target_id);

struct AppState {
    HINSTANCE instance = nullptr;
    HWND window = nullptr;
    HWND search = nullptr;
    HWND tree_header = nullptr;
    HWND tree = nullptr;
    HWND splitter = nullptr;
    HWND status = nullptr;
    HWND format_combo = nullptr;
    HWND trailing_checkbox = nullptr;
    HWND title = nullptr;
    HWND path_label = nullptr;
    HWND path_edit = nullptr;
    HWND key_label = nullptr;
    HWND key_edit = nullptr;
    HWND key_apply = nullptr;
    HWND type_label = nullptr;
    HWND type_combo = nullptr;
    HWND value_label = nullptr;
    HWND string_edit = nullptr;
    HWND number_edit = nullptr;
    HWND value_apply = nullptr;
    HWND boolean_check = nullptr;
    HWND info = nullptr;
    HWND validation = nullptr;
    HWND add_child = nullptr;
    HWND raw_button = nullptr;
    HWND tool_add = nullptr;
    HWND tool_duplicate = nullptr;
    HWND tool_delete = nullptr;
    HWND tool_up = nullptr;
    HWND tool_down = nullptr;
    HWND tool_sort = nullptr;
    HWND tool_raw = nullptr;

    jsondict::Document document;
    jsondict::NodeId selected_id = document.root_id();
    std::wstring current_path;
    std::optional<ExpectedTarget> saved_target;
    bool dirty = false;
    bool had_utf8_bom = false;
    bool loading_inspector = false;
    bool inspector_draft_dirty = false;
    bool resolving_inspector_drafts = false;
    bool rebuilding_tree = false;
    bool tree_operational = true;
    bool tree_refresh_needed = false;
    bool search_refresh_pending = false;
    bool search_mode = false;
    bool splitter_dragging = false;
    int split_logical = 540;
    UINT dpi = 96;
    HFONT ui_font = nullptr;
    HFONT title_font = nullptr;
    HFONT mono_font = nullptr;
    HBRUSH background_brush = nullptr;
    HBRUSH edit_brush = nullptr;
    COLORREF background_color = RGB(246, 246, 246);
    COLORREF edit_color = RGB(255, 255, 255);
    COLORREF text_color = RGB(25, 25, 25);
    bool high_contrast = false;
    std::set<jsondict::NodeId> expanded_ids;
    std::vector<HWND> controls;

    int Scale(int value) const { return MulDiv(value, static_cast<int>(dpi), 96); }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param);
    static LRESULT CALLBACK SplitterProc(HWND hwnd, UINT message, WPARAM w_param,
                                         LPARAM l_param, UINT_PTR, DWORD_PTR data);

    bool Create(HINSTANCE app_instance);
    int Run(std::optional<std::wstring> initial_path);
    void CreateControls();
    void CreateFonts();
    void ApplyFonts();
    void ApplyTheme();
    void Layout();
    void DrawSplitter(const DRAWITEMSTRUCT& item) const;
    void RebuildTree();
    void CaptureExpanded();
    void UpdateInspector();
    void UpdateDraftValidation();
    void UpdateStatusAndTitle();
    void UpdateCommandStates();
    void HandleCommand(int id, int notification, HWND source);
    LRESULT HandleTreeNotification(NMHDR* header);
    void ShowTreeContextMenu(POINT screen_point);
    void ShowAddMenu(HWND anchor);
    void AddNode(jsondict::Kind kind);
    void DuplicateSelected();
    void DeleteSelected();
    void MoveSelected(int offset);
    void SortSelected();
    void ChangeSelectedKind();
    void ApplyKeyDraft();
    void ApplyValueDraft();
    bool CommitInspectorDrafts(bool refresh);
    bool ResolveInspectorDrafts(bool refresh = true);
    void ToggleBoolean();
    void ExpandAll(bool expand);
    void MarkDirty();
    void FinishCommittedMutation();
    void SelectNode(jsondict::NodeId id);
    bool ConfirmSaveIfDirty();
    void NewDocument();
    void OpenDocument();
    bool OpenPath(const std::wstring& path, bool ask_about_dirty = true);
    bool SaveDocument();
    bool SaveDocumentAs();
    bool EnsureDocumentWithinUiLimits(const jsondict::Document& candidate,
                                      HWND message_owner = nullptr);
};

HMENU CreateMainMenu() {
    HMENU menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, ID_FILE_NEW, L"新建(&N)\tCtrl+N");
    AppendMenuW(file, MF_STRING, ID_FILE_OPEN, L"打开(&O)…\tCtrl+O");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, ID_FILE_SAVE, L"保存(&S)\tCtrl+S");
    AppendMenuW(file, MF_STRING, ID_FILE_SAVE_AS, L"另存为(&A)…\tCtrl+Shift+S");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, ID_FILE_EXIT, L"退出(&X)");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"文件(&F)");

    HMENU edit = CreatePopupMenu();
    AppendMenuW(edit, MF_STRING, ID_EDIT_FIND, L"搜索(&F)\tCtrl+F");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(edit), L"编辑(&E)");

    HMENU node = CreatePopupMenu();
    HMENU add = CreatePopupMenu();
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_STRING, L"字符串");
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_NUMBER, L"数字");
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_BOOLEAN, L"布尔值");
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_NULL, L"Null");
    AppendMenuW(add, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_OBJECT, L"对象");
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_ARRAY, L"数组");
    AppendMenuW(node, MF_POPUP, reinterpret_cast<UINT_PTR>(add), L"添加(&A)");
    AppendMenuW(node, MF_STRING, ID_NODE_DUPLICATE, L"复制(&D)\tCtrl+D");
    AppendMenuW(node, MF_STRING, ID_NODE_DELETE, L"删除\tDelete");
    AppendMenuW(node, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(node, MF_STRING, ID_NODE_MOVE_UP, L"上移\tAlt+↑");
    AppendMenuW(node, MF_STRING, ID_NODE_MOVE_DOWN, L"下移\tAlt+↓");
    AppendMenuW(node, MF_STRING, ID_NODE_SORT, L"按键名排序");
    AppendMenuW(node, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(node, MF_STRING, ID_NODE_RENAME, L"重命名键\tF2");
    AppendMenuW(node, MF_STRING, ID_NODE_RAW, L"编辑原始 JSON…\tCtrl+E");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(node), L"节点(&N)");

    HMENU view = CreatePopupMenu();
    AppendMenuW(view, MF_STRING, ID_VIEW_EXPAND_ALL, L"全部展开");
    AppendMenuW(view, MF_STRING, ID_VIEW_COLLAPSE_ALL, L"全部折叠");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(view), L"查看(&V)");

    HMENU help = CreatePopupMenu();
    AppendMenuW(help, MF_STRING, ID_HELP_ABOUT, L"关于 JSON 字典编辑器");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(help), L"帮助(&H)");
    return menu;
}

HWND CreateChild(AppState& app, DWORD extended_style, const wchar_t* class_name,
                 const wchar_t* text, DWORD style, int id) {
    HWND control = CreateWindowExW(
        extended_style, class_name, text, WS_CHILD | style,
        0, 0, 10, 10, app.window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        app.instance, nullptr);
    if (!control) throw Win32Error("创建界面控件失败", GetLastError());
    app.controls.push_back(control);
    return control;
}

void SetVisible(HWND control, bool visible) {
    ShowWindow(control, visible ? SW_SHOW : SW_HIDE);
}

bool IsHighContrast() {
    HIGHCONTRASTW high_contrast{};
    high_contrast.cbSize = sizeof(high_contrast);
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(high_contrast),
                                 &high_contrast, 0) &&
           (high_contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

bool AppState::Create(HINSTANCE app_instance) {
    instance = app_instance;
    dpi = GetDpiForSystem();
    if (dpi == 0) dpi = 96;
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.style = CS_HREDRAW | CS_VREDRAW;
    window_class.lpfnWndProc = WindowProc;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(kAppIconResource));
    if (!window_class.hIcon) window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.hbrBackground = nullptr;
    window_class.lpszClassName = kMainWindowClass;
    window_class.hIconSm = window_class.hIcon;
    if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    constexpr DWORD extended_style = WS_EX_ACCEPTFILES | WS_EX_CONTROLPARENT;
    constexpr DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    RECT frame{0, 0, Scale(1080), Scale(700)};
    if (!AdjustWindowRectExForDpi(&frame, style, TRUE, extended_style, dpi)) {
        AdjustWindowRectEx(&frame, style, TRUE, extended_style);
    }
    window = CreateWindowExW(
        extended_style, kMainWindowClass, kAppTitle, style,
        CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left, frame.bottom - frame.top,
        nullptr, CreateMainMenu(), instance, this);
    return window != nullptr;
}

void AppState::CreateControls() {
    dpi = GetDpiForWindow(window);
    if (dpi == 0) dpi = 96;

    tool_add = CreateChild(*this, 0, L"BUTTON", L"添加 ▾", WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                           ID_NODE_ADD_MENU);
    tool_duplicate = CreateChild(*this, 0, L"BUTTON", L"复制", WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                 ID_NODE_DUPLICATE);
    tool_delete = CreateChild(*this, 0, L"BUTTON", L"删除", WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                              ID_NODE_DELETE);
    tool_up = CreateChild(*this, 0, L"BUTTON", L"上移", WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                          ID_NODE_MOVE_UP);
    tool_down = CreateChild(*this, 0, L"BUTTON", L"下移", WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            ID_NODE_MOVE_DOWN);
    tool_sort = CreateChild(*this, 0, L"BUTTON", L"排序", WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                            ID_NODE_SORT);
    tool_raw = CreateChild(*this, 0, L"BUTTON", L"原始 JSON", WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                           ID_NODE_RAW);

    search = CreateChild(*this, WS_EX_CLIENTEDGE, L"EDIT", L"",
                         WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, IDC_SEARCH);
    SendMessageW(search, EM_SETCUEBANNER, TRUE,
                 reinterpret_cast<LPARAM>(L"搜索键名、路径、类型或值"));
    tree_header = CreateChild(*this, 0, L"STATIC", L"键 / 索引          类型          值",
                              WS_VISIBLE | SS_LEFT, IDC_TREE_HEADER);
    tree = CreateChild(*this, WS_EX_CLIENTEDGE, WC_TREEVIEWW, L"",
                       WS_VISIBLE | WS_TABSTOP | TVS_HASBUTTONS | TVS_HASLINES |
                           TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                       IDC_TREE);
    SetWindowTheme(tree, L"Explorer", nullptr);

    // Keep a descriptive window name, but paint only the divider and grip:
    // a default text STATIC wraps this caption into clipped vertical glyphs.
    splitter = CreateChild(*this, 0, L"STATIC", L"调整左右面板宽度",
                           WS_VISIBLE | WS_TABSTOP | SS_NOTIFY | SS_OWNERDRAW,
                           IDC_SPLITTER);
    if (!SetWindowSubclass(splitter, SplitterProc, 1, reinterpret_cast<DWORD_PTR>(this))) {
        throw std::runtime_error("无法初始化面板分隔条");
    }

    title = CreateChild(*this, 0, L"STATIC", L"根对象", WS_VISIBLE | SS_LEFT, IDC_INS_TITLE);
    path_label = CreateChild(*this, 0, L"STATIC", L"JSON 路径", WS_VISIBLE, IDC_INS_PATH_LABEL);
    path_edit = CreateChild(*this, WS_EX_CLIENTEDGE, L"EDIT", L"$",
                            WS_VISIBLE | WS_TABSTOP | ES_READONLY | ES_AUTOHSCROLL, IDC_INS_PATH);
    key_label = CreateChild(*this, 0, L"STATIC", L"键名", WS_VISIBLE, IDC_INS_KEY_LABEL);
    key_edit = CreateChild(*this, WS_EX_CLIENTEDGE, L"EDIT", L"",
                           WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, IDC_INS_KEY);
    key_apply = CreateChild(*this, 0, L"BUTTON", L"应用", WS_VISIBLE | WS_TABSTOP,
                            IDC_INS_KEY_APPLY);
    type_label = CreateChild(*this, 0, L"STATIC", L"值类型", WS_VISIBLE, IDC_INS_TYPE_LABEL);
    type_combo = CreateChild(*this, 0, WC_COMBOBOXW, L"",
                             WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
                             IDC_INS_TYPE);
    for (const wchar_t* label : {L"字符串", L"数字", L"布尔值", L"Null", L"对象", L"数组"}) {
        SendMessageW(type_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    }
    value_label = CreateChild(*this, 0, L"STATIC", L"值", WS_VISIBLE, IDC_INS_VALUE_LABEL);
    string_edit = CreateChild(
        *this, WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
        IDC_INS_STRING);
    number_edit = CreateChild(*this, WS_EX_CLIENTEDGE, L"EDIT", L"",
                              WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL, IDC_INS_NUMBER);
    value_apply = CreateChild(*this, 0, L"BUTTON", L"应用", WS_VISIBLE | WS_TABSTOP,
                              IDC_INS_VALUE_APPLY);
    boolean_check = CreateChild(*this, 0, L"BUTTON", L"True（真）",
                                WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX, IDC_INS_BOOLEAN);
    info = CreateChild(*this, 0, L"STATIC", L"", WS_VISIBLE | SS_LEFT, IDC_INS_INFO);
    validation = CreateChild(*this, 0, L"STATIC", L"", WS_VISIBLE | SS_LEFT,
                             IDC_INS_VALIDATION);
    add_child = CreateChild(*this, 0, L"BUTTON", L"添加子项 ▾",
                            WS_VISIBLE | WS_TABSTOP, IDC_INS_ADD);
    raw_button = CreateChild(*this, 0, L"BUTTON", L"编辑此节点的原始 JSON…",
                             WS_VISIBLE | WS_TABSTOP, IDC_INS_RAW);

    status = CreateChild(*this, 0, L"STATIC", L"有效 JSON 字典",
                         WS_VISIBLE | SS_SUNKEN | SS_LEFT, IDC_STATUS);
    format_combo = CreateChild(*this, 0, WC_COMBOBOXW, L"",
                               WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                               IDC_FORMAT);
    for (const wchar_t* label : {L"2 个空格", L"4 个空格", L"制表符", L"紧凑"}) {
        SendMessageW(format_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
    }
    trailing_checkbox = CreateChild(*this, 0, L"BUTTON", L"末尾换行",
                                    WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                                    IDC_TRAILING_NEWLINE);

    SendMessageW(key_edit, EM_SETLIMITTEXT,
                 static_cast<WPARAM>(kMaximumKeyCharacters), 0);
    SendMessageW(string_edit, EM_SETLIMITTEXT,
                 static_cast<WPARAM>(kMaximumInspectorCharacters), 0);
    SendMessageW(number_edit, EM_SETLIMITTEXT,
                 static_cast<WPARAM>(kMaximumInspectorCharacters), 0);

    CreateFonts();
    ApplyTheme();
    Layout();
    RebuildTree();
    UpdateInspector();
    UpdateStatusAndTitle();
    DragAcceptFiles(window, TRUE);
}

void AppState::CreateFonts() {
    HFONT new_ui = CreateFontW(-MulDiv(9, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL,
                               FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                               CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT new_title = CreateFontW(-MulDiv(14, static_cast<int>(dpi), 72), 0, 0, 0, FW_SEMIBOLD,
                                  FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
    HFONT new_mono = CreateFontW(-MulDiv(9, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL,
                                 FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                 CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
    if (!new_ui || !new_title || !new_mono) {
        if (new_ui) DeleteObject(new_ui);
        if (new_title) DeleteObject(new_title);
        if (new_mono) DeleteObject(new_mono);
        throw Win32Error("创建 DPI 字体失败", GetLastError());
    }
    HFONT old_ui = ui_font;
    HFONT old_title = title_font;
    HFONT old_mono = mono_font;
    ui_font = new_ui;
    title_font = new_title;
    mono_font = new_mono;
    ApplyFonts();
    if (old_ui) DeleteObject(old_ui);
    if (old_title) DeleteObject(old_title);
    if (old_mono) DeleteObject(old_mono);
}

void AppState::ApplyFonts() {
    for (HWND control : controls) SendMessageW(control, WM_SETFONT,
                                                reinterpret_cast<WPARAM>(ui_font), TRUE);
    SendMessageW(title, WM_SETFONT, reinterpret_cast<WPARAM>(title_font), TRUE);
    SendMessageW(path_edit, WM_SETFONT, reinterpret_cast<WPARAM>(mono_font), TRUE);
    SendMessageW(number_edit, WM_SETFONT, reinterpret_cast<WPARAM>(mono_font), TRUE);
}

void AppState::ApplyTheme() {
    high_contrast = IsHighContrast();
    background_color = GetSysColor(COLOR_BTNFACE);
    edit_color = GetSysColor(COLOR_WINDOW);
    text_color = GetSysColor(COLOR_WINDOWTEXT);
    if (background_brush) DeleteObject(background_brush);
    if (edit_brush) DeleteObject(edit_brush);
    background_brush = CreateSolidBrush(background_color);
    edit_brush = CreateSolidBrush(edit_color);

    SetWindowTheme(tree, L"Explorer", nullptr);
    for (HWND edit : {search, path_edit, key_edit, string_edit, number_edit}) {
        SetWindowTheme(edit, L"Explorer", nullptr);
    }
    InvalidateRect(window, nullptr, TRUE);
    for (HWND control : controls) InvalidateRect(control, nullptr, TRUE);
}

void AppState::Layout() {
    if (!window) return;
    RECT client{};
    GetClientRect(window, &client);
    const int width = client.right;
    const int height = client.bottom;
    const int margin = Scale(8);
    const int toolbar_height = Scale(42);
    const int footer_height = Scale(34);
    const int footer_y = std::max(0, height - footer_height);
    const int splitter_width = Scale(10);
    int left_width = Scale(split_logical);
    left_width = std::clamp(left_width, Scale(350),
                           std::max(Scale(350), width - Scale(320) - splitter_width));
    const int right_x = left_width + splitter_width;
    const int right_width = std::max(0, width - right_x);

    int x = margin;
    const int button_y = Scale(7);
    const int button_h = Scale(28);
    auto place_button = [&](HWND button, int logical_width) {
        const int button_w = Scale(logical_width);
        MoveWindow(button, x, button_y, button_w, button_h, TRUE);
        x += button_w + Scale(6);
    };
    place_button(tool_add, 76);
    place_button(tool_duplicate, 60);
    place_button(tool_delete, 60);
    x += Scale(6);
    place_button(tool_up, 60);
    place_button(tool_down, 60);
    x += Scale(6);
    place_button(tool_sort, 60);
    place_button(tool_raw, 92);

    const int search_y = toolbar_height + margin;
    MoveWindow(search, margin, search_y, left_width - margin * 2, Scale(30), TRUE);
    MoveWindow(tree_header, margin + Scale(4), search_y + Scale(35),
               left_width - margin * 2, Scale(23), TRUE);
    const int tree_y = search_y + Scale(58);
    MoveWindow(tree, margin, tree_y, left_width - margin * 2,
               std::max(0, height - footer_height - tree_y - margin), TRUE);
    MoveWindow(splitter, left_width, toolbar_height, splitter_width,
               std::max(0, height - footer_height - toolbar_height), TRUE);

    const int rx = right_x + Scale(18);
    const int rw = std::max(0, right_width - Scale(36));
    int y = toolbar_height + Scale(12);
    MoveWindow(title, rx, y, rw, Scale(30), TRUE);
    y += Scale(38);
    MoveWindow(path_label, rx, y, rw, Scale(18), TRUE);
    y += Scale(20);
    MoveWindow(path_edit, rx, y, rw, Scale(26), TRUE);
    y += Scale(36);
    MoveWindow(key_label, rx, y, rw, Scale(18), TRUE);
    y += Scale(20);
    MoveWindow(key_edit, rx, y, std::max(0, rw - Scale(72)), Scale(27), TRUE);
    MoveWindow(key_apply, rx + std::max(0, rw - Scale(66)), y, Scale(66), Scale(27), TRUE);
    y += Scale(39);
    MoveWindow(type_label, rx, y, rw, Scale(18), TRUE);
    y += Scale(20);
    MoveWindow(type_combo, rx, y, rw, Scale(300), TRUE);
    y += Scale(39);
    MoveWindow(value_label, rx, y, rw, Scale(18), TRUE);
    y += Scale(20);
    const int value_area_height = std::clamp(
        footer_y - y - Scale(100), Scale(80), Scale(150));
    MoveWindow(string_edit, rx, y, rw, value_area_height, TRUE);
    MoveWindow(number_edit, rx, y, std::max(0, rw - Scale(72)), Scale(27), TRUE);
    MoveWindow(value_apply, rx + std::max(0, rw - Scale(66)), y, Scale(66), Scale(27), TRUE);
    MoveWindow(boolean_check, rx, y, rw, Scale(27), TRUE);
    MoveWindow(info, rx, y, rw, std::min(value_area_height, Scale(58)), TRUE);
    y += value_area_height + Scale(10);
    MoveWindow(validation, rx, y, rw, Scale(42), TRUE);
    y += Scale(48);
    MoveWindow(add_child, rx, y, Scale(110), Scale(30), TRUE);
    MoveWindow(raw_button, rx + Scale(120), y, std::max(0, rw - Scale(120)), Scale(30), TRUE);

    const int format_width = Scale(120);
    const int newline_width = Scale(116);
    MoveWindow(status, 0, footer_y, std::max(0, width - format_width - newline_width),
               footer_height, TRUE);
    MoveWindow(format_combo, std::max(0, width - format_width - newline_width),
               footer_y + Scale(4), format_width, Scale(200), TRUE);
    MoveWindow(trailing_checkbox, std::max(0, width - newline_width), footer_y + Scale(5),
               newline_width, Scale(25), TRUE);
}

jsondict::NodeId TreeItemNodeId(HWND tree, HTREEITEM item) {
    if (!item) return 0;
    TVITEMW value{};
    value.mask = TVIF_PARAM;
    value.hItem = item;
    if (!TreeView_GetItem(tree, &value)) return 0;
    return static_cast<jsondict::NodeId>(static_cast<UINT_PTR>(value.lParam));
}

HTREEITEM FindTreeItemById(HWND tree, HTREEITEM item, jsondict::NodeId id) {
    while (item) {
        if (TreeItemNodeId(tree, item) == id) return item;
        if (HTREEITEM child = TreeView_GetChild(tree, item)) {
            if (HTREEITEM found = FindTreeItemById(tree, child, id)) return found;
        }
        item = TreeView_GetNextSibling(tree, item);
    }
    return nullptr;
}

HTREEITEM InsertTreeItem(HWND tree, HTREEITEM parent, std::wstring text,
                         jsondict::NodeId id) {
    TVINSERTSTRUCTW insertion{};
    insertion.hParent = parent;
    insertion.hInsertAfter = TVI_LAST;
    insertion.item.mask = TVIF_TEXT | TVIF_PARAM;
    insertion.item.pszText = text.data();
    insertion.item.lParam = static_cast<LPARAM>(static_cast<UINT_PTR>(id));
    HTREEITEM item = TreeView_InsertItem(tree, &insertion);
    if (!item) throw std::runtime_error("无法创建树节点；系统内存或控件资源不足");
    return item;
}

void AppState::CaptureExpanded() {
    if (search_mode || !tree) return;
    expanded_ids.clear();
    std::function<void(HTREEITEM)> visit = [&](HTREEITEM item) {
        while (item) {
            if ((TreeView_GetItemState(tree, item, TVIS_EXPANDED) & TVIS_EXPANDED) != 0) {
                const auto id = TreeItemNodeId(tree, item);
                if (id != 0) expanded_ids.insert(id);
            }
            if (HTREEITEM child = TreeView_GetChild(tree, item)) visit(child);
            item = TreeView_GetNextSibling(tree, item);
        }
    };
    visit(TreeView_GetRoot(tree));
}

void AppState::RebuildTree() {
    if (!tree) return;
    BoolFlagGuard rebuilding(rebuilding_tree);
    const bool inspector_refresh_required = !tree_operational;
    KillTimer(window, kSearchTimerId);
    search_refresh_pending = false;
    tree_refresh_needed = true;
    bool tree_was_cleared = false;
    try {
    std::wstring query = ToLowerInvariant(GetControlText(search));
    query.erase(query.begin(), std::find_if(query.begin(), query.end(), [](wchar_t value) {
        return value != L' ' && value != L'\t' && value != L'\r' && value != L'\n';
    }));
    while (!query.empty() && (query.back() == L' ' || query.back() == L'\t' ||
                              query.back() == L'\r' || query.back() == L'\n')) {
        query.pop_back();
    }
    const bool new_search_mode = !query.empty();
    search_mode = new_search_mode;

    WindowRedrawGuard redraw(tree);
    TreeView_DeleteAllItems(tree);
    tree_was_cleared = true;
    tree_operational = false;

    auto display_text = [](std::wstring_view name, const jsondict::Node& node) {
        return std::wstring(name) + L"    · " + KindTitle(node.kind()) + L"    · " +
               NodeSummary(node);
    };

    if (!search_mode) {
        std::function<void(HTREEITEM, const jsondict::Node&)> add_children;
        add_children = [&](HTREEITEM parent, const jsondict::Node& node) {
            if (node.kind() == jsondict::Kind::Object) {
                for (const auto& member : node.as_object()) {
                    const std::wstring name = EscapeForUi(Utf8Preview(member.key, 384), 120);
                    HTREEITEM item = InsertTreeItem(tree, parent,
                                                   display_text(name, member.value),
                                                   member.value.id());
                    add_children(item, member.value);
                    if (expanded_ids.count(member.value.id())) TreeView_Expand(tree, item, TVE_EXPAND);
                }
            } else if (node.kind() == jsondict::Kind::Array) {
                for (std::size_t index = 0; index < node.as_array().size(); ++index) {
                    const auto& child = node.as_array()[index];
                    const std::wstring name = L"[" + std::to_wstring(index) + L"]";
                    HTREEITEM item = InsertTreeItem(tree, parent,
                                                   display_text(name, child), child.id());
                    add_children(item, child);
                    if (expanded_ids.count(child.id())) TreeView_Expand(tree, item, TVE_EXPAND);
                }
            }
        };

        HTREEITEM root = InsertTreeItem(tree, TVI_ROOT,
            display_text(L"根对象", document.root()), document.root_id());
        add_children(root, document.root());
        TreeView_Expand(tree, root, TVE_EXPAND);
        expanded_ids.insert(document.root_id());
    } else {
        struct Match {
            jsondict::NodeId id;
            std::wstring display;
            std::wstring path;
        };
        std::vector<Match> matches;
        std::function<void(const jsondict::Node&, std::wstring, std::string)> collect;
        collect = [&](const jsondict::Node& node, std::wstring name, std::string path_utf8) {
            const std::wstring path = Utf8ToWide(path_utf8);
            if (node.id() != document.root_id() &&
                (ContainsInsensitive(name, query) || ContainsInsensitive(path, query) ||
                 ContainsInsensitive(KindTitle(node.kind()), query) ||
                 ContainsInsensitive(NodeSummary(node), query))) {
                matches.push_back(Match{node.id(), display_text(name, node), path});
            }
            if (node.kind() == jsondict::Kind::Object) {
                for (const auto& member : node.as_object()) {
                    collect(member.value, EscapeForUi(Utf8Preview(member.key, 384), 120),
                            SearchPathForKey(path_utf8, member.key));
                }
            } else if (node.kind() == jsondict::Kind::Array) {
                for (std::size_t index = 0; index < node.as_array().size(); ++index) {
                    collect(node.as_array()[index], L"[" + std::to_wstring(index) + L"]",
                            path_utf8 + "[" + std::to_string(index) + "]");
                }
            }
        };
        collect(document.root(), L"根对象", "$");
        HTREEITEM root = InsertTreeItem(tree, TVI_ROOT,
            L"搜索结果（" + std::to_wstring(matches.size()) + L"）", document.root_id());
        for (const auto& match : matches) {
            InsertTreeItem(tree, root, match.display + L"    " + match.path, match.id);
        }
        TreeView_Expand(tree, root, TVE_EXPAND);
    }

    HTREEITEM selected_item = FindTreeItemById(tree, TreeView_GetRoot(tree), selected_id);
    bool selection_changed = false;
    if (!selected_item) {
        selected_item = TreeView_GetRoot(tree);
        if (document.root_id() != selected_id) {
            selected_id = document.root_id();
            selection_changed = true;
        }
    }
    if (selected_item) {
        if (!TreeView_SelectItem(tree, selected_item) ||
            TreeView_GetSelection(tree) != selected_item) {
            throw std::runtime_error("树视图无法同步当前选中节点");
        }
        TreeView_EnsureVisible(tree, selected_item);
    } else {
        throw std::runtime_error("树视图没有可选中的根节点");
    }
    tree_operational = true;
    if (selection_changed || inspector_refresh_required) UpdateInspector();
    UpdateCommandStates();
    tree_refresh_needed = false;
    } catch (...) {
        if (tree_was_cleared) {
            tree_operational = false;
            selected_id = document.root_id();
            search_mode = false;
            TreeView_DeleteAllItems(tree);
            try {
                HTREEITEM root = InsertTreeItem(
                    tree, TVI_ROOT, L"树视图刷新失败（请重试搜索或重新打开）",
                    document.root_id());
                TreeView_SelectItem(tree, root);
            } catch (...) {
            }
            try {
                UpdateInspector();
            } catch (...) {
                loading_inspector = false;
            }
            try {
                UpdateCommandStates();
            } catch (...) {
            }
        }
        throw;
    }
}

void AppState::UpdateInspector() {
    try {
    const jsondict::Node* node = document.find(selected_id);
    if (!node) {
        selected_id = document.root_id();
        node = &document.root();
    }
    const auto location = document.location(selected_id);
    {
    BoolFlagGuard loading(loading_inspector);

    std::wstring node_name = L"根对象";
    if (location && location->key) {
        node_name = EscapeForUi(Utf8Preview(*location->key, 384), 120);
    } else if (location && location->index) {
        node_name = L"[" + std::to_wstring(*location->index) + L"]";
    }
    SetControlText(title, node_name + L"  ·  " + KindTitle(node->kind()));
    SetControlText(path_edit, location ? Utf8ToWide(location->path) : L"$");

    const bool is_root = selected_id == document.root_id();
    const bool has_key = location && location->key.has_value();
    SetVisible(key_label, has_key);
    SetVisible(key_edit, has_key);
    SetVisible(key_apply, has_key);
    if (has_key) {
        const bool obviously_too_large =
            location->key->size() > kMaximumKeyCharacters * 4ull;
        const std::wstring wide_key = obviously_too_large
            ? std::wstring{} : Utf8ToWide(*location->key);
        const bool unsafe_key = obviously_too_large ||
            wide_key.size() > kMaximumKeyCharacters ||
            std::any_of(wide_key.begin(), wide_key.end(), [](wchar_t unit) {
                return unit < 0x20;
            });
        if (unsafe_key) {
            SetControlText(key_edit, L"此键名包含控制字符或过长，请使用原始 JSON 编辑器。");
            EnableWindow(key_edit, FALSE);
            EnableWindow(key_apply, FALSE);
        } else {
            SetControlText(key_edit, wide_key);
            EnableWindow(key_edit, TRUE);
        }
    }

    SendMessageW(type_combo, CB_SETCURSEL, static_cast<WPARAM>(node->kind()), 0);
    EnableWindow(type_combo, !is_root);

    SetVisible(string_edit, false);
    SetVisible(number_edit, false);
    SetVisible(value_apply, false);
    SetVisible(boolean_check, false);
    SetVisible(info, false);
    SetVisible(add_child, false);
    SetVisible(value_label, true);
    EnableWindow(string_edit, TRUE);
    EnableWindow(value_apply, TRUE);

    switch (node->kind()) {
        case jsondict::Kind::String: {
            SetControlText(value_label, L"字符串值");
            const std::wstring value = Utf8ToWide(node->as_string());
            SetVisible(string_edit, true);
            SetVisible(value_apply, true);
            const bool unsafe_value = value.size() > kMaximumInspectorCharacters ||
                std::any_of(value.begin(), value.end(), [](wchar_t unit) {
                    return unit < 0x20 && unit != L'\n';
                });
            if (unsafe_value) {
                SetControlText(string_edit,
                    L"此字符串包含回车/控制字符或过长，请使用原始 JSON 编辑器。");
                EnableWindow(string_edit, FALSE);
                EnableWindow(value_apply, FALSE);
            } else {
                SetControlText(string_edit, ToEditNewlines(value));
            }
            break;
        }
        case jsondict::Kind::Number: {
            SetControlText(value_label, L"数字值");
            SetVisible(number_edit, true);
            SetVisible(value_apply, true);
            const std::wstring value = Utf8ToWide(node->as_number().text);
            if (value.size() > kMaximumInspectorCharacters) {
                SetControlText(number_edit, L"数字文本过长，请使用原始 JSON 编辑器。");
                EnableWindow(number_edit, FALSE);
                EnableWindow(value_apply, FALSE);
            } else {
                EnableWindow(number_edit, TRUE);
                SetControlText(number_edit, value);
            }
            break;
        }
        case jsondict::Kind::Boolean:
            SetControlText(value_label, L"布尔值");
            SendMessageW(boolean_check, BM_SETCHECK,
                         node->as_boolean() ? BST_CHECKED : BST_UNCHECKED, 0);
            SetControlText(boolean_check, node->as_boolean() ? L"True（真）" : L"False（假）");
            SetVisible(boolean_check, true);
            break;
        case jsondict::Kind::Null:
            SetControlText(value_label, L"Null");
            SetControlText(info, L"此值为空（null）。Null 与空字符串、数字 0 和 false 不相同。");
            SetVisible(info, true);
            break;
        case jsondict::Kind::Object:
            SetControlText(value_label, L"对象");
            SetControlText(info, node->child_count() == 0 ? L"空对象" :
                L"包含 " + std::to_wstring(node->child_count()) + L" 个唯一键。");
            SetVisible(info, true);
            SetVisible(add_child, true);
            break;
        case jsondict::Kind::Array:
            SetControlText(value_label, L"数组");
            SetControlText(info, node->child_count() == 0 ? L"空数组" :
                L"包含 " + std::to_wstring(node->child_count()) + L" 个有序元素。");
            SetVisible(info, true);
            SetVisible(add_child, true);
            break;
    }

    SetVisible(raw_button, true);
    SendMessageW(format_combo, CB_SETCURSEL,
                 static_cast<WPARAM>(document.formatting()), 0);
    SendMessageW(trailing_checkbox, BM_SETCHECK,
                 document.trailing_newline() ? BST_CHECKED : BST_UNCHECKED, 0);
    SetControlText(validation, L"");
    inspector_draft_dirty = false;
    }
    UpdateDraftValidation();
    UpdateCommandStates();
    } catch (...) {
        loading_inspector = false;
        inspector_draft_dirty = false;
        tree_operational = false;
        try {
            UpdateCommandStates();
        } catch (...) {
        }
        try {
            UpdateStatusAndTitle();
        } catch (...) {
        }
        throw;
    }
}

void AppState::UpdateDraftValidation() {
    if (loading_inspector) return;
    const jsondict::Node* node = document.find(selected_id);
    if (!node) return;
    std::wstring warning;
    bool pending = false;

    const auto location = document.location(selected_id);
    if (location && location->key && IsWindowVisible(key_edit) && IsWindowEnabled(key_edit)) {
        try {
            const std::string draft = WideToUtf8(GetControlText(key_edit));
            const bool available = document.is_key_available(draft, selected_id);
            const bool changed = draft != *location->key;
            pending = pending || changed;
            EnableWindow(key_apply, available && changed);
            if (!available) warning = L"同一对象中已经存在此键名。";
            else if (draft.empty()) warning = L"空字符串是合法 JSON 键，但通常不便维护。";
        } catch (const std::exception&) {
            pending = true;
            EnableWindow(key_apply, FALSE);
            warning = L"键名包含无效的 Unicode 文本。";
        }
    }

    if (node->kind() == jsondict::Kind::Number && IsWindowVisible(number_edit)) {
        try {
            const std::string draft = WideToUtf8(GetControlText(number_edit));
            const bool valid = jsondict::is_valid_number(draft);
            const bool changed = draft != node->as_number().text;
            pending = pending || changed;
            EnableWindow(value_apply, valid && changed);
            if (!valid) warning = L"请输入有效 JSON 数字；不支持前导零、NaN 或 Infinity。";
        } catch (const std::exception&) {
            pending = true;
            EnableWindow(value_apply, FALSE);
            warning = L"数字只能包含 JSON 数字语法中的 ASCII 字符。";
        }
    } else if (node->kind() == jsondict::Kind::String && IsWindowVisible(string_edit) &&
               IsWindowEnabled(string_edit)) {
        try {
            const std::string draft = WideToUtf8(NormalizeEditNewlines(GetControlText(string_edit)));
            const bool changed = draft != node->as_string();
            pending = pending || changed;
            EnableWindow(value_apply, changed);
        } catch (const std::exception&) {
            pending = true;
            EnableWindow(value_apply, FALSE);
            warning = L"字符串包含无效的 Unicode 文本。";
        }
    }
    if (pending && warning.empty()) {
        warning = L"草稿尚未应用；点击“应用”后才会写入文档。";
    }
    const bool pending_changed = inspector_draft_dirty != pending;
    inspector_draft_dirty = pending;
    SetControlText(validation, warning);
    InvalidateRect(validation, nullptr, TRUE);
    if (pending_changed) UpdateStatusAndTitle();
}

void AppState::UpdateStatusAndTitle() {
    const std::size_t top_level = document.root().child_count();
    std::wstring status_text = L"  ✓ 有效 JSON 字典    " + std::to_wstring(top_level) +
        L" 个顶层键 · " + std::to_wstring(document.node_count()) + L" 个节点";
    if (inspector_draft_dirty) status_text += L"    ·    有未应用草稿";
    SetControlText(status, status_text);
    std::wstring filename = current_path.empty() ? L"未命名.json" : current_path;
    const std::size_t separator = filename.find_last_of(L"\\/");
    if (separator != std::wstring::npos) filename.erase(0, separator + 1);
    const bool unsaved = dirty || inspector_draft_dirty;
    SetWindowTextW(window, (filename + (unsaved ? L" * — " : L" — ") +
                            kAppTitle).c_str());
    if (unsaved) {
        ShutdownBlockReasonCreate(window, L"JSON 字典包含未保存或未应用的更改。");
    } else {
        ShutdownBlockReasonDestroy(window);
    }
}

void AppState::UpdateCommandStates() {
    const jsondict::Node* node = document.find(selected_id);
    const auto location = document.location(selected_id);
    const bool non_root = tree_operational && selected_id != document.root_id();
    const bool can_up = tree_operational && document.can_move(selected_id, -1);
    const bool can_down = tree_operational && document.can_move(selected_id, 1);
    const bool can_sort = tree_operational && node && node->kind() == jsondict::Kind::Object;
    EnableWindow(tool_add, tree_operational);
    EnableWindow(tool_duplicate, non_root);
    EnableWindow(tool_delete, non_root);
    EnableWindow(tool_up, can_up);
    EnableWindow(tool_down, can_down);
    EnableWindow(tool_sort, can_sort);

    HMENU menu = GetMenu(window);
    auto enable = [&](UINT id, bool value) {
        EnableMenuItem(menu, id, MF_BYCOMMAND | (value ? MF_ENABLED : MF_GRAYED));
    };
    enable(ID_NODE_ADD_STRING, tree_operational);
    enable(ID_NODE_ADD_NUMBER, tree_operational);
    enable(ID_NODE_ADD_BOOLEAN, tree_operational);
    enable(ID_NODE_ADD_NULL, tree_operational);
    enable(ID_NODE_ADD_OBJECT, tree_operational);
    enable(ID_NODE_ADD_ARRAY, tree_operational);
    enable(ID_NODE_DUPLICATE, non_root);
    enable(ID_NODE_DELETE, non_root);
    enable(ID_NODE_MOVE_UP, can_up);
    enable(ID_NODE_MOVE_DOWN, can_down);
    enable(ID_NODE_SORT, can_sort);
    enable(ID_NODE_RAW, tree_operational);
    enable(ID_NODE_RENAME, tree_operational && location.has_value() && location->key.has_value());
    EnableWindow(add_child, tree_operational);
    EnableWindow(raw_button, tree_operational);
    if (!tree_operational) {
        EnableWindow(key_edit, FALSE);
        EnableWindow(key_apply, FALSE);
        EnableWindow(type_combo, FALSE);
        EnableWindow(string_edit, FALSE);
        EnableWindow(number_edit, FALSE);
        EnableWindow(value_apply, FALSE);
        EnableWindow(boolean_check, FALSE);
    }
    DrawMenuBar(window);
}

void AppState::MarkDirty() {
    dirty = true;
    try {
        UpdateStatusAndTitle();
    } catch (const std::exception& error) {
        MessageBoxW(window,
                    (L"更改已应用，但窗口状态刷新失败。请尽快保存。\n\n" +
                     ExceptionMessage(error)).c_str(),
                    L"更改已应用", MB_ICONWARNING | MB_OK);
    }
}

void AppState::FinishCommittedMutation() {
    MarkDirty();
    tree_refresh_needed = true;
    try {
        RebuildTree();
        UpdateInspector();
    } catch (const std::exception& error) {
        tree_refresh_needed = true;
        MessageBoxW(window,
                    (L"更改已应用到文档，但界面刷新失败。请立即保存并重新打开程序。\n\n" +
                     ExceptionMessage(error)).c_str(),
                    L"数据已应用，界面刷新失败", MB_ICONWARNING | MB_OK);
    }
}

void AppState::SelectNode(jsondict::NodeId id) {
    if (!document.find(id)) id = document.root_id();
    selected_id = id;
    if (HTREEITEM item = FindTreeItemById(tree, TreeView_GetRoot(tree), id)) {
        TreeView_SelectItem(tree, item);
        TreeView_EnsureVisible(tree, item);
    }
    UpdateInspector();
}

void AppState::ShowAddMenu(HWND anchor) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, ID_NODE_ADD_STRING, L"字符串");
    AppendMenuW(menu, MF_STRING, ID_NODE_ADD_NUMBER, L"数字");
    AppendMenuW(menu, MF_STRING, ID_NODE_ADD_BOOLEAN, L"布尔值");
    AppendMenuW(menu, MF_STRING, ID_NODE_ADD_NULL, L"Null");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, ID_NODE_ADD_OBJECT, L"对象");
    AppendMenuW(menu, MF_STRING, ID_NODE_ADD_ARRAY, L"数组");
    POINT point{};
    if (anchor) {
        RECT rect{};
        GetWindowRect(anchor, &rect);
        point = POINT{rect.left, rect.bottom};
    } else {
        GetCursorPos(&point);
    }
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                        point.x, point.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (command) SendMessageW(window, WM_COMMAND, command, 0);
}

void AppState::AddNode(jsondict::Kind kind) {
    if (!ResolveInspectorDrafts(false)) return;
    if (document.node_count() >= kMaximumDocumentNodes) {
        MessageBoxW(window, L"文档已达到 50000 个节点的图形编辑安全上限。",
                    L"无法添加节点", MB_ICONWARNING | MB_OK);
        return;
    }
    const jsondict::Node* selected = document.find(selected_id);
    jsondict::NodeId container_id = document.root_id();
    if (selected && selected->is_container()) {
        container_id = selected_id;
    } else if (const auto where = document.location(selected_id); where && where->parent_id) {
        container_id = *where->parent_id;
    }
    jsondict::Document candidate = document;
    const auto created = candidate.add_child(selected_id);
    if (!created) return;
    if (kind != jsondict::Kind::String && !candidate.change_kind(*created, kind)) return;
    if (!EnsureDocumentWithinUiLimits(candidate)) return;
    document = std::move(candidate);
    expanded_ids.insert(container_id);
    selected_id = *created;
    FinishCommittedMutation();
}

void AppState::DuplicateSelected() {
    if (!ResolveInspectorDrafts(false)) return;
    const jsondict::Node* source = document.find(selected_id);
    if (!source) return;
    if (document.node_count() + CountNodes(*source) > kMaximumDocumentNodes) {
        MessageBoxW(window, L"复制后会超过 50000 个节点的图形编辑安全上限。",
                    L"无法复制节点", MB_ICONWARNING | MB_OK);
        return;
    }
    jsondict::Document candidate = document;
    const auto duplicate = candidate.duplicate_node(selected_id);
    if (!duplicate) return;
    if (!EnsureDocumentWithinUiLimits(candidate)) return;
    document = std::move(candidate);
    if (const auto where = document.location(*duplicate); where && where->parent_id) {
        expanded_ids.insert(*where->parent_id);
    }
    selected_id = *duplicate;
    FinishCommittedMutation();
}

void AppState::DeleteSelected() {
    if (!ResolveInspectorDrafts(false)) return;
    if (selected_id == document.root_id()) return;
    const jsondict::Node* node = document.find(selected_id);
    if (!node) return;
    if (node->is_container() && node->child_count() > 0) {
        if (MessageBoxW(window,
                        L"所选容器包含子项。删除后，其中的所有内容也会被删除。\n\n确定继续吗？",
                        L"删除所选容器？", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
            return;
        }
    }
    const auto where = document.location(selected_id);
    const jsondict::NodeId next = where && where->parent_id ? *where->parent_id : document.root_id();
    if (!document.delete_node(selected_id)) return;
    expanded_ids.erase(selected_id);
    selected_id = next;
    FinishCommittedMutation();
}

void AppState::MoveSelected(int offset) {
    if (!ResolveInspectorDrafts(false)) return;
    if (!document.move_node(selected_id, offset)) return;
    FinishCommittedMutation();
}

void AppState::SortSelected() {
    if (!ResolveInspectorDrafts(false)) return;
    const jsondict::Node* node = document.find(selected_id);
    if (!node || node->kind() != jsondict::Kind::Object) return;
    std::vector<std::string> before;
    before.reserve(node->as_object().size());
    for (const auto& member : node->as_object()) before.push_back(member.key);
    if (!document.sort_object(selected_id)) return;
    const jsondict::Node* sorted = document.find(selected_id);
    bool changed = !sorted || sorted->kind() != jsondict::Kind::Object ||
                   sorted->as_object().size() != before.size();
    if (!changed) {
        for (std::size_t index = 0; index < before.size(); ++index) {
            if (before[index] != sorted->as_object()[index].key) {
                changed = true;
                break;
            }
        }
    }
    if (changed) FinishCommittedMutation();
}

void AppState::ChangeSelectedKind() {
    if (loading_inspector || selected_id == document.root_id()) return;
    const jsondict::Node* node = document.find(selected_id);
    if (!node) return;
    const LRESULT selected = SendMessageW(type_combo, CB_GETCURSEL, 0, 0);
    if (selected == CB_ERR || selected < 0 || selected > 5) return;
    const auto requested = static_cast<jsondict::Kind>(selected);
    if (requested == node->kind()) return;
    const jsondict::Kind original_kind = node->kind();
    if (!ResolveInspectorDrafts(false)) {
        BoolFlagGuard loading(loading_inspector);
        SendMessageW(type_combo, CB_SETCURSEL, static_cast<WPARAM>(original_kind), 0);
        return;
    }
    node = document.find(selected_id);
    if (!node) return;
    if (node->is_container() && node->child_count() > 0) {
        if (MessageBoxW(window,
                        L"更改类型会移除当前对象或数组中的全部子项。\n\n确定继续吗？",
                        L"更改类型会移除子项", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
            UpdateInspector();
            if (tree_refresh_needed) PostMessageW(window, kRebuildTreeMessage, 0, 0);
            return;
        }
    }
    jsondict::Document candidate = document;
    if (!candidate.change_kind(selected_id, requested) ||
        !EnsureDocumentWithinUiLimits(candidate)) {
        UpdateInspector();
        if (tree_refresh_needed) PostMessageW(window, kRebuildTreeMessage, 0, 0);
        return;
    }
    document = std::move(candidate);
    FinishCommittedMutation();
}

void AppState::ApplyKeyDraft() {
    (void)CommitInspectorDrafts(true);
}

void AppState::ApplyValueDraft() {
    (void)CommitInspectorDrafts(true);
}

bool AppState::CommitInspectorDrafts(bool refresh) {
    const jsondict::Node* node = document.find(selected_id);
    if (!node) return false;
    const auto location = document.location(selected_id);
    std::optional<std::string> key_draft;
    std::optional<std::string> string_draft;
    std::optional<std::string> number_draft;

    try {
        if (location && location->key && IsWindowVisible(key_edit) &&
            IsWindowEnabled(key_edit)) {
            const std::string value = WideToUtf8(GetControlText(key_edit));
            if (value != *location->key) {
                if (!document.is_key_available(value, selected_id)) {
                    throw std::runtime_error("同一对象中已经存在此键名");
                }
                key_draft = value;
            }
        }
        if (node->kind() == jsondict::Kind::String && IsWindowEnabled(string_edit)) {
            const std::string value = WideToUtf8(NormalizeEditNewlines(GetControlText(string_edit)));
            if (value != node->as_string()) string_draft = value;
        } else if (node->kind() == jsondict::Kind::Number) {
            const std::string value = WideToUtf8(GetControlText(number_edit));
            if (!jsondict::is_valid_number(value)) {
                throw std::runtime_error("请输入有效 JSON 数字；不支持前导零、NaN 或 Infinity");
            }
            if (value != node->as_number().text) number_draft = value;
        }
    } catch (const std::exception& error) {
        MessageBoxW(window, ExceptionMessage(error).c_str(), L"无法应用检查器草稿",
                    MB_ICONERROR | MB_OK);
        UpdateDraftValidation();
        return false;
    }

    if (!key_draft && !string_draft && !number_draft) {
        inspector_draft_dirty = false;
        UpdateStatusAndTitle();
        if (refresh) UpdateInspector();
        return true;
    }

    jsondict::Document candidate = document;
    bool committed = true;
    if (key_draft) committed = candidate.rename_node(selected_id, *key_draft);
    if (committed && string_draft) committed = candidate.set_string(selected_id, *string_draft);
    if (committed && number_draft) committed = candidate.set_number(selected_id, *number_draft);
    if (!committed) {
        MessageBoxW(window, L"文档在应用草稿前发生变化；草稿没有提交。",
                    L"无法应用检查器草稿", MB_ICONERROR | MB_OK);
        return false;
    }
    if (!EnsureDocumentWithinUiLimits(candidate)) return false;
    document = std::move(candidate);

    inspector_draft_dirty = false;
    tree_refresh_needed = true;
    MarkDirty();
    if (refresh) {
        try {
            RebuildTree();
            tree_refresh_needed = false;
            UpdateInspector();
        } catch (const std::exception& error) {
            MessageBoxW(window,
                        (L"草稿已经应用到文档，但界面刷新失败。请立即保存并重新打开程序。\n\n" +
                         ExceptionMessage(error)).c_str(),
                        L"数据已应用，界面刷新失败", MB_ICONWARNING | MB_OK);
        }
    }
    return true;
}

bool AppState::ResolveInspectorDrafts(bool refresh) {
    if (!inspector_draft_dirty) return true;
    BoolFlagGuard resolving(resolving_inspector_drafts);
    const int choice = MessageBoxW(
        window,
        L"检查器中有尚未应用的键名或值。\n\n"
        L"选择“是”应用草稿，选择“否”放弃草稿，选择“取消”返回编辑。",
        L"处理尚未应用的草稿", MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON1);
    if (choice == IDCANCEL || choice == 0) return false;
    if (choice == IDYES) {
        const bool committed = CommitInspectorDrafts(refresh);
        if (committed && !refresh && tree_refresh_needed) {
            PostMessageW(window, kRebuildTreeMessage, 0, 0);
        }
        return committed;
    }

    inspector_draft_dirty = false;
    UpdateStatusAndTitle();
    UpdateInspector();
    return true;
}

void AppState::ToggleBoolean() {
    if (loading_inspector) return;
    const jsondict::Node* node = document.find(selected_id);
    if (!node || node->kind() != jsondict::Kind::Boolean) return;
    const bool original = node->as_boolean();
    const bool requested = SendMessageW(boolean_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
    if (!ResolveInspectorDrafts(false)) {
        SendMessageW(boolean_check, BM_SETCHECK, original ? BST_CHECKED : BST_UNCHECKED, 0);
        return;
    }
    node = document.find(selected_id);
    if (!node || node->kind() != jsondict::Kind::Boolean) return;
    if (requested == node->as_boolean()) return;
    jsondict::Document candidate = document;
    if (!candidate.set_boolean(selected_id, requested) ||
        !EnsureDocumentWithinUiLimits(candidate)) {
        BoolFlagGuard loading(loading_inspector);
        SendMessageW(boolean_check, BM_SETCHECK,
                     node->as_boolean() ? BST_CHECKED : BST_UNCHECKED, 0);
        return;
    }
    document = std::move(candidate);
    FinishCommittedMutation();
}

void AppState::ExpandAll(bool expand) {
    std::function<void(HTREEITEM)> visit = [&](HTREEITEM item) {
        while (item) {
            TreeView_Expand(tree, item, expand ? TVE_EXPAND : TVE_COLLAPSE);
            if (HTREEITEM child = TreeView_GetChild(tree, item)) visit(child);
            item = TreeView_GetNextSibling(tree, item);
        }
    };
    visit(TreeView_GetRoot(tree));
    if (!expand) TreeView_Expand(tree, TreeView_GetRoot(tree), TVE_EXPAND);
    CaptureExpanded();
}

bool AppState::ConfirmSaveIfDirty() {
    if (!ResolveInspectorDrafts(true)) return false;
    if (!dirty) return true;
    const int choice = MessageBoxW(
        window, L"当前 JSON 字典包含尚未保存的更改。\n\n是否现在保存？",
        L"保存更改？", MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON1);
    if (choice == IDCANCEL || choice == 0) return false;
    if (choice == IDNO) return true;
    return SaveDocument();
}

void AppState::NewDocument() {
    if (!ConfirmSaveIfDirty()) return;
    document = jsondict::Document();
    selected_id = document.root_id();
    current_path.clear();
    saved_target.reset();
    had_utf8_bom = false;
    dirty = false;
    expanded_ids = {document.root_id()};
    loading_inspector = true;
    SetControlText(search, L"");
    loading_inspector = false;
    search_mode = false;
    RebuildTree();
    UpdateInspector();
    UpdateStatusAndTitle();
}

std::optional<std::wstring> ShowOpenDialog(HWND owner) {
    std::vector<wchar_t> buffer(32768, L'\0');
    const wchar_t filter[] = L"JSON 文件 (*.json)\0*.json\0所有文件 (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrDefExt = L"json";
    dialog.Flags = OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST |
                   OFN_NOCHANGEDIR | OFN_HIDEREADONLY;
    if (!GetOpenFileNameW(&dialog)) {
        const DWORD error = CommDlgExtendedError();
        if (error != 0) {
            throw std::runtime_error("打开文件对话框失败，错误码 " + std::to_string(error));
        }
        return std::nullopt;
    }
    return std::wstring(buffer.data());
}

std::optional<std::wstring> ShowSaveDialog(HWND owner, std::wstring initial_path) {
    std::vector<wchar_t> buffer(32768, L'\0');
    if (!initial_path.empty() && initial_path.size() + 1 < buffer.size()) {
        std::copy(initial_path.begin(), initial_path.end(), buffer.begin());
    }
    const wchar_t filter[] = L"JSON 文件 (*.json)\0*.json\0所有文件 (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = buffer.data();
    dialog.nMaxFile = static_cast<DWORD>(buffer.size());
    dialog.lpstrDefExt = L"json";
    dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR |
                   OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&dialog)) {
        const DWORD error = CommDlgExtendedError();
        if (error != 0) {
            throw std::runtime_error("另存为对话框失败，错误码 " + std::to_string(error));
        }
        return std::nullopt;
    }
    return std::wstring(buffer.data());
}

void AppState::OpenDocument() {
    if (!ConfirmSaveIfDirty()) return;
    try {
        const auto path = ShowOpenDialog(window);
        if (path) OpenPath(*path, false);
    } catch (const std::exception& error) {
        MessageBoxW(window, ExceptionMessage(error).c_str(), L"无法显示打开对话框",
                    MB_ICONERROR | MB_OK);
    }
}

bool AppState::EnsureDocumentWithinUiLimits(const jsondict::Document& candidate,
                                            HWND message_owner) {
    const HWND owner = message_owner ? message_owner : window;
    const std::size_t count = candidate.node_count();
    if (count > kMaximumDocumentNodes) {
        MessageBoxW(owner,
                    (L"此 JSON 包含 " + std::to_wstring(count) +
                     L" 个节点，超过 50000 个节点的图形编辑安全上限。").c_str(),
                    L"文档过大", MB_ICONWARNING | MB_OK);
        return false;
    }
    const std::string encoded = candidate.encoded_text();
    const std::uint64_t final_size = static_cast<std::uint64_t>(encoded.size()) +
        (candidate.trailing_newline() ? 1u : 0u) + 3u;
    if (final_size > kMaximumFileBytes) {
        MessageBoxW(owner,
                    L"按当前格式写出的 UTF-8 文件会超过 16 MiB。请减少内容或改用紧凑格式。",
                    L"文档过大", MB_ICONWARNING | MB_OK);
        return false;
    }
    return true;
}

bool AppState::OpenPath(const std::wstring& path, bool ask_about_dirty) {
    if (ask_about_dirty && !ConfirmSaveIfDirty()) return false;
    try {
        const FileContents file = ReadUtf8File(path);
        jsondict::Document loaded = jsondict::Document::from_json(file.utf8);
        if (!EnsureDocumentWithinUiLimits(loaded)) return false;
        document = std::move(loaded);
        selected_id = document.root_id();
        current_path = path;
        saved_target = ExpectedTarget::Match(file.identity);
        had_utf8_bom = file.had_utf8_bom;
        dirty = false;
        expanded_ids = {document.root_id()};
        loading_inspector = true;
        SetControlText(search, L"");
        loading_inspector = false;
        search_mode = false;
        RebuildTree();
        UpdateInspector();
        UpdateStatusAndTitle();
        SHAddToRecentDocs(SHARD_PATHW, current_path.c_str());
        return true;
    } catch (const std::exception& error) {
        MessageBoxW(window, ExceptionMessage(error).c_str(), L"无法打开 JSON 字典",
                    MB_ICONERROR | MB_OK);
        return false;
    }
}

bool AppState::SaveDocument() {
    if (!ResolveInspectorDrafts(true)) return false;
    if (current_path.empty()) return SaveDocumentAs();
    try {
        const auto current_identity = TryReadFileIdentity(current_path);
        ExpectedTarget expected = current_identity
            ? ExpectedTarget::Match(*current_identity) : ExpectedTarget::Absent();
        const bool baseline_known = saved_target.has_value();
        const bool unchanged = baseline_known &&
            MatchesExpectedTarget(current_identity, *saved_target);
        if (!unchanged) {
            const wchar_t* message = baseline_known
                ? L"磁盘上的文件自打开或上次保存后已被其他程序修改、替换或删除。\n\n"
                  L"选择“是”覆盖当前磁盘版本；选择“否”改为另存为；选择“取消”停止保存。"
                : L"程序无法确认当前路径是否仍与上次保存后一致。\n\n"
                  L"选择“是”以此刻的磁盘文件为基准覆盖；选择“否”改为另存为；选择“取消”停止保存。";
            const int choice = MessageBoxW(
                window, message,
                baseline_known ? L"检测到外部文件更改" : L"无法确认保存基准",
                MB_ICONWARNING | MB_YESNOCANCEL | MB_DEFBUTTON3);
            if (choice == IDCANCEL || choice == 0) return false;
            if (choice == IDNO) return SaveDocumentAs();
        }
        std::string bytes = jsondict::write(
            document.root(), jsondict::WriteOptions{
                document.formatting(), document.trailing_newline(), true});
        if (had_utf8_bom) bytes.insert(0, "\xEF\xBB\xBF", 3);
        if (bytes.size() > kMaximumFileBytes) {
            throw std::runtime_error(
                "格式化后的 UTF-8 文件超过 16 MiB；请减少内容或改用紧凑格式");
        }
        const Sha256Digest written_digest = ComputeSha256(bytes);
        WriteFileAtomically(current_path, bytes, expected);
        try {
            ExpectedTarget verified = SnapshotExpectedTarget(current_path);
            if (verified.mode != ExpectedTargetMode::MustMatch ||
                verified.identity.digest != written_digest) {
                throw std::runtime_error(
                    "目标文件在原子替换后立即被删除或改写");
            }
            saved_target = std::move(verified);
        } catch (const std::exception& error) {
            saved_target.reset();
            MarkDirty();
            MessageBoxW(
                window,
                (L"程序已完成原子写入，但随后无法确认磁盘内容仍与当前文档一致。"
                 L"当前文档仍保持“未保存”状态，程序不会继续关闭或新建。\n\n" +
                 ExceptionMessage(error)).c_str(),
                L"保存后验证失败", MB_ICONWARNING | MB_OK);
            return false;
        }
        dirty = false;
        UpdateStatusAndTitle();
        SHAddToRecentDocs(SHARD_PATHW, current_path.c_str());
        return true;
    } catch (const std::exception& error) {
        MessageBoxW(window, ExceptionMessage(error).c_str(), L"保存失败",
                    MB_ICONERROR | MB_OK);
        return false;
    }
}

bool AppState::SaveDocumentAs() {
    if (!ResolveInspectorDrafts(true)) return false;
    std::wstring initial = current_path.empty() ? L"未命名.json" : current_path;
    std::optional<std::wstring> path;
    try {
        path = ShowSaveDialog(window, std::move(initial));
    } catch (const std::exception& error) {
        MessageBoxW(window, ExceptionMessage(error).c_str(), L"无法显示另存为对话框",
                    MB_ICONERROR | MB_OK);
        return false;
    }
    if (!path) return false;
    const std::wstring old_path = current_path;
    const auto old_target = saved_target;
    current_path = *path;
    try {
        saved_target = SnapshotExpectedTarget(current_path);
    } catch (const std::exception& error) {
        current_path = old_path;
        saved_target = old_target;
        MessageBoxW(window, ExceptionMessage(error).c_str(), L"无法检查另存为目标",
                    MB_ICONERROR | MB_OK);
        return false;
    }
    if (SaveDocument()) return true;
    current_path = old_path;
    saved_target = old_target;
    UpdateStatusAndTitle();
    return false;
}

void AppState::ShowTreeContextMenu(POINT screen_point) {
    const jsondict::Node* node = document.find(selected_id);
    if (!node) return;
    HMENU menu = CreatePopupMenu();
    HMENU add = CreatePopupMenu();
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_STRING, L"字符串");
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_NUMBER, L"数字");
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_BOOLEAN, L"布尔值");
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_NULL, L"Null");
    AppendMenuW(add, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_OBJECT, L"对象");
    AppendMenuW(add, MF_STRING, ID_NODE_ADD_ARRAY, L"数组");
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(add),
                node->is_container() ? L"添加子项" : L"添加同级项");
    AppendMenuW(menu, MF_STRING, ID_NODE_RAW, L"编辑原始 JSON…");
    if (node->kind() == jsondict::Kind::Object) {
        AppendMenuW(menu, MF_STRING, ID_NODE_SORT, L"按键名排序");
    }
    if (selected_id != document.root_id()) {
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, ID_NODE_DUPLICATE, L"复制");
        AppendMenuW(menu, MF_STRING, ID_NODE_DELETE, L"删除");
    }
    const UINT command = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_TOPALIGN,
                                        screen_point.x, screen_point.y, 0, window, nullptr);
    DestroyMenu(menu);
    if (command) SendMessageW(window, WM_COMMAND, command, 0);
}

LRESULT AppState::HandleTreeNotification(NMHDR* header) {
    if (header->code == TVN_SELCHANGINGW && !rebuilding_tree) {
        const auto* change = reinterpret_cast<const NMTREEVIEWW*>(header);
        const auto next_id = static_cast<jsondict::NodeId>(
            static_cast<UINT_PTR>(change->itemNew.lParam));
        if (next_id != 0 && next_id != selected_id && !ResolveInspectorDrafts(false)) {
            return TRUE;
        }
    } else if (header->code == TVN_SELCHANGEDW && !rebuilding_tree) {
        const auto* change = reinterpret_cast<const NMTREEVIEWW*>(header);
        const auto id = static_cast<jsondict::NodeId>(
            static_cast<UINT_PTR>(change->itemNew.lParam));
        if (id != 0 && document.find(id)) {
            selected_id = id;
            UpdateInspector();
            if (tree_refresh_needed) PostMessageW(window, kRebuildTreeMessage, 0, 0);
        }
    } else if (header->code == TVN_ITEMEXPANDEDW && !search_mode) {
        const auto* change = reinterpret_cast<const NMTREEVIEWW*>(header);
        const auto id = static_cast<jsondict::NodeId>(
            static_cast<UINT_PTR>(change->itemNew.lParam));
        if (change->action == TVE_EXPAND) expanded_ids.insert(id);
        else if (change->action == TVE_COLLAPSE) expanded_ids.erase(id);
    } else if (header->code == NM_RCLICK) {
        POINT client_point{};
        GetCursorPos(&client_point);
        ScreenToClient(tree, &client_point);
        TVHITTESTINFO hit{};
        hit.pt = client_point;
        if (TreeView_HitTest(tree, &hit) && hit.hItem) {
            TreeView_SelectItem(tree, hit.hItem);
        }
    }
    return 0;
}

void AppState::HandleCommand(int id, int notification, HWND source) {
    const bool requires_operational_tree =
        (id >= ID_NODE_ADD_MENU && id <= ID_NODE_RENAME) ||
        id == IDC_INS_KEY_APPLY || id == IDC_INS_TYPE ||
        id == IDC_INS_VALUE_APPLY || id == IDC_INS_BOOLEAN ||
        id == IDC_INS_ADD || id == IDC_INS_RAW;
    if (!tree_operational && requires_operational_tree) {
        MessageBoxW(window,
                    L"树视图尚未完整刷新，为避免操作到不可见节点，节点修改已暂时停用。"
                    L"请在搜索框中重试搜索，或重新打开文件。",
                    L"树视图不可用", MB_ICONWARNING | MB_OK);
        return;
    }
    if (source == tool_add && notification == BN_CLICKED) {
        ShowAddMenu(tool_add);
        return;
    }
    if (id == IDC_SEARCH && notification == EN_SETFOCUS) {
        if (!loading_inspector && inspector_draft_dirty &&
            !ResolveInspectorDrafts(true)) {
            SetFocus(tree);
        } else if (search_refresh_pending && GetFocus() == search) {
            PostMessageW(window, kSearchRefreshMessage, 0, 0);
        }
        return;
    }
    if (id == IDC_SEARCH && notification == EN_CHANGE) {
        if (!loading_inspector) {
            search_refresh_pending = true;
            KillTimer(window, kSearchTimerId);
            SetTimer(window, kSearchTimerId, 250, nullptr);
        }
        return;
    }
    if ((id == IDC_INS_KEY || id == IDC_INS_NUMBER || id == IDC_INS_STRING) &&
        notification == EN_CHANGE) {
        UpdateDraftValidation();
        return;
    }
    if (id == IDC_INS_TYPE && notification == CBN_SELCHANGE) {
        ChangeSelectedKind();
        return;
    }
    if (id == IDC_FORMAT && notification == CBN_SELCHANGE && !loading_inspector) {
        const LRESULT value = SendMessageW(format_combo, CB_GETCURSEL, 0, 0);
        if (value != CB_ERR && value >= 0 && value <= 3) {
            const auto requested = static_cast<jsondict::Formatting>(value);
            if (requested != document.formatting()) {
                jsondict::Document candidate = document;
                candidate.set_formatting(requested);
                if (EnsureDocumentWithinUiLimits(candidate)) {
                    document = std::move(candidate);
                    MarkDirty();
                } else {
                    BoolFlagGuard loading(loading_inspector);
                    SendMessageW(format_combo, CB_SETCURSEL,
                                 static_cast<WPARAM>(document.formatting()), 0);
                }
            }
        }
        return;
    }
    if (id == IDC_TRAILING_NEWLINE && notification == BN_CLICKED && !loading_inspector) {
        const bool value = SendMessageW(trailing_checkbox, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (value != document.trailing_newline()) {
            jsondict::Document candidate = document;
            candidate.set_trailing_newline(value);
            if (EnsureDocumentWithinUiLimits(candidate)) {
                document = std::move(candidate);
                MarkDirty();
            } else {
                BoolFlagGuard loading(loading_inspector);
                SendMessageW(trailing_checkbox, BM_SETCHECK,
                             document.trailing_newline() ? BST_CHECKED : BST_UNCHECKED, 0);
            }
        }
        return;
    }

    switch (id) {
        case ID_FILE_NEW: NewDocument(); break;
        case ID_FILE_OPEN: OpenDocument(); break;
        case ID_FILE_SAVE: SaveDocument(); break;
        case ID_FILE_SAVE_AS: SaveDocumentAs(); break;
        case ID_FILE_EXIT: SendMessageW(window, WM_CLOSE, 0, 0); break;
        case ID_EDIT_FIND:
            SetFocus(search);
            SendMessageW(search, EM_SETSEL, 0, -1);
            break;
        case ID_NODE_ADD_MENU: ShowAddMenu(tool_add); break;
        case ID_NODE_ADD_STRING: AddNode(jsondict::Kind::String); break;
        case ID_NODE_ADD_NUMBER: AddNode(jsondict::Kind::Number); break;
        case ID_NODE_ADD_BOOLEAN: AddNode(jsondict::Kind::Boolean); break;
        case ID_NODE_ADD_NULL: AddNode(jsondict::Kind::Null); break;
        case ID_NODE_ADD_OBJECT: AddNode(jsondict::Kind::Object); break;
        case ID_NODE_ADD_ARRAY: AddNode(jsondict::Kind::Array); break;
        case ID_NODE_DUPLICATE: DuplicateSelected(); break;
        case ID_NODE_DELETE: DeleteSelected(); break;
        case ID_NODE_MOVE_UP: MoveSelected(-1); break;
        case ID_NODE_MOVE_DOWN: MoveSelected(1); break;
        case ID_NODE_SORT: SortSelected(); break;
        case ID_NODE_RAW:
        case IDC_INS_RAW:
            if (ResolveInspectorDrafts(true)) ShowRawEditor(*this, selected_id);
            break;
        case ID_NODE_RENAME:
            if (IsWindowVisible(key_edit) && IsWindowEnabled(key_edit)) {
                SetFocus(key_edit);
                SendMessageW(key_edit, EM_SETSEL, 0, -1);
            }
            break;
        case ID_VIEW_EXPAND_ALL: ExpandAll(true); break;
        case ID_VIEW_COLLAPSE_ALL: ExpandAll(false); break;
        case IDC_INS_KEY_APPLY: ApplyKeyDraft(); break;
        case IDC_INS_VALUE_APPLY: ApplyValueDraft(); break;
        case IDC_INS_BOOLEAN: ToggleBoolean(); break;
        case IDC_INS_ADD: ShowAddMenu(add_child); break;
        case ID_HELP_ABOUT:
            MessageBoxW(window,
                L"JSON 字典编辑器 Windows 版\n\n"
                L"原生 Win32 图形化编辑器。文件仅在本机以 UTF-8 处理，不上传网络。",
                L"关于", MB_ICONINFORMATION | MB_OK);
            break;
        default: break;
    }
}

void AppState::DrawSplitter(const DRAWITEMSTRUCT& item) const {
    const RECT& bounds = item.rcItem;
    FillRect(item.hDC, &bounds, GetSysColorBrush(COLOR_BTNFACE));

    const int width = bounds.right - bounds.left;
    const int height = bounds.bottom - bounds.top;
    const int inset = Scale(8);
    if (width <= 0 || height <= inset * 2) return;

    const bool active = GetFocus() == item.hwndItem || splitter_dragging;
    const int color = high_contrast ? COLOR_WINDOWTEXT
                                   : (active ? COLOR_HIGHLIGHT : COLOR_3DSHADOW);
    const HBRUSH brush = GetSysColorBrush(color);
    const int line_width = std::min(width, std::max(1, Scale(2)));
    const LONG line_x = bounds.left + (width - line_width) / 2;
    RECT line{line_x, bounds.top + inset,
              line_x + line_width, bounds.bottom - inset};
    FillRect(item.hDC, &line, brush);

    const int grip_width = std::min(width, std::max(1, Scale(4)));
    const int grip_height = std::min(height - inset * 2, Scale(32));
    const LONG grip_x = bounds.left + (width - grip_width) / 2;
    const LONG grip_y = bounds.top + (height - grip_height) / 2;
    RECT grip{grip_x, grip_y, grip_x + grip_width, grip_y + grip_height};
    FillRect(item.hDC, &grip, brush);
    if (active) DrawFocusRect(item.hDC, &bounds);
}

LRESULT CALLBACK AppState::SplitterProc(HWND hwnd, UINT message, WPARAM w_param,
                                        LPARAM l_param, UINT_PTR, DWORD_PTR data) {
    auto* app = reinterpret_cast<AppState*>(data);
    if (!app) return DefSubclassProc(hwnd, message, w_param, l_param);
    switch (message) {
        case WM_LBUTTONDOWN:
            app->splitter_dragging = true;
            SetFocus(hwnd);
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_MOUSEMOVE:
            if (app->splitter_dragging && (w_param & MK_LBUTTON)) {
                POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
                ClientToScreen(hwnd, &point);
                ScreenToClient(app->window, &point);
                app->split_logical = MulDiv(point.x, 96, static_cast<int>(app->dpi));
                app->Layout();
            }
            return 0;
        case WM_LBUTTONUP:
        case WM_CANCELMODE:
            app->splitter_dragging = false;
            if (GetCapture() == hwnd) ReleaseCapture();
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_CAPTURECHANGED:
            app->splitter_dragging = false;
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        case WM_SETFOCUS:
        case WM_KILLFOCUS:
            InvalidateRect(hwnd, nullptr, FALSE);
            break;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_SIZEWE));
            return TRUE;
        case WM_GETDLGCODE:
            return DLGC_WANTARROWS;
        case WM_KEYDOWN:
            if (w_param == VK_LEFT || w_param == VK_RIGHT) {
                app->split_logical += w_param == VK_LEFT ? -24 : 24;
                app->Layout();
                return 0;
            }
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(hwnd, SplitterProc, 1);
            break;
        default: break;
    }
    return DefSubclassProc(hwnd, message, w_param, l_param);
}

LRESULT CALLBACK AppState::WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
    AppState* app = reinterpret_cast<AppState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        app = reinterpret_cast<AppState*>(create->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(app));
        if (app) app->window = hwnd;
    }
    if (!app) return DefWindowProcW(hwnd, message, w_param, l_param);

    switch (message) {
        case WM_CREATE:
            try {
                app->CreateControls();
            } catch (const std::exception& error) {
                MessageBoxW(hwnd, ExceptionMessage(error).c_str(), L"无法创建应用界面",
                            MB_ICONERROR | MB_OK);
                return -1;
            }
            return 0;
        case WM_DRAWITEM: {
            const auto* item = reinterpret_cast<const DRAWITEMSTRUCT*>(l_param);
            if (item && item->CtlType == ODT_STATIC && item->CtlID == IDC_SPLITTER) {
                app->DrawSplitter(*item);
                return TRUE;
            }
            break;
        }
        case WM_COMMAND:
            try {
                app->HandleCommand(LOWORD(w_param), HIWORD(w_param),
                                   reinterpret_cast<HWND>(l_param));
            } catch (const std::exception& error) {
                MessageBoxW(hwnd, ExceptionMessage(error).c_str(), L"操作失败",
                            MB_ICONERROR | MB_OK);
            }
            return 0;
        case WM_NOTIFY: {
            auto* header = reinterpret_cast<NMHDR*>(l_param);
            if (header && header->hwndFrom == app->tree) {
                try {
                    return app->HandleTreeNotification(header);
                } catch (const std::exception& error) {
                    MessageBoxW(hwnd, ExceptionMessage(error).c_str(), L"树操作失败",
                                MB_ICONERROR | MB_OK);
                    return 0;
                }
            }
            return 0;
        }
        case WM_TIMER:
            if (w_param == kSearchTimerId) {
                KillTimer(hwnd, kSearchTimerId);
                if (app->resolving_inspector_drafts || app->inspector_draft_dirty ||
                    GetFocus() != app->search) {
                    app->search_refresh_pending = true;
                    return 0;
                }
                try {
                    app->RebuildTree();
                } catch (const std::exception& error) {
                    app->search_refresh_pending = true;
                    MessageBoxW(hwnd, ExceptionMessage(error).c_str(), L"搜索失败",
                                MB_ICONERROR | MB_OK);
                }
                return 0;
            }
            break;
        case kSearchRefreshMessage:
            if (!app->search_refresh_pending) return 0;
            if (app->resolving_inspector_drafts || app->inspector_draft_dirty ||
                GetFocus() != app->search) {
                return 0;
            }
            try {
                app->RebuildTree();
            } catch (const std::exception& error) {
                app->search_refresh_pending = true;
                MessageBoxW(hwnd, ExceptionMessage(error).c_str(), L"搜索失败",
                            MB_ICONERROR | MB_OK);
            }
            return 0;
        case kRebuildTreeMessage:
            if (app->tree_refresh_needed) {
                app->tree_refresh_needed = false;
                try {
                    app->RebuildTree();
                } catch (const std::exception& error) {
                    app->tree_refresh_needed = true;
                    MessageBoxW(hwnd, ExceptionMessage(error).c_str(), L"树刷新失败",
                                MB_ICONWARNING | MB_OK);
                }
            }
            return 0;
        case WM_CONTEXTMENU:
            if (reinterpret_cast<HWND>(w_param) == app->tree) {
                POINT point{GET_X_LPARAM(l_param), GET_Y_LPARAM(l_param)};
                if (point.x == -1 && point.y == -1) {
                    RECT rect{};
                    HTREEITEM selected = TreeView_GetSelection(app->tree);
                    if (selected && TreeView_GetItemRect(app->tree, selected, &rect, TRUE)) {
                        point = POINT{rect.left, rect.bottom};
                        ClientToScreen(app->tree, &point);
                    } else {
                        GetCursorPos(&point);
                    }
                }
                app->ShowTreeContextMenu(point);
                return 0;
            }
            break;
        case WM_SIZE:
            app->Layout();
            return 0;
        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(l_param);
            info->ptMinTrackSize.x = app->Scale(820);
            info->ptMinTrackSize.y = app->Scale(540);
            return 0;
        }
        case WM_DPICHANGED: {
            app->dpi = HIWORD(w_param);
            const auto* suggested = reinterpret_cast<const RECT*>(l_param);
            SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                         suggested->right - suggested->left, suggested->bottom - suggested->top,
                         SWP_NOACTIVATE | SWP_NOZORDER);
            try {
                app->CreateFonts();
                app->Layout();
            } catch (const std::exception& error) {
                MessageBoxW(hwnd, ExceptionMessage(error).c_str(), L"DPI 布局更新失败",
                            MB_ICONWARNING | MB_OK);
            }
            return 0;
        }
        case WM_SETTINGCHANGE:
        case WM_THEMECHANGED:
        case WM_SYSCOLORCHANGE:
            app->ApplyTheme();
            return 0;
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC dc = reinterpret_cast<HDC>(w_param);
            HWND control = reinterpret_cast<HWND>(l_param);
            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, control == app->validation &&
                             !app->high_contrast && !GetControlText(app->validation).empty()
                             ? RGB(205, 45, 45) : app->text_color);
            return reinterpret_cast<LRESULT>(app->background_brush);
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORLISTBOX: {
            HDC dc = reinterpret_cast<HDC>(w_param);
            SetBkColor(dc, app->edit_color);
            SetTextColor(dc, app->text_color);
            return reinterpret_cast<LRESULT>(app->edit_brush);
        }
        case WM_ERASEBKGND: {
            RECT client{};
            GetClientRect(hwnd, &client);
            FillRect(reinterpret_cast<HDC>(w_param), &client, app->background_brush);
            return TRUE;
        }
        case WM_DROPFILES: {
            HDROP drop = reinterpret_cast<HDROP>(w_param);
            const UINT count = DragQueryFileW(drop, 0xFFFFFFFFu, nullptr, 0);
            if (count != 1) {
                DragFinish(drop);
                MessageBoxW(hwnd, L"请一次只拖入一个 JSON 文件。", L"无法打开多个文件",
                            MB_ICONINFORMATION | MB_OK);
                return 0;
            }
            const UINT length = DragQueryFileW(drop, 0, nullptr, 0);
            std::wstring path(static_cast<std::size_t>(length) + 1, L'\0');
            DragQueryFileW(drop, 0, path.data(), length + 1);
            path.resize(length);
            DragFinish(drop);
            app->OpenPath(path, true);
            return 0;
        }
        case WM_QUERYENDSESSION:
            return (app->dirty || app->inspector_draft_dirty) ? FALSE : TRUE;
        case WM_CLOSE:
            if (app->ConfirmSaveIfDirty()) DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, kSearchTimerId);
            ShutdownBlockReasonDestroy(hwnd);
            DragAcceptFiles(hwnd, FALSE);
            if (app->ui_font) DeleteObject(app->ui_font);
            if (app->title_font) DeleteObject(app->title_font);
            if (app->mono_font) DeleteObject(app->mono_font);
            if (app->background_brush) DeleteObject(app->background_brush);
            if (app->edit_brush) DeleteObject(app->edit_brush);
            app->ui_font = app->title_font = app->mono_font = nullptr;
            app->background_brush = app->edit_brush = nullptr;
            PostQuitMessage(0);
            return 0;
        default: break;
    }
    return DefWindowProcW(hwnd, message, w_param, l_param);
}

bool IsTextControl(HWND control) {
    if (!control) return false;
    wchar_t class_name[64]{};
    GetClassNameW(control, class_name, static_cast<int>(std::size(class_name)));
    return _wcsicmp(class_name, L"Edit") == 0 ||
           _wcsicmp(class_name, L"RICHEDIT50W") == 0;
}

int AppState::Run(std::optional<std::wstring> initial_path) {
    ShowWindow(window, SW_SHOWDEFAULT);
    UpdateWindow(window);
    if (initial_path) OpenPath(*initial_path, false);

    std::array<ACCEL, 14> entries{{
        {FVIRTKEY | FCONTROL, 'N', ID_FILE_NEW},
        {FVIRTKEY | FCONTROL, 'O', ID_FILE_OPEN},
        {FVIRTKEY | FCONTROL, 'S', ID_FILE_SAVE},
        {FVIRTKEY | FCONTROL | FSHIFT, 'S', ID_FILE_SAVE_AS},
        {FVIRTKEY | FCONTROL, 'F', ID_EDIT_FIND},
        {FVIRTKEY, VK_INSERT, ID_NODE_ADD_MENU},
        {FVIRTKEY | FCONTROL, 'D', ID_NODE_DUPLICATE},
        {FVIRTKEY, VK_DELETE, ID_NODE_DELETE},
        {FVIRTKEY | FALT, VK_UP, ID_NODE_MOVE_UP},
        {FVIRTKEY | FALT, VK_DOWN, ID_NODE_MOVE_DOWN},
        {FVIRTKEY | FCONTROL, 'E', ID_NODE_RAW},
        {FVIRTKEY, VK_F2, ID_NODE_RENAME},
        {FVIRTKEY | FCONTROL, 'W', ID_FILE_EXIT},
        {FVIRTKEY, VK_F5, ID_VIEW_EXPAND_ALL},
    }};
    HACCEL accelerators = CreateAcceleratorTableW(entries.data(), static_cast<int>(entries.size()));
    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        HWND focus = GetFocus();
        if (message.message == WM_KEYDOWN && message.wParam == VK_RETURN) {
            if (focus == key_edit) {
                ApplyKeyDraft();
                continue;
            }
            if (focus == number_edit ||
                (focus == string_edit && (GetKeyState(VK_CONTROL) & 0x8000) != 0)) {
                ApplyValueDraft();
                continue;
            }
        }
        if (message.message == WM_KEYDOWN && message.wParam == VK_ESCAPE &&
            focus == search && !GetControlText(search).empty()) {
            SetControlText(search, L"");
            continue;
        }
        const bool protected_text_key =
            IsTextControl(focus) && message.message == WM_KEYDOWN &&
            (message.wParam == VK_DELETE || message.wParam == VK_BACK);
        if (!protected_text_key && accelerators &&
            TranslateAcceleratorW(window, accelerators, &message)) {
            continue;
        }
        if (IsDialogMessageW(window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (accelerators) DestroyAcceleratorTable(accelerators);
    return static_cast<int>(message.wParam);
}

struct RawEditorState {
    AppState* app = nullptr;
    jsondict::NodeId target_id = 0;
    HWND window = nullptr;
    HWND edit = nullptr;
    HWND status = nullptr;
    HWND check = nullptr;
    HWND format = nullptr;
    HWND apply = nullptr;
    HWND cancel = nullptr;
    HFONT ui_font = nullptr;
    HFONT mono_font = nullptr;
    UINT dpi = 96;
    bool loading = false;
    bool draft_dirty = false;
    std::wstring recovery_text;

    int Scale(int value) const { return MulDiv(value, static_cast<int>(dpi), 96); }
    bool is_root() const { return app && target_id == app->document.root_id(); }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param) {
        auto* state = reinterpret_cast<RawEditorState*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
            state = reinterpret_cast<RawEditorState*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
            if (state) state->window = hwnd;
        }
        if (!state) return DefWindowProcW(hwnd, message, w_param, l_param);
        switch (message) {
            case WM_CREATE:
                try {
                    state->CreateControls();
                } catch (const std::exception& error) {
                    MessageBoxW(hwnd, ExceptionMessage(error).c_str(),
                                L"无法创建原始 JSON 编辑器", MB_ICONERROR | MB_OK);
                    return -1;
                }
                return 0;
            case WM_SIZE:
                state->Layout();
                return 0;
            case WM_GETMINMAXINFO: {
                auto* info = reinterpret_cast<MINMAXINFO*>(l_param);
                info->ptMinTrackSize.x = state->Scale(660);
                info->ptMinTrackSize.y = state->Scale(460);
                return 0;
            }
            case WM_DPICHANGED: {
                state->dpi = HIWORD(w_param);
                const auto* suggested = reinterpret_cast<const RECT*>(l_param);
                SetWindowPos(hwnd, nullptr, suggested->left, suggested->top,
                             suggested->right - suggested->left,
                             suggested->bottom - suggested->top,
                             SWP_NOACTIVATE | SWP_NOZORDER);
                try {
                    state->CreateFonts();
                    state->Layout();
                } catch (const std::exception& error) {
                    MessageBoxW(hwnd, ExceptionMessage(error).c_str(),
                                L"DPI 布局更新失败", MB_ICONWARNING | MB_OK);
                }
                return 0;
            }
            case WM_COMMAND:
                try {
                    if (LOWORD(w_param) == IDC_RAW_EDIT && HIWORD(w_param) == EN_CHANGE) {
                        if (!state->loading) {
                            state->draft_dirty = true;
                            SetControlText(state->status, L"内容已更改，尚未应用");
                            ShutdownBlockReasonCreate(hwnd, L"原始 JSON 编辑器包含未应用的草稿。");
                        }
                        return 0;
                    }
                    switch (LOWORD(w_param)) {
                        case IDC_RAW_CHECK: state->CheckDraft(); return 0;
                        case IDC_RAW_FORMAT: state->FormatDraft(); return 0;
                        case IDC_RAW_APPLY: state->ApplyDraft(); return 0;
                        case IDC_RAW_CANCEL: SendMessageW(hwnd, WM_CLOSE, 0, 0); return 0;
                        default: break;
                    }
                } catch (const std::exception& error) {
                    state->loading = false;
                    SetControlText(state->status, L"操作失败：" + ExceptionMessage(error));
                    MessageBoxW(hwnd, ExceptionMessage(error).c_str(), L"原始 JSON 操作失败",
                                MB_ICONERROR | MB_OK);
                }
                return 0;
            case WM_CLOSE:
                if (state->draft_dirty &&
                    MessageBoxW(hwnd,
                                L"原始 JSON 草稿尚未应用。关闭窗口会放弃这些更改。\n\n确定关闭吗？",
                                L"放弃原始 JSON 草稿？",
                                MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) != IDYES) {
                    return 0;
                }
                DestroyWindow(hwnd);
                return 0;
            case WM_QUERYENDSESSION:
                return state->draft_dirty ? FALSE : TRUE;
            case WM_DESTROY:
                ShutdownBlockReasonDestroy(hwnd);
                if (state->ui_font) DeleteObject(state->ui_font);
                if (state->mono_font) DeleteObject(state->mono_font);
                state->ui_font = state->mono_font = nullptr;
                state->window = nullptr;
                return 0;
            default: break;
        }
        return DefWindowProcW(hwnd, message, w_param, l_param);
    }

    void CreateControls() {
        dpi = GetDpiForWindow(window);
        if (dpi == 0) dpi = 96;
        edit = CreateWindowExW(
            WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE | ES_AUTOVSCROLL |
                ES_AUTOHSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL,
            0, 0, 10, 10, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RAW_EDIT)), app->instance, nullptr);
        status = CreateWindowExW(
            0, L"STATIC", L"尚未检查", WS_CHILD | WS_VISIBLE | SS_LEFT,
            0, 0, 10, 10, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RAW_STATUS)), app->instance, nullptr);
        check = CreateWindowExW(
            0, L"BUTTON", L"检查", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 10, 10, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RAW_CHECK)), app->instance, nullptr);
        format = CreateWindowExW(
            0, L"BUTTON", L"格式化", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 10, 10, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RAW_FORMAT)), app->instance, nullptr);
        apply = CreateWindowExW(
            0, L"BUTTON", L"应用", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
            0, 0, 10, 10, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RAW_APPLY)), app->instance, nullptr);
        cancel = CreateWindowExW(
            0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | WS_TABSTOP,
            0, 0, 10, 10, window,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(IDC_RAW_CANCEL)), app->instance, nullptr);
        if (!edit || !status || !check || !format || !apply || !cancel) {
            throw Win32Error("创建原始 JSON 编辑器失败", GetLastError());
        }
        SendMessageW(edit, EM_SETLIMITTEXT, static_cast<WPARAM>(kMaximumRawCharacters), 0);
        CreateFonts();
        Layout();
        loading = true;
        const std::string initial = app->document.encoded_text(
            target_id, jsondict::Formatting::TwoSpaces);
        if (initial.size() > kMaximumRawUtf8Bytes) {
            throw std::runtime_error(
                "所选 JSON 超过 4 MiB 的原始文本编辑安全上限");
        }
        const std::wstring edit_text = ToEditNewlines(Utf8ToWide(initial));
        if (edit_text.size() > kMaximumRawCharacters ||
            !SetWindowTextW(edit, edit_text.c_str()) ||
            static_cast<std::size_t>(GetWindowTextLengthW(edit)) != edit_text.size()) {
            throw std::runtime_error("原始 JSON 文本无法完整载入编辑控件");
        }
        loading = false;
        draft_dirty = false;
        SetControlText(status, is_root() ? L"正在编辑完整 JSON 字典；根节点必须是对象。" :
                                          L"正在编辑所选节点；应用前会完整验证。" );
    }

    void CreateFonts() {
        HFONT new_ui = CreateFontW(-MulDiv(9, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL,
                                   FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        HFONT new_mono = CreateFontW(-MulDiv(10, static_cast<int>(dpi), 72), 0, 0, 0, FW_NORMAL,
                                     FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                     CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, FIXED_PITCH, L"Consolas");
        if (!new_ui || !new_mono) {
            if (new_ui) DeleteObject(new_ui);
            if (new_mono) DeleteObject(new_mono);
            throw Win32Error("创建原始 JSON 编辑器字体失败", GetLastError());
        }
        HFONT old_ui = ui_font;
        HFONT old_mono = mono_font;
        ui_font = new_ui;
        mono_font = new_mono;
        ApplyFonts();
        if (old_ui) DeleteObject(old_ui);
        if (old_mono) DeleteObject(old_mono);
    }

    void ApplyFonts() {
        for (HWND control : {status, check, format, apply, cancel}) {
            if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(ui_font), TRUE);
        }
        if (edit) SendMessageW(edit, WM_SETFONT, reinterpret_cast<WPARAM>(mono_font), TRUE);
    }

    void Layout() {
        if (!window) return;
        RECT client{};
        GetClientRect(window, &client);
        const int margin = Scale(14);
        const int button_h = Scale(30);
        const int footer_h = Scale(58);
        const int button_w = Scale(78);
        const int client_width = static_cast<int>(client.right);
        const int client_height = static_cast<int>(client.bottom);
        MoveWindow(edit, margin, margin,
                   std::max(0, client_width - margin * 2),
                   std::max(0, client_height - margin * 2 - footer_h), TRUE);
        const int footer_y = std::max(margin, client_height - margin - button_h);
        int right = client_width - margin;
        MoveWindow(apply, right - button_w, footer_y, button_w, button_h, TRUE);
        right -= button_w + Scale(8);
        MoveWindow(cancel, right - button_w, footer_y, button_w, button_h, TRUE);
        right -= button_w + Scale(8);
        MoveWindow(check, right - button_w, footer_y, button_w, button_h, TRUE);
        right -= button_w + Scale(8);
        MoveWindow(format, right - button_w, footer_y, button_w, button_h, TRUE);
        MoveWindow(status, margin, footer_y + Scale(5),
                   std::max(0, right - button_w - margin - Scale(8)), Scale(24), TRUE);
    }

    jsondict::Node ParseDraft() const {
        if (!recovery_text.empty()) {
            throw std::runtime_error(
                "原草稿正在程序内存中等待恢复；请先点击“格式化”恢复原文本");
        }
        const std::wstring wide = NormalizeEditNewlines(GetControlText(edit));
        const std::string utf8 = WideToUtf8(wide);
        if (utf8.size() > kMaximumRawUtf8Bytes) {
            throw std::runtime_error("原始 JSON 草稿超过 4 MiB 的 UTF-8 安全上限");
        }
        jsondict::Node node = jsondict::parse(utf8);
        jsondict::validate(node, is_root());
        return node;
    }

    void CheckDraft() {
        try {
            const jsondict::Node node = ParseDraft();
            SetControlText(status, L"✓ 有效的 " + KindTitle(node.kind()) + L" · " +
                                      std::to_wstring(CountNodes(node)) + L" 个节点");
        } catch (const std::exception& error) {
            SetControlText(status, L"错误：" + ExceptionMessage(error));
        }
    }

    void FormatDraft() {
        try {
            if (!recovery_text.empty()) {
                loading = true;
                const bool restored = SetWindowTextW(edit, recovery_text.c_str()) != FALSE &&
                    static_cast<std::size_t>(GetWindowTextLengthW(edit)) == recovery_text.size();
                loading = false;
                if (!restored) {
                    throw std::runtime_error(
                        "原草稿仍保留在程序内存中，但编辑控件暂时无法恢复；"
                        "请释放系统内存后再次点击“格式化”");
                }
                recovery_text.clear();
                draft_dirty = true;
                ShutdownBlockReasonCreate(window, L"原始 JSON 编辑器包含未应用的草稿。");
                SetControlText(status, L"✓ 原草稿已从内存恢复，尚未应用");
                return;
            }
            const jsondict::Node node = ParseDraft();
            const std::string formatted = jsondict::write(
                node, jsondict::WriteOptions{jsondict::Formatting::TwoSpaces, false, is_root()});
            if (formatted.size() > kMaximumRawUtf8Bytes) {
                throw std::runtime_error(
                    "格式化结果超过 4 MiB 的 UTF-8 原始编辑安全上限");
            }
            const std::wstring replacement = ToEditNewlines(Utf8ToWide(formatted));
            if (replacement.size() > kMaximumRawCharacters) {
                throw std::runtime_error(
                    "格式化结果超过原始编辑器的字符安全上限");
            }
            std::wstring previous = GetControlText(edit);
            loading = true;
            const bool replaced = SetWindowTextW(edit, replacement.c_str()) != FALSE &&
                static_cast<std::size_t>(GetWindowTextLengthW(edit)) == replacement.size();
            if (!replaced) {
                const bool restored = SetWindowTextW(edit, previous.c_str()) != FALSE &&
                    static_cast<std::size_t>(GetWindowTextLengthW(edit)) == previous.size();
                loading = false;
                if (!restored) {
                    recovery_text = std::move(previous);
                    draft_dirty = true;
                    ShutdownBlockReasonCreate(
                        window, L"原始 JSON 编辑器在内存中保留着待恢复草稿。");
                    throw std::runtime_error(
                        "编辑控件无法恢复原草稿；原文本仍保留在程序内存中。"
                        "请释放系统内存后再次点击“格式化”恢复");
                }
                throw std::runtime_error(
                    "编辑控件无法完整装载格式化结果；原草稿已保留");
            }
            loading = false;
            draft_dirty = true;
            ShutdownBlockReasonCreate(window, L"原始 JSON 编辑器包含未应用的草稿。");
            SetControlText(status, L"✓ 已格式化并通过检查");
        } catch (const std::exception& error) {
            loading = false;
            SetControlText(status, L"错误：" + ExceptionMessage(error));
        }
    }

    void ApplyDraft() {
        std::optional<jsondict::Node> parsed;
        try {
            parsed = ParseDraft();
        } catch (const std::exception& error) {
            SetControlText(status, L"错误：" + ExceptionMessage(error));
            return;
        }

        const jsondict::Node* old_node = app->document.find(target_id);
        if (!old_node) {
            SetControlText(status, L"错误：目标节点已不存在");
            return;
        }
        if (jsondict::equivalent(*old_node, *parsed)) {
            draft_dirty = false;
            ShutdownBlockReasonDestroy(window);
            DestroyWindow(window);
            return;
        }
        const std::size_t future_count = app->document.node_count() - CountNodes(*old_node) +
                                         CountNodes(*parsed);
        if (future_count > kMaximumDocumentNodes) {
            SetControlText(status, L"错误：应用后会超过 50000 个节点的图形编辑安全上限");
            return;
        }
        jsondict::Document candidate = app->document;
        if (!candidate.replace_node(target_id, *parsed)) {
            SetControlText(status, L"错误：替换会违反根对象、重复键或 512 层嵌套限制");
            return;
        }
        if (!app->EnsureDocumentWithinUiLimits(candidate, window)) {
            SetControlText(status, L"错误：应用后会超过图形编辑安全上限");
            return;
        }
        app->document = std::move(candidate);

        draft_dirty = false;
        ShutdownBlockReasonDestroy(window);
        app->selected_id = target_id;
        app->inspector_draft_dirty = false;
        app->MarkDirty();
        try {
            app->RebuildTree();
            app->UpdateInspector();
        } catch (const std::exception& error) {
            MessageBoxW(window,
                        (L"数据已经应用，但主窗口刷新失败。请立即保存并重新打开程序。\n\n" +
                         ExceptionMessage(error)).c_str(),
                        L"数据已应用，界面刷新失败", MB_ICONWARNING | MB_OK);
        }
        DestroyWindow(window);
    }
};

void ShowRawEditor(AppState& app, jsondict::NodeId target_id) {
    if (!app.document.find(target_id)) return;
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW window_class{};
        window_class.cbSize = sizeof(window_class);
        window_class.style = CS_HREDRAW | CS_VREDRAW;
        window_class.lpfnWndProc = RawEditorState::WindowProc;
        window_class.hInstance = app.instance;
        window_class.hIcon = LoadIconW(app.instance, MAKEINTRESOURCEW(kAppIconResource));
        if (!window_class.hIcon) window_class.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
        window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
        window_class.lpszClassName = kRawWindowClass;
        if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            MessageBoxW(app.window, L"无法注册原始 JSON 编辑器窗口。", L"错误",
                        MB_ICONERROR | MB_OK);
            return;
        }
        registered = true;
    }

    RawEditorState state;
    state.app = &app;
    state.target_id = target_id;
    state.dpi = app.dpi;
    const wchar_t* title = target_id == app.document.root_id()
        ? L"编辑完整 JSON 字典" : L"编辑所选节点的原始 JSON";
    constexpr DWORD extended_style = WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT;
    constexpr DWORD style = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;
    RECT frame{0, 0, MulDiv(760, static_cast<int>(state.dpi), 96),
                     MulDiv(560, static_cast<int>(state.dpi), 96)};
    if (!AdjustWindowRectExForDpi(&frame, style, FALSE, extended_style, state.dpi)) {
        AdjustWindowRectEx(&frame, style, FALSE, extended_style);
    }
    HWND window = CreateWindowExW(
        extended_style, kRawWindowClass, title, style,
        CW_USEDEFAULT, CW_USEDEFAULT, frame.right - frame.left, frame.bottom - frame.top,
        app.window, nullptr, app.instance, &state);
    if (!window) {
        MessageBoxW(app.window, L"无法创建原始 JSON 编辑器窗口。", L"错误",
                    MB_ICONERROR | MB_OK);
        return;
    }

    RECT owner_rect{};
    RECT dialog_rect{};
    GetWindowRect(app.window, &owner_rect);
    GetWindowRect(window, &dialog_rect);
    const int width = dialog_rect.right - dialog_rect.left;
    const int height = dialog_rect.bottom - dialog_rect.top;
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!GetMonitorInfoW(MonitorFromWindow(app.window, MONITOR_DEFAULTTONEAREST), &monitor_info)) {
        monitor_info.rcWork = owner_rect;
    }
    const RECT work_area = monitor_info.rcWork;
    const int centered_x = owner_rect.left + ((owner_rect.right - owner_rect.left) - width) / 2;
    const int centered_y = owner_rect.top + ((owner_rect.bottom - owner_rect.top) - height) / 2;
    const int x = std::clamp(centered_x, static_cast<int>(work_area.left),
                             std::max(static_cast<int>(work_area.left),
                                      static_cast<int>(work_area.right) - width));
    const int y = std::clamp(centered_y, static_cast<int>(work_area.top),
                             std::max(static_cast<int>(work_area.top),
                                      static_cast<int>(work_area.bottom) - height));
    SetWindowPos(window, HWND_TOP, x, y,
                 width, height, SWP_NOACTIVATE);

    EnableWindow(app.window, FALSE);
    ShowWindow(window, SW_SHOW);
    UpdateWindow(window);
    SetFocus(state.edit);
    std::array<ACCEL, 4> entries{{
        {FVIRTKEY, VK_F5, IDC_RAW_CHECK},
        {FVIRTKEY | FCONTROL | FSHIFT, 'F', IDC_RAW_FORMAT},
        {FVIRTKEY | FCONTROL, VK_RETURN, IDC_RAW_APPLY},
        {FVIRTKEY, VK_ESCAPE, IDC_RAW_CANCEL},
    }};
    HACCEL accelerators = CreateAcceleratorTableW(entries.data(), static_cast<int>(entries.size()));
    MSG message{};
    bool saw_quit = false;
    int quit_code = 0;
    while (state.window) {
        const BOOL result = GetMessageW(&message, nullptr, 0, 0);
        if (result <= 0) {
            if (result == 0) {
                saw_quit = true;
                quit_code = static_cast<int>(message.wParam);
            }
            break;
        }
        if (accelerators && TranslateAcceleratorW(window, accelerators, &message)) continue;
        if (IsDialogMessageW(window, &message)) continue;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (accelerators) DestroyAcceleratorTable(accelerators);
    if (IsWindow(window)) DestroyWindow(window);
    EnableWindow(app.window, TRUE);
    SetForegroundWindow(app.window);
    if (saw_quit) PostQuitMessage(quit_code);
}

void WriteStandardHandle(DWORD handle_id, std::string_view text) {
    HANDLE handle = GetStdHandle(handle_id);
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        handle = GetStdHandle(handle_id);
    }
    if (!handle || handle == INVALID_HANDLE_VALUE) {
        OutputDebugStringA(std::string(text).c_str());
        return;
    }
    std::size_t offset = 0;
    while (offset < text.size()) {
        const DWORD request = static_cast<DWORD>(
            std::min<std::size_t>(text.size() - offset, 1u << 20));
        DWORD written = 0;
        if (!WriteFile(handle, text.data() + offset, request, &written, nullptr) || written == 0) {
            break;
        }
        offset += written;
    }
}

void PrintOut(std::string text) {
    text.push_back('\n');
    WriteStandardHandle(STD_OUTPUT_HANDLE, text);
}

void PrintError(std::string text) {
    text.push_back('\n');
    WriteStandardHandle(STD_ERROR_HANDLE, text);
}

void SelfTestExpect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Function>
void SelfTestExpectJsonError(Function&& function, jsondict::ErrorCode expected,
                             const char* message) {
    try {
        function();
    } catch (const jsondict::Error& error) {
        if (error.code() == expected) return;
        throw std::runtime_error(std::string(message) + " (wrong error code)");
    }
    throw std::runtime_error(std::string(message) + " (no error)");
}

int RunIntegratedSelfTest() {
    try {
        const jsondict::Node parsed = jsondict::parse(
            R"({"z":1.2300e+04,"a":"文字","enabled":true,"nothing":null,"nested":{"x":2},"list":[1,"2",false]})");
        SelfTestExpect(parsed.kind() == jsondict::Kind::Object, "root type changed");
        SelfTestExpect(parsed.as_object().size() == 6, "member count changed");
        SelfTestExpect(parsed.as_object()[0].key == "z" && parsed.as_object()[1].key == "a",
                       "object order changed");
        SelfTestExpect(parsed.as_object()[0].value.as_number().text == "1.2300e+04",
                       "number text changed");

        const jsondict::Node unicode = jsondict::parse(
            R"({"中文":"你好\n世界","emoji":"\uD83D\uDE80","nul":"\u0000"})");
        SelfTestExpect(unicode.as_object()[1].value.as_string() == "\xF0\x9F\x9A\x80",
                       "surrogate pair decoding failed");
        const std::string unicode_round_trip = jsondict::write(
            unicode, jsondict::WriteOptions{jsondict::Formatting::TwoSpaces, false, true});
        SelfTestExpect(jsondict::equivalent(unicode, jsondict::parse(unicode_round_trip)),
                       "Unicode round trip failed");

        const std::array<const char*, 6> valid_numbers{{
            "0", "-0", "12", "-12.50", "6.02e23", "1E-9"}};
        for (const char* number : valid_numbers) {
            SelfTestExpect(jsondict::is_valid_number(number), "valid number rejected");
        }
        const std::array<const char*, 9> invalid_numbers{{
            "", "+1", "01", "1.", ".5", "NaN", "Infinity", "1e", "--2"}};
        for (const char* number : invalid_numbers) {
            SelfTestExpect(!jsondict::is_valid_number(number), "invalid number accepted");
        }
        SelfTestExpectJsonError(
            [] { (void)jsondict::parse(R"({"a":1,"\u0061":2})"); },
            jsondict::ErrorCode::DuplicateKey, "escaped duplicate key accepted");
        SelfTestExpectJsonError(
            [] { (void)jsondict::parse(R"({"a":[1,]})"); },
            jsondict::ErrorCode::Parse, "trailing comma accepted");
        SelfTestExpectJsonError(
            [] { (void)jsondict::Document::from_json("[1,2]"); },
            jsondict::ErrorCode::RootMustBeObject, "array dictionary root accepted");
        const std::string too_deep = std::string(513, '[') + "null" + std::string(513, ']');
        SelfTestExpectJsonError(
            [&] { (void)jsondict::parse(too_deep); },
            jsondict::ErrorCode::Parse, "excessive nesting accepted");

        jsondict::Document document;
        const auto name = document.add_child();
        SelfTestExpect(name.has_value() && document.rename_node(*name, "name") &&
                       document.set_string(*name, "editor"), "basic document edit failed");
        const auto object = document.add_child();
        SelfTestExpect(object.has_value() && document.rename_node(*object, "settings") &&
                       document.change_kind(*object, jsondict::Kind::Object),
                       "object creation failed");
        const auto nested = document.add_child(*object);
        SelfTestExpect(nested.has_value() && document.rename_node(*nested, "enabled") &&
                       document.change_kind(*nested, jsondict::Kind::Boolean) &&
                       document.set_boolean(*nested, true), "nested edit failed");
        SelfTestExpect(!document.rename_node(*object, "name"), "duplicate rename accepted");
        const auto duplicate = document.duplicate_node(*object);
        SelfTestExpect(duplicate.has_value(), "duplicate failed");
        SelfTestExpect(document.can_move(*duplicate, -1) && document.move_node(*duplicate, -1),
                       "move failed");
        SelfTestExpect(document.delete_node(*duplicate), "delete failed");

        const jsondict::Node replacement = jsondict::parse(R"({"nested":[1,2,3]})");
        const jsondict::NodeId retained_id = *name;
        SelfTestExpect(document.replace_node(retained_id, replacement) &&
                       document.find(retained_id) != nullptr &&
                       document.find(retained_id)->kind() == jsondict::Kind::Object,
                       "raw replacement identity failed");
        const std::string saved = jsondict::write(
            document.root(), jsondict::WriteOptions{
                jsondict::Formatting::FourSpaces, true, true});
        SelfTestExpect(!saved.empty() && saved.back() == '\n', "trailing newline missing");
        SelfTestExpect(jsondict::equivalent(document.root(), jsondict::parse(saved)),
                       "document round trip failed");

        wchar_t temporary_directory[MAX_PATH + 1]{};
        const DWORD temp_length = GetTempPathW(MAX_PATH, temporary_directory);
        SelfTestExpect(temp_length != 0 && temp_length <= MAX_PATH, "GetTempPath failed");
        const std::wstring test_path = std::wstring(temporary_directory) +
            L"JSONDictionaryEditorSelfTest-" + std::to_wstring(GetCurrentProcessId()) + L".json";
        DeleteFileW(test_path.c_str());
        WriteFileAtomically(test_path, saved, ExpectedTarget::Absent());
        const FileContents first_reopen = ReadUtf8File(test_path);
        const std::string external_version = "{\"external\":true}\n";
        WriteFileAtomically(
            test_path, external_version, ExpectedTarget::Match(first_reopen.identity));
        bool conflict_rejected = false;
        try {
            WriteFileAtomically(
                test_path, saved, ExpectedTarget::Match(first_reopen.identity));
        } catch (const std::exception&) {
            conflict_rejected = true;
        }
        SelfTestExpect(conflict_rejected, "stale file identity was not rejected");
        bool unexpected_existing_rejected = false;
        try {
            WriteFileAtomically(test_path, saved, ExpectedTarget::Absent());
        } catch (const std::exception&) {
            unexpected_existing_rejected = true;
        }
        SelfTestExpect(unexpected_existing_rejected,
                       "must-be-absent target unexpectedly overwrote an existing file");
        const FileContents reopened = ReadUtf8File(test_path);
        DeleteFileW(test_path.c_str());
        const jsondict::Document round_trip = jsondict::Document::from_json(first_reopen.utf8);
        SelfTestExpect(jsondict::equivalent(document.root(), round_trip.root()),
                       "atomic file round trip failed");
        SelfTestExpect(reopened.utf8 == external_version,
                       "conflict rejection modified the external version");

        PrintOut("SELF_TEST_OK: parser, Unicode, numbers, validation, formatting, tree operations, identity, file round-trip, conflict protection");
        return 0;
    } catch (const std::exception& error) {
        PrintError(std::string("SELF_TEST_FAILED: ") + error.what());
        return 1;
    }
}

int ValidateJsonFile(const std::wstring& path) {
    try {
        const FileContents file = ReadUtf8File(path);
        const jsondict::Document document = jsondict::Document::from_json(file.utf8);
        PrintOut("VALID_JSON_DICTIONARY: " + std::to_string(document.node_count()) + " nodes");
        return 0;
    } catch (const std::exception& error) {
        PrintError(std::string("INVALID_JSON_DICTIONARY: ") + error.what());
        return 1;
    }
}

std::optional<int> HandleCommandLine(int argument_count, wchar_t** arguments,
                                     std::optional<std::wstring>& initial_path) {
    if (argument_count <= 1) return std::nullopt;
    const std::wstring_view command(arguments[1]);
    if (command == L"--self-test") {
        if (argument_count != 2) {
            PrintError("usage: JSONDictionaryEditor.exe --self-test");
            return 2;
        }
        return RunIntegratedSelfTest();
    }
    if (command == L"--validate-json") {
        if (argument_count != 3) {
            PrintError("usage: JSONDictionaryEditor.exe --validate-json <path>");
            return 2;
        }
        return ValidateJsonFile(arguments[2]);
    }
    if (command == L"--version") {
        PrintOut("JSON Dictionary Editor for Windows 1.0.1");
        return 0;
    }
    if (command == L"--help") {
        PrintOut(
            "JSON Dictionary Editor for Windows 1.0.1\n"
            "  --self-test             run deterministic core and file self-tests\n"
            "  --validate-json <path>  validate a UTF-8 JSON object file\n"
            "  --version               display version\n"
            "  <path.json>             open a JSON dictionary in the GUI");
        return 0;
    }
    if (command.size() >= 2 && command[0] == L'-' && command[1] == L'-') {
        PrintError("unknown command; use --help");
        return 2;
    }
    initial_path = std::wstring(command);
    return std::nullopt;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
    int argument_count = 0;
    wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argument_count);
    std::optional<std::wstring> initial_path;
    if (arguments) {
        if (const auto result = HandleCommandLine(argument_count, arguments, initial_path)) {
            LocalFree(static_cast<HLOCAL>(arguments));
            return *result;
        }
        LocalFree(static_cast<HLOCAL>(arguments));
    }

    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX common_controls{
        sizeof(common_controls), ICC_STANDARD_CLASSES | ICC_WIN95_CLASSES |
                                     ICC_TREEVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&common_controls);
    const HRESULT com_result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    AppState app;
    if (!app.Create(instance)) {
        MessageBoxW(nullptr, L"无法创建 JSON 字典编辑器主窗口。", L"启动失败",
                    MB_ICONERROR | MB_OK);
        if (SUCCEEDED(com_result)) CoUninitialize();
        return 1;
    }
    const int result = app.Run(std::move(initial_path));
    if (SUCCEEDED(com_result)) CoUninitialize();
    return result;
}
