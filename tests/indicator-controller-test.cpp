#include "indicator-controller.hpp"

#include <cassert>

namespace {
void assert_kind(const IndicatorLayout &layout, std::size_t index, IndicatorKind expected)
{
	assert(index < layout.entries.size());
	assert(layout.entries[index].kind == expected);
}

void assert_muted(const IndicatorLayout &layout, std::size_t index, bool expected)
{
	assert(index < layout.entries.size());
	assert(layout.entries[index].muted == expected);
}
}

int main()
{
	IndicatorState state;
	assert(IndicatorController::build_layout(state).entries.empty());

	state.recording = true;
	IndicatorLayout layout = IndicatorController::build_layout(state);
	assert(layout.entries.size() == 1);
	assert_kind(layout, 0, IndicatorKind::Recording);

	state.recordingPaused = true;
	layout = IndicatorController::build_layout(state);
	assert(layout.entries.size() == 1);
	assert_kind(layout, 0, IndicatorKind::Paused);

	state.replayBuffer = true;
	state.microphoneAvailable = true;
	state.saving = true;
	layout = IndicatorController::build_layout(state);
	assert(layout.entries.size() == 4);
	assert_kind(layout, 0, IndicatorKind::Paused);
	assert_kind(layout, 1, IndicatorKind::ReplayBuffer);
	assert_kind(layout, 2, IndicatorKind::Microphone);
	assert_muted(layout, 2, false);
	assert_kind(layout, 3, IndicatorKind::Saving);

	state.microphoneMuted = true;
	layout = IndicatorController::build_layout(state);
	assert_kind(layout, 2, IndicatorKind::Microphone);
	assert_muted(layout, 2, true);

	state.recordingPaused = false;
	state.recording = false;
	layout = IndicatorController::build_layout(state);
	assert(layout.entries.size() == 3);
	assert_kind(layout, 0, IndicatorKind::ReplayBuffer);
	assert_kind(layout, 1, IndicatorKind::Microphone);
	assert_kind(layout, 2, IndicatorKind::Saving);

	state.replayBuffer = false;
	layout = IndicatorController::build_layout(state);
	assert(layout.entries.size() == 2);
	assert_kind(layout, 0, IndicatorKind::Microphone);
	assert_kind(layout, 1, IndicatorKind::Saving);

	state.microphoneAvailable = false;
	layout = IndicatorController::build_layout(state);
	assert(layout.entries.size() == 1);
	assert_kind(layout, 0, IndicatorKind::Saving);
	return 0;
}
