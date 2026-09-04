#include <obs-frontend-api.h>
#include <obs-module.h>

#include <new>

#include <obs-state-provider.hpp>
#include <indicator-controller.hpp>
#include <plugin-support.h>
#include <windows-overlay-renderer.hpp>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static ObsStateProvider *state_provider = nullptr;
static WindowsOverlayRenderer *overlay_renderer = nullptr;

static void state_changed(const IndicatorState &state, void *context)
{
	UNUSED_PARAMETER(context);

	obs_log(LOG_INFO, "state snapshot: recording=%d paused=%d replay=%d", state.recording,
		state.recordingPaused, state.replayBuffer);
	const IndicatorLayout layout = IndicatorController::build_layout(state);
	if (overlay_renderer)
		overlay_renderer->update_layout(layout);
}

bool obs_module_load(void)
{
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
	if (overlay_renderer)
		overlay_renderer->update_layout(IndicatorController::build_layout(state_provider->state()));

	obs_log(LOG_INFO, "plugin loaded; state provider started");
	return true;
}

void obs_module_unload(void)
{
	delete overlay_renderer;
	overlay_renderer = nullptr;
	delete state_provider;
	state_provider = nullptr;
	obs_log(LOG_INFO, "plugin unloaded; state provider destroyed");
}
