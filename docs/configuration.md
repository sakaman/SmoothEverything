# Configuration

Settings file: `%LocalAppData%\SmoothEverything\settings.json`.

The current schema version is `1`. The control panel saves automatically. Before editing the file manually, exit the engine and preserve `schema_version`.

```json
{
  "schema_version": 1,
  "enabled": true,
  "motion": {
    "distance_scale": 1.0,
    "animation_time_ms": 360.0,
    "acceleration_window_ms": 70.0,
    "acceleration_max": 7.0,
    "tail_to_head_ratio": 3.0,
    "easing_enabled": true
  },
  "input": {
    "horizontal_smoothing": true,
    "shift_for_horizontal": true,
    "reverse_direction": false,
    "pass_through_ctrl": true,
    "pass_through_alt": true,
    "bypass_high_resolution": true
  },
  "system": {
    "ui_language": "system",
    "start_with_windows": false,
    "show_tray_icon": true
  },
  "excluded_apps": ["game.exe"],
  "profiles": [
    {
      "executable": "browser.exe",
      "enabled": true,
      "compatibility_mode": false,
      "motion": {
        "distance_scale": 1.1,
        "animation_time_ms": 300.0,
        "acceleration_window_ms": 70.0,
        "acceleration_max": 6.0,
        "tail_to_head_ratio": 3.0,
        "easing_enabled": true
      }
    }
  ]
}
```

## Motion Fields

| Field | Range | Meaning |
|---|---:|---|
| `distance_scale` | 0.1–4.0 | Total displacement multiplier for each wheel step |
| `animation_time_ms` | 1–2000 | Duration of one impulse |
| `acceleration_window_ms` | 0–500 | Maximum gap that preserves repeated-scroll acceleration |
| `acceleration_max` | 1–20 | Repeated-scroll acceleration limit |
| `tail_to_head_ratio` | 0.1–10 | Relative timing weight of the ending versus the start |
| `easing_enabled` | bool | Whether to use the quintic easing curve |

Finite numbers outside these ranges are clamped; invalid types use defaults. Executable fields retain only case-insensitive basenames, and duplicates are removed.

## Language Setting

`system.ui_language` controls the native control panel language. Supported values are `system`, `en`, and `zh-CN`. `system` selects Simplified Chinese for a Simplified Chinese Windows locale and English otherwise. Unsupported values normalize to `system`, and missing translations fall back to English. The installer chooses its own language separately.
