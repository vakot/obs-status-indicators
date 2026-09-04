#include <obs-frontend-api.h>
#include <obs-module.h>

#include <new>

#include <obs-state-provider.hpp>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

static ObsStateProvider *state_provider = nullptr;

static void state_changed(const IndicatorState &state, void *context)
{
	UNUSED_PARAMETER(context);

	obs_log(LOG_INFO, "state snapshot: recording=%d paused=%d replay=%d", state.recording,
		state.recordingPaused, state.replayBuffer);
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

	obs_log(LOG_INFO, "plugin loaded; state provider started");
	return true;
}

void obs_module_unload(void)
{
	delete state_provider;
	state_provider = nullptr;
	obs_log(LOG_INFO, "plugin unloaded; state provider destroyed");
}
