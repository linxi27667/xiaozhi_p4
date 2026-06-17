# Smart Home UI TF Card Assets

The smart-home UI can optionally load PNG assets from:

```text
/sdcard/xiaozhi_ui/
```

LVGL drive `S:` is registered at runtime by `ui_asset_service`, so a file such as:

```text
/sdcard/xiaozhi_ui/color_wheel_220.png
```

can be used as:

```c
lv_image_set_src(img, "S:color_wheel_220.png");
```

Recommended assets for the design mockup:

| File | Size | Purpose |
| --- | --- | --- |
| `logo_robot.png` | 96x96 | Sidebar/brand robot |
| `floor_1.png` | 120x84 | First-floor overview thumbnail |
| `floor_2.png` | 120x84 | Second-floor overview thumbnail |
| `floor_3.png` | 120x84 | Third-floor overview thumbnail |
| `scene_home.png` | 96x72 | Home scene illustration |
| `scene_sleep.png` | 96x72 | Sleep scene illustration |
| `scene_movie.png` | 96x72 | Movie scene illustration |
| `scene_rain.png` | 96x72 | Rain collection scene illustration |
| `scene_fire.png` | 96x72 | Fire demo warning illustration |
| `weather_cloud.png` | 96x72 | Outdoor weather card |
| `alarm_siren.png` | 96x72 | Fire alarm popup |
| `color_wheel_220.png` | 220x220 | RGB light color wheel |

Keep PNGs transparent where possible. Avoid full-screen 1024x600 images; dynamic values and touch targets should stay as LVGL widgets.
