#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QAction>
#include <QMainWindow>

#include <new>

#include <obs-state-provider.hpp>
#include <indicator-controller.hpp>
#include <overlay-settings-dialog.hpp>
#include <overlay-settings.hpp>
#include <plugin-support.h>
#include <windows-overlay-renderer.hpp>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static ObsStateProvider *state_provider = nullptr;
static WindowsOverlayRenderer *overlay_renderer = nullptr;
static OverlaySettings overlay_settings;
static OverlaySettingsDialog *settings_dialog = nullptr;
static QAction *settings_action = nullptr;

static void apply_overlay_settings(const OverlaySettings &settings)
{
	overlay_settings = normalize_overlay_settings(settings);
	if (!save_overlay_settings(overlay_settings))
		obs_log(LOG_WARNING, "overlay settings could not be saved");
	if (overlay_renderer)
		overlay_renderer->update_settings(overlay_settings);
}

static void show_settings_dialog()
{
	if (!settings_dialog)
		return;
	settings_dialog->show();
	settings_dialog->raise();
	settings_dialog->activateWindow();
}

static void state_changed(const IndicatorState &state, void *context)
{
	UNUSED_PARAMETER(context);

	obs_log(LOG_INFO, "state snapshot: recording=%d paused=%d replay=%d mic=%d muted=%d saving=%d",
		state.recording, state.recordingPaused, state.replayBuffer, state.microphoneAvailable,
		state.microphoneMuted, state.saving);
	const IndicatorLayout layout = IndicatorController::build_layout(state);
	if (overlay_renderer)
		overlay_renderer->update_layout(layout);
}

bool obs_module_load(void)
{
	overlay_settings = load_overlay_settings();
	obs_log(LOG_INFO, "overlay settings loaded: origin=%d orientation=%d offset=%d gap=%d opacity=%d background=0x%08x icon=0x%08x",
		static_cast<int>(overlay_settings.origin), static_cast<int>(overlay_settings.orientation),
		overlay_settings.offset, overlay_settings.gap, overlay_settings.opacity,
		overlay_settings.background_color,
		overlay_settings.icon_color);
	state_provider = new (std::nothrow) ObsStateProvider(state_changed, nullptr);
	if (!state_provider) {
		obs_log(LOG_ERROR, "state provider allocation failed");
		return false;
	}

	if (!state_provider->start()) {
		delete state_provider;
		state_provider = nullptr;
		obs_log(LOG_ERROR, "state provider startup failed");
		return false;
	}

	overlay_renderer = new (std::nothrow) WindowsOverlayRenderer();
	if (!overlay_renderer || !overlay_renderer->start()) {
		obs_log(LOG_WARNING, "overlay startup failed; continuing without overlay");
		delete overlay_renderer;
		overlay_renderer = nullptr;
	}
	if (overlay_renderer) {
		overlay_renderer->update_settings(overlay_settings);
		overlay_renderer->update_layout(IndicatorController::build_layout(state_provider->state()));
	}

	QMainWindow *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (main_window) {
		settings_action = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction(
			obs_module_text("SettingsMenu")));
		obs_frontend_push_ui_translation(obs_module_get_string);
		settings_dialog = new (std::nothrow)
			OverlaySettingsDialog(main_window, overlay_settings, apply_overlay_settings);
		obs_frontend_pop_ui_translation();

		if (settings_dialog && settings_action)
			QObject::connect(settings_action, &QAction::triggered, show_settings_dialog);
		else
			obs_log(LOG_WARNING, "settings dialog could not be initialized");
	} else {
		obs_log(LOG_WARNING, "OBS main window unavailable; settings menu not added");
	}

	obs_log(LOG_INFO, "plugin loaded; state provider started");
	return true;
}

void obs_module_unload(void)
{
	delete settings_dialog;
	settings_dialog = nullptr;
	settings_action = nullptr;
	delete overlay_renderer;
	overlay_renderer = nullptr;
	delete state_provider;
	state_provider = nullptr;
	obs_log(LOG_INFO, "plugin unloaded; state provider destroyed");
}
