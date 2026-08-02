#include "smootheverything/control_panel/localization.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void Require(const bool condition, const std::string_view message) {
    if (!condition) {
        throw std::runtime_error(std::string(message));
    }
}

}  // namespace

int main() {
    try {
        const smootheverything::control_panel::Localizer english("en");
        Require(
            english.Language() == smootheverything::control_panel::UiLanguage::English,
            "English preference");
        Require(english.Translate(L"Home") == L"Home", "English source text");

        const smootheverything::control_panel::Localizer chinese("zh-CN");
        Require(
            chinese.Language() == smootheverything::control_panel::UiLanguage::SimplifiedChinese,
            "Simplified Chinese preference");
        Require(chinese.Translate(L"Home") == L"主页", "Simplified Chinese translation");
        Require(
            chinese.Translate(L"Untranslated future text") == L"Untranslated future text",
            "English fallback");
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "3 localization checks passed\n";
    return EXIT_SUCCESS;
}
