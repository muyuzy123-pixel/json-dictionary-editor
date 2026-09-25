#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace jsondict {

using NodeId = std::uint64_t;

enum class Kind {
    String,
    Number,
    Boolean,
    Null,
    Object,
    Array,
};

struct Number {
    std::string text;

    friend bool operator==(const Number& lhs, const Number& rhs) {
        return lhs.text == rhs.text;
    }
};

struct Null {
    friend constexpr bool operator==(Null, Null) noexcept { return true; }
};

class Node {
public:
    struct Member;
    using Object = std::vector<Member>;
    using Array = std::vector<Node>;

    Node();

    static Node string(std::string value);
    static Node number(std::string original_text);
    static Node boolean(bool value);
    static Node null();
    static Node object(Object members = {});
    static Node array(Array values = {});

    NodeId id() const noexcept { return id_; }
    Kind kind() const noexcept;
    bool is_container() const noexcept;
    std::size_t child_count() const noexcept;

    const std::string& as_string() const;
    std::string& as_string();
    const Number& as_number() const;
    Number& as_number();
    bool as_boolean() const;
    bool& as_boolean();
    const Object& as_object() const;
    Object& as_object();
    const Array& as_array() const;
    Array& as_array();

    // Ordinary copies preserve IDs, which is useful for document snapshots.
    // deep_copy() recursively allocates fresh IDs, which is used by Duplicate.
    Node deep_copy() const;
    Node clone_with_fresh_ids() const { return deep_copy(); }
    void regenerate_ids();

    friend bool equivalent(const Node& lhs, const Node& rhs);

private:
    using Storage = std::variant<std::string, Number, bool, Null, Object, Array>;

    explicit Node(Storage value);
    Node(NodeId id, Storage value);

    NodeId id_;
    Storage value_;

    friend class Document;
};

struct Node::Member {
    std::string key;
    Node value;

    friend bool operator==(const Member& lhs, const Member& rhs) {
        return lhs.key == rhs.key && equivalent(lhs.value, rhs.value);
    }
};

bool equivalent(const Node& lhs, const Node& rhs);

enum class ErrorCode {
    Parse,
    InvalidUtf8,
    InvalidNumber,
    DuplicateKey,
    RootMustBeObject,
};

enum class ErrorReason {
    Unknown,
    InputNotUtf8, Empty, TrailingContent, MissingValue, TooManyNodes,
    UnrecognizedValue, TooDeep, ExpectedObjectOpen, QuotedKey,
    DuplicateKey, ExpectedColon, ExpectedObjectComma, ExpectedArrayOpen,
    ExpectedArrayComma, UnicodeIncomplete, UnicodeHex, OpeningQuote,
    StringEscapeIncomplete, HighSurrogate, InvalidLowSurrogate,
    IsolatedLowSurrogate, UnsupportedEscape, ControlCharacter,
    UnterminatedString, InvalidNumber, InvalidLiteral, LiteralSuffix,
    StringNotUtf8, KeyNotUtf8, RootObject,
};

class Error : public std::runtime_error {
public:
    Error(ErrorCode code,
          std::string message,
          std::size_t line = 0,
          std::size_t column = 0,
          std::string path = {},
          ErrorReason reason = ErrorReason::Unknown,
          std::string argument = {});

    ErrorCode code() const noexcept { return code_; }
    std::size_t line() const noexcept { return line_; }
    std::size_t column() const noexcept { return column_; }
    const std::string& path() const noexcept { return path_; }
    ErrorReason reason() const noexcept { return reason_; }
    const std::string& argument() const noexcept { return argument_; }

private:
    ErrorCode code_;
    std::size_t line_;
    std::size_t column_;
    std::string path_;
    ErrorReason reason_;
    std::string argument_;
};

bool is_valid_utf8(std::string_view text) noexcept;
bool is_valid_number(std::string_view text) noexcept;

// Parses exactly one RFC 8259 JSON value. Object member order and number text
// are preserved. Duplicate object keys are rejected after escape decoding.
Node parse(std::string_view utf8_text);

enum class Formatting {
    TwoSpaces,
    FourSpaces,
    Tabs,
    Compact,
};

Formatting detect_formatting(std::string_view utf8_text) noexcept;

struct WriteOptions {
    Formatting formatting = Formatting::TwoSpaces;
    bool trailing_newline = false;
    bool require_root_object = false;
};

// Checks every stored string/key for UTF-8, every number token for JSON number
// grammar, and every object for duplicate keys.
void validate(const Node& node, bool require_root_object = false);
std::string write(const Node& node, WriteOptions options = {});

struct Location {
    NodeId node_id = 0;
    std::optional<NodeId> parent_id;
    std::optional<std::string> key;
    std::optional<std::size_t> index;
    std::string path;

    bool is_object_member() const noexcept { return key.has_value(); }
    bool is_array_element() const noexcept { return index.has_value(); }
};

class Document {
public:
    Document();
    explicit Document(Node root,
                      Formatting formatting = Formatting::TwoSpaces,
                      bool trailing_newline = true);

    static Document from_json(std::string_view utf8_text);

    const Node& root() const noexcept { return root_; }
    NodeId root_id() const noexcept { return root_.id(); }
    Formatting formatting() const noexcept { return formatting_; }
    bool trailing_newline() const noexcept { return trailing_newline_; }
    void set_formatting(Formatting value) noexcept { formatting_ = value; }
    void set_trailing_newline(bool value) noexcept { trailing_newline_ = value; }

    const Node* find(NodeId id) const noexcept;
    std::optional<Location> location(NodeId id) const;
    std::size_t node_count() const noexcept;

    bool is_key_available(std::string_view candidate, NodeId node_id) const;
    bool rename_node(NodeId node_id, std::string new_key);
    bool set_string(NodeId node_id, std::string value);
    bool set_number(NodeId node_id, std::string original_text);
    bool set_boolean(NodeId node_id, bool value);
    bool change_kind(NodeId node_id, Kind kind);

    // Keeps the selected node's ID, matching raw-editor replacement behavior.
    bool replace_node(NodeId node_id, const Node& replacement);

    // Adds to the selected container, or to the selected node's parent when the
    // selection is a scalar. Object members receive a unique "新键" key.
    std::optional<NodeId> add_child(std::optional<NodeId> preferred_node_id = {});
    bool delete_node(NodeId node_id);
    std::optional<NodeId> duplicate_node(NodeId node_id);
    bool can_move(NodeId node_id, int offset) const;
    bool move_node(NodeId node_id, int offset);
    bool sort_object(NodeId node_id);

    std::string encoded_text(std::optional<NodeId> node_id = {},
                             std::optional<Formatting> formatting = {}) const;

private:
    Node root_;
    Formatting formatting_ = Formatting::TwoSpaces;
    bool trailing_newline_ = true;

    Node* find_mutable(NodeId id) noexcept;
};

}  // namespace jsondict
