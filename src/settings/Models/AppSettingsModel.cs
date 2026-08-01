using System.Text.Json.Serialization;

namespace SmoothEverything.Settings.Models;

public sealed class MotionSettingsModel
{
    [JsonPropertyName("distance_scale")]
    public double DistanceScale { get; set; } = 1.0;

    [JsonPropertyName("animation_time_ms")]
    public double AnimationTimeMs { get; set; } = 360.0;

    [JsonPropertyName("acceleration_window_ms")]
    public double AccelerationWindowMs { get; set; } = 70.0;

    [JsonPropertyName("acceleration_max")]
    public double AccelerationMax { get; set; } = 7.0;

    [JsonPropertyName("tail_to_head_ratio")]
    public double TailToHeadRatio { get; set; } = 3.0;

    [JsonPropertyName("easing_enabled")]
    public bool EasingEnabled { get; set; } = true;

    public MotionSettingsModel Clone() => new()
    {
        DistanceScale = DistanceScale,
        AnimationTimeMs = AnimationTimeMs,
        AccelerationWindowMs = AccelerationWindowMs,
        AccelerationMax = AccelerationMax,
        TailToHeadRatio = TailToHeadRatio,
        EasingEnabled = EasingEnabled,
    };

    public void Normalize()
    {
        DistanceScale = Math.Clamp(DistanceScale, 0.1, 4.0);
        AnimationTimeMs = Math.Clamp(AnimationTimeMs, 1.0, 2000.0);
        AccelerationWindowMs = Math.Clamp(AccelerationWindowMs, 0.0, 500.0);
        AccelerationMax = Math.Clamp(AccelerationMax, 1.0, 20.0);
        TailToHeadRatio = Math.Clamp(TailToHeadRatio, 0.1, 10.0);
    }
}

public sealed class InputSettingsModel
{
    [JsonPropertyName("horizontal_smoothing")]
    public bool HorizontalSmoothing { get; set; } = true;

    [JsonPropertyName("shift_for_horizontal")]
    public bool ShiftForHorizontal { get; set; } = true;

    [JsonPropertyName("reverse_direction")]
    public bool ReverseDirection { get; set; }

    [JsonPropertyName("pass_through_ctrl")]
    public bool PassThroughCtrl { get; set; } = true;

    [JsonPropertyName("pass_through_alt")]
    public bool PassThroughAlt { get; set; } = true;

    [JsonPropertyName("bypass_high_resolution")]
    public bool BypassHighResolution { get; set; } = true;
}

public sealed class SystemSettingsModel
{
    [JsonPropertyName("start_with_windows")]
    public bool StartWithWindows { get; set; }

    [JsonPropertyName("show_tray_icon")]
    public bool ShowTrayIcon { get; set; } = true;
}

public sealed class AppProfileModel
{
    [JsonPropertyName("executable")]
    public string Executable { get; set; } = string.Empty;

    [JsonPropertyName("enabled")]
    public bool Enabled { get; set; } = true;

    [JsonPropertyName("compatibility_mode")]
    public bool CompatibilityMode { get; set; }

    [JsonPropertyName("motion")]
    public MotionSettingsModel Motion { get; set; } = new();
}

public sealed class AppSettingsModel
{
    [JsonPropertyName("schema_version")]
    public int SchemaVersion { get; set; } = 1;

    [JsonPropertyName("enabled")]
    public bool Enabled { get; set; } = true;

    [JsonPropertyName("motion")]
    public MotionSettingsModel Motion { get; set; } = new();

    [JsonPropertyName("input")]
    public InputSettingsModel Input { get; set; } = new();

    [JsonPropertyName("system")]
    public SystemSettingsModel System { get; set; } = new();

    [JsonPropertyName("excluded_apps")]
    public List<string> ExcludedApps { get; set; } = [];

    [JsonPropertyName("profiles")]
    public List<AppProfileModel> Profiles { get; set; } = [];

    public void Normalize()
    {
        SchemaVersion = 1;
        Motion.Normalize();

        ExcludedApps = ExcludedApps
            .Select(NormalizeExecutable)
            .Where(static value => value.Length > 0)
            .Distinct(StringComparer.OrdinalIgnoreCase)
            .OrderBy(static value => value, StringComparer.OrdinalIgnoreCase)
            .ToList();

        Profiles = Profiles
            .Select(profile =>
            {
                profile.Executable = NormalizeExecutable(profile.Executable);
                profile.Motion.Normalize();
                return profile;
            })
            .Where(static profile => profile.Executable.Length > 0)
            .GroupBy(static profile => profile.Executable, StringComparer.OrdinalIgnoreCase)
            .Select(static group => group.First())
            .ToList();
    }

    public static string NormalizeExecutable(string value)
    {
        try
        {
            return Path.GetFileName(value.Trim()).ToLowerInvariant();
        }
        catch (ArgumentException)
        {
            return string.Empty;
        }
    }
}

public sealed class EngineDiagnosticsModel
{
    [JsonPropertyName("physical_events")]
    public long PhysicalEvents { get; set; }

    [JsonPropertyName("smoothed_events")]
    public long SmoothedEvents { get; set; }

    [JsonPropertyName("passed_events")]
    public long PassedEvents { get; set; }

    [JsonPropertyName("injected_events")]
    public long InjectedEvents { get; set; }

    [JsonPropertyName("injected_delta")]
    public long InjectedDelta { get; set; }

    [JsonPropertyName("queue_overflows")]
    public long QueueOverflows { get; set; }

    [JsonPropertyName("injection_failures")]
    public long InjectionFailures { get; set; }

    [JsonPropertyName("target_changes")]
    public long TargetChanges { get; set; }

    [JsonPropertyName("settings_generation")]
    public long SettingsGeneration { get; set; }
}

public sealed class EngineResponseModel
{
    [JsonPropertyName("ok")]
    public bool Ok { get; set; }

    [JsonPropertyName("error")]
    public string Error { get; set; } = string.Empty;

    [JsonPropertyName("settings")]
    public AppSettingsModel? Settings { get; set; }

    [JsonPropertyName("diagnostics")]
    public EngineDiagnosticsModel? Diagnostics { get; set; }
}
