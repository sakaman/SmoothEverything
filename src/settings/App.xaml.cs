using Microsoft.UI.Xaml;

namespace SmoothEverything.Settings;

public partial class App : Application
{
    public static MainWindow? MainWindow { get; private set; }

    public App()
    {
        InitializeComponent();
        UnhandledException += App_UnhandledException;
    }

    protected override void OnLaunched(LaunchActivatedEventArgs args)
    {
        try
        {
            MainWindow = new MainWindow();
            MainWindow.Activate();
        }
        catch (Exception error)
        {
            WriteCrashLog("OnLaunched", error);
            throw;
        }
    }

    private static void App_UnhandledException(
        object sender,
        Microsoft.UI.Xaml.UnhandledExceptionEventArgs args)
    {
        WriteCrashLog("UnhandledException", args.Exception);
    }

    private static void WriteCrashLog(string stage, Exception error)
    {
        try
        {
            var directory = Path.Combine(
                Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                "SmoothEverything");
            Directory.CreateDirectory(directory);
            File.AppendAllText(
                Path.Combine(directory, "settings-crash.log"),
                $"[{DateTimeOffset.Now:O}] {stage}{Environment.NewLine}{error}{Environment.NewLine}{Environment.NewLine}");
        }
        catch
        {
        }
    }
}
