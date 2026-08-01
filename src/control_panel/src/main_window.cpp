#include "smootheverything/control_panel/main_window.h"

#include "smootheverything/control_panel/resource.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <dwmapi.h>
#include <uxtheme.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>

namespace smootheverything::control_panel {
namespace {

constexpr wchar_t kWindowClass[] = L"SmoothEverything.NativeControlPanel";
constexpr wchar_t kWindowTitle[] = L"SmoothEverything";
constexpr UINT kConnectMessage = WM_APP + 1U;
constexpr UINT_PTR kApplyTimer = 1U;
constexpr UINT_PTR kConnectTimer = 2U;

constexpr COLORREF kBackground = RGB(248, 247, 246);
constexpr COLORREF kNavigation = RGB(243, 242, 241);
constexpr COLORREF kCard = RGB(255, 255, 255);
constexpr COLORREF kText = RGB(32, 31, 30);
constexpr COLORREF kSecondaryText = RGB(96, 94, 92);
constexpr COLORREF kBorder = RGB(229, 227, 224);
constexpr COLORREF kAccent = RGB(0, 103, 192);
constexpr COLORREF kAccentPale = RGB(224, 239, 252);
constexpr COLORREF kOnline = RGB(16, 124, 16);
constexpr COLORREF kOffline = RGB(202, 80, 16);

enum ControlId : int {
    NavHome = 100,
    NavApplications,
    NavAdvanced,
    NavDiagnostics,
    StatusText,

    HomeTitle = 200,
    HomeDescription,
    HomeGlobalCard,
    HomeGlobalTitle,
    HomeGlobalDescription,
    HomeEnabled,
    HomePresetTitle,
    HomePresetResponsive,
    HomePresetBalanced,
    HomePresetSmooth,
    HomePresetClassic,
    HomeMotionCard,
    HomeMotionTitle,
    HomeMotionDescription,
    HomeMotionLabel0,
    HomeMotionLabel1,
    HomeMotionLabel2,
    HomeMotionLabel3,
    HomeMotionLabel4,
    HomeMotionValue0,
    HomeMotionValue1,
    HomeMotionValue2,
    HomeMotionValue3,
    HomeMotionValue4,
    HomeMotionSlider0,
    HomeMotionSlider1,
    HomeMotionSlider2,
    HomeMotionSlider3,
    HomeMotionSlider4,
    HomeEasing,

    ApplicationsTitle = 300,
    ApplicationsDescription,
    ExcludedCard,
    ExcludedTitle,
    ExcludedDescription,
    ExcludedEdit,
    ExcludedBrowse,
    ExcludedAdd,
    ExcludedList,
    ExcludedRemove,
    ProfileCard,
    ProfileTitle,
    ProfileDescription,
    ProfileEdit,
    ProfileBrowse,
    ProfileAdd,
    ProfileList,
    ProfileRemove,
    ProfileEditorTitle,
    ProfileEnabled,
    ProfileCompatibility,
    ProfileEasing,
    ProfileMotionLabel0,
    ProfileMotionLabel1,
    ProfileMotionLabel2,
    ProfileMotionLabel3,
    ProfileMotionLabel4,
    ProfileMotionEdit0,
    ProfileMotionEdit1,
    ProfileMotionEdit2,
    ProfileMotionEdit3,
    ProfileMotionEdit4,

    AdvancedTitle = 400,
    AdvancedDescription,
    AdvancedCard0,
    AdvancedCard1,
    AdvancedCard2,
    AdvancedCard3,
    AdvancedCardTitle0,
    AdvancedCardTitle1,
    AdvancedCardTitle2,
    AdvancedCardTitle3,
    AdvancedCardDescription0,
    AdvancedCardDescription1,
    AdvancedCardDescription2,
    AdvancedCardDescription3,
    AdvancedHorizontal,
    AdvancedShiftHorizontal,
    AdvancedCtrl,
    AdvancedAlt,
    AdvancedHighResolution,
    AdvancedReverse,
    AdvancedStartup,
    AdvancedTray,

    DiagnosticsTitle = 500,
    DiagnosticsDescription,
    DiagnosticsStart,
    DiagnosticsRefresh,
    DiagnosticsStatusCard,
    DiagnosticsStatusTitle,
    DiagnosticsStatusDetail,
    DiagnosticsStatCard0,
    DiagnosticsStatCard1,
    DiagnosticsStatCard2,
    DiagnosticsStatCard3,
    DiagnosticsStatCard4,
    DiagnosticsStatCard5,
    DiagnosticsStatCard6,
    DiagnosticsStatCard7,
    DiagnosticsStatCard8,
    DiagnosticsStatLabel0,
    DiagnosticsStatLabel1,
    DiagnosticsStatLabel2,
    DiagnosticsStatLabel3,
    DiagnosticsStatLabel4,
    DiagnosticsStatLabel5,
    DiagnosticsStatLabel6,
    DiagnosticsStatLabel7,
    DiagnosticsStatLabel8,
    DiagnosticsStatValue0,
    DiagnosticsStatValue1,
    DiagnosticsStatValue2,
    DiagnosticsStatValue3,
    DiagnosticsStatValue4,
    DiagnosticsStatValue5,
    DiagnosticsStatValue6,
    DiagnosticsStatValue7,
    DiagnosticsStatValue8,
    DiagnosticsInfoCard,
    DiagnosticsPathLabel,
    DiagnosticsPath,
    DiagnosticsErrorLabel,
    DiagnosticsError,
};

[[nodiscard]] std::wstring FormatNumber(const double value, const int precision) {
    std::wostringstream stream;
    stream << std::fixed << std::setprecision(precision) << value;
    return stream.str();
}

[[nodiscard]] std::wstring WindowText(const HWND control) {
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) {
        return {};
    }
    std::wstring value(static_cast<std::size_t>(length + 1), L'\0');
    const int copied = GetWindowTextW(control, value.data(), length + 1);
    value.resize(static_cast<std::size_t>(std::max(copied, 0)));
    return value;
}

[[nodiscard]] bool ContainsExecutable(
    const std::vector<std::string>& values,
    const std::string_view executable) {
    return std::find(values.begin(), values.end(), executable) != values.end();
}

void SetCheck(const HWND control, const bool checked) {
    SendMessageW(control, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

[[nodiscard]] bool IsChecked(const HWND control) {
    return SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

}  // namespace

MainWindow::MainWindow(const HINSTANCE instance) : instance_(instance) {}

MainWindow::~MainWindow() {
    for (const HFONT font : {font_body_, font_small_, font_title_, font_section_, font_value_, font_mono_}) {
        if (font != nullptr) {
            DeleteObject(font);
        }
    }
    for (const HBRUSH brush : {brush_background_, brush_navigation_, brush_card_, brush_edit_}) {
        if (brush != nullptr) {
            DeleteObject(brush);
        }
    }
    if (icon_ != nullptr) {
        DestroyIcon(icon_);
    }
}

bool MainWindow::Create(const int show_command) {
    icon_ = static_cast<HICON>(LoadImageW(
        instance_, MAKEINTRESOURCEW(IDI_SMOOTHEVERYTHING), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(WNDCLASSEXW);
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    window_class.lpfnWndProc = &MainWindow::WindowProcedure;
    window_class.hInstance = instance_;
    window_class.hIcon = icon_;
    window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    window_class.lpszClassName = kWindowClass;
    window_class.hIconSm = icon_;
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        return false;
    }

    dpi_ = GetDpiForSystem();
    RECT bounds{0, 0, Scale(1080), Scale(780)};
    static_cast<void>(AdjustWindowRectExForDpi(
        &bounds, WS_OVERLAPPEDWINDOW, FALSE, 0, dpi_));
    window_ = CreateWindowExW(
        0,
        kWindowClass,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        bounds.right - bounds.left,
        bounds.bottom - bounds.top,
        nullptr,
        nullptr,
        instance_,
        this);
    if (window_ == nullptr) {
        return false;
    }
    ShowWindow(window_, show_command);
    UpdateWindow(window_);
    return true;
}

HWND MainWindow::Handle() const noexcept {
    return window_;
}

LRESULT CALLBACK MainWindow::WindowProcedure(
    const HWND window,
    const UINT message,
    const WPARAM wparam,
    const LPARAM lparam) {
    MainWindow* self = reinterpret_cast<MainWindow*>(
        GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lparam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self != nullptr
        ? self->HandleMessage(message, wparam, lparam)
        : DefWindowProcW(window, message, wparam, lparam);
}

LRESULT MainWindow::HandleMessage(
    const UINT message,
    const WPARAM wparam,
    const LPARAM lparam) {
    switch (message) {
    case WM_CREATE:
        dpi_ = GetDpiForWindow(window_);
        CreateUi();
        ApplyWindowAppearance();
        PostMessageW(window_, kConnectMessage, 0, 0);
        return 0;
    case WM_SIZE:
        Layout();
        return 0;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lparam);
        info->ptMinTrackSize.x = Scale(900);
        info->ptMinTrackSize.y = Scale(660);
        return 0;
    }
    case WM_DPICHANGED: {
        dpi_ = HIWORD(wparam);
        const auto* suggested = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(
            window_,
            nullptr,
            suggested->left,
            suggested->top,
            suggested->right - suggested->left,
            suggested->bottom - suggested->top,
            SWP_NOACTIVATE | SWP_NOZORDER);
        RecreateFonts();
        ApplyFonts();
        Layout();
        return 0;
    }
    case WM_COMMAND:
        HandleCommand(LOWORD(wparam), HIWORD(wparam), reinterpret_cast<HWND>(lparam));
        return 0;
    case WM_HSCROLL:
        HandleSlider(reinterpret_cast<HWND>(lparam));
        return 0;
    case WM_TIMER:
        if (wparam == kApplyTimer) {
            KillTimer(window_, kApplyTimer);
            ApplyPendingChanges();
        } else if (wparam == kConnectTimer) {
            PollEngineConnection();
        }
        return 0;
    case kConnectMessage:
        BeginEngineConnection();
        return 0;
    case WM_DRAWITEM:
        DrawOwnerControl(*reinterpret_cast<const DRAWITEMSTRUCT*>(lparam));
        return TRUE;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN: {
        const HDC dc = reinterpret_cast<HDC>(wparam);
        const HWND control = reinterpret_cast<HWND>(lparam);
        SetTextColor(dc, kText);
        SetBkMode(dc, OPAQUE);
        if (card_children_.contains(control)) {
            SetBkColor(dc, kCard);
            return reinterpret_cast<LRESULT>(brush_card_);
        }
        SetBkColor(dc, kBackground);
        return reinterpret_cast<LRESULT>(brush_background_);
    }
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        const HDC dc = reinterpret_cast<HDC>(wparam);
        SetTextColor(dc, kText);
        SetBkColor(dc, kCard);
        return reinterpret_cast<LRESULT>(brush_edit_);
    }
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT:
        Paint();
        return 0;
    case WM_CLOSE:
        DestroyWindow(window_);
        return 0;
    case WM_DESTROY:
        KillTimer(window_, kApplyTimer);
        KillTimer(window_, kConnectTimer);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window_, message, wparam, lparam);
    }
}

void MainWindow::CreateUi() {
    brush_background_ = CreateSolidBrush(kBackground);
    brush_navigation_ = CreateSolidBrush(kNavigation);
    brush_card_ = CreateSolidBrush(kCard);
    brush_edit_ = CreateSolidBrush(kCard);
    RecreateFonts();
    CreateNavigation();
    CreateHomePage();
    CreateApplicationsPage();
    CreateAdvancedPage();
    CreateDiagnosticsPage();
    ApplyFonts();
    ShowPage(Page::Home);
    SyncAllControls();
}

void MainWindow::CreateNavigation() {
    constexpr std::array<std::wstring_view, 4> labels{
        L"主页", L"应用规则", L"高级", L"诊断"};
    constexpr std::array<int, 4> identifiers{
        NavHome, NavApplications, NavAdvanced, NavDiagnostics};
    for (std::size_t index = 0; index < labels.size(); ++index) {
        navigation_buttons_[index] = CreateWindowExW(
            0,
            L"BUTTON",
            std::wstring(labels[index]).c_str(),
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
            0,
            0,
            0,
            0,
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifiers[index])),
            instance_,
            nullptr);
        controls_[identifiers[index]] = navigation_buttons_[index];
    }
    status_text_ = CreateWindowExW(
        0,
        L"STATIC",
        L"正在连接引擎…",
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_NOPREFIX,
        0,
        0,
        0,
        0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(StatusText)),
        instance_,
        nullptr);
    controls_[StatusText] = status_text_;
}

void MainWindow::CreateHomePage() {
    CreateLabel(Page::Home, HomeTitle, L"滚动体验", font_title_);
    CreateLabel(
        Page::Home,
        HomeDescription,
        L"为传统滚轮加入连续、低延迟的原生平滑滚动。",
        font_body_);

    home_cards_[0] = CreateCard(Page::Home, HomeGlobalCard);
    CreateLabel(Page::Home, HomeGlobalTitle, L"全局平滑", font_section_, true);
    CreateLabel(
        Page::Home,
        HomeGlobalDescription,
        L"暂停时，滚轮事件会原样交还给目标应用，不改变系统输入。",
        font_body_,
        true);
    home_enabled_ = CreateCheckbox(Page::Home, HomeEnabled, L"运行中");

    CreateLabel(Page::Home, HomePresetTitle, L"手感预设", font_section_);
    constexpr std::array<int, 4> preset_ids{
        HomePresetResponsive, HomePresetBalanced, HomePresetSmooth, HomePresetClassic};
    constexpr std::array<std::wstring_view, 4> preset_labels{
        L"响应优先", L"均衡", L"丝滑长尾", L"经典手感"};
    for (std::size_t index = 0; index < preset_ids.size(); ++index) {
        preset_buttons_[index] = CreateButton(
            Page::Home, preset_ids[index], preset_labels[index], false);
    }

    home_cards_[1] = CreateCard(Page::Home, HomeMotionCard);
    CreateLabel(Page::Home, HomeMotionTitle, L"运动参数", font_section_, true);
    CreateLabel(
        Page::Home,
        HomeMotionDescription,
        L"调整会自动保存，并立即同步到后台引擎。",
        font_small_,
        true);
    constexpr std::array<std::wstring_view, 5> motion_labels{
        L"滚动距离", L"动画时长", L"连滚加速窗口", L"最大加速倍率", L"尾 / 头比例"};
    constexpr std::array<int, 5> label_ids{
        HomeMotionLabel0, HomeMotionLabel1, HomeMotionLabel2, HomeMotionLabel3, HomeMotionLabel4};
    constexpr std::array<int, 5> value_ids{
        HomeMotionValue0, HomeMotionValue1, HomeMotionValue2, HomeMotionValue3, HomeMotionValue4};
    constexpr std::array<int, 5> slider_ids{
        HomeMotionSlider0, HomeMotionSlider1, HomeMotionSlider2, HomeMotionSlider3, HomeMotionSlider4};
    constexpr std::array<std::pair<int, int>, 5> ranges{
        std::pair{10, 400}, std::pair{40, 1000}, std::pair{0, 300},
        std::pair{10, 200}, std::pair{1, 100}};
    for (std::size_t index = 0; index < motion_labels.size(); ++index) {
        CreateLabel(Page::Home, label_ids[index], motion_labels[index], font_body_, true);
        home_values_[index] = CreateLabel(
            Page::Home, value_ids[index], L"", font_small_, true, SS_RIGHT | SS_NOPREFIX);
        home_sliders_[index] = CreateWindowExW(
            0,
            TRACKBAR_CLASSW,
            L"",
            WS_CHILD | WS_TABSTOP | TBS_HORZ | TBS_AUTOTICKS,
            0,
            0,
            0,
            0,
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(slider_ids[index])),
            instance_,
            nullptr);
        controls_[slider_ids[index]] = home_sliders_[index];
        AddPageControl(Page::Home, home_sliders_[index], true);
        SendMessageW(
            home_sliders_[index],
            TBM_SETRANGE,
            TRUE,
            MAKELPARAM(ranges[index].first, ranges[index].second));
        SendMessageW(home_sliders_[index], TBM_SETPAGESIZE, 0, 10);
        SetWindowTheme(home_sliders_[index], L"Explorer", nullptr);
    }
    home_easing_ = CreateCheckbox(Page::Home, HomeEasing, L"启用缓入缓出");
}

void MainWindow::CreateApplicationsPage() {
    CreateLabel(Page::Applications, ApplicationsTitle, L"应用规则", font_title_);
    CreateLabel(
        Page::Applications,
        ApplicationsDescription,
        L"按可执行文件名设置兼容策略；排除项优先于独立配置。",
        font_body_);

    application_cards_[0] = CreateCard(Page::Applications, ExcludedCard);
    CreateLabel(Page::Applications, ExcludedTitle, L"完全排除", font_section_, true);
    CreateLabel(
        Page::Applications,
        ExcludedDescription,
        L"游戏、远程桌面或自带滚动引擎的软件将收到原始滚轮事件。",
        font_small_,
        true);
    excluded_edit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ExcludedEdit)),
        instance_,
        nullptr);
    controls_[ExcludedEdit] = excluded_edit_;
    AddPageControl(Page::Applications, excluded_edit_, true);
    SendMessageW(excluded_edit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"例如 game.exe"));
    CreateButton(Page::Applications, ExcludedBrowse, L"浏览…", true);
    CreateButton(Page::Applications, ExcludedAdd, L"添加", true);
    excluded_list_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ExcludedList)),
        instance_,
        nullptr);
    controls_[ExcludedList] = excluded_list_;
    AddPageControl(Page::Applications, excluded_list_, true);
    CreateButton(Page::Applications, ExcludedRemove, L"移除所选", true);

    application_cards_[1] = CreateCard(Page::Applications, ProfileCard);
    CreateLabel(Page::Applications, ProfileTitle, L"独立配置", font_section_, true);
    CreateLabel(
        Page::Applications,
        ProfileDescription,
        L"覆盖全局运动参数，或开启兼容模式直接放行。",
        font_small_,
        true);
    profile_edit_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT",
        L"",
        WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
        0, 0, 0, 0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ProfileEdit)),
        instance_,
        nullptr);
    controls_[ProfileEdit] = profile_edit_;
    AddPageControl(Page::Applications, profile_edit_, true);
    SendMessageW(profile_edit_, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"例如 chrome.exe"));
    CreateButton(Page::Applications, ProfileBrowse, L"浏览…", true);
    CreateButton(Page::Applications, ProfileAdd, L"创建", true);
    profile_list_ = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"LISTBOX",
        L"",
        WS_CHILD | WS_TABSTOP | WS_VSCROLL | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
        0, 0, 0, 0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(ProfileList)),
        instance_,
        nullptr);
    controls_[ProfileList] = profile_list_;
    AddPageControl(Page::Applications, profile_list_, true);
    CreateButton(Page::Applications, ProfileRemove, L"移除所选", true);
    profile_editor_controls_.push_back(CreateLabel(
        Page::Applications, ProfileEditorTitle, L"所选应用参数", font_section_, true));
    profile_enabled_ = CreateCheckbox(Page::Applications, ProfileEnabled, L"启用平滑");
    profile_compatibility_ = CreateCheckbox(
        Page::Applications, ProfileCompatibility, L"兼容模式（直接放行）");
    profile_easing_ = CreateCheckbox(Page::Applications, ProfileEasing, L"缓入缓出");
    profile_editor_controls_.push_back(profile_enabled_);
    profile_editor_controls_.push_back(profile_compatibility_);
    profile_editor_controls_.push_back(profile_easing_);

    constexpr std::array<std::wstring_view, 5> labels{
        L"距离倍率", L"动画时长 (ms)", L"加速窗口 (ms)", L"最大加速", L"尾 / 头比例"};
    constexpr std::array<int, 5> label_ids{
        ProfileMotionLabel0, ProfileMotionLabel1, ProfileMotionLabel2,
        ProfileMotionLabel3, ProfileMotionLabel4};
    constexpr std::array<int, 5> edit_ids{
        ProfileMotionEdit0, ProfileMotionEdit1, ProfileMotionEdit2,
        ProfileMotionEdit3, ProfileMotionEdit4};
    for (std::size_t index = 0; index < labels.size(); ++index) {
        profile_editor_controls_.push_back(CreateLabel(
            Page::Applications, label_ids[index], labels[index], font_small_, true));
        profile_motion_edits_[index] = CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_TABSTOP | ES_AUTOHSCROLL,
            0, 0, 0, 0,
            window_,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(edit_ids[index])),
            instance_,
            nullptr);
        controls_[edit_ids[index]] = profile_motion_edits_[index];
        AddPageControl(Page::Applications, profile_motion_edits_[index], true);
        profile_editor_controls_.push_back(profile_motion_edits_[index]);
    }
}

void MainWindow::CreateAdvancedPage() {
    CreateLabel(Page::Advanced, AdvancedTitle, L"高级", font_title_);
    CreateLabel(
        Page::Advanced,
        AdvancedDescription,
        L"输入兼容、方向与系统集成选项。",
        font_body_);
    constexpr std::array<int, 4> card_ids{
        AdvancedCard0, AdvancedCard1, AdvancedCard2, AdvancedCard3};
    constexpr std::array<int, 4> title_ids{
        AdvancedCardTitle0, AdvancedCardTitle1, AdvancedCardTitle2, AdvancedCardTitle3};
    constexpr std::array<int, 4> description_ids{
        AdvancedCardDescription0, AdvancedCardDescription1,
        AdvancedCardDescription2, AdvancedCardDescription3};
    constexpr std::array<std::wstring_view, 4> titles{
        L"横向滚动", L"输入兼容", L"方向", L"系统集成"};
    constexpr std::array<std::wstring_view, 4> descriptions{
        L"控制横向滚轮与 Shift 手势。",
        L"保留缩放和高分辨率设备的原生行为。",
        L"同时作用于纵向和横向平滑事件。",
        L"仅使用当前用户权限，不安装系统服务。"};
    for (std::size_t index = 0; index < card_ids.size(); ++index) {
        advanced_cards_[index] = CreateCard(Page::Advanced, card_ids[index]);
        CreateLabel(Page::Advanced, title_ids[index], titles[index], font_section_, true);
        CreateLabel(
            Page::Advanced, description_ids[index], descriptions[index], font_small_, true);
    }
    constexpr std::array<int, 8> check_ids{
        AdvancedHorizontal, AdvancedShiftHorizontal, AdvancedCtrl, AdvancedAlt,
        AdvancedHighResolution, AdvancedReverse, AdvancedStartup, AdvancedTray};
    constexpr std::array<std::wstring_view, 8> check_labels{
        L"平滑横向滚轮",
        L"Shift + 垂直滚轮转为横向滚动",
        L"按住 Ctrl 时直接放行",
        L"按住 Alt 时直接放行",
        L"绕过高分辨率输入",
        L"反转滚动方向（自然方向）",
        L"登录 Windows 后启动",
        L"显示通知区域图标"};
    for (std::size_t index = 0; index < check_ids.size(); ++index) {
        advanced_checks_[index] = CreateCheckbox(
            Page::Advanced, check_ids[index], check_labels[index]);
    }
}

void MainWindow::CreateDiagnosticsPage() {
    CreateLabel(Page::Diagnostics, DiagnosticsTitle, L"诊断", font_title_);
    CreateLabel(
        Page::Diagnostics,
        DiagnosticsDescription,
        L"查看引擎连接、输入计数和失败保护状态。",
        font_body_);
    CreateButton(Page::Diagnostics, DiagnosticsStart, L"启动引擎");
    CreateButton(Page::Diagnostics, DiagnosticsRefresh, L"刷新");

    diagnostics_cards_[0] = CreateCard(Page::Diagnostics, DiagnosticsStatusCard);
    diagnostics_status_title_ = CreateLabel(
        Page::Diagnostics, DiagnosticsStatusTitle, L"引擎离线", font_section_, true);
    diagnostics_status_detail_ = CreateLabel(
        Page::Diagnostics, DiagnosticsStatusDetail, L"正在连接…", font_body_, true);

    constexpr std::array<std::wstring_view, 9> labels{
        L"物理输入", L"已平滑", L"直接放行", L"注入帧", L"注入总位移",
        L"目标切换", L"队列溢出", L"注入失败", L"配置代次"};
    constexpr std::array<int, 9> card_ids{
        DiagnosticsStatCard0, DiagnosticsStatCard1, DiagnosticsStatCard2,
        DiagnosticsStatCard3, DiagnosticsStatCard4, DiagnosticsStatCard5,
        DiagnosticsStatCard6, DiagnosticsStatCard7, DiagnosticsStatCard8};
    constexpr std::array<int, 9> label_ids{
        DiagnosticsStatLabel0, DiagnosticsStatLabel1, DiagnosticsStatLabel2,
        DiagnosticsStatLabel3, DiagnosticsStatLabel4, DiagnosticsStatLabel5,
        DiagnosticsStatLabel6, DiagnosticsStatLabel7, DiagnosticsStatLabel8};
    constexpr std::array<int, 9> value_ids{
        DiagnosticsStatValue0, DiagnosticsStatValue1, DiagnosticsStatValue2,
        DiagnosticsStatValue3, DiagnosticsStatValue4, DiagnosticsStatValue5,
        DiagnosticsStatValue6, DiagnosticsStatValue7, DiagnosticsStatValue8};
    for (std::size_t index = 0; index < labels.size(); ++index) {
        diagnostics_cards_[index + 1U] = CreateCard(Page::Diagnostics, card_ids[index]);
        CreateLabel(Page::Diagnostics, label_ids[index], labels[index], font_small_, true);
        diagnostics_values_[index] = CreateLabel(
            Page::Diagnostics, value_ids[index], L"0", font_value_, true);
    }

    diagnostics_cards_[10] = CreateCard(Page::Diagnostics, DiagnosticsInfoCard);
    CreateLabel(Page::Diagnostics, DiagnosticsPathLabel, L"配置文件", font_section_, true);
    diagnostics_path_ = CreateLabel(
        Page::Diagnostics,
        DiagnosticsPath,
        client_.SettingsPath().wstring(),
        font_mono_,
        true,
        SS_LEFT | SS_NOPREFIX | SS_PATHELLIPSIS);
    CreateLabel(Page::Diagnostics, DiagnosticsErrorLabel, L"最近错误", font_section_, true);
    diagnostics_error_ = CreateLabel(
        Page::Diagnostics, DiagnosticsError, L"无", font_body_, true);
}

HWND MainWindow::CreateLabel(
    const Page page,
    const int identifier,
    const std::wstring_view text,
    const HFONT font,
    const bool card_child,
    const DWORD style) {
    const std::wstring copy(text);
    const HWND control = CreateWindowExW(
        0,
        L"STATIC",
        copy.c_str(),
        WS_CHILD | style,
        0, 0, 0, 0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)),
        instance_,
        nullptr);
    controls_[identifier] = control;
    AddPageControl(page, control, card_child);
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), FALSE);
    return control;
}

HWND MainWindow::CreateButton(
    const Page page,
    const int identifier,
    const std::wstring_view text,
    const bool card_child,
    const DWORD style) {
    const std::wstring copy(text);
    const HWND control = CreateWindowExW(
        0,
        L"BUTTON",
        copy.c_str(),
        WS_CHILD | WS_TABSTOP | style,
        0, 0, 0, 0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)),
        instance_,
        nullptr);
    controls_[identifier] = control;
    AddPageControl(page, control, card_child);
    SetWindowTheme(control, L"Explorer", nullptr);
    return control;
}

HWND MainWindow::CreateCheckbox(
    const Page page,
    const int identifier,
    const std::wstring_view text,
    const bool card_child) {
    return CreateButton(
        page, identifier, text, card_child, BS_AUTOCHECKBOX | BS_MULTILINE);
}

HWND MainWindow::CreateCard(const Page page, const int identifier) {
    const HWND control = CreateWindowExW(
        0,
        L"STATIC",
        L"",
        WS_CHILD | SS_OWNERDRAW,
        0, 0, 0, 0,
        window_,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)),
        instance_,
        nullptr);
    controls_[identifier] = control;
    cards_.insert(control);
    AddPageControl(page, control, false);
    return control;
}

void MainWindow::AddPageControl(
    const Page page,
    const HWND control,
    const bool card_child) {
    page_controls_[static_cast<std::size_t>(page)].push_back(control);
    if (card_child) {
        card_children_.insert(control);
    }
}

HWND MainWindow::Control(const int identifier) const noexcept {
    const auto value = controls_.find(identifier);
    return value == controls_.end() ? nullptr : value->second;
}

void MainWindow::RecreateFonts() {
    for (HFONT* font : {&font_body_, &font_small_, &font_title_, &font_section_, &font_value_, &font_mono_}) {
        if (*font != nullptr) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
    const auto make_font = [this](const int points, const int weight, const wchar_t* face) {
        return CreateFontW(
            -MulDiv(points, static_cast<int>(dpi_), 72),
            0, 0, 0, weight, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
    };
    font_body_ = make_font(10, FW_NORMAL, L"Segoe UI Variable Text");
    font_small_ = make_font(9, FW_NORMAL, L"Segoe UI Variable Text");
    font_title_ = make_font(22, FW_SEMIBOLD, L"Segoe UI Variable Display");
    font_section_ = make_font(12, FW_SEMIBOLD, L"Segoe UI Variable Text");
    font_value_ = make_font(17, FW_SEMIBOLD, L"Segoe UI Variable Display");
    font_mono_ = make_font(9, FW_NORMAL, L"Cascadia Mono");
}

void MainWindow::ApplyFonts() {
    for (const auto& [identifier, control] : controls_) {
        HFONT font = font_body_;
        if (identifier == StatusText) {
            font = font_small_;
        }
        SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    for (const int identifier : {HomeTitle, ApplicationsTitle, AdvancedTitle, DiagnosticsTitle}) {
        SendMessageW(Control(identifier), WM_SETFONT, reinterpret_cast<WPARAM>(font_title_), TRUE);
    }
    for (const int identifier : {
             HomeGlobalTitle, HomePresetTitle, HomeMotionTitle, ExcludedTitle, ProfileTitle,
             ProfileEditorTitle, AdvancedCardTitle0, AdvancedCardTitle1, AdvancedCardTitle2,
             AdvancedCardTitle3, DiagnosticsStatusTitle, DiagnosticsPathLabel,
             DiagnosticsErrorLabel}) {
        SendMessageW(Control(identifier), WM_SETFONT, reinterpret_cast<WPARAM>(font_section_), TRUE);
    }
    for (const HWND value : diagnostics_values_) {
        SendMessageW(value, WM_SETFONT, reinterpret_cast<WPARAM>(font_value_), TRUE);
    }
    SendMessageW(diagnostics_path_, WM_SETFONT, reinterpret_cast<WPARAM>(font_mono_), TRUE);
}

void MainWindow::ApplyWindowAppearance() {
    constexpr DWORD round = 2;
    constexpr DWORD backdrop = 2;
    static_cast<void>(DwmSetWindowAttribute(window_, 33, &round, sizeof(round)));
    static_cast<void>(DwmSetWindowAttribute(window_, 38, &backdrop, sizeof(backdrop)));
}

int MainWindow::Scale(const int value) const noexcept {
    return MulDiv(value, static_cast<int>(dpi_), 96);
}

void MainWindow::SetBounds(
    const HWND control,
    const int x,
    const int y,
    const int width,
    const int height) const {
    if (control != nullptr) {
        MoveWindow(control, x, y, std::max(width, 0), std::max(height, 0), TRUE);
    }
}

void MainWindow::Layout() {
    if (window_ == nullptr) {
        return;
    }
    RECT client{};
    GetClientRect(window_, &client);
    const int width = client.right;
    const int height = client.bottom;
    const int navigation_width = Scale(204);
    const int status_height = Scale(40);
    const int margin = Scale(28);

    for (std::size_t index = 0; index < navigation_buttons_.size(); ++index) {
        SetBounds(
            navigation_buttons_[index],
            Scale(12),
            Scale(74 + static_cast<int>(index) * 48),
            navigation_width - Scale(24),
            Scale(40));
    }
    SetBounds(status_text_, Scale(36), height - status_height, width - Scale(260), status_height);

    const int content_x = navigation_width + margin;
    const int content_y = 0;
    const int content_width = width - content_x - margin;
    const int content_height = height - status_height;
    LayoutHome(content_x, content_y, content_width, content_height);
    LayoutApplications(content_x, content_y, content_width, content_height);
    LayoutAdvanced(content_x, content_y, content_width, content_height);
    LayoutDiagnostics(content_x, content_y, content_width, content_height);
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::LayoutHome(const int x, const int y, const int width, const int height) {
    const int gap = Scale(12);
    SetBounds(Control(HomeTitle), x, y + Scale(26), width, Scale(38));
    SetBounds(Control(HomeDescription), x, y + Scale(64), width, Scale(24));

    const int global_y = y + Scale(102);
    SetBounds(Control(HomeGlobalCard), x, global_y, width, Scale(94));
    SetBounds(Control(HomeGlobalTitle), x + Scale(20), global_y + Scale(17), width - Scale(180), Scale(24));
    SetBounds(Control(HomeGlobalDescription), x + Scale(20), global_y + Scale(47), width - Scale(190), Scale(24));
    SetBounds(home_enabled_, x + width - Scale(145), global_y + Scale(27), Scale(120), Scale(36));

    SetBounds(Control(HomePresetTitle), x, y + Scale(215), width, Scale(26));
    const int preset_width = (width - gap * 3) / 4;
    for (std::size_t index = 0; index < preset_buttons_.size(); ++index) {
        SetBounds(
            preset_buttons_[index],
            x + static_cast<int>(index) * (preset_width + gap),
            y + Scale(246),
            preset_width,
            Scale(36));
    }

    const int motion_y = y + Scale(302);
    const int motion_height = std::max(height - motion_y - Scale(22), Scale(300));
    SetBounds(Control(HomeMotionCard), x, motion_y, width, motion_height);
    SetBounds(Control(HomeMotionTitle), x + Scale(20), motion_y + Scale(15), width - Scale(40), Scale(24));
    SetBounds(Control(HomeMotionDescription), x + Scale(20), motion_y + Scale(42), width - Scale(40), Scale(20));

    const int inner_x = x + Scale(20);
    const int inner_width = width - Scale(40);
    const int column_gap = Scale(30);
    const int column_width = (inner_width - column_gap) / 2;
    constexpr std::array<int, 5> label_ids{
        HomeMotionLabel0, HomeMotionLabel1, HomeMotionLabel2, HomeMotionLabel3, HomeMotionLabel4};
    for (std::size_t index = 0; index < home_sliders_.size(); ++index) {
        const int row = static_cast<int>(index / 2U);
        const int column = static_cast<int>(index % 2U);
        const int left = inner_x + column * (column_width + column_gap);
        const int top = motion_y + Scale(78 + row * 78);
        SetBounds(Control(label_ids[index]), left, top, column_width - Scale(78), Scale(22));
        SetBounds(home_values_[index], left + column_width - Scale(82), top, Scale(82), Scale(22));
        SetBounds(home_sliders_[index], left, top + Scale(24), column_width, Scale(32));
    }
    SetBounds(
        home_easing_,
        inner_x + column_width + column_gap,
        motion_y + Scale(78 + 2 * 78),
        column_width,
        Scale(32));
}

void MainWindow::LayoutApplications(
    const int x,
    const int y,
    const int width,
    const int height) {
    SetBounds(Control(ApplicationsTitle), x, y + Scale(26), width, Scale(38));
    SetBounds(Control(ApplicationsDescription), x, y + Scale(64), width, Scale(24));
    const int card_y = y + Scale(102);
    const int card_height = std::max(height - card_y - Scale(22), Scale(450));
    const int gap = Scale(16);
    const int left_width = (width - gap) * 2 / 5;
    const int right_x = x + left_width + gap;
    const int right_width = width - left_width - gap;
    SetBounds(Control(ExcludedCard), x, card_y, left_width, card_height);
    SetBounds(Control(ProfileCard), right_x, card_y, right_width, card_height);

    SetBounds(Control(ExcludedTitle), x + Scale(18), card_y + Scale(15), left_width - Scale(36), Scale(24));
    SetBounds(Control(ExcludedDescription), x + Scale(18), card_y + Scale(43), left_width - Scale(36), Scale(42));
    const int left_inner = left_width - Scale(36);
    const int browse_width = Scale(64);
    const int add_width = Scale(52);
    SetBounds(excluded_edit_, x + Scale(18), card_y + Scale(92), left_inner - browse_width - add_width - Scale(12), Scale(32));
    SetBounds(Control(ExcludedBrowse), x + left_width - browse_width - add_width - Scale(24), card_y + Scale(92), browse_width, Scale(32));
    SetBounds(Control(ExcludedAdd), x + left_width - add_width - Scale(18), card_y + Scale(92), add_width, Scale(32));
    SetBounds(excluded_list_, x + Scale(18), card_y + Scale(136), left_inner, card_height - Scale(194));
    SetBounds(Control(ExcludedRemove), x + Scale(18), card_y + card_height - Scale(46), Scale(96), Scale(30));

    SetBounds(Control(ProfileTitle), right_x + Scale(18), card_y + Scale(15), right_width - Scale(36), Scale(24));
    SetBounds(Control(ProfileDescription), right_x + Scale(18), card_y + Scale(43), right_width - Scale(36), Scale(24));
    const int right_inner = right_width - Scale(36);
    SetBounds(profile_edit_, right_x + Scale(18), card_y + Scale(76), right_inner - browse_width - add_width - Scale(12), Scale(32));
    SetBounds(Control(ProfileBrowse), right_x + right_width - browse_width - add_width - Scale(24), card_y + Scale(76), browse_width, Scale(32));
    SetBounds(Control(ProfileAdd), right_x + right_width - add_width - Scale(18), card_y + Scale(76), add_width, Scale(32));
    SetBounds(profile_list_, right_x + Scale(18), card_y + Scale(120), right_inner, Scale(112));
    SetBounds(Control(ProfileRemove), right_x + Scale(18), card_y + Scale(240), Scale(96), Scale(30));
    SetBounds(Control(ProfileEditorTitle), right_x + Scale(18), card_y + Scale(282), right_inner, Scale(24));
    SetBounds(profile_enabled_, right_x + Scale(18), card_y + Scale(314), right_inner / 2, Scale(28));
    SetBounds(profile_compatibility_, right_x + Scale(18) + right_inner / 2, card_y + Scale(314), right_inner / 2, Scale(34));

    constexpr std::array<int, 5> label_ids{
        ProfileMotionLabel0, ProfileMotionLabel1, ProfileMotionLabel2,
        ProfileMotionLabel3, ProfileMotionLabel4};
    const int field_gap = Scale(12);
    const int field_width = (right_inner - field_gap) / 2;
    for (std::size_t index = 0; index < profile_motion_edits_.size(); ++index) {
        const int row = static_cast<int>(index / 2U);
        const int column = static_cast<int>(index % 2U);
        const int left = right_x + Scale(18) + column * (field_width + field_gap);
        const int top = card_y + Scale(358 + row * 58);
        SetBounds(Control(label_ids[index]), left, top, field_width, Scale(20));
        SetBounds(profile_motion_edits_[index], left, top + Scale(20), field_width, Scale(28));
    }
    SetBounds(
        profile_easing_,
        right_x + Scale(18) + field_width + field_gap,
        card_y + Scale(358 + 2 * 58),
        field_width,
        Scale(30));
}

void MainWindow::LayoutAdvanced(
    const int x,
    const int y,
    const int width,
    const int height) {
    SetBounds(Control(AdvancedTitle), x, y + Scale(26), width, Scale(38));
    SetBounds(Control(AdvancedDescription), x, y + Scale(64), width, Scale(24));
    const int grid_y = y + Scale(102);
    const int gap = Scale(16);
    const int card_width = (width - gap) / 2;
    const int card_height = (height - grid_y - Scale(22) - gap) / 2;
    constexpr std::array<int, 4> card_ids{AdvancedCard0, AdvancedCard1, AdvancedCard2, AdvancedCard3};
    constexpr std::array<int, 4> title_ids{
        AdvancedCardTitle0, AdvancedCardTitle1, AdvancedCardTitle2, AdvancedCardTitle3};
    constexpr std::array<int, 4> description_ids{
        AdvancedCardDescription0, AdvancedCardDescription1,
        AdvancedCardDescription2, AdvancedCardDescription3};
    for (std::size_t index = 0; index < card_ids.size(); ++index) {
        const int row = static_cast<int>(index / 2U);
        const int column = static_cast<int>(index % 2U);
        const int left = x + column * (card_width + gap);
        const int top = grid_y + row * (card_height + gap);
        SetBounds(Control(card_ids[index]), left, top, card_width, card_height);
        SetBounds(Control(title_ids[index]), left + Scale(20), top + Scale(16), card_width - Scale(40), Scale(24));
        SetBounds(Control(description_ids[index]), left + Scale(20), top + Scale(43), card_width - Scale(40), Scale(38));
    }
    const int left0 = x + Scale(20);
    const int top0 = grid_y + Scale(92);
    SetBounds(advanced_checks_[0], left0, top0, card_width - Scale(40), Scale(30));
    SetBounds(advanced_checks_[1], left0, top0 + Scale(42), card_width - Scale(40), Scale(36));

    const int left1 = x + card_width + gap + Scale(20);
    SetBounds(advanced_checks_[2], left1, top0, card_width - Scale(40), Scale(30));
    SetBounds(advanced_checks_[3], left1, top0 + Scale(38), card_width - Scale(40), Scale(30));
    SetBounds(advanced_checks_[4], left1, top0 + Scale(76), card_width - Scale(40), Scale(30));

    const int second_top = grid_y + card_height + gap;
    SetBounds(advanced_checks_[5], left0, second_top + Scale(92), card_width - Scale(40), Scale(34));
    SetBounds(advanced_checks_[6], left1, second_top + Scale(92), card_width - Scale(40), Scale(30));
    SetBounds(advanced_checks_[7], left1, second_top + Scale(134), card_width - Scale(40), Scale(30));
}

void MainWindow::LayoutDiagnostics(
    const int x,
    const int y,
    const int width,
    const int height) {
    SetBounds(Control(DiagnosticsTitle), x, y + Scale(26), width - Scale(190), Scale(38));
    SetBounds(Control(DiagnosticsDescription), x, y + Scale(64), width - Scale(190), Scale(24));
    SetBounds(Control(DiagnosticsStart), x + width - Scale(176), y + Scale(38), Scale(96), Scale(34));
    SetBounds(Control(DiagnosticsRefresh), x + width - Scale(68), y + Scale(38), Scale(68), Scale(34));

    const int status_y = y + Scale(102);
    SetBounds(Control(DiagnosticsStatusCard), x, status_y, width, Scale(78));
    SetBounds(diagnostics_status_title_, x + Scale(22), status_y + Scale(14), width - Scale(44), Scale(24));
    SetBounds(diagnostics_status_detail_, x + Scale(22), status_y + Scale(42), width - Scale(44), Scale(22));

    const int gap = Scale(10);
    const int card_width = (width - gap * 2) / 3;
    const int stats_y = status_y + Scale(94);
    const int stat_height = Scale(68);
    constexpr std::array<int, 9> card_ids{
        DiagnosticsStatCard0, DiagnosticsStatCard1, DiagnosticsStatCard2,
        DiagnosticsStatCard3, DiagnosticsStatCard4, DiagnosticsStatCard5,
        DiagnosticsStatCard6, DiagnosticsStatCard7, DiagnosticsStatCard8};
    constexpr std::array<int, 9> label_ids{
        DiagnosticsStatLabel0, DiagnosticsStatLabel1, DiagnosticsStatLabel2,
        DiagnosticsStatLabel3, DiagnosticsStatLabel4, DiagnosticsStatLabel5,
        DiagnosticsStatLabel6, DiagnosticsStatLabel7, DiagnosticsStatLabel8};
    for (std::size_t index = 0; index < card_ids.size(); ++index) {
        const int row = static_cast<int>(index / 3U);
        const int column = static_cast<int>(index % 3U);
        const int left = x + column * (card_width + gap);
        const int top = stats_y + row * (stat_height + gap);
        SetBounds(Control(card_ids[index]), left, top, card_width, stat_height);
        SetBounds(Control(label_ids[index]), left + Scale(14), top + Scale(9), card_width - Scale(28), Scale(18));
        SetBounds(diagnostics_values_[index], left + Scale(14), top + Scale(28), card_width - Scale(28), Scale(30));
    }

    const int info_y = stats_y + 3 * (stat_height + gap) + Scale(6);
    const int info_height = std::max(height - info_y - Scale(22), Scale(120));
    SetBounds(Control(DiagnosticsInfoCard), x, info_y, width, info_height);
    SetBounds(Control(DiagnosticsPathLabel), x + Scale(20), info_y + Scale(14), Scale(100), Scale(24));
    SetBounds(diagnostics_path_, x + Scale(130), info_y + Scale(14), width - Scale(150), Scale(24));
    SetBounds(Control(DiagnosticsErrorLabel), x + Scale(20), info_y + Scale(52), Scale(100), Scale(24));
    SetBounds(diagnostics_error_, x + Scale(130), info_y + Scale(52), width - Scale(150), info_height - Scale(66));
}

void MainWindow::ShowPage(const Page page) {
    current_page_ = page;
    for (std::size_t index = 0; index < page_controls_.size(); ++index) {
        const int command = index == static_cast<std::size_t>(page) ? SW_SHOW : SW_HIDE;
        for (const HWND control : page_controls_[index]) {
            ShowWindow(control, command);
        }
    }
    for (const HWND button : navigation_buttons_) {
        InvalidateRect(button, nullptr, TRUE);
    }
    if (page == Page::Diagnostics) {
        static_cast<void>(client_.Refresh(250));
        SyncDiagnostics();
        UpdateStatus();
    }
    SetFocus(navigation_buttons_[static_cast<std::size_t>(page)]);
}

void MainWindow::SyncAllControls() {
    updating_ = true;
    SyncHomeControls();
    SyncApplicationLists();
    SyncAdvancedControls();
    SyncDiagnostics();
    UpdateStatus();
    updating_ = false;
}

void MainWindow::SyncHomeControls() {
    const AppSettings& settings = client_.State().settings;
    SetCheck(home_enabled_, settings.enabled);
    SendMessageW(home_sliders_[0], TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(settings.motion.distance_scale * 100.0)));
    SendMessageW(home_sliders_[1], TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(settings.motion.animation_time_ms)));
    SendMessageW(home_sliders_[2], TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(settings.motion.acceleration_window_ms)));
    SendMessageW(home_sliders_[3], TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(settings.motion.acceleration_max * 10.0)));
    SendMessageW(home_sliders_[4], TBM_SETPOS, TRUE, static_cast<LPARAM>(std::lround(settings.motion.tail_to_head_ratio * 10.0)));
    SetCheck(home_easing_, settings.motion.easing_enabled);
    UpdateMotionLabels();
}

void MainWindow::SyncApplicationLists() {
    const int excluded_selection = static_cast<int>(SendMessageW(excluded_list_, LB_GETCURSEL, 0, 0));
    SendMessageW(excluded_list_, LB_RESETCONTENT, 0, 0);
    for (const std::string& executable : client_.State().settings.excluded_apps) {
        const std::wstring wide = Utf8ToWide(executable);
        SendMessageW(excluded_list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
    }
    if (!client_.State().settings.excluded_apps.empty()) {
        SendMessageW(
            excluded_list_,
            LB_SETCURSEL,
            static_cast<WPARAM>(std::clamp(
                excluded_selection,
                0,
                static_cast<int>(client_.State().settings.excluded_apps.size() - 1U))),
            0);
    }

    const int profile_selection = static_cast<int>(SendMessageW(profile_list_, LB_GETCURSEL, 0, 0));
    SendMessageW(profile_list_, LB_RESETCONTENT, 0, 0);
    for (const AppProfile& profile : client_.State().settings.profiles) {
        const std::wstring wide = Utf8ToWide(profile.executable);
        SendMessageW(profile_list_, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(wide.c_str()));
    }
    if (!client_.State().settings.profiles.empty()) {
        SendMessageW(
            profile_list_,
            LB_SETCURSEL,
            static_cast<WPARAM>(std::clamp(
                profile_selection,
                0,
                static_cast<int>(client_.State().settings.profiles.size() - 1U))),
            0);
    }
    SyncProfileEditor();
}

void MainWindow::SyncProfileEditor() {
    const AppProfile* profile = SelectedProfile();
    const bool enabled = profile != nullptr;
    for (const HWND control : profile_editor_controls_) {
        EnableWindow(control, enabled ? TRUE : FALSE);
    }
    if (profile == nullptr) {
        SetWindowTextW(Control(ProfileEditorTitle), L"选择一个应用以编辑参数");
        for (const HWND edit : profile_motion_edits_) {
            SetWindowTextW(edit, L"");
        }
        SetCheck(profile_enabled_, false);
        SetCheck(profile_compatibility_, false);
        SetCheck(profile_easing_, false);
        return;
    }
    const std::wstring title = L"所选应用参数 · " + Utf8ToWide(profile->executable);
    SetWindowTextW(Control(ProfileEditorTitle), title.c_str());
    SetCheck(profile_enabled_, profile->enabled);
    SetCheck(profile_compatibility_, profile->compatibility_mode);
    SetCheck(profile_easing_, profile->motion.easing_enabled);
    const std::array<std::wstring, 5> values{
        FormatNumber(profile->motion.distance_scale, 2),
        FormatNumber(profile->motion.animation_time_ms, 0),
        FormatNumber(profile->motion.acceleration_window_ms, 0),
        FormatNumber(profile->motion.acceleration_max, 1),
        FormatNumber(profile->motion.tail_to_head_ratio, 1),
    };
    for (std::size_t index = 0; index < values.size(); ++index) {
        SetWindowTextW(profile_motion_edits_[index], values[index].c_str());
    }
}

void MainWindow::SyncAdvancedControls() {
    const AppSettings& settings = client_.State().settings;
    constexpr std::array<bool AppSettings::*, 8> fields{
        &AppSettings::horizontal_smoothing,
        &AppSettings::shift_for_horizontal,
        &AppSettings::pass_through_ctrl,
        &AppSettings::pass_through_alt,
        &AppSettings::bypass_high_resolution,
        &AppSettings::reverse_direction,
        &AppSettings::start_with_windows,
        &AppSettings::show_tray_icon,
    };
    for (std::size_t index = 0; index < fields.size(); ++index) {
        SetCheck(advanced_checks_[index], settings.*fields[index]);
    }
}

void MainWindow::SyncDiagnostics() {
    const SessionState& state = client_.State();
    SetWindowTextW(diagnostics_status_title_, state.online ? L"引擎在线" : L"引擎离线");
    SetWindowTextW(diagnostics_status_detail_, state.status.c_str());
    const EngineDiagnostics& diagnostics = state.diagnostics;
    const std::array<std::int64_t, 9> values{
        diagnostics.physical_events,
        diagnostics.smoothed_events,
        diagnostics.passed_events,
        diagnostics.injected_events,
        diagnostics.injected_delta,
        diagnostics.target_changes,
        diagnostics.queue_overflows,
        diagnostics.injection_failures,
        diagnostics.settings_generation,
    };
    for (std::size_t index = 0; index < values.size(); ++index) {
        const std::wstring value = std::to_wstring(values[index]);
        SetWindowTextW(diagnostics_values_[index], value.c_str());
    }
    SetWindowTextW(diagnostics_path_, client_.SettingsPath().c_str());
    SetWindowTextW(
        diagnostics_error_,
        state.last_error.empty() ? L"无" : state.last_error.c_str());
}

void MainWindow::UpdateStatus() {
    SetWindowTextW(status_text_, client_.State().status.c_str());
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::UpdateMotionLabels() {
    const std::array<std::wstring, 5> labels{
        FormatNumber(static_cast<double>(SendMessageW(home_sliders_[0], TBM_GETPOS, 0, 0)) / 100.0, 2) + L"×",
        std::to_wstring(SendMessageW(home_sliders_[1], TBM_GETPOS, 0, 0)) + L" ms",
        std::to_wstring(SendMessageW(home_sliders_[2], TBM_GETPOS, 0, 0)) + L" ms",
        FormatNumber(static_cast<double>(SendMessageW(home_sliders_[3], TBM_GETPOS, 0, 0)) / 10.0, 1) + L"×",
        FormatNumber(static_cast<double>(SendMessageW(home_sliders_[4], TBM_GETPOS, 0, 0)) / 10.0, 1) + L":1",
    };
    for (std::size_t index = 0; index < labels.size(); ++index) {
        SetWindowTextW(home_values_[index], labels[index].c_str());
    }
}

void MainWindow::ScheduleApply() {
    if (updating_) {
        return;
    }
    client_.MutableState().status = L"正在保存更改…";
    UpdateStatus();
    KillTimer(window_, kApplyTimer);
    SetTimer(window_, kApplyTimer, 250U, nullptr);
}

void MainWindow::ApplyPendingChanges() {
    static_cast<void>(client_.Apply());
    updating_ = true;
    SyncHomeControls();
    SyncAdvancedControls();
    SyncDiagnostics();
    updating_ = false;
    UpdateStatus();
}

void MainWindow::BeginEngineConnection() {
    if (client_.Refresh(100)) {
        SyncAllControls();
        return;
    }
    if (!engine_started_) {
        engine_started_ = client_.StartSiblingEngine();
    }
    connection_attempts_ = 0;
    UpdateStatus();
    SetTimer(window_, kConnectTimer, 120U, nullptr);
}

void MainWindow::PollEngineConnection() {
    ++connection_attempts_;
    if (client_.Refresh(100)) {
        KillTimer(window_, kConnectTimer);
        SyncAllControls();
        return;
    }
    if (connection_attempts_ >= 25) {
        KillTimer(window_, kConnectTimer);
        client_.MutableState().status = L"引擎离线；更改仍会保存到本机";
        UpdateStatus();
    }
}

void MainWindow::HandleCommand(
    const int identifier,
    const int notification,
    const HWND source) {
    if (identifier >= NavHome && identifier <= NavDiagnostics && notification == BN_CLICKED) {
        ShowPage(static_cast<Page>(identifier - NavHome));
        return;
    }
    if (notification == BN_CLICKED) {
        switch (identifier) {
        case HomeEnabled:
            client_.MutableState().settings.enabled = IsChecked(home_enabled_);
            ScheduleApply();
            return;
        case HomePresetResponsive:
        case HomePresetBalanced:
        case HomePresetSmooth:
        case HomePresetClassic:
            ApplyPreset(identifier);
            return;
        case HomeEasing:
            client_.MutableState().settings.motion.easing_enabled = IsChecked(home_easing_);
            ScheduleApply();
            return;
        case ExcludedBrowse:
            BrowseExecutable(excluded_edit_);
            return;
        case ExcludedAdd:
            AddExcludedApplication();
            return;
        case ExcludedRemove:
            RemoveExcludedApplication();
            return;
        case ProfileBrowse:
            BrowseExecutable(profile_edit_);
            return;
        case ProfileAdd:
            AddProfile();
            return;
        case ProfileRemove:
            RemoveProfile();
            return;
        case ProfileEnabled:
        case ProfileCompatibility:
        case ProfileEasing:
            UpdateProfileFromEditor(identifier);
            return;
        case DiagnosticsStart:
            engine_started_ = client_.StartSiblingEngine();
            connection_attempts_ = 0;
            SetTimer(window_, kConnectTimer, 120U, nullptr);
            UpdateStatus();
            return;
        case DiagnosticsRefresh:
            static_cast<void>(client_.Refresh());
            SyncDiagnostics();
            UpdateStatus();
            return;
        default:
            break;
        }
        if (identifier >= AdvancedHorizontal && identifier <= AdvancedTray) {
            constexpr std::array<bool AppSettings::*, 8> fields{
                &AppSettings::horizontal_smoothing,
                &AppSettings::shift_for_horizontal,
                &AppSettings::pass_through_ctrl,
                &AppSettings::pass_through_alt,
                &AppSettings::bypass_high_resolution,
                &AppSettings::reverse_direction,
                &AppSettings::start_with_windows,
                &AppSettings::show_tray_icon,
            };
            client_.MutableState().settings.*fields[static_cast<std::size_t>(identifier - AdvancedHorizontal)] =
                IsChecked(source);
            ScheduleApply();
            return;
        }
    }
    if (identifier == ProfileList && notification == LBN_SELCHANGE) {
        updating_ = true;
        SyncProfileEditor();
        updating_ = false;
        return;
    }
    if (identifier >= ProfileMotionEdit0 && identifier <= ProfileMotionEdit4 &&
        notification == EN_KILLFOCUS) {
        UpdateProfileFromEditor(identifier);
    }
}

void MainWindow::HandleSlider(const HWND slider) {
    if (updating_ || slider == nullptr) {
        return;
    }
    AppSettings& settings = client_.MutableState().settings;
    if (slider == home_sliders_[0]) {
        settings.motion.distance_scale = static_cast<double>(SendMessageW(slider, TBM_GETPOS, 0, 0)) / 100.0;
    } else if (slider == home_sliders_[1]) {
        settings.motion.animation_time_ms = static_cast<double>(SendMessageW(slider, TBM_GETPOS, 0, 0));
    } else if (slider == home_sliders_[2]) {
        settings.motion.acceleration_window_ms = static_cast<double>(SendMessageW(slider, TBM_GETPOS, 0, 0));
    } else if (slider == home_sliders_[3]) {
        settings.motion.acceleration_max = static_cast<double>(SendMessageW(slider, TBM_GETPOS, 0, 0)) / 10.0;
    } else if (slider == home_sliders_[4]) {
        settings.motion.tail_to_head_ratio = static_cast<double>(SendMessageW(slider, TBM_GETPOS, 0, 0)) / 10.0;
    } else {
        return;
    }
    UpdateMotionLabels();
    ScheduleApply();
}

void MainWindow::ApplyPreset(const int identifier) {
    MotionSettings motion{};
    if (identifier == HomePresetResponsive) {
        motion = MotionSettings{0.9, 180.0, 55.0, 4.0, 1.8, true};
    } else if (identifier == HomePresetSmooth) {
        motion = MotionSettings{1.1, 520.0, 90.0, 8.0, 4.5, true};
    } else if (identifier == HomePresetClassic) {
        motion = MotionSettings{1.0, 420.0, 80.0, 7.0, 3.5, true};
    }
    client_.MutableState().settings.motion = motion;
    updating_ = true;
    SyncHomeControls();
    updating_ = false;
    ScheduleApply();
}

void MainWindow::AddExcludedApplication() {
    const std::string executable = NormalizeExecutableKey(WideToUtf8(WindowText(excluded_edit_)));
    if (executable.empty()) {
        MessageBoxW(window_, L"请输入有效的 .exe 文件名。", kWindowTitle, MB_OK | MB_ICONWARNING);
        return;
    }
    auto& values = client_.MutableState().settings.excluded_apps;
    if (ContainsExecutable(values, executable)) {
        MessageBoxW(window_, L"该应用已经位于排除列表中。", kWindowTitle, MB_OK | MB_ICONINFORMATION);
        return;
    }
    values.push_back(executable);
    std::sort(values.begin(), values.end());
    SetWindowTextW(excluded_edit_, L"");
    updating_ = true;
    SyncApplicationLists();
    SendMessageW(excluded_list_, LB_SETCURSEL, static_cast<WPARAM>(values.size() - 1U), 0);
    updating_ = false;
    ScheduleApply();
}

void MainWindow::RemoveExcludedApplication() {
    const LRESULT selection = SendMessageW(excluded_list_, LB_GETCURSEL, 0, 0);
    auto& values = client_.MutableState().settings.excluded_apps;
    if (selection == LB_ERR || static_cast<std::size_t>(selection) >= values.size()) {
        return;
    }
    values.erase(values.begin() + selection);
    updating_ = true;
    SyncApplicationLists();
    updating_ = false;
    ScheduleApply();
}

void MainWindow::AddProfile() {
    const std::string executable = NormalizeExecutableKey(WideToUtf8(WindowText(profile_edit_)));
    if (executable.empty()) {
        MessageBoxW(window_, L"请输入有效的 .exe 文件名。", kWindowTitle, MB_OK | MB_ICONWARNING);
        return;
    }
    auto& profiles = client_.MutableState().settings.profiles;
    const auto duplicate = std::find_if(
        profiles.begin(), profiles.end(), [&executable](const AppProfile& profile) {
            return profile.executable == executable;
        });
    if (duplicate != profiles.end()) {
        MessageBoxW(window_, L"该应用已经具有独立配置。", kWindowTitle, MB_OK | MB_ICONINFORMATION);
        return;
    }
    profiles.push_back(AppProfile{
        .executable = executable,
        .enabled = true,
        .compatibility_mode = false,
        .motion = client_.State().settings.motion,
    });
    SetWindowTextW(profile_edit_, L"");
    updating_ = true;
    SyncApplicationLists();
    SendMessageW(profile_list_, LB_SETCURSEL, static_cast<WPARAM>(profiles.size() - 1U), 0);
    SyncProfileEditor();
    updating_ = false;
    ScheduleApply();
}

void MainWindow::RemoveProfile() {
    const LRESULT selection = SendMessageW(profile_list_, LB_GETCURSEL, 0, 0);
    auto& profiles = client_.MutableState().settings.profiles;
    if (selection == LB_ERR || static_cast<std::size_t>(selection) >= profiles.size()) {
        return;
    }
    profiles.erase(profiles.begin() + selection);
    updating_ = true;
    SyncApplicationLists();
    updating_ = false;
    ScheduleApply();
}

void MainWindow::UpdateProfileFromEditor(const int identifier) {
    if (updating_) {
        return;
    }
    AppProfile* profile = SelectedProfile();
    if (profile == nullptr) {
        return;
    }
    if (identifier == ProfileEnabled) {
        profile->enabled = IsChecked(profile_enabled_);
    } else if (identifier == ProfileCompatibility) {
        profile->compatibility_mode = IsChecked(profile_compatibility_);
    } else if (identifier == ProfileEasing) {
        profile->motion.easing_enabled = IsChecked(profile_easing_);
    } else if (identifier >= ProfileMotionEdit0 && identifier <= ProfileMotionEdit4) {
        const std::wstring text = WindowText(Control(identifier));
        wchar_t* end = nullptr;
        const double value = std::wcstod(text.c_str(), &end);
        if (end == text.c_str() || !std::isfinite(value)) {
            updating_ = true;
            SyncProfileEditor();
            updating_ = false;
            return;
        }
        switch (identifier) {
        case ProfileMotionEdit0: profile->motion.distance_scale = value; break;
        case ProfileMotionEdit1: profile->motion.animation_time_ms = value; break;
        case ProfileMotionEdit2: profile->motion.acceleration_window_ms = value; break;
        case ProfileMotionEdit3: profile->motion.acceleration_max = value; break;
        case ProfileMotionEdit4: profile->motion.tail_to_head_ratio = value; break;
        default: break;
        }
        profile->motion = NormalizeMotionSettings(profile->motion);
        updating_ = true;
        SyncProfileEditor();
        updating_ = false;
    }
    ScheduleApply();
}

void MainWindow::BrowseExecutable(const HWND target_edit) {
    std::vector<wchar_t> path(32768U, L'\0');
    constexpr wchar_t filter[] = L"Windows 程序 (*.exe)\0*.exe\0所有文件 (*.*)\0*.*\0\0";
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(OPENFILENAMEW);
    dialog.hwndOwner = window_;
    dialog.lpstrFilter = filter;
    dialog.lpstrFile = path.data();
    dialog.nMaxFile = static_cast<DWORD>(path.size());
    dialog.lpstrDefExt = L"exe";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    if (GetOpenFileNameW(&dialog)) {
        const std::filesystem::path executable(path.data());
        SetWindowTextW(target_edit, executable.filename().c_str());
        SetFocus(target_edit);
    }
}

AppProfile* MainWindow::SelectedProfile() {
    const LRESULT selection = SendMessageW(profile_list_, LB_GETCURSEL, 0, 0);
    auto& profiles = client_.MutableState().settings.profiles;
    if (selection == LB_ERR || static_cast<std::size_t>(selection) >= profiles.size()) {
        return nullptr;
    }
    return &profiles[static_cast<std::size_t>(selection)];
}

const AppProfile* MainWindow::SelectedProfile() const {
    const LRESULT selection = SendMessageW(profile_list_, LB_GETCURSEL, 0, 0);
    const auto& profiles = client_.State().settings.profiles;
    if (selection == LB_ERR || static_cast<std::size_t>(selection) >= profiles.size()) {
        return nullptr;
    }
    return &profiles[static_cast<std::size_t>(selection)];
}

void MainWindow::Paint() {
    PAINTSTRUCT paint{};
    const HDC target = BeginPaint(window_, &paint);
    RECT client{};
    GetClientRect(window_, &client);
    const HDC buffer = CreateCompatibleDC(target);
    const HBITMAP bitmap = CreateCompatibleBitmap(target, client.right, client.bottom);
    const HGDIOBJ previous_bitmap = SelectObject(buffer, bitmap);

    FillRect(buffer, &client, brush_background_);
    const int navigation_width = Scale(204);
    const int status_height = Scale(40);
    RECT navigation{0, 0, navigation_width, client.bottom - status_height};
    FillRect(buffer, &navigation, brush_navigation_);
    RECT status{0, client.bottom - status_height, client.right, client.bottom};
    FillRect(buffer, &status, brush_background_);

    const HPEN separator_pen = CreatePen(PS_SOLID, 1, kBorder);
    const HGDIOBJ old_pen = SelectObject(buffer, separator_pen);
    MoveToEx(buffer, navigation_width - 1, 0, nullptr);
    LineTo(buffer, navigation_width - 1, client.bottom - status_height);
    MoveToEx(buffer, 0, client.bottom - status_height, nullptr);
    LineTo(buffer, client.right, client.bottom - status_height);

    if (icon_ != nullptr) {
        DrawIconEx(buffer, Scale(18), Scale(18), icon_, Scale(28), Scale(28), 0, nullptr, DI_NORMAL);
    }
    SetBkMode(buffer, TRANSPARENT);
    SetTextColor(buffer, kText);
    SelectObject(buffer, font_body_);
    RECT brand{Scale(56), Scale(18), navigation_width - Scale(12), Scale(54)};
    DrawTextW(buffer, kWindowTitle, -1, &brand, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    const COLORREF status_color = client_.State().online ? kOnline : kOffline;
    const HBRUSH dot_brush = CreateSolidBrush(status_color);
    const HGDIOBJ old_brush = SelectObject(buffer, dot_brush);
    SelectObject(buffer, GetStockObject(NULL_PEN));
    const int dot = Scale(8);
    const int dot_y = client.bottom - status_height + (status_height - dot) / 2;
    Ellipse(buffer, Scale(18), dot_y, Scale(18) + dot, dot_y + dot);
    SelectObject(buffer, old_brush);
    DeleteObject(dot_brush);

    SelectObject(buffer, font_small_);
    SetTextColor(buffer, kSecondaryText);
    RECT shortcut{
        client.right - Scale(220),
        client.bottom - status_height,
        client.right - Scale(18),
        client.bottom};
    DrawTextW(
        buffer,
        L"Ctrl + Alt + S  快速开关",
        -1,
        &shortcut,
        DT_RIGHT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(buffer, old_pen);
    DeleteObject(separator_pen);
    BitBlt(target, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY);
    SelectObject(buffer, previous_bitmap);
    DeleteObject(bitmap);
    DeleteDC(buffer);
    EndPaint(window_, &paint);
}

void MainWindow::DrawOwnerControl(const DRAWITEMSTRUCT& item) {
    if (cards_.contains(item.hwndItem)) {
        DrawCard(item);
    } else {
        DrawNavigationButton(item);
    }
}

void MainWindow::DrawNavigationButton(const DRAWITEMSTRUCT& item) {
    RECT bounds = item.rcItem;
    const bool selected =
        static_cast<int>(item.itemID) - NavHome == static_cast<int>(current_page_);
    const bool pressed = (item.itemState & ODS_SELECTED) != 0;
    const bool hot = (item.itemState & ODS_HOTLIGHT) != 0;
    const COLORREF background = selected
        ? kAccentPale
        : (pressed || hot ? RGB(235, 234, 232) : kNavigation);
    const HBRUSH brush = CreateSolidBrush(background);
    const HPEN pen = CreatePen(PS_NULL, 0, background);
    const HGDIOBJ old_brush = SelectObject(item.hDC, brush);
    const HGDIOBJ old_pen = SelectObject(item.hDC, pen);
    RoundRect(item.hDC, bounds.left, bounds.top, bounds.right, bounds.bottom, Scale(8), Scale(8));

    std::wstring label = WindowText(item.hwndItem);
    bounds.left += Scale(16);
    SetBkMode(item.hDC, TRANSPARENT);
    SetTextColor(item.hDC, selected ? kAccent : kText);
    SelectObject(item.hDC, font_body_);
    DrawTextW(item.hDC, label.c_str(), -1, &bounds, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
    if ((item.itemState & ODS_FOCUS) != 0) {
        RECT focus = item.rcItem;
        InflateRect(&focus, -Scale(3), -Scale(3));
        DrawFocusRect(item.hDC, &focus);
    }
    SelectObject(item.hDC, old_pen);
    SelectObject(item.hDC, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

void MainWindow::DrawCard(const DRAWITEMSTRUCT& item) {
    RECT bounds = item.rcItem;
    bounds.right -= 1;
    bounds.bottom -= 1;
    const HBRUSH brush = CreateSolidBrush(kCard);
    const HPEN pen = CreatePen(PS_SOLID, 1, kBorder);
    const HGDIOBJ old_brush = SelectObject(item.hDC, brush);
    const HGDIOBJ old_pen = SelectObject(item.hDC, pen);
    RoundRect(item.hDC, bounds.left, bounds.top, bounds.right, bounds.bottom, Scale(10), Scale(10));
    SelectObject(item.hDC, old_pen);
    SelectObject(item.hDC, old_brush);
    DeleteObject(pen);
    DeleteObject(brush);
}

}  // namespace smootheverything::control_panel
