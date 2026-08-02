#pragma once

#include <string_view>

namespace smootheverything::control_panel {

enum class UiLanguage {
    English,
    SimplifiedChinese,
};

class Localizer final {
public:
    explicit Localizer(std::string_view preference = "system") noexcept;

    void SetPreference(std::string_view preference) noexcept;
    [[nodiscard]] UiLanguage Language() const noexcept;
    [[nodiscard]] std::wstring_view Translate(std::wstring_view english) const noexcept;

private:
    UiLanguage language_{UiLanguage::English};
};

}  // namespace smootheverything::control_panel
