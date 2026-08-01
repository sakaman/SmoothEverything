using Microsoft.UI;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using SmoothEverything.Settings.Pages;
using SmoothEverything.Settings.Services;
using Windows.Graphics;
using WinRT.Interop;

namespace SmoothEverything.Settings;

public sealed partial class MainWindow : Window
{
    private readonly SettingsSession _session = SettingsSession.Instance;

    public nint WindowHandle => WindowNative.GetWindowHandle(this);

    public MainWindow()
    {
        InitializeComponent();
        ExtendsContentIntoTitleBar = true;
        SetTitleBar(AppTitleBar);
        ConfigureTitleBar();
        AppWindow.Resize(new SizeInt32(1080, 780));
        AppWindow.Title = "SmoothEverything";
        AppWindow.SetIcon(Path.Combine(AppContext.BaseDirectory, "Assets", "AppIcon.ico"));
        _session.StateChanged += Session_StateChanged;
        ContentFrame.Navigate(typeof(HomePage));
    }

    private void ConfigureTitleBar()
    {
        if (!AppWindowTitleBar.IsCustomizationSupported())
        {
            return;
        }

        AppWindow.TitleBar.ButtonBackgroundColor = Colors.Transparent;
        AppWindow.TitleBar.ButtonInactiveBackgroundColor = Colors.Transparent;
        RootGrid.ActualThemeChanged += (_, _) => UpdateTitleBarButtonColors();
        UpdateTitleBarButtonColors();
    }

    private void UpdateTitleBarButtonColors()
    {
        if (!AppWindowTitleBar.IsCustomizationSupported())
        {
            return;
        }

        var dark = RootGrid.ActualTheme == ElementTheme.Dark;
        var foreground = dark ? Colors.White : Colors.Black;
        AppWindow.TitleBar.ButtonForegroundColor = foreground;
        AppWindow.TitleBar.ButtonHoverForegroundColor = foreground;
        AppWindow.TitleBar.ButtonPressedForegroundColor = foreground;
        AppWindow.TitleBar.ButtonInactiveForegroundColor = dark
            ? ColorHelper.FromArgb(0x99, 0xFF, 0xFF, 0xFF)
            : ColorHelper.FromArgb(0x99, 0x00, 0x00, 0x00);
        AppWindow.TitleBar.ButtonHoverBackgroundColor = dark
            ? ColorHelper.FromArgb(0x18, 0xFF, 0xFF, 0xFF)
            : ColorHelper.FromArgb(0x10, 0x00, 0x00, 0x00);
        AppWindow.TitleBar.ButtonPressedBackgroundColor = dark
            ? ColorHelper.FromArgb(0x2A, 0xFF, 0xFF, 0xFF)
            : ColorHelper.FromArgb(0x18, 0x00, 0x00, 0x00);
    }

    private async void RootGrid_Loaded(object sender, RoutedEventArgs e)
    {
        UpdateStatus();
        await _session.InitializeAsync();
    }

    private void Navigation_SelectionChanged(
        NavigationView sender,
        NavigationViewSelectionChangedEventArgs args)
    {
        if (args.SelectedItem is not NavigationViewItem item)
        {
            return;
        }

        var page = item.Tag switch
        {
            "home" => typeof(HomePage),
            "applications" => typeof(ApplicationsPage),
            "advanced" => typeof(AdvancedPage),
            "diagnostics" => typeof(DiagnosticsPage),
            _ => typeof(HomePage),
        };
        if (ContentFrame.CurrentSourcePageType != page)
        {
            ContentFrame.Navigate(page);
        }
    }

    private void Session_StateChanged(object? sender, EventArgs e)
    {
        DispatcherQueue.TryEnqueue(UpdateStatus);
    }

    private void UpdateStatus()
    {
        StatusText.Text = _session.StatusText;
        StatusDot.Fill = new SolidColorBrush(
            _session.IsOnline ? Colors.MediumSeaGreen : Colors.DarkOrange);
    }
}
