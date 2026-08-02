#include "smootheverything/control_panel/localization.h"

#include <windows.h>

#include <string_view>

namespace smootheverything::control_panel {
namespace {

struct Translation final {
    std::wstring_view english;
    std::wstring_view simplified_chinese;
};

constexpr Translation kTranslations[] = {
    {L"Home", L"主页"},
    {L"Application Rules", L"应用规则"},
    {L"Advanced", L"高级"},
    {L"Diagnostics", L"诊断"},
    {L"Connecting to engine...", L"正在连接引擎…"},
    {L"Not connected to engine", L"未连接到引擎"},
    {L"Engine connected", L"引擎已连接"},
    {L"Applied to engine", L"已应用到引擎"},
    {L"Saved; engine offline", L"已保存；引擎离线"},
    {L"Save failed", L"保存失败"},
    {L"Unable to start engine", L"无法启动引擎"},
    {L"Waiting for engine...", L"正在等待引擎…"},
    {L"Saving changes...", L"正在保存更改…"},
    {L"Engine offline; changes will still be saved locally", L"引擎离线；更改仍会保存到本机"},
    {L"Scrolling Experience", L"滚动体验"},
    {L"Add continuous, low-latency native smoothing to a traditional mouse wheel.", L"为传统滚轮加入连续、低延迟的原生平滑滚动。"},
    {L"Global Smoothing", L"全局平滑"},
    {L"When paused, wheel events pass through to the target application unchanged.", L"暂停时，滚轮事件会原样交还给目标应用，不改变系统输入。"},
    {L"Enabled", L"运行中"},
    {L"Motion Presets", L"手感预设"},
    {L"Responsive", L"响应优先"},
    {L"Balanced", L"均衡"},
    {L"Smooth Tail", L"丝滑长尾"},
    {L"Classic", L"经典手感"},
    {L"Motion Settings", L"运动参数"},
    {L"Changes are saved automatically and applied to the background engine immediately.", L"调整会自动保存，并立即同步到后台引擎。"},
    {L"Scroll distance", L"滚动距离"},
    {L"Animation duration", L"动画时长"},
    {L"Acceleration window", L"连滚加速窗口"},
    {L"Maximum acceleration", L"最大加速倍率"},
    {L"Tail / head ratio", L"尾 / 头比例"},
    {L"Enable easing", L"启用缓入缓出"},
    {L"Set compatibility behavior by executable name; exclusions take precedence over profiles.", L"按可执行文件名设置兼容策略；排除项优先于独立配置。"},
    {L"Excluded Applications", L"完全排除"},
    {L"Games, Remote Desktop, and applications with their own scrolling receive raw wheel events.", L"游戏、远程桌面或自带滚动引擎的软件将收到原始滚轮事件。"},
    {L"For example, game.exe", L"例如 game.exe"},
    {L"Browse...", L"浏览…"},
    {L"Add", L"添加"},
    {L"Remove Selected", L"移除所选"},
    {L"Application Profiles", L"独立配置"},
    {L"Override global motion settings or enable compatibility mode to pass input through.", L"覆盖全局运动参数，或开启兼容模式直接放行。"},
    {L"For example, chrome.exe", L"例如 chrome.exe"},
    {L"Create", L"创建"},
    {L"Selected Application Settings", L"所选应用参数"},
    {L"Select an application to edit its settings", L"选择一个应用以编辑参数"},
    {L"Selected Application Settings - ", L"所选应用参数 - "},
    {L"Enable smoothing", L"启用平滑"},
    {L"Compatibility mode (pass through)", L"兼容模式（直接放行）"},
    {L"Easing", L"缓入缓出"},
    {L"Distance multiplier", L"距离倍率"},
    {L"Animation duration (ms)", L"动画时长 (ms)"},
    {L"Acceleration window (ms)", L"加速窗口 (ms)"},
    {L"Maximum acceleration", L"最大加速"},
    {L"Tail / head ratio", L"尾 / 头比例"},
    {L"Input compatibility, direction, and system integration options.", L"输入兼容、方向与系统集成选项。"},
    {L"Horizontal Scrolling", L"横向滚动"},
    {L"Input Compatibility", L"输入兼容"},
    {L"Direction", L"方向"},
    {L"System Integration", L"系统集成"},
    {L"Control horizontal wheel input and Shift gestures.", L"控制横向滚轮与 Shift 手势。"},
    {L"Preserve native zoom and high-resolution device behavior.", L"保留缩放和高分辨率设备的原生行为。"},
    {L"Apply direction changes to vertical and horizontal smoothing.", L"同时作用于纵向和横向平滑事件。"},
    {L"Use current-user permissions only; no system service is installed.", L"仅使用当前用户权限，不安装系统服务。"},
    {L"Smooth horizontal wheel input", L"平滑横向滚轮"},
    {L"Convert Shift + vertical wheel to horizontal", L"Shift + 垂直滚轮转为横向滚动"},
    {L"Pass through while Ctrl is held", L"按住 Ctrl 时直接放行"},
    {L"Pass through while Alt is held", L"按住 Alt 时直接放行"},
    {L"Bypass high-resolution input", L"绕过高分辨率输入"},
    {L"Reverse scrolling direction (natural)", L"反转滚动方向（自然方向）"},
    {L"Start after Windows sign-in", L"登录 Windows 后启动"},
    {L"Show notification-area icon", L"显示通知区域图标"},
    {L"Language", L"语言"},
    {L"System default", L"跟随系统"},
    {L"English", L"English"},
    {L"Simplified Chinese", L"简体中文"},
    {L"Inspect the engine connection, input counters, and fail-open status.", L"查看引擎连接、输入计数和失败保护状态。"},
    {L"Start Engine", L"启动引擎"},
    {L"Refresh", L"刷新"},
    {L"Engine Online", L"引擎在线"},
    {L"Engine Offline", L"引擎离线"},
    {L"Connecting...", L"正在连接…"},
    {L"Physical input", L"物理输入"},
    {L"Smoothed", L"已平滑"},
    {L"Passed through", L"直接放行"},
    {L"Injected frames", L"注入帧"},
    {L"Injected displacement", L"注入总位移"},
    {L"Target changes", L"目标切换"},
    {L"Queue overflows", L"队列溢出"},
    {L"Injection failures", L"注入失败"},
    {L"Settings generation", L"配置代次"},
    {L"Settings File", L"配置文件"},
    {L"Latest Error", L"最近错误"},
    {L"None", L"无"},
    {L"Ctrl + Alt + S  Quick toggle", L"Ctrl + Alt + S  快速开关"},
    {L"Enter a valid .exe filename.", L"请输入有效的 .exe 文件名。"},
    {L"This application is already in the exclusion list.", L"该应用已经位于排除列表中。"},
    {L"This application already has a profile.", L"该应用已经具有独立配置。"},
    {L"Windows programs (*.exe)", L"Windows 程序 (*.exe)"},
    {L"All files (*.*)", L"所有文件 (*.*)"},
    {L"Error code", L"错误代码"},
    {L"Failed to get application directory", L"获取程序目录失败"},
    {L"SmoothEverything.Engine.exe was not found next to the control panel", L"在设置程序同目录中找不到 SmoothEverything.Engine.exe"},
    {L"Failed to start engine", L"启动引擎失败"},
    {L"Failed to read local settings", L"无法读取本地配置"},
    {L"Invalid local settings size", L"本地配置大小无效"},
    {L"Failed to parse local settings: ", L"无法解析本地配置："},
    {L"Failed to create settings directory: ", L"无法创建配置目录："},
    {L"Failed to create temporary settings file", L"无法创建临时配置"},
    {L"Failed to write settings", L"无法写入配置"},
    {L"Failed to replace settings", L"无法替换配置"},
    {L"Request is too large", L"请求数据过大"},
    {L"Failed to connect to engine", L"连接引擎失败"},
    {L"Failed to open engine pipe", L"打开引擎管道失败"},
    {L"Failed to send request to engine", L"向引擎发送请求失败"},
    {L"Failed to read engine response", L"读取引擎响应失败"},
    {L"Engine response exceeds the 1 MiB limit", L"引擎响应超过 1 MiB 上限"},
    {L"Engine response is not a JSON object", L"引擎返回的数据不是 JSON 对象"},
    {L"Engine rejected the request", L"引擎拒绝了请求"},
    {L"Engine returned invalid settings: ", L"引擎返回了无效配置："},
    {L"Engine returned invalid data: ", L"引擎返回了无效数据："},
};

[[nodiscard]] bool SystemUsesSimplifiedChinese() noexcept {
    const LANGID language = GetUserDefaultUILanguage();
    if (PRIMARYLANGID(language) != LANG_CHINESE) {
        return false;
    }
    const WORD sublanguage = SUBLANGID(language);
    return sublanguage == SUBLANG_CHINESE_SIMPLIFIED ||
        sublanguage == SUBLANG_CHINESE_SINGAPORE;
}

}  // namespace

Localizer::Localizer(const std::string_view preference) noexcept {
    SetPreference(preference);
}

void Localizer::SetPreference(const std::string_view preference) noexcept {
    if (preference == "zh-CN") {
        language_ = UiLanguage::SimplifiedChinese;
    } else if (preference == "system" && SystemUsesSimplifiedChinese()) {
        language_ = UiLanguage::SimplifiedChinese;
    } else {
        language_ = UiLanguage::English;
    }
}

UiLanguage Localizer::Language() const noexcept {
    return language_;
}

std::wstring_view Localizer::Translate(const std::wstring_view english) const noexcept {
    if (language_ == UiLanguage::SimplifiedChinese) {
        for (const Translation& translation : kTranslations) {
            if (translation.english == english && !translation.simplified_chinese.empty()) {
                return translation.simplified_chinese;
            }
        }
    }
    return english;
}

}  // namespace smootheverything::control_panel
