#include "obs-state-provider.hpp"

#include <obs.h>
#include <callback/calldata.h>

#include <cstring>

#include <plugin-support.h>

namespace {
constexpr char kMicrophoneSourceId[] = "wasapi_input_capture";
constexpr float kSavingDisplaySeconds = 1.0f;

struct microphone_search {
	obs_source_t *preferred = nullptr;
	obs_source_t *fallback = nullptr;
};

bool find_microphone(void *data, obs_source_t *source)
{
	if (!source)
		return true;

	if ((obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) == 0)
		return true;

	if (!static_cast<microphone_search *>(data)->fallback)
		static_cast<microphone_search *>(data)->fallback = obs_source_get_ref(source);

	if (std::strcmp(obs_source_get_id(source), kMicrophoneSourceId) == 0) {
		static_cast<microphone_search *>(data)->preferred = obs_source_get_ref(source);
		return false;
	}

	return true;
}
}

ObsStateProvider::ObsStateProvider(StateCallback callback, void *context)
	: callback_(callback), context_(context)
{
}

ObsStateProvider::~ObsStateProvider()
{
	stop();
	clear_microphone();
	clear_replay_output();
	if (saving_tick_registered_) {
		obs_remove_tick_callback(saving_tick, this);
		saving_tick_registered_ = false;
	}
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
	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED:
		if (event == OBS_FRONTEND_EVENT_REPLAY_BUFFER_SAVED)
			begin_saving();
		refresh_objects();
		reconcile(false);
		break;
	default:
		break;
	}
}

void ObsStateProvider::reconcile(bool force_publish)
{
	const IndicatorState next = read_state();
	publish(next, force_publish);
}

IndicatorState ObsStateProvider::state() const
{
	std::lock_guard<std::mutex> lock(mutex_);
	return state_;
}

IndicatorState ObsStateProvider::read_state() const
{
	IndicatorState state;
	state.recording = obs_frontend_recording_active();
	state.recordingPaused = state.recording && obs_frontend_recording_paused();
	state.replayBuffer = obs_frontend_replay_buffer_active();
	{
		std::lock_guard<std::mutex> lock(mutex_);
		state.microphoneAvailable = microphone_ != nullptr && obs_source_enabled(microphone_);
		state.microphoneMuted = microphone_ != nullptr && obs_source_muted(microphone_);
		state.saving = state_.saving;
	}
	return state;
}

void ObsStateProvider::publish(const IndicatorState &next, bool force_publish)
{
	StateCallback callback = nullptr;
	void *context = nullptr;
	IndicatorState snapshot;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!force_publish && next == state_)
			return;
		state_ = next;
		snapshot = state_;
		callback = callback_;
		context = context_;
	}

	if (callback)
		callback(snapshot, context);
}

void ObsStateProvider::refresh_objects()
{
	resolve_microphone();
	resolve_replay_output();
}

void ObsStateProvider::resolve_microphone()
{
	microphone_search search;
	obs_enum_sources(find_microphone, &search);
	obs_source_t *next = search.preferred ? search.preferred : search.fallback;
	if (search.preferred && search.fallback)
		obs_source_release(search.fallback);

	if (microphone_ && next && std::strcmp(obs_source_get_uuid(microphone_), obs_source_get_uuid(next)) == 0) {
		obs_source_release(next);
		return;
	}

	clear_microphone();
	microphone_ = next;
	if (!microphone_) {
		obs_log(LOG_WARNING, "microphone source unavailable");
		set_microphone_state(false, false);
		return;
	}

	signal_handler_t *signals = obs_source_get_signal_handler(microphone_);
	for (const char *signal : {"mute", "rename", "update", "enable", "audio_activate", "audio_deactivate"})
		signal_handler_connect(signals, signal, microphone_state_signal, this);
	for (const char *signal : {"destroy", "remove"})
		signal_handler_connect(signals, signal, microphone_lifetime_signal, this);
	set_microphone_state(obs_source_enabled(microphone_), obs_source_muted(microphone_));
	obs_log(LOG_INFO, "microphone source resolved by stable source type");
}

void ObsStateProvider::resolve_replay_output()
{
	obs_output_t *next = obs_frontend_get_replay_buffer_output();
	if (replay_output_ == next) {
		if (next)
			obs_output_release(next);
		return;
	}

	clear_replay_output();
	replay_output_ = next;
	if (!replay_output_) {
		obs_log(LOG_WARNING, "Replay Buffer output unavailable; saving indicator disabled");
		return;
	}

	signal_handler_connect(obs_output_get_signal_handler(replay_output_), "saved", replay_saved_signal,
		this);
	obs_log(LOG_INFO, "Replay Buffer saved signal connected");
}

void ObsStateProvider::clear_microphone()
{
	if (!microphone_)
		return;

	signal_handler_t *signals = obs_source_get_signal_handler(microphone_);
	for (const char *signal : {"mute", "rename", "update", "enable", "audio_activate", "audio_deactivate"})
		signal_handler_disconnect(signals, signal, microphone_state_signal, this);
	for (const char *signal : {"destroy", "remove"})
		signal_handler_disconnect(signals, signal, microphone_lifetime_signal, this);
	obs_source_release(microphone_);
	microphone_ = nullptr;
}

void ObsStateProvider::clear_replay_output()
{
	if (!replay_output_)
		return;

	signal_handler_disconnect(obs_output_get_signal_handler(replay_output_), "saved", replay_saved_signal,
		this);
	obs_output_release(replay_output_);
	replay_output_ = nullptr;
}

void ObsStateProvider::microphone_state_signal(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	auto *provider = static_cast<ObsStateProvider *>(data);
	provider->set_microphone_state(provider->microphone_ && obs_source_enabled(provider->microphone_),
		provider->microphone_ && obs_source_muted(provider->microphone_));
}

void ObsStateProvider::microphone_lifetime_signal(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	static_cast<ObsStateProvider *>(data)->handle_microphone_lifetime_signal();
}

void ObsStateProvider::handle_microphone_lifetime_signal()
{
	obs_log(LOG_WARNING, "microphone source removed or destroyed");
	set_microphone_state(false, false);
}

void ObsStateProvider::set_microphone_state(bool available, bool muted)
{
	IndicatorState next;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		next = state_;
		next.microphoneAvailable = available;
		next.microphoneMuted = muted;
	}
	publish(next, false);
}

void ObsStateProvider::replay_saved_signal(void *data, calldata_t *params)
{
	UNUSED_PARAMETER(params);
	obs_log(LOG_INFO, "Replay Buffer saved signal received");
	UNUSED_PARAMETER(data);
}

void ObsStateProvider::begin_saving()
{
	bool register_tick = false;
	IndicatorState next;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		next = state_;
		next.saving = true;
		saving_remaining_seconds_ = kSavingDisplaySeconds;
		if (!saving_tick_registered_) {
			saving_tick_registered_ = true;
			register_tick = true;
		}
	}
	if (register_tick)
		obs_add_tick_callback(saving_tick, this);
	publish(next, false);
}

void ObsStateProvider::saving_tick(void *data, float seconds)
{
	static_cast<ObsStateProvider *>(data)->handle_saving_tick(seconds);
}

void ObsStateProvider::handle_saving_tick(float seconds)
{
	bool remove_tick = false;
	IndicatorState next;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!saving_tick_registered_)
			return;
		saving_remaining_seconds_ -= seconds;
		next = state_;
		if (saving_remaining_seconds_ <= 0.0f) {
			next.saving = false;
			saving_tick_registered_ = false;
			remove_tick = true;
		}
	}
	if (remove_tick)
		obs_remove_tick_callback(saving_tick, this);
	publish(next, false);
}
