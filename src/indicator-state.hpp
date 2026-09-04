#pragma once

struct IndicatorState {
	bool recording = false;
	bool recordingPaused = false;
	bool replayBuffer = false;
	bool microphoneAvailable = false;
	bool microphoneMuted = false;
	bool saving = false;

	bool operator==(const IndicatorState &other) const
	{
		return recording == other.recording && recordingPaused == other.recordingPaused &&
		       replayBuffer == other.replayBuffer &&
		       microphoneAvailable == other.microphoneAvailable &&
		       microphoneMuted == other.microphoneMuted && saving == other.saving;
	}

	bool operator!=(const IndicatorState &other) const { return !(*this == other); }
};
