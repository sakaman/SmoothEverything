using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Input;
using SmoothEverything.Settings.Models;
using SmoothEverything.Settings.Services;
using System.Collections.ObjectModel;
using Windows.Storage.Pickers;
using Windows.System;
using WinRT.Interop;

namespace SmoothEverything.Settings.Pages;

public sealed partial class ApplicationsPage : Page
{
    private readonly SettingsSession _session = SettingsSession.Instance;
    private readonly ObservableCollection<string> _excluded = [];
    private readonly ObservableCollection<AppProfileModel> _profiles = [];
    private AppSettingsModel? _boundSettings;
    private bool _updating;

    public ApplicationsPage()
    {
        InitializeComponent();
        ExcludedItems.ItemsSource = _excluded;
        ProfileItems.ItemsSource = _profiles;
    }

    private void Page_Loaded(object sender, RoutedEventArgs e)
    {
        _session.StateChanged += Session_StateChanged;
        ReloadFromModel(force: true);
    }

    private void Page_Unloaded(object sender, RoutedEventArgs e)
    {
        _session.StateChanged -= Session_StateChanged;
    }

    private void Session_StateChanged(object? sender, EventArgs e)
    {
        DispatcherQueue.TryEnqueue(() => ReloadFromModel(force: false));
    }

    private void ReloadFromModel(bool force)
    {
        if (!force && ReferenceEquals(_boundSettings, _session.Current))
        {
            return;
        }
        _boundSettings = _session.Current;
        _updating = true;
        _excluded.Clear();
        foreach (var executable in _session.Current.ExcludedApps)
        {
            _excluded.Add(executable);
        }
        _profiles.Clear();
        foreach (var profile in _session.Current.Profiles)
        {
            _profiles.Add(profile);
        }
        _updating = false;
        UpdateEmptyStates();
    }

    private void AddExcluded_Click(object sender, RoutedEventArgs e) => AddExcluded();

    private void ExcludedInput_KeyDown(object sender, KeyRoutedEventArgs e)
    {
        if (e.Key == VirtualKey.Enter)
        {
            AddExcluded();
            e.Handled = true;
        }
    }

    private void AddExcluded()
    {
        var executable = AppSettingsModel.NormalizeExecutable(ExcludedInput.Text);
        if (executable.Length == 0)
        {
            ShowMessage("Enter a valid .exe filename.", InfoBarSeverity.Warning);
            return;
        }
        if (_excluded.Contains(executable, StringComparer.OrdinalIgnoreCase))
        {
            ShowMessage($"{executable} is already in the exclusion list.", InfoBarSeverity.Informational);
            return;
        }
        _excluded.Add(executable);
        _session.Current.ExcludedApps.Add(executable);
        ExcludedInput.Text = string.Empty;
        UpdateEmptyStates();
        _session.QueueApply();
    }

    private void RemoveExcluded_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button { DataContext: string executable })
        {
            return;
        }
        _excluded.Remove(executable);
        _session.Current.ExcludedApps.RemoveAll(
            value => string.Equals(value, executable, StringComparison.OrdinalIgnoreCase));
        UpdateEmptyStates();
        _session.QueueApply();
    }

    private void AddProfile_Click(object sender, RoutedEventArgs e) => AddProfile();

    private void ProfileInput_KeyDown(object sender, KeyRoutedEventArgs e)
    {
        if (e.Key == VirtualKey.Enter)
        {
            AddProfile();
            e.Handled = true;
        }
    }

    private void AddProfile()
    {
        var executable = AppSettingsModel.NormalizeExecutable(ProfileInput.Text);
        if (executable.Length == 0)
        {
            ShowMessage("Enter a valid .exe filename.", InfoBarSeverity.Warning);
            return;
        }
        if (_profiles.Any(profile => string.Equals(
                profile.Executable,
                executable,
                StringComparison.OrdinalIgnoreCase)))
        {
            ShowMessage($"{executable} already has an application profile.", InfoBarSeverity.Informational);
            return;
        }

        var profile = new AppProfileModel
        {
            Executable = executable,
            Motion = _session.Current.Motion.Clone(),
        };
        _profiles.Add(profile);
        _session.Current.Profiles.Add(profile);
        ProfileInput.Text = string.Empty;
        UpdateEmptyStates();
        _session.QueueApply();
    }

    private void RemoveProfile_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button { DataContext: AppProfileModel profile })
        {
            return;
        }
        _profiles.Remove(profile);
        _session.Current.Profiles.Remove(profile);
        UpdateEmptyStates();
        _session.QueueApply();
    }

    private void ProfileToggle_Toggled(object sender, RoutedEventArgs e)
    {
        if (_updating || sender is not ToggleSwitch toggle ||
            toggle.DataContext is not AppProfileModel profile)
        {
            return;
        }
        switch (toggle.Tag)
        {
            case "enabled":
                profile.Enabled = toggle.IsOn;
                break;
            case "compatibility":
                profile.CompatibilityMode = toggle.IsOn;
                break;
            case "easing":
                profile.Motion.EasingEnabled = toggle.IsOn;
                break;
        }
        _session.QueueApply();
    }

    private void ProfileNumberBox_ValueChanged(
        NumberBox sender,
        NumberBoxValueChangedEventArgs args)
    {
        if (_updating || sender.DataContext is not AppProfileModel profile ||
            double.IsNaN(args.NewValue))
        {
            return;
        }
        switch (sender.Tag)
        {
            case "distance": profile.Motion.DistanceScale = args.NewValue; break;
            case "duration": profile.Motion.AnimationTimeMs = args.NewValue; break;
            case "acceleration": profile.Motion.AccelerationMax = args.NewValue; break;
            case "window": profile.Motion.AccelerationWindowMs = args.NewValue; break;
            case "tail": profile.Motion.TailToHeadRatio = args.NewValue; break;
        }
        _session.QueueApply();
    }

    private async void Browse_Click(object sender, RoutedEventArgs e)
    {
        if (App.MainWindow is null || sender is not Button { Tag: string target })
        {
            return;
        }
        var picker = new FileOpenPicker
        {
            SuggestedStartLocation = PickerLocationId.ComputerFolder,
            ViewMode = PickerViewMode.List,
        };
        picker.FileTypeFilter.Add(".exe");
        InitializeWithWindow.Initialize(picker, App.MainWindow.WindowHandle);
        var file = await picker.PickSingleFileAsync();
        if (file is null)
        {
            return;
        }
        if (target == "excluded")
        {
            ExcludedInput.Text = file.Name;
        }
        else
        {
            ProfileInput.Text = file.Name;
        }
    }

    private void UpdateEmptyStates()
    {
        NoExclusionsText.Visibility = _excluded.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
        NoProfilesText.Visibility = _profiles.Count == 0 ? Visibility.Visible : Visibility.Collapsed;
    }

    private void ShowMessage(string message, InfoBarSeverity severity)
    {
        PageMessage.Message = message;
        PageMessage.Severity = severity;
        PageMessage.IsOpen = true;
    }
}
