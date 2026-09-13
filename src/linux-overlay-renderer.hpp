#pragma once

#include "indicator-controller.hpp"
#include "overlay-settings.hpp"

#include <mutex>

class LinuxOverlayWindow;

class LinuxOverlayRenderer {
public:
	LinuxOverlayRenderer() = default;
	~LinuxOverlayRenderer();

	LinuxOverlayRenderer(const LinuxOverlayRenderer &) = delete;
	LinuxOverlayRenderer &operator=(const LinuxOverlayRenderer &) = delete;

	bool start();
	void stop();
	void update_layout(const IndicatorLayout &layout);
	void update_settings(const OverlaySettings &settings);

private:
	bool start_on_gui_thread();
	void stop_on_gui_thread();

	std::mutex mutex_;
	LinuxOverlayWindow *window_ = nullptr;
};
