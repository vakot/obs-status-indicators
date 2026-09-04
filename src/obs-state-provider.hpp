#pragma once

#include "indicator-state.hpp"

#include <mutex>

#include <obs-frontend-api.h>

class ObsStateProvider {
public:
	using StateCallback = void (*)(const IndicatorState &state, void *context);

	ObsStateProvider(StateCallback callback, void *context);
	~ObsStateProvider();

	ObsStateProvider(const ObsStateProvider &) = delete;
	ObsStateProvider &operator=(const ObsStateProvider &) = delete;

	bool start();
	void stop();

	IndicatorState state() const;

private:
	static void frontend_event(enum obs_frontend_event event, void *data);
	static void microphone_state_signal(void *data, calldata_t *params);
	static void microphone_lifetime_signal(void *data, calldata_t *params);
	static void replay_saved_signal(void *data, calldata_t *params);
	static void saving_tick(void *data, float seconds);

	void handle_frontend_event(enum obs_frontend_event event);
	void reconcile(bool force_publish);
	void refresh_objects();
	void resolve_microphone();
	void resolve_replay_output();
	void clear_microphone();
	void clear_replay_output();
	void set_microphone_state(bool available, bool muted);
	void handle_microphone_lifetime_signal();
	void begin_saving();
	void handle_saving_tick(float seconds);
	IndicatorState read_state() const;
	void publish(const IndicatorState &next, bool force_publish);

	StateCallback callback_ = nullptr;
	void *context_ = nullptr;
	IndicatorState state_;
	bool registered_ = false;
	obs_source_t *microphone_ = nullptr;
	obs_output_t *replay_output_ = nullptr;
	bool saving_tick_registered_ = false;
	float saving_remaining_seconds_ = 0.0f;
	mutable std::mutex mutex_;
};
