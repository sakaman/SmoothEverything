#include "smootheverything/control_panel/main_window.h"
#include "smootheverything/control_panel/single_instance.h"

#include <windows.h>
#include <commctrl.h>
#include <objbase.h>

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, const int show_command) {
    static_cast<void>(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2));

    const smootheverything::control_panel::SingleInstance single_instance;
    if (single_instance.AlreadyRunning()) {
        static_cast<void>(
            smootheverything::control_panel::ActivateExistingControlPanelWindow());
        return 0;
    }

    INITCOMMONCONTROLSEX controls{
        .dwSize = sizeof(INITCOMMONCONTROLSEX),
        .dwICC = ICC_STANDARD_CLASSES | ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES,
    };
    if (!InitCommonControlsEx(&controls)) {
        return 1;
    }

    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    smootheverything::control_panel::MainWindow window(instance);
    if (!window.Create(show_command)) {
        if (SUCCEEDED(com)) {
            CoUninitialize();
        }
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(window.Handle(), &message)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
    if (SUCCEEDED(com)) {
        CoUninitialize();
    }
    return static_cast<int>(message.wParam);
}
