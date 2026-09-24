#include "json_core.hpp"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <limits>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace jsondict {
namespace {

std::atomic<NodeId> g_next_node_id{1};

NodeId next_node_id() noexcept {
    return g_next_node_id.fetch_add(1, std::memory_order_relaxed);
}

bool is_continuation(unsigned char byte) noexcept {
    return byte >= 0x80 && byte <= 0xBF;
}

std::optional<std::size_t> invalid_utf8_offset(std::string_view text) noexcept {
    std::size_t i = 0;
    while (i < text.size()) {
        const auto first = static_cast<unsigned char>(text[i]);
        if (first <= 0x7F) {
            ++i;
            continue;
        }

        const auto has = [&](std::size_t count) { return i + count < text.size(); };
        const auto byte = [&](std::size_t offset) {
            return static_cast<unsigned char>(text[i + offset]);
        };

        if (first >= 0xC2 && first <= 0xDF) {
            if (!has(1) || !is_continuation(byte(1))) return i;
            i += 2;
        } else if (first == 0xE0) {
            if (!has(2) || byte(1) < 0xA0 || byte(1) > 0xBF ||
                !is_continuation(byte(2))) return i;
            i += 3;
        } else if ((first >= 0xE1 && first <= 0xEC) ||
                   (first >= 0xEE && first <= 0xEF)) {
            if (!has(2) || !is_continuation(byte(1)) ||
                !is_continuation(byte(2))) return i;
            i += 3;
        } else if (first == 0xED) {
            // UTF-8 must not directly encode UTF-16 surrogate code points.
            if (!has(2) || byte(1) < 0x80 || byte(1) > 0x9F ||
                !is_continuation(byte(2))) return i;
            i += 3;
        } else if (first == 0xF0) {
            if (!has(3) || byte(1) < 0x90 || byte(1) > 0xBF ||
                !is_continuation(byte(2)) || !is_continuation(byte(3))) return i;
            i += 4;
        } else if (first >= 0xF1 && first <= 0xF3) {
            if (!has(3) || !is_continuation(byte(1)) ||
                !is_continuation(byte(2)) || !is_continuation(byte(3))) return i;
            i += 4;
        } else if (first == 0xF4) {
            if (!has(3) || byte(1) < 0x80 || byte(1) > 0x8F ||
                !is_continuation(byte(2)) || !is_continuation(byte(3))) return i;
            i += 4;
        } else {
            return i;
        }
    }
    return std::nullopt;
}

std::pair<std::size_t, std::size_t> line_and_column(std::string_view text,
                                                    std::size_t offset) noexcept {
    std::size_t line = 1;
    std::size_t column = 1;
    const std::size_t end = std::min(offset, text.size());
    for (std::size_t i = 0; i < end; ++i) {
        if (text[i] == '\n') {
            ++line;
            column = 1;
        } else if ((static_cast<unsigned char>(text[i]) & 0xC0u) != 0x80u) {
            ++column;
        }
    }
    return {line, column};
}

void append_utf8(std::string& output, std::uint32_t scalar) {
    if (scalar <= 0x7F) {
        output.push_back(static_cast<char>(scalar));
    } else if (scalar <= 0x7FF) {
        output.push_back(static_cast<char>(0xC0 | (scalar >> 6)));
        output.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    } else if (scalar <= 0xFFFF) {
        output.push_back(static_cast<char>(0xE0 | (scalar >> 12)));
        output.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    } else {
        output.push_back(static_cast<char>(0xF0 | (scalar >> 18)));
        output.push_back(static_cast<char>(0x80 | ((scalar >> 12) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | ((scalar >> 6) & 0x3F)));
        output.push_back(static_cast<char>(0x80 | (scalar & 0x3F)));
    }
}

bool is_identifier_key(std::string_view key) noexcept {
    if (key.empty()) return false;
    const auto first = static_cast<unsigned char>(key.front());
    if (!((first >= 'A' && first <= 'Z') ||
          (first >= 'a' && first <= 'z') || first == '_' || first == '$')) {
        return false;
    }
    for (std::size_t i = 1; i < key.size(); ++i) {
        const auto byte = static_cast<unsigned char>(key[i]);
        if (!((byte >= 'A' && byte <= 'Z') ||
              (byte >= 'a' && byte <= 'z') ||
              (byte >= '0' && byte <= '9') || byte == '_' || byte == '$')) {
            return false;
        }
    }
    return true;
}

bool utf8_byte_less(std::string_view lhs, std::string_view rhs) noexcept {
    return std::lexicographical_compare(
        lhs.begin(), lhs.end(), rhs.begin(), rhs.end(),
        [](char left, char right) {
            return static_cast<unsigned char>(left) < static_cast<unsigned char>(right);
        });
}

std::string quote_string(std::string_view value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(value.size() + 2);
    result.push_back('"');
    for (unsigned char byte : value) {
        switch (byte) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case 0x08: result += "\\b"; break;
            case 0x0C: result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (byte < 0x20) {
                    result += "\\u00";
                    result.push_back(hex[(byte >> 4) & 0x0F]);
                    result.push_back(hex[byte & 0x0F]);
                } else {
                    result.push_back(static_cast<char>(byte));
                }
        }
    }
    result.push_back('"');
    return result;
}

std::string path_for_key(std::string_view base, std::string_view key) {
    if (is_identifier_key(key)) {
        std::string result(base);
        result.push_back('.');
        result.append(key.data(), key.size());
        return result;
    }
    std::string result(base);
    result.push_back('[');
    result += quote_string(key);
    result.push_back(']');
    return result;
}

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {}

    Node run() {
        if (const auto bad = invalid_utf8_offset(text_)) {
            const auto [line, column] = line_and_column(text_, *bad);
            throw Error(ErrorCode::InvalidUtf8, "Input is not valid UTF-8", line, column);
        }
        skip_whitespace();
        if (at_end()) fail("JSON content is empty");
        Node result = parse_value();
        skip_whitespace();
        if (!at_end()) fail("Unexpected content after JSON value");
        return result;
    }

private:
    static constexpr std::size_t kMaximumNestingDepth = 512;
    static constexpr std::size_t kMaximumNodeCount = 250000;

    class DepthGuard {
    public:
        explicit DepthGuard(std::size_t& depth) : depth_(depth) { ++depth_; }
        ~DepthGuard() { --depth_; }
        DepthGuard(const DepthGuard&) = delete;
        DepthGuard& operator=(const DepthGuard&) = delete;

    private:
        std::size_t& depth_;
    };

    std::string_view text_;
    std::size_t index_ = 0;
    std::size_t depth_ = 0;
    std::size_t node_count_ = 0;

    bool at_end() const noexcept { return index_ >= text_.size(); }
    unsigned char current() const noexcept {
        return at_end() ? 0 : static_cast<unsigned char>(text_[index_]);
    }

    [[noreturn]] void fail(std::string message) const {
        const auto [line, column] = line_and_column(text_, index_);
        throw Error(ErrorCode::Parse, std::move(message), line, column);
    }

    [[noreturn]] void fail(ErrorCode code, std::string message) const {
        const auto [line, column] = line_and_column(text_, index_);
        throw Error(code, std::move(message), line, column);
    }

    static bool is_delimiter(unsigned char byte) noexcept {
        return byte == ' ' || byte == '\t' || byte == '\n' || byte == '\r' ||
               byte == ',' || byte == ']' || byte == '}';
    }

    void skip_whitespace() noexcept {
        while (!at_end()) {
            const unsigned char byte = current();
            if (byte != ' ' && byte != '\t' && byte != '\n' && byte != '\r') break;
            ++index_;
        }
    }

    bool consume_if(unsigned char expected) noexcept {
        if (at_end() || current() != expected) return false;
        ++index_;
        return true;
    }

    void expect(unsigned char expected, const char* message) {
        if (!consume_if(expected)) fail(message);
    }

    Node parse_value() {
        if (at_end()) fail("Missing JSON value");
        if (++node_count_ > kMaximumNodeCount) {
            fail("JSON exceeds the 250000-node safety limit");
        }
        switch (current()) {
            case '"': return Node::string(parse_string());
            case '{': return parse_object();
            case '[': return parse_array();
            case 't': consume_literal("true"); return Node::boolean(true);
            case 'f': consume_literal("false"); return Node::boolean(false);
            case 'n': consume_literal("null"); return Node::null();
            case '-':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                return Node::number(parse_number());
            default: fail("Unrecognized JSON value");
        }
    }

    Node parse_object() {
        if (depth_ >= kMaximumNestingDepth) {
            fail("JSON nesting exceeds the 512-level safety limit");
        }
        DepthGuard depth_guard(depth_);
        expect('{', "Expected '{'");
        skip_whitespace();
        Node::Object members;
        std::unordered_set<std::string> keys;
        if (consume_if('}')) return Node::object(std::move(members));

        while (true) {
            if (at_end() || current() != '"') fail("Object key must be a quoted string");
            std::string key = parse_string();
            if (!keys.insert(key).second) {
                fail(ErrorCode::DuplicateKey, "Duplicate object key: " + key);
            }
            skip_whitespace();
            expect(':', "Expected ':' after object key");
            skip_whitespace();
            Node value = parse_value();
            members.push_back(Node::Member{std::move(key), std::move(value)});
            skip_whitespace();
            if (consume_if('}')) break;
            expect(',', "Expected ',' between object members");
            skip_whitespace();
        }
        return Node::object(std::move(members));
    }

    Node parse_array() {
        if (depth_ >= kMaximumNestingDepth) {
            fail("JSON nesting exceeds the 512-level safety limit");
        }
        DepthGuard depth_guard(depth_);
        expect('[', "Expected '['");
        skip_whitespace();
        Node::Array values;
        if (consume_if(']')) return Node::array(std::move(values));

        while (true) {
            values.push_back(parse_value());
            skip_whitespace();
            if (consume_if(']')) break;
            expect(',', "Expected ',' between array elements");
            skip_whitespace();
        }
        return Node::array(std::move(values));
    }

    std::uint32_t parse_hex_code_unit() {
        if (index_ + 4 > text_.size()) fail("Incomplete Unicode escape");
        std::uint32_t value = 0;
        for (int count = 0; count < 4; ++count) {
            const unsigned char byte = current();
            ++index_;
            value <<= 4;
            if (byte >= '0' && byte <= '9') value += byte - '0';
            else if (byte >= 'A' && byte <= 'F') value += byte - 'A' + 10;
            else if (byte >= 'a' && byte <= 'f') value += byte - 'a' + 10;
            else fail("Unicode escape requires four hexadecimal digits");
        }
        return value;
    }

    std::string parse_string() {
        expect('"', "Expected opening quote");
        std::string result;
        while (!at_end()) {
            const unsigned char byte = current();
            ++index_;
            if (byte == '"') return result;
            if (byte == '\\') {
                if (at_end()) fail("Incomplete string escape");
                const unsigned char escaped = current();
                ++index_;
                switch (escaped) {
                    case '"': result.push_back('"'); break;
                    case '\\': result.push_back('\\'); break;
                    case '/': result.push_back('/'); break;
                    case 'b': result.push_back('\b'); break;
                    case 'f': result.push_back('\f'); break;
                    case 'n': result.push_back('\n'); break;
                    case 'r': result.push_back('\r'); break;
                    case 't': result.push_back('\t'); break;
                    case 'u': {
                        const std::uint32_t first = parse_hex_code_unit();
                        if (first >= 0xD800 && first <= 0xDBFF) {
                            if (index_ + 2 > text_.size() || text_[index_] != '\\' ||
                                text_[index_ + 1] != 'u') {
                                fail("High surrogate must be followed by a low surrogate");
                            }
                            index_ += 2;
                            const std::uint32_t second = parse_hex_code_unit();
                            if (second < 0xDC00 || second > 0xDFFF) {
                                fail("Invalid low surrogate");
                            }
                            const std::uint32_t scalar =
                                0x10000 + ((first - 0xD800) << 10) + (second - 0xDC00);
                            append_utf8(result, scalar);
                        } else if (first >= 0xDC00 && first <= 0xDFFF) {
                            fail("Isolated low surrogate");
                        } else {
                            append_utf8(result, first);
                        }
                        break;
                    }
                    default: fail("Unsupported string escape");
                }
            } else {
                if (byte < 0x20) fail("Unescaped control character in string");
                result.push_back(static_cast<char>(byte));
            }
        }
        fail("Unterminated string");
    }

    std::string parse_number() {
        const std::size_t start = index_;
        while (!at_end() && !is_delimiter(current())) ++index_;
        const std::string token(text_.substr(start, index_ - start));
        if (!is_valid_number(token)) fail("Invalid JSON number: " + token);
        return token;
    }

    void consume_literal(std::string_view literal) {
        if (index_ + literal.size() > text_.size() ||
            text_.substr(index_, literal.size()) != literal) {
            fail("Invalid JSON literal");
        }
        index_ += literal.size();
        if (!at_end() && !is_delimiter(current())) {
            fail("Invalid character after JSON literal");
        }
    }
};

std::string indent_unit(Formatting formatting) {
    switch (formatting) {
        case Formatting::TwoSpaces: return "  ";
        case Formatting::FourSpaces: return "    ";
        case Formatting::Tabs: return "\t";
        case Formatting::Compact: return {};
    }
    return {};
}

std::string repeated_indent(const std::string& unit, std::size_t level) {
    std::string result;
    result.reserve(unit.size() * level);
    for (std::size_t i = 0; i < level; ++i) result += unit;
    return result;
}

void validate_at(const Node& node, const std::string& path, std::size_t container_depth) {
    constexpr std::size_t kMaximumNestingDepth = 512;
    const bool container = node.kind() == Kind::Object || node.kind() == Kind::Array;
    if (container && container_depth >= kMaximumNestingDepth) {
        throw Error(ErrorCode::Parse,
                    "JSON nesting exceeds the 512-level safety limit", 0, 0, path);
    }
    const std::size_t child_depth = container ? container_depth + 1 : container_depth;
    switch (node.kind()) {
        case Kind::String:
            if (!is_valid_utf8(node.as_string())) {
                throw Error(ErrorCode::InvalidUtf8,
                            "String value is not valid UTF-8", 0, 0, path);
            }
            break;
        case Kind::Number:
            if (!is_valid_number(node.as_number().text)) {
                throw Error(ErrorCode::InvalidNumber,
                            "Invalid JSON number: " + node.as_number().text, 0, 0, path);
            }
            break;
        case Kind::Object: {
            std::unordered_set<std::string> keys;
            for (const auto& member : node.as_object()) {
                if (!is_valid_utf8(member.key)) {
                    throw Error(ErrorCode::InvalidUtf8,
                                "Object key is not valid UTF-8", 0, 0, path);
                }
                if (!keys.insert(member.key).second) {
                    throw Error(ErrorCode::DuplicateKey,
                                "Duplicate object key: " + member.key, 0, 0, path);
                }
                validate_at(member.value, path_for_key(path, member.key), child_depth);
            }
            break;
        }
        case Kind::Array: {
            const auto& values = node.as_array();
            for (std::size_t i = 0; i < values.size(); ++i) {
                validate_at(values[i], path + "[" + std::to_string(i) + "]", child_depth);
            }
            break;
        }
        case Kind::Boolean:
        case Kind::Null:
            break;
    }
}

std::string write_at(const Node& node,
                     Formatting formatting,
                     const std::string& unit,
                     std::size_t level) {
    switch (node.kind()) {
        case Kind::String: return quote_string(node.as_string());
        case Kind::Number: return node.as_number().text;
        case Kind::Boolean: return node.as_boolean() ? "true" : "false";
        case Kind::Null: return "null";
        case Kind::Object: {
            const auto& members = node.as_object();
            if (members.empty()) return "{}";
            std::string result = "{";
            if (formatting == Formatting::Compact) {
                for (std::size_t i = 0; i < members.size(); ++i) {
                    if (i != 0) result.push_back(',');
                    result += quote_string(members[i].key);
                    result.push_back(':');
                    result += write_at(members[i].value, formatting, unit, level + 1);
                }
                result.push_back('}');
                return result;
            }
            result.push_back('\n');
            const std::string prefix = repeated_indent(unit, level + 1);
            for (std::size_t i = 0; i < members.size(); ++i) {
                if (i != 0) result += ",\n";
                result += prefix;
                result += quote_string(members[i].key);
                result += ": ";
                result += write_at(members[i].value, formatting, unit, level + 1);
            }
            result.push_back('\n');
            result += repeated_indent(unit, level);
            result.push_back('}');
            return result;
        }
        case Kind::Array: {
            const auto& values = node.as_array();
            if (values.empty()) return "[]";
            std::string result = "[";
            if (formatting == Formatting::Compact) {
                for (std::size_t i = 0; i < values.size(); ++i) {
                    if (i != 0) result.push_back(',');
                    result += write_at(values[i], formatting, unit, level + 1);
                }
                result.push_back(']');
                return result;
            }
            result.push_back('\n');
            const std::string prefix = repeated_indent(unit, level + 1);
            for (std::size_t i = 0; i < values.size(); ++i) {
                if (i != 0) result += ",\n";
                result += prefix;
                result += write_at(values[i], formatting, unit, level + 1);
            }
            result.push_back('\n');
            result += repeated_indent(unit, level);
            result.push_back(']');
            return result;
        }
    }
    return {};
}

const Node* find_node(const Node& node, NodeId id) noexcept {
    if (node.id() == id) return &node;
    if (node.kind() == Kind::Object) {
        for (const auto& member : node.as_object()) {
            if (const Node* found = find_node(member.value, id)) return found;
        }
    } else if (node.kind() == Kind::Array) {
        for (const auto& value : node.as_array()) {
            if (const Node* found = find_node(value, id)) return found;
        }
    }
    return nullptr;
}

Node* find_node(Node& node, NodeId id) noexcept {
    if (node.id() == id) return &node;
    if (node.kind() == Kind::Object) {
        for (auto& member : node.as_object()) {
            if (Node* found = find_node(member.value, id)) return found;
        }
    } else if (node.kind() == Kind::Array) {
        for (auto& value : node.as_array()) {
            if (Node* found = find_node(value, id)) return found;
        }
    }
    return nullptr;
}

std::optional<Location> find_location(const Node& node,
                                      NodeId target,
                                      const std::string& path) {
    if (node.kind() == Kind::Object) {
        for (const auto& member : node.as_object()) {
            const std::string child_path = path_for_key(path, member.key);
            if (member.value.id() == target) {
                return Location{target, node.id(), member.key, std::nullopt, child_path};
            }
            if (auto result = find_location(member.value, target, child_path)) return result;
        }
    } else if (node.kind() == Kind::Array) {
        const auto& values = node.as_array();
        for (std::size_t i = 0; i < values.size(); ++i) {
            const std::string child_path = path + "[" + std::to_string(i) + "]";
            if (values[i].id() == target) {
                return Location{target, node.id(), std::nullopt, i, child_path};
            }
            if (auto result = find_location(values[i], target, child_path)) return result;
        }
    }
    return std::nullopt;
}

std::size_t count_nodes(const Node& node) noexcept {
    std::size_t result = 1;
    if (node.kind() == Kind::Object) {
        for (const auto& member : node.as_object()) result += count_nodes(member.value);
    } else if (node.kind() == Kind::Array) {
        for (const auto& value : node.as_array()) result += count_nodes(value);
    }
    return result;
}

std::string unique_key(std::string_view base, const Node::Object& members) {
    const auto used = [&](std::string_view candidate) {
        return std::any_of(members.begin(), members.end(), [&](const Node::Member& member) {
            return member.key == candidate;
        });
    };
    if (!used(base)) return std::string(base);
    for (std::size_t number = 2; number < std::numeric_limits<std::size_t>::max(); ++number) {
        std::string candidate(base);
        candidate.push_back(' ');
        candidate += std::to_string(number);
        if (!used(candidate)) return candidate;
    }
    throw std::overflow_error("Unable to allocate a unique object key");
}

bool delete_from(Node& node, NodeId id) {
    if (node.kind() == Kind::Object) {
        auto& members = node.as_object();
        const auto direct = std::find_if(members.begin(), members.end(),
            [&](const Node::Member& member) { return member.value.id() == id; });
        if (direct != members.end()) {
            members.erase(direct);
            return true;
        }
        for (auto& member : members) {
            if (delete_from(member.value, id)) return true;
        }
    } else if (node.kind() == Kind::Array) {
        auto& values = node.as_array();
        const auto direct = std::find_if(values.begin(), values.end(),
            [&](const Node& value) { return value.id() == id; });
        if (direct != values.end()) {
            values.erase(direct);
            return true;
        }
        for (auto& value : values) {
            if (delete_from(value, id)) return true;
        }
    }
    return false;
}

std::optional<NodeId> duplicate_in(Node& node, NodeId id) {
    if (node.kind() == Kind::Object) {
        auto& members = node.as_object();
        for (std::size_t i = 0; i < members.size(); ++i) {
            if (members[i].value.id() == id) {
                Node copy = members[i].value.deep_copy();
                const NodeId copy_id = copy.id();
                const std::string key = unique_key(members[i].key, members);
                members.insert(members.begin() + static_cast<std::ptrdiff_t>(i + 1),
                               Node::Member{key, std::move(copy)});
                return copy_id;
            }
        }
        for (auto& member : members) {
            if (auto copy = duplicate_in(member.value, id)) return copy;
        }
    } else if (node.kind() == Kind::Array) {
        auto& values = node.as_array();
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (values[i].id() == id) {
                Node copy = values[i].deep_copy();
                const NodeId copy_id = copy.id();
                values.insert(values.begin() + static_cast<std::ptrdiff_t>(i + 1),
                              std::move(copy));
                return copy_id;
            }
        }
        for (auto& value : values) {
            if (auto copy = duplicate_in(value, id)) return copy;
        }
    }
    return std::nullopt;
}

bool move_in(Node& node, NodeId id, int offset) {
    if (node.kind() == Kind::Object) {
        auto& members = node.as_object();
        for (std::size_t i = 0; i < members.size(); ++i) {
            if (members[i].value.id() == id) {
                const auto target = static_cast<std::ptrdiff_t>(i) + offset;
                if (target < 0 || target >= static_cast<std::ptrdiff_t>(members.size())) return false;
                std::swap(members[i], members[static_cast<std::size_t>(target)]);
                return true;
            }
        }
        for (auto& member : members) {
            if (move_in(member.value, id, offset)) return true;
        }
    } else if (node.kind() == Kind::Array) {
        auto& values = node.as_array();
        for (std::size_t i = 0; i < values.size(); ++i) {
            if (values[i].id() == id) {
                const auto target = static_cast<std::ptrdiff_t>(i) + offset;
                if (target < 0 || target >= static_cast<std::ptrdiff_t>(values.size())) return false;
                std::swap(values[i], values[static_cast<std::size_t>(target)]);
                return true;
            }
        }
        for (auto& value : values) {
            if (move_in(value, id, offset)) return true;
        }
    }
    return false;
}

}  // namespace

Node::Node() : Node(Storage(Null{})) {}
Node::Node(Storage value) : id_(next_node_id()), value_(std::move(value)) {}
Node::Node(NodeId id, Storage value) : id_(id), value_(std::move(value)) {}

Node Node::string(std::string value) { return Node(Storage(std::move(value))); }
Node Node::number(std::string original_text) {
    return Node(Storage(Number{std::move(original_text)}));
}
Node Node::boolean(bool value) { return Node(Storage(value)); }
Node Node::null() { return Node(Storage(Null{})); }
Node Node::object(Object members) { return Node(Storage(std::move(members))); }
Node Node::array(Array values) { return Node(Storage(std::move(values))); }

Kind Node::kind() const noexcept {
    switch (value_.index()) {
        case 0: return Kind::String;
        case 1: return Kind::Number;
        case 2: return Kind::Boolean;
        case 3: return Kind::Null;
        case 4: return Kind::Object;
        case 5: return Kind::Array;
        default: return Kind::Null;
    }
}

bool Node::is_container() const noexcept {
    return kind() == Kind::Object || kind() == Kind::Array;
}

std::size_t Node::child_count() const noexcept {
    if (kind() == Kind::Object) return std::get<Object>(value_).size();
    if (kind() == Kind::Array) return std::get<Array>(value_).size();
    return 0;
}

const std::string& Node::as_string() const { return std::get<std::string>(value_); }
std::string& Node::as_string() { return std::get<std::string>(value_); }
const Number& Node::as_number() const { return std::get<Number>(value_); }
Number& Node::as_number() { return std::get<Number>(value_); }
bool Node::as_boolean() const { return std::get<bool>(value_); }
bool& Node::as_boolean() { return std::get<bool>(value_); }
const Node::Object& Node::as_object() const { return std::get<Object>(value_); }
Node::Object& Node::as_object() { return std::get<Object>(value_); }
const Node::Array& Node::as_array() const { return std::get<Array>(value_); }
Node::Array& Node::as_array() { return std::get<Array>(value_); }

Node Node::deep_copy() const {
    switch (kind()) {
        case Kind::String: return Node::string(as_string());
        case Kind::Number: return Node::number(as_number().text);
        case Kind::Boolean: return Node::boolean(as_boolean());
        case Kind::Null: return Node::null();
        case Kind::Object: {
            Object members;
            members.reserve(as_object().size());
            for (const auto& member : as_object()) {
                members.push_back(Member{member.key, member.value.deep_copy()});
            }
            return Node::object(std::move(members));
        }
        case Kind::Array: {
            Array values;
            values.reserve(as_array().size());
            for (const auto& value : as_array()) values.push_back(value.deep_copy());
            return Node::array(std::move(values));
        }
    }
    return Node::null();
}

void Node::regenerate_ids() {
    id_ = next_node_id();
    if (kind() == Kind::Object) {
        for (auto& member : as_object()) member.value.regenerate_ids();
    } else if (kind() == Kind::Array) {
        for (auto& value : as_array()) value.regenerate_ids();
    }
}

bool equivalent(const Node& lhs, const Node& rhs) {
    if (lhs.kind() != rhs.kind()) return false;
    switch (lhs.kind()) {
        case Kind::String: return lhs.as_string() == rhs.as_string();
        case Kind::Number: return lhs.as_number() == rhs.as_number();
        case Kind::Boolean: return lhs.as_boolean() == rhs.as_boolean();
        case Kind::Null: return true;
        case Kind::Object: {
            const auto& a = lhs.as_object();
            const auto& b = rhs.as_object();
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (a[i].key != b[i].key || !equivalent(a[i].value, b[i].value)) return false;
            }
            return true;
        }
        case Kind::Array: {
            const auto& a = lhs.as_array();
            const auto& b = rhs.as_array();
            if (a.size() != b.size()) return false;
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (!equivalent(a[i], b[i])) return false;
            }
            return true;
        }
    }
    return false;
}

Error::Error(ErrorCode code,
             std::string message,
             std::size_t line,
             std::size_t column,
             std::string path)
    : std::runtime_error(std::move(message)),
      code_(code), line_(line), column_(column), path_(std::move(path)) {}

bool is_valid_utf8(std::string_view text) noexcept {
    return !invalid_utf8_offset(text).has_value();
}

bool is_valid_number(std::string_view text) noexcept {
    if (text.empty()) return false;
    std::size_t i = 0;
    if (text[i] == '-') {
        ++i;
        if (i == text.size()) return false;
    }
    if (text[i] == '0') {
        ++i;
        if (i < text.size() && text[i] >= '0' && text[i] <= '9') return false;
    } else if (text[i] >= '1' && text[i] <= '9') {
        do { ++i; } while (i < text.size() && text[i] >= '0' && text[i] <= '9');
    } else {
        return false;
    }
    if (i < text.size() && text[i] == '.') {
        ++i;
        const std::size_t start = i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') ++i;
        if (i == start) return false;
    }
    if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        if (i < text.size() && (text[i] == '+' || text[i] == '-')) ++i;
        const std::size_t start = i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9') ++i;
        if (i == start) return false;
    }
    return i == text.size();
}

Node parse(std::string_view utf8_text) {
    return Parser(utf8_text).run();
}

Formatting detect_formatting(std::string_view text) noexcept {
    if (text.find('\n') == std::string_view::npos) return Formatting::Compact;
    std::size_t line_start = text.find('\n') + 1;
    while (line_start <= text.size()) {
        std::size_t i = line_start;
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
        if (i > line_start) {
            if (text[line_start] == '\t') return Formatting::Tabs;
            return i - line_start >= 4 ? Formatting::FourSpaces : Formatting::TwoSpaces;
        }
        const std::size_t next = text.find('\n', line_start);
        if (next == std::string_view::npos) break;
        line_start = next + 1;
    }
    return Formatting::TwoSpaces;
}

void validate(const Node& node, bool require_root_object) {
    if (require_root_object && node.kind() != Kind::Object) {
        throw Error(ErrorCode::RootMustBeObject, "JSON root must be an object", 0, 0, "$");
    }
    validate_at(node, "$", 0);
}

std::string write(const Node& node, WriteOptions options) {
    validate(node, options.require_root_object);
    const std::string unit = indent_unit(options.formatting);
    std::string result = write_at(node, options.formatting, unit, 0);
    if (options.trailing_newline) result.push_back('\n');
    return result;
}

Document::Document() : root_(Node::object()) {}

Document::Document(Node root, Formatting formatting, bool trailing_newline)
    : root_(std::move(root)), formatting_(formatting), trailing_newline_(trailing_newline) {
    validate(root_, true);
}

Document Document::from_json(std::string_view text) {
    Node root = parse(text);
    validate(root, true);
    const bool trailing = !text.empty() && text.back() == '\n';
    return Document(std::move(root), detect_formatting(text), trailing);
}

const Node* Document::find(NodeId id) const noexcept { return find_node(root_, id); }
Node* Document::find_mutable(NodeId id) noexcept { return find_node(root_, id); }

std::optional<Location> Document::location(NodeId id) const {
    if (root_.id() == id) return Location{id, std::nullopt, std::nullopt, std::nullopt, "$"};
    return find_location(root_, id, "$");
}

std::size_t Document::node_count() const noexcept { return count_nodes(root_); }

bool Document::is_key_available(std::string_view candidate, NodeId node_id) const {
    const auto where = location(node_id);
    if (!where || !where->parent_id || !where->key) return false;
    const Node* parent = find(*where->parent_id);
    if (!parent || parent->kind() != Kind::Object) return false;
    for (const auto& member : parent->as_object()) {
        if (member.value.id() != node_id && member.key == candidate) return false;
    }
    return true;
}

bool Document::rename_node(NodeId node_id, std::string new_key) {
    if (!is_valid_utf8(new_key) || !is_key_available(new_key, node_id)) return false;
    const auto where = location(node_id);
    Node* parent = where && where->parent_id ? find_mutable(*where->parent_id) : nullptr;
    if (!parent || parent->kind() != Kind::Object) return false;
    for (auto& member : parent->as_object()) {
        if (member.value.id() == node_id) {
            member.key = std::move(new_key);
            return true;
        }
    }
    return false;
}

bool Document::set_string(NodeId node_id, std::string value) {
    if (node_id == root_.id() || !is_valid_utf8(value)) return false;
    Node* node = find_mutable(node_id);
    if (!node) return false;
    node->value_ = std::move(value);
    return true;
}

bool Document::set_number(NodeId node_id, std::string original_text) {
    if (node_id == root_.id() || !is_valid_number(original_text)) return false;
    Node* node = find_mutable(node_id);
    if (!node) return false;
    node->value_ = Number{std::move(original_text)};
    return true;
}

bool Document::set_boolean(NodeId node_id, bool value) {
    if (node_id == root_.id()) return false;
    Node* node = find_mutable(node_id);
    if (!node) return false;
    node->value_ = value;
    return true;
}

bool Document::change_kind(NodeId node_id, Kind requested) {
    Node* node = find_mutable(node_id);
    if (!node || (node_id == root_.id() && requested != Kind::Object)) return false;
    if (node->kind() == requested) return true;
    Node previous = *node;

    switch (requested) {
        case Kind::String:
            if (node->kind() == Kind::Number) node->value_ = node->as_number().text;
            else if (node->kind() == Kind::Boolean) node->value_ = std::string(node->as_boolean() ? "true" : "false");
            else node->value_ = std::string{};
            break;
        case Kind::Number:
            if (node->kind() == Kind::String && is_valid_number(node->as_string())) {
                const std::string text = node->as_string();
                node->value_ = Number{text};
            } else if (node->kind() == Kind::Boolean) {
                node->value_ = Number{node->as_boolean() ? "1" : "0"};
            } else {
                node->value_ = Number{"0"};
            }
            break;
        case Kind::Boolean:
            if (node->kind() == Kind::String) {
                std::string lower = node->as_string();
                std::transform(lower.begin(), lower.end(), lower.begin(), [](char byte) {
                    return byte >= 'A' && byte <= 'Z'
                        ? static_cast<char>(byte - 'A' + 'a')
                        : byte;
                });
                node->value_ = (lower == "true");
            } else if (node->kind() == Kind::Number) {
                node->value_ = (node->as_number().text != "0");
            } else {
                node->value_ = false;
            }
            break;
        case Kind::Null: node->value_ = Null{}; break;
        case Kind::Object: node->value_ = Node::Object{}; break;
        case Kind::Array: node->value_ = Node::Array{}; break;
    }
    try {
        validate(root_, true);
    } catch (const Error&) {
        *node = std::move(previous);
        return false;
    }
    return true;
}

bool Document::replace_node(NodeId node_id, const Node& replacement) {
    if (node_id == root_.id() && replacement.kind() != Kind::Object) return false;
    try {
        validate(replacement, node_id == root_.id());
    } catch (const Error&) {
        return false;
    }
    Node* node = find_mutable(node_id);
    if (!node) return false;
    Node previous = *node;
    Node copy = replacement;
    copy.regenerate_ids();
    copy.id_ = node->id_;
    *node = std::move(copy);
    try {
        validate(root_, true);
    } catch (const Error&) {
        *node = std::move(previous);
        return false;
    }
    return true;
}

std::optional<NodeId> Document::add_child(std::optional<NodeId> preferred_node_id) {
    const NodeId selected = preferred_node_id.value_or(root_.id());
    NodeId container_id = root_.id();
    if (const Node* selected_node = find(selected); selected_node && selected_node->is_container()) {
        container_id = selected;
    } else if (const auto where = location(selected); where && where->parent_id) {
        container_id = *where->parent_id;
    }

    Node* container = find_mutable(container_id);
    if (!container) return std::nullopt;
    Node child = Node::string("");
    const NodeId child_id = child.id();
    if (container->kind() == Kind::Object) {
        auto& members = container->as_object();
        const std::string key = unique_key(u8"新键", members);
        members.push_back(Node::Member{key, std::move(child)});
        return child_id;
    }
    if (container->kind() == Kind::Array) {
        container->as_array().push_back(std::move(child));
        return child_id;
    }
    return std::nullopt;
}

bool Document::delete_node(NodeId node_id) {
    return node_id != root_.id() && delete_from(root_, node_id);
}

std::optional<NodeId> Document::duplicate_node(NodeId node_id) {
    if (node_id == root_.id()) return std::nullopt;
    return duplicate_in(root_, node_id);
}

bool Document::can_move(NodeId node_id, int offset) const {
    if (node_id == root_.id() || offset == 0) return false;
    const auto where = location(node_id);
    if (!where || !where->parent_id) return false;
    const Node* parent = find(*where->parent_id);
    if (!parent) return false;
    std::optional<std::size_t> current_index = where->index;
    if (!current_index && parent->kind() == Kind::Object) {
        const auto& members = parent->as_object();
        for (std::size_t i = 0; i < members.size(); ++i) {
            if (members[i].value.id() == node_id) { current_index = i; break; }
        }
    }
    if (!current_index) return false;
    const auto target = static_cast<std::ptrdiff_t>(*current_index) + offset;
    return target >= 0 && target < static_cast<std::ptrdiff_t>(parent->child_count());
}

bool Document::move_node(NodeId node_id, int offset) {
    if (!can_move(node_id, offset)) return false;
    return move_in(root_, node_id, offset);
}

bool Document::sort_object(NodeId node_id) {
    Node* node = find_mutable(node_id);
    if (!node || node->kind() != Kind::Object) return false;
    auto& members = node->as_object();
    std::stable_sort(members.begin(), members.end(), [](const Node::Member& lhs,
                                                        const Node::Member& rhs) {
        return utf8_byte_less(lhs.key, rhs.key);
    });
    return true;
}

std::string Document::encoded_text(std::optional<NodeId> node_id,
                                   std::optional<Formatting> formatting) const {
    const Node* target = node_id ? find(*node_id) : &root_;
    if (!target) return {};
    return write(*target, WriteOptions{formatting.value_or(formatting_), false, false});
}

}  // namespace jsondict
