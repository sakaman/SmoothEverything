#include "smootheverything/json.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>

namespace smootheverything {
namespace {

class Parser final {
public:
    explicit Parser(const std::string_view source) : source_(source) {}

    [[nodiscard]] JsonValue Parse() {
        SkipWhitespace();
        JsonValue result = ParseValue();
        SkipWhitespace();
        if (offset_ != source_.size()) {
            Fail("unexpected trailing content");
        }
        return result;
    }

private:
    [[noreturn]] void Fail(const std::string& message) const {
        throw JsonParseError(offset_, message);
    }

    void SkipWhitespace() noexcept {
        while (offset_ < source_.size()) {
            const char value = source_[offset_];
            if (value != ' ' && value != '\t' && value != '\r' && value != '\n') {
                break;
            }
            ++offset_;
        }
    }

    [[nodiscard]] char Peek() const noexcept {
        return offset_ < source_.size() ? source_[offset_] : '\0';
    }

    [[nodiscard]] bool Consume(const char expected) noexcept {
        if (Peek() != expected) {
            return false;
        }
        ++offset_;
        return true;
    }

    void Expect(const char expected) {
        if (!Consume(expected)) {
            Fail(std::string("expected '") + expected + "'");
        }
    }

    void ExpectLiteral(const std::string_view literal) {
        if (source_.substr(offset_, literal.size()) != literal) {
            Fail("invalid literal");
        }
        offset_ += literal.size();
    }

    [[nodiscard]] JsonValue ParseValue() {
        SkipWhitespace();
        switch (Peek()) {
            case 'n':
                ExpectLiteral("null");
                return JsonValue{};
            case 't':
                ExpectLiteral("true");
                return JsonValue{true};
            case 'f':
                ExpectLiteral("false");
                return JsonValue{false};
            case '"':
                return JsonValue{ParseString()};
            case '[':
                return ParseArray();
            case '{':
                return ParseObject();
            default:
                if (Peek() == '-' || (Peek() >= '0' && Peek() <= '9')) {
                    return JsonValue{ParseNumber()};
                }
                Fail("expected a JSON value");
        }
    }

    [[nodiscard]] JsonValue ParseArray() {
        Expect('[');
        SkipWhitespace();
        JsonValue::Array values;
        if (Consume(']')) {
            return JsonValue{std::move(values)};
        }

        for (;;) {
            values.push_back(ParseValue());
            SkipWhitespace();
            if (Consume(']')) {
                break;
            }
            Expect(',');
            SkipWhitespace();
        }
        return JsonValue{std::move(values)};
    }

    [[nodiscard]] JsonValue ParseObject() {
        Expect('{');
        SkipWhitespace();
        JsonValue::Object values;
        if (Consume('}')) {
            return JsonValue{std::move(values)};
        }

        for (;;) {
            if (Peek() != '"') {
                Fail("expected an object key");
            }
            std::string key = ParseString();
            SkipWhitespace();
            Expect(':');
            SkipWhitespace();
            auto [iterator, inserted] = values.emplace(std::move(key), ParseValue());
            if (!inserted) {
                Fail("duplicate object key");
            }
            static_cast<void>(iterator);
            SkipWhitespace();
            if (Consume('}')) {
                break;
            }
            Expect(',');
            SkipWhitespace();
        }
        return JsonValue{std::move(values)};
    }

    static void AppendUtf8(std::string& output, const std::uint32_t code_point) {
        if (code_point <= 0x7F) {
            output.push_back(static_cast<char>(code_point));
        } else if (code_point <= 0x7FF) {
            output.push_back(static_cast<char>(0xC0U | (code_point >> 6U)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        } else if (code_point <= 0xFFFF) {
            output.push_back(static_cast<char>(0xE0U | (code_point >> 12U)));
            output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        } else {
            output.push_back(static_cast<char>(0xF0U | (code_point >> 18U)));
            output.push_back(static_cast<char>(0x80U | ((code_point >> 12U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | ((code_point >> 6U) & 0x3FU)));
            output.push_back(static_cast<char>(0x80U | (code_point & 0x3FU)));
        }
    }

    [[nodiscard]] std::uint32_t ParseHexQuad() {
        if (offset_ + 4 > source_.size()) {
            Fail("incomplete unicode escape");
        }
        std::uint32_t result = 0;
        for (int index = 0; index < 4; ++index) {
            const char value = source_[offset_++];
            result <<= 4U;
            if (value >= '0' && value <= '9') {
                result |= static_cast<std::uint32_t>(value - '0');
            } else if (value >= 'a' && value <= 'f') {
                result |= static_cast<std::uint32_t>(value - 'a' + 10);
            } else if (value >= 'A' && value <= 'F') {
                result |= static_cast<std::uint32_t>(value - 'A' + 10);
            } else {
                Fail("invalid unicode escape");
            }
        }
        return result;
    }

    [[nodiscard]] std::string ParseString() {
        Expect('"');
        std::string output;
        while (offset_ < source_.size()) {
            const unsigned char value = static_cast<unsigned char>(source_[offset_++]);
            if (value == '"') {
                return output;
            }
            if (value < 0x20U) {
                Fail("unescaped control character in string");
            }
            if (value != '\\') {
                output.push_back(static_cast<char>(value));
                continue;
            }

            if (offset_ >= source_.size()) {
                Fail("incomplete escape sequence");
            }
            const char escape = source_[offset_++];
            switch (escape) {
                case '"': output.push_back('"'); break;
                case '\\': output.push_back('\\'); break;
                case '/': output.push_back('/'); break;
                case 'b': output.push_back('\b'); break;
                case 'f': output.push_back('\f'); break;
                case 'n': output.push_back('\n'); break;
                case 'r': output.push_back('\r'); break;
                case 't': output.push_back('\t'); break;
                case 'u': {
                    std::uint32_t code_point = ParseHexQuad();
                    if (code_point >= 0xD800U && code_point <= 0xDBFFU) {
                        if (offset_ + 2 > source_.size() || source_[offset_] != '\\' || source_[offset_ + 1] != 'u') {
                            Fail("high surrogate without low surrogate");
                        }
                        offset_ += 2;
                        const std::uint32_t low = ParseHexQuad();
                        if (low < 0xDC00U || low > 0xDFFFU) {
                            Fail("invalid low surrogate");
                        }
                        code_point = 0x10000U + ((code_point - 0xD800U) << 10U) + (low - 0xDC00U);
                    } else if (code_point >= 0xDC00U && code_point <= 0xDFFFU) {
                        Fail("unexpected low surrogate");
                    }
                    AppendUtf8(output, code_point);
                    break;
                }
                default:
                    Fail("invalid escape sequence");
            }
        }
        Fail("unterminated string");
    }

    [[nodiscard]] double ParseNumber() {
        const std::size_t start = offset_;
        static_cast<void>(Consume('-'));
        if (Consume('0')) {
            if (Peek() >= '0' && Peek() <= '9') {
                Fail("leading zero in number");
            }
        } else {
            if (Peek() < '1' || Peek() > '9') {
                Fail("invalid number");
            }
            while (Peek() >= '0' && Peek() <= '9') {
                ++offset_;
            }
        }

        if (Consume('.')) {
            if (Peek() < '0' || Peek() > '9') {
                Fail("fraction requires a digit");
            }
            while (Peek() >= '0' && Peek() <= '9') {
                ++offset_;
            }
        }

        if (Peek() == 'e' || Peek() == 'E') {
            ++offset_;
            if (Peek() == '+' || Peek() == '-') {
                ++offset_;
            }
            if (Peek() < '0' || Peek() > '9') {
                Fail("exponent requires a digit");
            }
            while (Peek() >= '0' && Peek() <= '9') {
                ++offset_;
            }
        }

        double result = 0.0;
        const auto number = source_.substr(start, offset_ - start);
        const auto conversion = std::from_chars(number.data(), number.data() + number.size(), result);
        if (conversion.ec != std::errc{} || conversion.ptr != number.data() + number.size() || !std::isfinite(result)) {
            Fail("number is outside the supported range");
        }
        return result;
    }

    std::string_view source_;
    std::size_t offset_{0};
};

void AppendIndent(std::string& output, const int depth) {
    output.append(static_cast<std::size_t>(depth * 2), ' ');
}

void AppendEscapedString(std::string& output, const std::string_view value) {
    static constexpr char HexDigits[] = "0123456789abcdef";
    output.push_back('"');
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        switch (character) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (character < 0x20U) {
                    output += "\\u00";
                    output.push_back(HexDigits[(character >> 4U) & 0x0FU]);
                    output.push_back(HexDigits[character & 0x0FU]);
                } else {
                    output.push_back(static_cast<char>(character));
                }
                break;
        }
    }
    output.push_back('"');
}

void AppendJson(std::string& output, const JsonValue& value, const bool pretty, const int depth) {
    if (value.IsNull()) {
        output += "null";
    } else if (value.IsBoolean()) {
        output += value.AsBoolean() ? "true" : "false";
    } else if (value.IsNumber()) {
        std::ostringstream stream;
        stream << std::setprecision(std::numeric_limits<double>::max_digits10) << value.AsNumber();
        output += stream.str();
    } else if (value.IsString()) {
        AppendEscapedString(output, value.AsString());
    } else if (value.IsArray()) {
        const auto& array = value.AsArray();
        output.push_back('[');
        for (std::size_t index = 0; index < array.size(); ++index) {
            if (index != 0) {
                output.push_back(',');
            }
            if (pretty) {
                output.push_back('\n');
                AppendIndent(output, depth + 1);
            }
            AppendJson(output, array[index], pretty, depth + 1);
        }
        if (pretty && !array.empty()) {
            output.push_back('\n');
            AppendIndent(output, depth);
        }
        output.push_back(']');
    } else {
        const auto& object = value.AsObject();
        output.push_back('{');
        std::size_t index = 0;
        for (const auto& [key, child] : object) {
            if (index++ != 0) {
                output.push_back(',');
            }
            if (pretty) {
                output.push_back('\n');
                AppendIndent(output, depth + 1);
            }
            AppendEscapedString(output, key);
            output += pretty ? ": " : ":";
            AppendJson(output, child, pretty, depth + 1);
        }
        if (pretty && !object.empty()) {
            output.push_back('\n');
            AppendIndent(output, depth);
        }
        output.push_back('}');
    }
}

}  // namespace

JsonParseError::JsonParseError(const std::size_t offset, std::string message)
    : std::runtime_error(std::move(message)), offset_(offset) {}

std::size_t JsonParseError::Offset() const noexcept {
    return offset_;
}

JsonValue::JsonValue() noexcept : storage_(nullptr) {}
JsonValue::JsonValue(std::nullptr_t) noexcept : storage_(nullptr) {}
JsonValue::JsonValue(const bool value) noexcept : storage_(value) {}
JsonValue::JsonValue(const double value) noexcept : storage_(value) {}
JsonValue::JsonValue(const int value) noexcept : storage_(static_cast<double>(value)) {}
JsonValue::JsonValue(std::string value) : storage_(std::move(value)) {}
JsonValue::JsonValue(const char* value) : storage_(std::string(value)) {}
JsonValue::JsonValue(Array value) : storage_(std::move(value)) {}
JsonValue::JsonValue(Object value) : storage_(std::move(value)) {}

bool JsonValue::IsNull() const noexcept { return std::holds_alternative<std::nullptr_t>(storage_); }
bool JsonValue::IsBoolean() const noexcept { return std::holds_alternative<bool>(storage_); }
bool JsonValue::IsNumber() const noexcept { return std::holds_alternative<double>(storage_); }
bool JsonValue::IsString() const noexcept { return std::holds_alternative<std::string>(storage_); }
bool JsonValue::IsArray() const noexcept { return std::holds_alternative<Array>(storage_); }
bool JsonValue::IsObject() const noexcept { return std::holds_alternative<Object>(storage_); }

bool JsonValue::AsBoolean() const { return std::get<bool>(storage_); }
double JsonValue::AsNumber() const { return std::get<double>(storage_); }
const std::string& JsonValue::AsString() const { return std::get<std::string>(storage_); }
const JsonValue::Array& JsonValue::AsArray() const { return std::get<Array>(storage_); }
JsonValue::Array& JsonValue::AsArray() { return std::get<Array>(storage_); }
const JsonValue::Object& JsonValue::AsObject() const { return std::get<Object>(storage_); }
JsonValue::Object& JsonValue::AsObject() { return std::get<Object>(storage_); }

const JsonValue* JsonValue::Find(const std::string_view key) const noexcept {
    if (!IsObject()) {
        return nullptr;
    }
    const auto iterator = AsObject().find(key);
    return iterator == AsObject().end() ? nullptr : &iterator->second;
}

JsonValue* JsonValue::Find(const std::string_view key) noexcept {
    if (!IsObject()) {
        return nullptr;
    }
    const auto iterator = AsObject().find(key);
    return iterator == AsObject().end() ? nullptr : &iterator->second;
}

JsonValue ParseJson(const std::string_view source) {
    return Parser(source).Parse();
}

std::string SerializeJson(const JsonValue& value, const bool pretty) {
    std::string output;
    AppendJson(output, value, pretty, 0);
    if (pretty) {
        output.push_back('\n');
    }
    return output;
}

}  // namespace smootheverything
