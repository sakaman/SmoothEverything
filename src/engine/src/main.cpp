#include "smootheverything/engine/engine_app.h"
#include "smootheverything/engine/settings_store.h"

#include <windows.h>

#include <utility>

int WINAPI wWinMain(const HINSTANCE instance, HINSTANCE, const PWSTR command_line, int) {
    using namespace smootheverything::engine;

    SettingsStore store(SettingsStore::DefaultPath());
    const SettingsLoadResult loaded = store.Load();
    const bool sync_autostart = command_line == nullptr
        || wcsstr(command_line, L"--no-autostart-sync") == nullptr;
    EngineApp app(instance, std::move(store), loaded.settings, loaded.error, sync_autostart);
    return app.Run();
}
