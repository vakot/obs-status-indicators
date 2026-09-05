# Post-MVP plugin settings investigation

## Verdict

Adding persistent custom settings is **possible** and fits the existing Windows/Qt architecture. The plugin is a module-only frontend plugin, so the settings should be exposed through an OBS Tools-menu action that opens a plugin-owned Qt dialog.

## OBS mechanisms reviewed

### Automatic properties UI

OBS's `obs_properties_t` API describes settings for libobs objects such as sources, filters, encoders, outputs, and services. The OBS frontend can generate widgets from those properties when an object exposes a `get_properties` callback.

This plugin does not register a libobs object; it exports `obs_module_load()` and owns a native overlay. Adding `obs_properties_t` alone would therefore not create a visible plugin settings page.

Reference: [OBS properties API](https://docs.obsproject.com/reference-properties) and [OBS plugin properties guide](https://github.com/obsproject/obs-studio/blob/master/docs/sphinx/plugins.rst).

### Plugin-owned settings dialog

The frontend API provides `obs_frontend_add_tools_menu_qaction()` and `obs_frontend_get_main_window()`. A plugin can add a Tools action, connect its Qt `triggered` signal, and show a `QDialog` parented to the OBS main window.

This is the same integration shape used by OBS WebSocket for its settings dialog. The repository's Qt build already links Qt Core, Gui, and Svg; Qt Widgets must also be linked for `QDialog`, layouts, controls, and `QColorDialog`.

References: [OBS frontend API](https://docs.obsproject.com/reference-frontend-api) and [OBS WebSocket settings action](https://github.com/obsproject/obs-websocket/blob/master/src/obs-websocket.cpp).

### Persistent storage

The module API provides `obs_module_config_path(file)`, which resolves a file below the OBS plugin configuration directory. The plugin can create that directory, load JSON using `obs_data_create_from_json_file_safe()`, and save atomically with `obs_data_save_json_safe()`.

The configuration is plugin-owned and installation/user scoped rather than scene-item scoped. The initial file is expected at:

```text
config/obs-studio/plugin_config/obs-status-indicators/settings.json
```

Reference: [OBS module API](https://github.com/obsproject/obs-studio/blob/master/libobs/obs-module.h) and [OBS data settings API](https://docs.obsproject.com/reference-settings).

## Runtime ownership

The dialog runs on OBS's Qt UI thread. It must not access the overlay `HWND` directly. Applying settings should copy an immutable settings snapshot into `WindowsOverlayRenderer`, which already owns a dedicated UI thread and private message queue.

The renderer should combine the latest layout snapshot and settings snapshot in its existing update message. This keeps OBS callbacks and Qt controls away from Win32 window operations.

## Scope boundary

This phase covers position origin, orientation, offset, gap, background RGBA color, and icon RGBA color. It does not add monitor selection, per-indicator settings, animation, or a general settings framework.
