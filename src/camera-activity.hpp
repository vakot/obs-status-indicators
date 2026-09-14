#pragma once

#include <algorithm>

class CameraActivity {
public:
	static constexpr float kOutputTimeoutSeconds = 1.0f;

	void observe()
	{
		frame_observed_ = true;
	}

	bool tick(float seconds)
	{
		if (frame_observed_) {
			frame_observed_ = false;
			silence_elapsed_seconds_ = 0.0f;
		} else {
			silence_elapsed_seconds_ = std::min(kOutputTimeoutSeconds,
				silence_elapsed_seconds_ + std::max(0.0f, seconds));
		}

		const bool next_active = silence_elapsed_seconds_ < kOutputTimeoutSeconds;
		const bool changed = active_ != next_active;
		active_ = next_active;
		return changed;
	}

	bool active() const { return active_; }

private:
	bool frame_observed_ = false;
	float silence_elapsed_seconds_ = kOutputTimeoutSeconds;
	bool active_ = false;
};
