#pragma once

#include <algorithm>
#include <atomic>

class MicrophoneActivity {
public:
	static constexpr float kSilenceTimeoutSeconds = 2.0f;
	static constexpr float kAudibleThresholdDb = -60.0f;

	void reset()
	{
		audio_observed_.store(false, std::memory_order_release);
		silence_elapsed_seconds_ = 0.0f;
		active_ = true;
	}

	void observe(float input_peak_db)
	{
		if (input_peak_db > kAudibleThresholdDb)
			audio_observed_.store(true, std::memory_order_release);
	}

	bool tick(float seconds)
	{
		if (audio_observed_.exchange(false, std::memory_order_acq_rel))
			silence_elapsed_seconds_ = 0.0f;
		else
			silence_elapsed_seconds_ = std::min(kSilenceTimeoutSeconds,
				silence_elapsed_seconds_ + std::max(0.0f, seconds));

		const bool next_active = silence_elapsed_seconds_ < kSilenceTimeoutSeconds;
		const bool changed = active_ != next_active;
		active_ = next_active;
		return changed;
	}

	bool active() const { return active_; }

private:
	std::atomic<bool> audio_observed_{false};
	float silence_elapsed_seconds_ = 0.0f;
	bool active_ = true;
};
