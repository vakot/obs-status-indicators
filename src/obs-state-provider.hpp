#pragma once

#include "indicator-state.hpp"

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

	const IndicatorState &state() const { return state_; }

private:
	static void frontend_event(enum obs_frontend_event event, void *data);

	void handle_frontend_event(enum obs_frontend_event event);
	void reconcile(bool force_publish);
	IndicatorState read_state() const;
	void publish();

	StateCallback callback_ = nullptr;
	void *context_ = nullptr;
	IndicatorState state_;
	bool registered_ = false;
};
