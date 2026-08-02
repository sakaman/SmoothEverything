#include "smootheverything/control_panel/single_instance.h"

#include <windows.h>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
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
        const std::wstring mutex_name =
            L"Local\\SmoothEverything.ControlPanel.Tests." +
            std::to_wstring(GetCurrentProcessId());

        const smootheverything::control_panel::SingleInstance first(mutex_name.c_str());
        Require(first.Valid(), "first instance mutex");
        Require(!first.AlreadyRunning(), "first instance is primary");

        const smootheverything::control_panel::SingleInstance second(mutex_name.c_str());
        Require(second.Valid(), "second instance mutex");
        Require(second.AlreadyRunning(), "second instance detects the first");
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "4 single-instance checks passed\n";
    return EXIT_SUCCESS;
}
