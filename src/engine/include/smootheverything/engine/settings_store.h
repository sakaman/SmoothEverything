#pragma once

#include "smootheverything/settings.h"

#include <string>

namespace smootheverything::engine {

struct SettingsLoadResult final {
    AppSettings settings{};
    bool found{false};
    bool valid{true};
    std::string error;
};

class SettingsStore final {
public:
    explicit SettingsStore(std::wstring path);

    [[nodiscard]] static std::wstring DefaultPath();
    [[nodiscard]] const std::wstring& Path() const noexcept;
    [[nodiscard]] SettingsLoadResult Load() const noexcept;
    [[nodiscard]] bool Save(const AppSettings& settings, std::string& error) const noexcept;

private:
    std::wstring path_;
};

}  // namespace smootheverything::engine
