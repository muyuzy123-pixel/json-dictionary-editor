#include "language.hpp"
#include "file_store.hpp"
#include <QCoreApplication>
#include <QLocale>
#include <QResource>
#include <QFile>
#include <cstring>

static void initializeEditorResources() {
    Q_INIT_RESOURCE(translations);
    Q_INIT_RESOURCE(sample);
}

namespace jsondict_linux {

QString ui(const char* source) { return QCoreApplication::translate("Editor", source); }
QString checkedText(const std::string& text) {
    if (!jsondict::is_valid_utf8(text)) throw std::runtime_error("Invalid UTF-8 text.");
    return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}
std::string checkedUtf8(const QString& text) {
    for (qsizetype i = 0; i < text.size(); ++i) {
        const auto unit = text[i].unicode();
        if (unit >= 0xd800 && unit <= 0xdbff) {
            if (++i >= text.size() || text[i].unicode() < 0xdc00 || text[i].unicode() > 0xdfff)
                throw std::runtime_error("Invalid Unicode text.");
        } else if (unit >= 0xdc00 && unit <= 0xdfff) {
            throw std::runtime_error("Invalid Unicode text.");
        }
    }
    const QByteArray bytes = text.toUtf8();
    return std::string(bytes.constData(), static_cast<std::size_t>(bytes.size()));
}
QString kindTitle(jsondict::Kind kind) {
    switch (kind) {
        case jsondict::Kind::String: return ui("String");
        case jsondict::Kind::Number: return ui("Number");
        case jsondict::Kind::Boolean: return ui("Boolean");
        case jsondict::Kind::Null: return QStringLiteral("Null");
        case jsondict::Kind::Object: return ui("Object");
        case jsondict::Kind::Array: return ui("Array");
    }
    return {};
}
QString summary(const jsondict::Node& node) {
    switch (node.kind()) {
        case jsondict::Kind::String: {
            auto text = checkedText(node.as_string());
            text.replace('\n', QStringLiteral(" ↩ ")).replace('\r', QStringLiteral(" "));
            if (text.size() > 96) text = text.left(95) + QChar(0x2026);
            return text;
        }
        case jsondict::Kind::Number: return checkedText(node.as_number().text).left(96);
        case jsondict::Kind::Boolean: return node.as_boolean() ? QStringLiteral("true") : QStringLiteral("false");
        case jsondict::Kind::Null: return QStringLiteral("null");
        case jsondict::Kind::Object:
            return node.child_count() == 0 ? ui("Empty Object") :
                (node.child_count() == 1 ? ui("%1 key") : ui("%1 keys")).arg(node.child_count());
        case jsondict::Kind::Array:
            return node.child_count() == 0 ? ui("Empty Array") :
                (node.child_count() == 1 ? ui("%1 element") : ui("%1 elements")).arg(node.child_count());
    }
    return {};
}
QString errorText(const std::exception& error) {
    if (const auto* file = dynamic_cast<const FileError*>(&error)) {
        QString text;
        switch (file->type) {
            case FileFailure::Conflict: text = file->committed
                ? ui("A competing version was replaced; the recovery copy is retained. The document remains unsaved.")
                : ui("The file changed outside the editor. Nothing was silently overwritten."); break;
            case FileFailure::UnsupportedAtomicSave: text = ui("This filesystem or target does not support the required atomic save. Direct overwrite is disabled."); break;
            case FileFailure::Symlink: text = ui("Symbolic-link targets are not supported."); break;
            case FileFailure::Hardlink: text = ui("Hard-linked targets are not supported."); break;
            case FileFailure::NotRegular: text = ui("The target must be a regular file."); break;
            case FileFailure::ChangedDuringRead: text = ui("The file changed while it was being read. Retry."); break;
            case FileFailure::TooLarge: text = ui("The file exceeds the 16 MiB editor limit."); break;
            case FileFailure::Verification: text = ui("Saved content could not be verified. The document remains unsaved."); break;
            case FileFailure::Durability: text = ui("The file or directory could not be flushed. Durability is not confirmed."); break;
            case FileFailure::IO: text = ui("File operation failed."); break;
        }
        if (file->committed) text += "\n" + ui("The commit may already have occurred; inspect the target before retrying.");
        if (!file->backup.isEmpty()) text += "\n" + ui("Retained previous version: %1").arg(file->backup);
        if (file->system_error) text += "\n" + ui("System error %1: %2").arg(
            QString::number(file->system_error), QString::fromLocal8Bit(std::strerror(file->system_error)));
        return text;
    }
    if (const auto* json = dynamic_cast<const jsondict::Error*>(&error)) {
        const char* key = nullptr;
        using R = jsondict::ErrorReason;
        switch (json->reason()) {
            case R::InputNotUtf8: case R::StringNotUtf8: case R::KeyNotUtf8: key = "Invalid UTF-8 text."; break;
            case R::Empty: key = "JSON content is empty."; break;
            case R::TrailingContent: key = "Unexpected content after JSON value."; break;
            case R::MissingValue: key = "Missing JSON value."; break;
            case R::TooManyNodes: key = "JSON exceeds the parser node limit."; break;
            case R::UnrecognizedValue: key = "Unrecognized JSON value."; break;
            case R::TooDeep: key = "JSON nesting exceeds 512 levels."; break;
            case R::QuotedKey: key = "Object keys must be quoted strings."; break;
            case R::DuplicateKey: key = "Duplicate object key: %1"; break;
            case R::InvalidNumber: key = "Invalid JSON number: %1"; break;
            case R::RootObject: key = "The JSON root must be an object."; break;
            case R::UnicodeIncomplete: key = "Incomplete Unicode escape."; break;
            case R::UnicodeHex: key = "Unicode escape requires four hexadecimal digits."; break;
            case R::HighSurrogate: case R::InvalidLowSurrogate: case R::IsolatedLowSurrogate: key = "Invalid Unicode surrogate pair."; break;
            case R::StringEscapeIncomplete: key = "Incomplete string escape."; break;
            case R::UnsupportedEscape: key = "Unsupported string escape."; break;
            case R::ControlCharacter: key = "Unescaped control character in string."; break;
            case R::UnterminatedString: key = "Unterminated string."; break;
            case R::InvalidLiteral: case R::LiteralSuffix: key = "Invalid JSON literal."; break;
            case R::ExpectedObjectOpen: key = "Expected '{'."; break;
            case R::ExpectedArrayOpen: key = "Expected '['."; break;
            case R::OpeningQuote: key = "Expected an opening quote."; break;
            case R::ExpectedColon: key = "Expected ':' after object key."; break;
            case R::ExpectedObjectComma: case R::ExpectedArrayComma: key = "Expected ',' between members or elements."; break;
            case R::Unknown: break;
        }
        QString text = key ? ui(key) : QString::fromUtf8(json->what());
        if (key && QByteArray(key).contains("%1")) text = text.arg(checkedText(json->argument()));
        if (json->line()) text = ui("Line %1, column %2: %3").arg(
            QString::number(json->line()), QString::number(json->column()), text);
        if (!json->path().empty()) text += "\n" + ui("Path: %1").arg(checkedText(json->path()));
        return text;
    }
    return ui(error.what());
}

QString LanguageService::resolve(Preference preference, const QString& systemLanguage) {
    if (preference == Preference::Chinese) return QStringLiteral("zh-Hans");
    if (preference == Preference::English) return QStringLiteral("en");
    return systemLanguage.startsWith("zh", Qt::CaseInsensitive) ? QStringLiteral("zh-Hans") : QStringLiteral("en");
}
LanguageService::LanguageService(QObject* parent, bool persist) : QObject(parent), persist_(persist) {
    initializeEditorResources();
    if (persist_) {
        QSettings settings;
        const auto value = settings.value("uiLanguage", "system").toString();
        preference_ = value == "zh-Hans" ? Preference::Chinese : value == "en" ? Preference::English : Preference::System;
    }
    select(preference_);
}
bool LanguageService::select(Preference preference) {
    preference_ = preference;
    effective_ = resolve(preference, QLocale::system().uiLanguages().value(0, QStringLiteral("en")));
    QCoreApplication::removeTranslator(&translator_);
    const auto resource = effective_ == "zh-Hans" ? QStringLiteral(":/i18n/editor_zh_CN.qm") : QStringLiteral(":/i18n/editor_en.qm");
    if (!translator_.load(resource)) throw std::runtime_error("The bundled language resource could not be loaded.");
    QCoreApplication::installTranslator(&translator_);
    bool saved = true;
    if (persist_) {
        QSettings settings;
        settings.setAtomicSyncRequired(true);
        settings.setValue("uiLanguage", preference == Preference::Chinese ? "zh-Hans" : preference == Preference::English ? "en" : "system");
        settings.sync();
        saved = settings.status() == QSettings::NoError;
    }
    emit changed();
    return saved;
}

}  // namespace jsondict_linux
