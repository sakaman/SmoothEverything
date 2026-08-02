using Microsoft.UI;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using SmoothEverything.Settings.Services;

namespace SmoothEverything.Settings.Pages;

public sealed partial class DiagnosticsPage : Page
{
    private readonly SettingsSession _session = SettingsSession.Instance;

    public DiagnosticsPage()
    {
        InitializeComponent();
    }

    private void Page_Loaded(object sender, RoutedEventArgs e)
    {
        _session.StateChanged += Session_StateChanged;
        UpdateView();
    }

    private void Page_Unloaded(object sender, RoutedEventArgs e)
    {
        _session.StateChanged -= Session_StateChanged;
    }

    private void Session_StateChanged(object? sender, EventArgs e)
    {
        DispatcherQueue.TryEnqueue(UpdateView);
    }

    private async void Refresh_Click(object sender, RoutedEventArgs e)
    {
        await _session.RefreshAsync();
    }

    private async void StartEngine_Click(object sender, RoutedEventArgs e)
    {
        await _session.EnsureEngineAsync();
    }

    private void UpdateView()
    {
        var diagnostics = _session.Diagnostics;
        EngineStatusTitle.Text = _session.IsOnline ? "Engine Online" : "Engine Offline";
        EngineStatusDetail.Text = _session.StatusText;
        EngineStatusIcon.Foreground = new SolidColorBrush(
            _session.IsOnline ? Colors.MediumSeaGreen : Colors.DarkOrange);

        PhysicalEventsValue.Text = diagnostics.PhysicalEvents.ToString("N0");
        SmoothedEventsValue.Text = diagnostics.SmoothedEvents.ToString("N0");
        PassedEventsValue.Text = diagnostics.PassedEvents.ToString("N0");
        InjectedEventsValue.Text = diagnostics.InjectedEvents.ToString("N0");
        InjectedDeltaValue.Text = diagnostics.InjectedDelta.ToString("N0");
        TargetChangesValue.Text = diagnostics.TargetChanges.ToString("N0");
        QueueOverflowsValue.Text = diagnostics.QueueOverflows.ToString("N0");
        InjectionFailuresValue.Text = diagnostics.InjectionFailures.ToString("N0");
        SettingsGenerationValue.Text = diagnostics.SettingsGeneration.ToString("N0");

        SettingsPathText.Text = _session.SettingsFilePath;
        LastErrorText.Text = string.IsNullOrWhiteSpace(_session.LastError)
            ? "None"
            : _session.LastError;
    }
}
