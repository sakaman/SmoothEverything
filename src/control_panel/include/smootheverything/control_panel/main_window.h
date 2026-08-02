#pragma once

#include "smootheverything/control_panel/settings_client.h"

#include <windows.h>

#include <array>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace smootheverything::control_panel {

class MainWindow final {
public:
    explicit MainWindow(HINSTANCE instance);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    [[nodiscard]] bool Create(int show_command);
    [[nodiscard]] HWND Handle() const noexcept;

private:
    enum class Page : std::size_t {
        Home,
        Applications,
        Advanced,
        Diagnostics,
        Count,
    };

    static LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
    LRESULT HandleMessage(UINT message, WPARAM wparam, LPARAM lparam);

    void CreateUi();
    void CreateNavigation();
    void CreateHomePage();
    void CreateApplicationsPage();
    void CreateAdvancedPage();
    void CreateDiagnosticsPage();
    void ApplyLocalization();
    void RecreateFonts();
    void ApplyFonts();
    void ApplyWindowAppearance();
    void Layout();
    void LayoutHome(int x, int y, int width, int height);
    void LayoutApplications(int x, int y, int width, int height);
    void LayoutAdvanced(int x, int y, int width, int height);
    void LayoutDiagnostics(int x, int y, int width, int height);
    void ShowPage(Page page);

    HWND CreateLabel(
        Page page,
        int identifier,
        std::wstring_view text,
        HFONT font,
        bool card_child = false,
        DWORD style = SS_LEFT | SS_NOPREFIX);
    HWND CreateButton(
        Page page,
        int identifier,
        std::wstring_view text,
        bool card_child = false,
        DWORD style = BS_PUSHBUTTON);
    HWND CreateCheckbox(
        Page page,
        int identifier,
        std::wstring_view text,
        bool card_child = true);
    HWND CreateCard(Page page, int identifier);
    void AddPageControl(Page page, HWND control, bool card_child = false);
    [[nodiscard]] HWND Control(int identifier) const noexcept;
    void SetBounds(HWND control, int x, int y, int width, int height) const;
    [[nodiscard]] int Scale(int value) const noexcept;

    void SyncAllControls();
    void SyncHomeControls();
    void SyncApplicationLists();
    void SyncProfileEditor();
    void SyncAdvancedControls();
    void SyncLanguageControl();
    void SyncDiagnostics();
    void UpdateStatus();
    [[nodiscard]] std::wstring_view Text(std::wstring_view english) const noexcept;
    [[nodiscard]] std::wstring_view LocalizedStatus(SessionStatus status) const noexcept;
    void UpdateMotionLabels();
    void ScheduleApply();
    void ApplyPendingChanges();
    void BeginEngineConnection();
    void PollEngineConnection();
    void HandleCommand(int identifier, int notification, HWND source);
    void HandleSlider(HWND slider);
    void ApplyPreset(int identifier);
    void AddExcludedApplication();
    void RemoveExcludedApplication();
    void AddProfile();
    void RemoveProfile();
    void UpdateProfileFromEditor(int identifier);
    void BrowseExecutable(HWND target_edit);
    [[nodiscard]] AppProfile* SelectedProfile();
    [[nodiscard]] const AppProfile* SelectedProfile() const;

    void Paint();
    void DrawOwnerControl(const DRAWITEMSTRUCT& item);
    void DrawNavigationButton(const DRAWITEMSTRUCT& item);
    void DrawCard(const DRAWITEMSTRUCT& item);

    HINSTANCE instance_{};
    HWND window_{};
    UINT dpi_{96};
    Page current_page_{Page::Home};
    Localizer localizer_{};
    SettingsClient client_;
    bool updating_{};
    bool engine_started_{};
    int connection_attempts_{};

    HFONT font_body_{};
    HFONT font_small_{};
    HFONT font_title_{};
    HFONT font_section_{};
    HFONT font_value_{};
    HFONT font_mono_{};
    HBRUSH brush_background_{};
    HBRUSH brush_navigation_{};
    HBRUSH brush_card_{};
    HBRUSH brush_edit_{};
    HICON icon_{};

    std::array<HWND, 4> navigation_buttons_{};
    std::array<std::vector<HWND>, static_cast<std::size_t>(Page::Count)> page_controls_{};
    std::unordered_set<HWND> card_children_;
    std::unordered_set<HWND> cards_;
    std::unordered_map<int, HWND> controls_;
    HWND status_text_{};

    HWND home_enabled_{};
    std::array<HWND, 5> home_sliders_{};
    std::array<HWND, 5> home_values_{};
    HWND home_easing_{};
    std::array<HWND, 4> preset_buttons_{};
    std::array<HWND, 2> home_cards_{};

    HWND excluded_edit_{};
    HWND excluded_list_{};
    HWND profile_edit_{};
    HWND profile_list_{};
    HWND profile_enabled_{};
    HWND profile_compatibility_{};
    HWND profile_easing_{};
    std::array<HWND, 5> profile_motion_edits_{};
    std::array<HWND, 2> application_cards_{};
    std::vector<HWND> profile_editor_controls_;

    std::array<HWND, 8> advanced_checks_{};
    std::array<HWND, 4> advanced_cards_{};
    HWND language_combo_{};

    HWND diagnostics_status_title_{};
    HWND diagnostics_status_detail_{};
    std::array<HWND, 9> diagnostics_values_{};
    std::array<HWND, 11> diagnostics_cards_{};
    HWND diagnostics_path_{};
    HWND diagnostics_error_{};
};

}  // namespace smootheverything::control_panel
