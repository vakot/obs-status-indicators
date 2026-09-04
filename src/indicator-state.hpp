#pragma once

struct IndicatorState {
	bool recording = false;
	bool recordingPaused = false;
	bool replayBuffer = false;

	bool operator==(const IndicatorState &other) const
	{
		return recording == other.recording && recordingPaused == other.recordingPaused &&
		       replayBuffer == other.replayBuffer;
	}

	bool operator!=(const IndicatorState &other) const { return !(*this == other); }
};
