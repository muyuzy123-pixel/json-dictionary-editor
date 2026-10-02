#pragma once
#include <QObject>
#include <QSettings>
#include <QTranslator>
#include <QString>
#include <exception>
#include "json_core.hpp"

namespace jsondict_linux {

QString ui(const char* source);
QString checkedText(const std::string& text);
std::string checkedUtf8(const QString& text);
QString kindTitle(jsondict::Kind kind);
QString summary(const jsondict::Node& node);
QString errorText(const std::exception& error);

class LanguageService : public QObject {
    Q_OBJECT
public:
    enum class Preference { System, Chinese, English };
    explicit LanguageService(QObject* parent = nullptr, bool persist = true);
    Preference preference() const { return preference_; }
    bool chinese() const { return effective_ == QStringLiteral("zh-Hans"); }
    QString effective() const { return effective_; }
    bool select(Preference preference);
    static QString resolve(Preference preference, const QString& systemLanguage);
signals:
    void changed();
private:
    Preference preference_ = Preference::System;
    QString effective_;
    QTranslator translator_;
    bool persist_;
};

}  // namespace jsondict_linux
