#include "editor_session.hpp"
#include <stdexcept>

namespace jsondict_linux {

void EditorSession::check_limits(const jsondict::Document& candidate, bool bom) {
    jsondict::validate(candidate.root(), true);
    if (candidate.node_count() > kMaximumDocumentNodes)
        throw std::runtime_error("The document exceeds the 50,000-node editor limit.");
    const auto bytes = jsondict::write(candidate.root(), {
        candidate.formatting(), candidate.trailing_newline(), true});
    if (bytes.size() + (bom ? 3u : 0u) > kMaximumFileBytes)
        throw std::runtime_error("The saved UTF-8 document would exceed 16 MiB.");
}

void EditorSession::load(std::string_view bytes) {
    if (bytes.size() > kMaximumFileBytes)
        throw std::runtime_error("The file exceeds the 16 MiB editor limit.");
    if (bytes.size() >= 2 &&
        ((static_cast<unsigned char>(bytes[0]) == 0xff && static_cast<unsigned char>(bytes[1]) == 0xfe) ||
         (static_cast<unsigned char>(bytes[0]) == 0xfe && static_cast<unsigned char>(bytes[1]) == 0xff)))
        throw std::runtime_error("UTF-16 files are not supported. Use UTF-8 JSON.");
    const bool bom = bytes.substr(0, 3) == "\xef\xbb\xbf";
    auto candidate = jsondict::Document::from_json(bom ? bytes.substr(3) : bytes);
    check_limits(candidate, bom);
    document = std::move(candidate);
    selected = document.root_id();
    utf8_bom = bom;
    dirty = false;
}

std::string EditorSession::encoded() const {
    check_limits(document, utf8_bom);
    auto text = jsondict::write(document.root(), {
        document.formatting(), document.trailing_newline(), true});
    if (utf8_bom) text.insert(0, "\xef\xbb\xbf", 3);
    return text;
}

bool EditorSession::mutate(const std::function<bool(jsondict::Document&)>& operation) {
    auto candidate = document;
    if (!operation(candidate)) return false;
    check_limits(candidate, utf8_bom);
    const bool changed = !jsondict::equivalent(document.root(), candidate.root()) ||
        document.formatting() != candidate.formatting() ||
        document.trailing_newline() != candidate.trailing_newline();
    if (changed) { document = std::move(candidate); dirty = true; }
    return changed;
}

bool EditorSession::apply_draft(const InspectorDraft& draft) {
    auto candidate = document;
    if (draft.key && !candidate.rename_node(selected, *draft.key))
        throw std::runtime_error("This key already exists in the object.");
    if (draft.value) {
        const auto* node = candidate.find(selected);
        if (!node || node->kind() != draft.kind)
            throw std::runtime_error("The selected node changed before the draft was applied.");
        bool success = false;
        if (draft.kind == jsondict::Kind::String) success = candidate.set_string(selected, *draft.value);
        if (draft.kind == jsondict::Kind::Number) success = candidate.set_number(selected, *draft.value);
        if (!success) throw std::runtime_error("Enter a valid JSON number.");
    }
    check_limits(candidate, utf8_bom);
    if (jsondict::equivalent(document.root(), candidate.root())) return false;
    document = std::move(candidate);
    dirty = true;
    return true;
}

bool EditorSession::apply_raw(jsondict::NodeId target, std::string_view text) {
    if (text.size() > kMaximumRawBytes)
        throw std::runtime_error("The raw JSON draft exceeds 4 MiB.");
    const auto replacement = jsondict::parse(text);
    jsondict::validate(replacement, target == document.root_id());
    return mutate([&](jsondict::Document& candidate) {
        if (!candidate.replace_node(target, replacement))
            throw std::runtime_error("The raw JSON replacement violates document limits.");
        return true;
    });
}

}  // namespace jsondict_linux
