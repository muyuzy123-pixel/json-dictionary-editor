#pragma once
#include "json_core.hpp"
#include <functional>
#include <optional>
#include <string>

namespace jsondict_linux {

constexpr std::size_t kMaximumFileBytes = 16u * 1024u * 1024u;
constexpr std::size_t kMaximumRawBytes = 4u * 1024u * 1024u;
constexpr std::size_t kMaximumInspectorCharacters = 1024u * 1024u;
constexpr std::size_t kMaximumKeyCharacters = 65535;
constexpr std::size_t kMaximumDocumentNodes = 50000;

// No Qt data types or numeric conversions are allowed in the document layer.
struct InspectorDraft {
    std::optional<std::string> key;
    std::optional<std::string> value;
    jsondict::Kind kind = jsondict::Kind::String;
    std::optional<bool> boolean;
};

class EditorSession {
public:
    jsondict::Document document;
    jsondict::NodeId selected = document.root_id();
    bool dirty = false;
    bool utf8_bom = false;

    static void check_limits(const jsondict::Document& candidate, bool bom = false);
    void load(std::string_view bytes);
    std::string encoded() const;
    bool mutate(const std::function<bool(jsondict::Document&)>& operation);
    bool apply_draft(const InspectorDraft& draft);
    bool apply_raw(jsondict::NodeId target, std::string_view text);
};

}  // namespace jsondict_linux
