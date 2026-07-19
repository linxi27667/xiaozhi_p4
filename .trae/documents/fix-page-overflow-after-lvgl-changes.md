# 修复计划:智能家居页面右侧内容溢出屏幕

## 1. Summary

修复"页面右侧内容溢出屏幕"的问题(用户截图:设置页/网络页/总览页内容均向右溢出)。
保留现有的 `lv_pct(100)` 布局写法,仅在 `UI_Manager_Init` 末尾和 `SwitchShellPage` 中显式调用
`lv_obj_update_layout()` 触发完整布局链,让 `lv_pct(100)` 在 HIDDEN → 可见的转换中被正确解算。

## 2. Current State Analysis

### 2.1 调用链

```
LcdDisplay::CreateSmartHomeShell()              [lcd_display.cc:588]
  └─ smart_home_page_ = lv_obj_create(screen);  // pos=(128,0), size=(896,600), HIDDEN
  └─ EnsureSmartHomeUi();                       [lcd_display.cc:496-503]
       └─ UI_Manager_Init(smart_home_page_);    [ui_manager.c:235]
            └─ create_embedded_content_area(scr);  [ui_manager.c:220-233]
                 └─ content = lv_obj_create(scr);
                 └─ lv_obj_set_size(content, lv_pct(100), lv_pct(100));  ← 关键点
                 └─ s_app_ctx.content_parent = content;

LcdDisplay::SwitchShellPage(ShellPage page)     [lcd_display.cc:505-587]
  └─ lv_obj_remove_flag(smart_home_page_, LV_OBJ_FLAG_HIDDEN);
  └─ lv_obj_set_pos(smart_home_page_, kShellSidebarWidth, 0);
  └─ lv_obj_set_size(smart_home_page_, 896, 600);
  └─ lv_obj_update_layout(smart_home_page_);    ← 只更新 scr 自己
  └─ EnsureSmartHomeUi();                       // smart_home_ui_initialized_=true,跳过
  └─ UI_Manager_Switch_Page(UI_PAGE_xxx);
  └─ UI_Manager_Poll();
       └─ do_switch_page();                    [ui_manager.c:123-147]
            └─ page_xxx_create(s_app_ctx.content_parent);
```

### 2.2 根因

1. `CreateSmartHomeShell` 调用 `EnsureSmartHomeUi` 时 `smart_home_page_` 是 **HIDDEN**。
2. `create_embedded_content_area` 在 HIDDEN 状态下创建 `content_parent`,用 `lv_pct(100)`。
3. LVGL 中 HIDDEN 父对象的 `lv_pct(100)` 子对象可能不参与 layout 计算,`content_parent` 的 width
   在初次创建时未被正确解算(实测解为 0 或异常值,不是 896)。
4. `SwitchShellPage` 中 `lv_obj_update_layout(smart_home_page_)` 只触发 scr 自己的 layout 链,
   但 `content_parent` 已存在,它的 `lv_pct(100)` 缓存未刷新。
5. 50ms 后 `do_switch_page` 创建 page,page 的 `lv_pct(100)` 跟随 `content_parent` 的旧缓存 width
   (不是 896),导致 page 实际渲染宽度小于预期(目测 ~600-700px),看起来内容溢出屏幕。

### 2.3 截图佐证

- 设置页:设置项右端(slider、"中文"、"xiao...")被屏幕右边缘裁掉,左侧距侧栏 ~150px,
  右侧距屏幕右边 ~250px(空白被裁掉)
- 网络页:"Broker"、"Client ID"、"8.134.1..."、"xiao..."、"1F 离线 2F..." 均被裁
- 总览页:"AI 赋能设计"、"天气"卡、"5 设备 0..." 溢出

## 3. Proposed Changes

### 3.1 文件:`main/smart_home/ui/ui_manager.c`

**修改 A:在 `UI_Manager_Init` 末尾,embedded 模式下强制 update_layout 链**

在 `UI_Manager_Init` 函数末尾(`ESP_LOGI` 之前)添加:

```c
/* embedded 模式:content_parent 在 scr HIDDEN 状态下创建,
   lv_pct(100) 未被正确解算。强制 update_layout 链触发解算,
   让 content_parent 拿到 scr 的真实 896x600 尺寸。 */
if (embedded && s_app_ctx.content_parent) {
    lv_obj_update_layout(scr);
    /* 同时 update content_parent 确保 lv_pct(100) 解析到 scr 尺寸 */
    lv_obj_update_layout(s_app_ctx.content_parent);
}
```

**位置:** [ui_manager.c:285-289](file:///e:/MCU/esp32/p4/xiaozhi-for-p4/main/smart_home/ui/ui_manager.c#L285-289) 之间

**为什么:** 即使 `smart_home_page_` 仍 HIDDEN,`lv_obj_update_layout` 会刷新对象 coords,
让 `lv_pct(100)` 解算为 896×600(基于 scr 已 set_size 896×600)。后续 `SwitchShellPage` 解除
HIDDEN 时 coords 已正确,page 创建后 100% 跟随 content_parent(896),不再溢出。

### 3.2 文件:`main/display/lcd_display.cc`

**修改 B:在 `SwitchShellPage` 中 `EnsureSmartHomeUi()` 之后、`UI_Manager_Switch_Page` 之前
强制 update_layout**

```c
EnsureSmartHomeUi();
/* 强制刷新 content_parent 的 layout,确保 lv_pct(100) 在 HIDDEN→可见转换中
   正确解算到 896x600。否则 50ms 后创建的 page 会按旧 width 渲染导致溢出。 */
if (s_app_ctx.content_parent) {
    lv_obj_update_layout(smart_home_page_);
    lv_obj_update_layout(s_app_ctx.content_parent);
}
```

**位置:** [lcd_display.cc:551](file:///e:/MCU/esp32/p4/xiaozhi-for-p4/main/display/lcd_display.cc#L551) 后

**为什么:** 双保险。即使 Init 中的 update_layout 因为某种原因未生效,SwitchShellPage 解除
HIDDEN 后再 update_layout 一次,确保 page 创建前 content_parent 是 896×600。

**注意:** `s_app_ctx.content_parent` 是 C 模块的内部状态,需要在 lcd_display.cc 中暴露或
通过现有接口访问。**最简洁方式**:在 `ui_manager.h` 中加一个 helper:

```c
/* ui_manager.h */
void UI_Manager_Force_Layout(void);
```

```c
/* ui_manager.c */
void UI_Manager_Force_Layout(void) {
    if (s_app_ctx.content_parent) {
        lv_obj_update_layout(s_app_ctx.content_parent);
    }
}
```

`lcd_display.cc` 调用 `UI_Manager_Force_Layout()`,无需 include 内部状态。

### 3.3 不修改

- `create_embedded_content_area` 的 `lv_pct(100)` 写法(用户选择保留百分比)
- `ui_kit.c` 各 helper(它们的 `lv_pct(100)` 在 layout 正确时表现良好)
- 各个 `page_*.c`(硬编码 width 的如 page_data.c 已在项目规则 10.1 列入已知问题,
  此次只解决溢出,不重写 page)
- 从机固件、协议层、UI 主题色板

## 4. Assumptions & Decisions

1. **保留 `lv_pct(100)` 写法** — 用户明确选择方案 B,代码改动最小、风险最低
2. **不重构 page_data 等硬编码 width** — 项目规则 10.1 已记录为已知问题,本次仅修溢出
3. **使用 helper 函数 `UI_Manager_Force_Layout`** — 避免 lcd_display.cc 直接访问
   `s_app_ctx.content_parent`,保持 ui_manager 模块封装
4. **触发两次 update_layout** — Init 一次(预热)+ SwitchShellPage 一次(运行时),
   冗余但安全,代价仅一次 layout 遍历(~ms 级)

## 5. Verification Steps

1. 编译 P4 主机固件,确认无编译错误
2. 烧录并启动设备
3. 切换到"设置"页,检查所有设置项右端 slider/箭头/文字是否完整显示
4. 切换到"网络"页,检查 Broker、Client ID、控制器状态右端是否完整
5. 切换到"总览"页,检查标题、AI 赋能 logo、天气卡右端是否完整
6. 切换"灯光/场景/环境/控制"页,确认每页右侧无溢出
7. 切换语言(中→英→中),确认 layout 在重建后仍正常
8. 烧录前:用 `idf.py build` 确认无 warning
