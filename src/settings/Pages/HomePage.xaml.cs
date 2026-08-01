using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using SmoothEverything.Settings.Models;
using SmoothEverything.Settings.Services;

namespace SmoothEverything.Settings.Pages;

public sealed partial class HomePage : Page
{
    private readonly SettingsSession _session = SettingsSession.Instance;
    private AppSettingsModel? _boundSettings;
    private bool _updating = true;

    public HomePage()
    {
        InitializeComponent();
        _updating = false;
        UpdateLabels();
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
        var motion = _session.Current.Motion;
        _updating = true;
        EnabledToggle.IsOn = _session.Current.Enabled;
        DistanceSlider.Value = motion.DistanceScale;
        DurationSlider.Value = motion.AnimationTimeMs;
        AccelerationWindowSlider.Value = motion.AccelerationWindowMs;
        AccelerationMaxSlider.Value = motion.AccelerationMax;
        TailRatioSlider.Value = motion.TailToHeadRatio;
        EasingToggle.IsOn = motion.EasingEnabled;
        UpdateLabels();
        _updating = false;
    }

    private void EnabledToggle_Toggled(object sender, RoutedEventArgs e)
    {
        if (_updating)
        {
            return;
        }
        _session.Current.Enabled = EnabledToggle.IsOn;
        _session.QueueApply();
    }

    private void MotionSlider_ValueChanged(object sender, object e)
    {
        if (_updating)
        {
            return;
        }
        UpdateLabels();
        WriteMotionToModel();
        _session.QueueApply();
    }

    private void EasingToggle_Toggled(object sender, RoutedEventArgs e)
    {
        if (_updating)
        {
            return;
        }
        _session.Current.Motion.EasingEnabled = EasingToggle.IsOn;
        _session.QueueApply();
    }

    private void Preset_Click(object sender, RoutedEventArgs e)
    {
        if (sender is not Button { Tag: string preset })
        {
            return;
        }

        var motion = preset switch
        {
            "responsive" => new MotionSettingsModel
            {
                DistanceScale = 0.9,
                AnimationTimeMs = 180,
                AccelerationWindowMs = 55,
                AccelerationMax = 4,
                TailToHeadRatio = 1.8,
            },
            "smooth" => new MotionSettingsModel
            {
                DistanceScale = 1.1,
                AnimationTimeMs = 520,
                AccelerationWindowMs = 90,
                AccelerationMax = 8,
                TailToHeadRatio = 4.5,
            },
            "classic" => new MotionSettingsModel
            {
                DistanceScale = 1.0,
                AnimationTimeMs = 420,
                AccelerationWindowMs = 80,
                AccelerationMax = 7,
                TailToHeadRatio = 3.5,
            },
            _ => new MotionSettingsModel(),
        };
        _session.Current.Motion = motion;
        _boundSettings = null;
        UpdateFromModel(force: true);
        _session.QueueApply();
    }

    private void WriteMotionToModel()
    {
        var motion = _session.Current.Motion;
        motion.DistanceScale = DistanceSlider.Value;
        motion.AnimationTimeMs = DurationSlider.Value;
        motion.AccelerationWindowMs = AccelerationWindowSlider.Value;
        motion.AccelerationMax = AccelerationMaxSlider.Value;
        motion.TailToHeadRatio = TailRatioSlider.Value;
    }

    private void UpdateLabels()
    {
        DistanceValue.Text = $"{DistanceSlider.Value:0.00}×";
        DurationValue.Text = $"{DurationSlider.Value:0} ms";
        AccelerationWindowValue.Text = $"{AccelerationWindowSlider.Value:0} ms";
        AccelerationMaxValue.Text = $"{AccelerationMaxSlider.Value:0.0}×";
        TailRatioValue.Text = $"{TailRatioSlider.Value:0.0}:1";
    }
}
