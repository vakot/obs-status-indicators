#include "indicator-controller.hpp"

IndicatorLayout IndicatorController::build_layout(const IndicatorState &state)
{
	IndicatorLayout layout;
	layout.entries.reserve(5);

	if (state.recordingPaused)
		layout.entries.push_back({IndicatorKind::Paused});
	else if (state.recording && state.replayBuffer)
		layout.entries.push_back({IndicatorKind::RecordingReplay});
	else if (state.recording)
		layout.entries.push_back({IndicatorKind::Recording});
	else if (state.replayBuffer)
		layout.entries.push_back({IndicatorKind::ReplayBuffer});

	if (state.microphoneAvailable)
		layout.entries.push_back({IndicatorKind::Microphone, state.microphoneMuted});

	if (state.saving)
		layout.entries.push_back({IndicatorKind::Saving});

	return layout;
}
