#include "obs-state-provider.hpp"

#include <plugin-support.h>

ObsStateProvider::ObsStateProvider(StateCallback callback, void *context)
	: callback_(callback), context_(context)
{
}

ObsStateProvider::~ObsStateProvider()
{
	stop();
}

bool ObsStateProvider::start()
{
	if (registered_)
		return true;

	obs_frontend_add_event_callback(frontend_event, this);
	registered_ = true;
	reconcile(true);
	obs_log(LOG_INFO, "state provider started; initial state reconciled");
	return true;
}

void ObsStateProvider::stop()
{
	if (!registered_)
		return;

	obs_frontend_remove_event_callback(frontend_event, this);
	registered_ = false;
	obs_log(LOG_INFO, "state provider stopped; frontend callback removed");
}

void ObsStateProvider::frontend_event(enum obs_frontend_event event, void *data)
{
	if (!data)
		return;

	static_cast<ObsStateProvider *>(data)->handle_frontend_event(event);
}

void ObsStateProvider::handle_frontend_event(enum obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
	case OBS_FRONTEND_EVENT_RECORDING_PAUSED:
	case OBS_FRONTEND_EVENT_RECORDING_UNPAUSED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTED:
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
	case OBS_FRONTEND_EVENT_PROFILE_CHANGING:
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		reconcile(false);
		break;
	default:
		break;
	}
}

void ObsStateProvider::reconcile(bool force_publish)
{
	const IndicatorState next = read_state();
	if (!force_publish && next == state_)
		return;

	state_ = next;
	publish();
}

IndicatorState ObsStateProvider::read_state() const
{
	IndicatorState state;
	state.recording = obs_frontend_recording_active();
	state.recordingPaused = state.recording && obs_frontend_recording_paused();
	state.replayBuffer = obs_frontend_replay_buffer_active();
	return state;
}

void ObsStateProvider::publish()
{
	if (callback_)
		callback_(state_, context_);
}
