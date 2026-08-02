using SmoothEverything.Settings.Models;
using System.Text.Json;

var tests = new (string Name, Action Body)[]
{
    ("defaults match native schema", DefaultsMatchNativeSchema),
    ("normalization clamps and deduplicates", NormalizationClampsAndDeduplicates),
    ("motion clone is independent", MotionCloneIsIndependent),
};

var failed = 0;
foreach (var (name, body) in tests)
{
    try
    {
        body();
        Console.WriteLine($"PASS {name}");
    }
    catch (Exception error)
    {
        ++failed;
        Console.Error.WriteLine($"FAIL {name}: {error.Message}");
    }
}

return failed == 0 ? 0 : 1;

static void DefaultsMatchNativeSchema()
{
    var settings = new AppSettingsModel();
    var json = JsonSerializer.Serialize(settings);
    using var document = JsonDocument.Parse(json);
    var root = document.RootElement;
    Require(root.GetProperty("schema_version").GetInt32() == 1, "schema_version");
    Require(root.GetProperty("enabled").GetBoolean(), "enabled");
    Require(root.GetProperty("motion").GetProperty("animation_time_ms").GetDouble() == 360.0, "duration");
    Require(root.GetProperty("input").GetProperty("pass_through_ctrl").GetBoolean(), "ctrl bypass");
    Require(root.GetProperty("system").GetProperty("show_tray_icon").GetBoolean(), "tray icon");
    Require(root.GetProperty("system").GetProperty("ui_language").GetString() == "system", "UI language");
}

static void NormalizationClampsAndDeduplicates()
{
    var settings = new AppSettingsModel
    {
        Motion = new MotionSettingsModel
        {
            DistanceScale = 9,
            AnimationTimeMs = -1,
            AccelerationWindowMs = 900,
            AccelerationMax = 0,
            TailToHeadRatio = 99,
        },
        System = new SystemSettingsModel { UiLanguage = "fr-FR" },
        ExcludedApps = [" C:\\Apps\\Game.EXE ", "game.exe", "   "],
        Profiles =
        [
            new AppProfileModel { Executable = "C:\\Apps\\Browser.EXE" },
            new AppProfileModel { Executable = "browser.exe" },
        ],
    };

    settings.Normalize();
    Require(settings.Motion.DistanceScale == 4, "distance clamp");
    Require(settings.Motion.AnimationTimeMs == 1, "duration clamp");
    Require(settings.Motion.AccelerationWindowMs == 500, "window clamp");
    Require(settings.Motion.AccelerationMax == 1, "acceleration clamp");
    Require(settings.Motion.TailToHeadRatio == 10, "tail clamp");
    Require(settings.System.UiLanguage == "system", "language normalization");
    Require(settings.ExcludedApps.SequenceEqual(["game.exe"]), "excluded normalization");
    Require(settings.Profiles.Count == 1 && settings.Profiles[0].Executable == "browser.exe", "profile normalization");
}

static void MotionCloneIsIndependent()
{
    var original = new MotionSettingsModel { AnimationTimeMs = 250 };
    var clone = original.Clone();
    clone.AnimationTimeMs = 700;
    Require(original.AnimationTimeMs == 250, "clone changed original");
}

static void Require(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}
