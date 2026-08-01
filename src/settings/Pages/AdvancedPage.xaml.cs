using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using SmoothEverything.Settings.Models;
using SmoothEverything.Settings.Services;

namespace SmoothEverything.Settings.Pages;

public sealed partial class AdvancedPage : Page
{
    private readonly SettingsSession _session = SettingsSession.Instance;
    private AppSettingsModel? _boundSettings;
    private bool _updating;

    public AdvancedPage()
    {
        InitializeComponent();
    }

    private void Page_Loaded(object sender, RoutedEventArgs e)
    {
        _session.StateChanged += Session_StateChanged;
        UpdateFromModel(force: true);
    }

    private void Page_Unloaded(object sender, RoutedEventArgs e)
    {
        _session.StateChanged -= Session_StateChanged;
    }

    private void Session_StateChanged(object? sender, EventArgs e)
    {
        DispatcherQueue.TryEnqueue(() => UpdateFromModel(force: false));
    }

    private void UpdateFromModel(bool force)
    {
        if (!force && ReferenceEquals(_boundSettings, _session.Current))
        {
            return;
        }
        _boundSettings = _session.Current;
        var input = _session.Current.Input;
        var system = _session.Current.System;
        _updating = true;
        HorizontalToggle.IsOn = input.HorizontalSmoothing;
        ShiftHorizontalToggle.IsOn = input.ShiftForHorizontal;
        CtrlPassToggle.IsOn = input.PassThroughCtrl;
        AltPassToggle.IsOn = input.PassThroughAlt;
        HighResolutionToggle.IsOn = input.BypassHighResolution;
        ReverseToggle.IsOn = input.ReverseDirection;
        StartupToggle.IsOn = system.StartWithWindows;
        TrayToggle.IsOn = system.ShowTrayIcon;
        _updating = false;
    }

    private void SettingToggle_Toggled(object sender, RoutedEventArgs e)
    {
        if (_updating || sender is not ToggleSwitch { Tag: string setting } toggle)
        {
            return;
        }
        var input = _session.Current.Input;
        var system = _session.Current.System;
        switch (setting)
        {
            case "horizontal": input.HorizontalSmoothing = toggle.IsOn; break;
            case "shift-horizontal": input.ShiftForHorizontal = toggle.IsOn; break;
            case "ctrl": input.PassThroughCtrl = toggle.IsOn; break;
            case "alt": input.PassThroughAlt = toggle.IsOn; break;
            case "high-resolution": input.BypassHighResolution = toggle.IsOn; break;
            case "reverse": input.ReverseDirection = toggle.IsOn; break;
            case "startup": system.StartWithWindows = toggle.IsOn; break;
            case "tray": system.ShowTrayIcon = toggle.IsOn; break;
        }
        _session.QueueApply();
    }
}
