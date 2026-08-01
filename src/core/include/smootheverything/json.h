#pragma once

#include <cstddef>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace smootheverything {

class JsonParseError final : public std::runtime_error {
public:
    JsonParseError(std::size_t offset, std::string message);

    [[nodiscard]] std::size_t Offset() const noexcept;

private:
    std::size_t offset_;
};

class JsonValue final {
public:
    using Array = std::vector<JsonValue>;
    using Object = std::map<std::string, JsonValue, std::less<>>;

    JsonValue() noexcept;
    JsonValue(std::nullptr_t) noexcept;
    JsonValue(bool value) noexcept;
    JsonValue(double value) noexcept;
    JsonValue(int value) noexcept;
    JsonValue(std::string value);
    JsonValue(const char* value);
    JsonValue(Array value);
    JsonValue(Object value);

    [[nodiscard]] bool IsNull() const noexcept;
    [[nodiscard]] bool IsBoolean() const noexcept;
    [[nodiscard]] bool IsNumber() const noexcept;
    [[nodiscard]] bool IsString() const noexcept;
    [[nodiscard]] bool IsArray() const noexcept;
    [[nodiscard]] bool IsObject() const noexcept;

    [[nodiscard]] bool AsBoolean() const;
    [[nodiscard]] double AsNumber() const;
    [[nodiscard]] const std::string& AsString() const;
    [[nodiscard]] const Array& AsArray() const;
    [[nodiscard]] Array& AsArray();
    [[nodiscard]] const Object& AsObject() const;
    [[nodiscard]] Object& AsObject();

    [[nodiscard]] const JsonValue* Find(std::string_view key) const noexcept;
    [[nodiscard]] JsonValue* Find(std::string_view key) noexcept;

private:
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, Array, Object>;
    Storage storage_;
};

[[nodiscard]] JsonValue ParseJson(std::string_view source);
[[nodiscard]] std::string SerializeJson(const JsonValue& value, bool pretty = true);

}  // namespace smootheverything
