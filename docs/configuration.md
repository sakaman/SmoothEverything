# 配置说明

配置文件：`%LocalAppData%\SmoothEverything\settings.json`。

当前 schema 为 `1`。控制面板会自动保存；如需手工编辑，请先退出引擎并保留 `schema_version`。

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

## 运动字段

| 字段 | 范围 | 含义 |
|---|---:|---|
| `distance_scale` | 0.1–4.0 | 每格滚轮的总位移倍率 |
| `animation_time_ms` | 1–2000 | 单个脉冲持续时间 |
| `acceleration_window_ms` | 0–500 | 连续输入保持加速的最大间隔 |
| `acceleration_max` | 1–20 | 连滚加速上限 |
| `tail_to_head_ratio` | 0.1–10 | 收尾相对起步的时间权重 |
| `easing_enabled` | bool | 是否使用五次平滑曲线 |

超出范围的有限数值会被钳制；无效类型使用默认值。可执行文件字段只保留不区分大小写的基名，重复项会去除。
