#pragma once

#include "indicator-controller.hpp"
#include "overlay-settings.hpp"

#include <windows.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

class WindowsOverlayRenderer {
public:
	WindowsOverlayRenderer() = default;
	~WindowsOverlayRenderer();

	WindowsOverlayRenderer(const WindowsOverlayRenderer &) = delete;
	WindowsOverlayRenderer &operator=(const WindowsOverlayRenderer &) = delete;

	bool start();
	void stop();
	void update_layout(const IndicatorLayout &layout);
	void update_settings(const OverlaySettings &settings);

private:
	static constexpr UINT kUpdateLayoutMessage = WM_APP + 1;
	static constexpr UINT kReassertTopmostMessage = WM_APP + 2;

	static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);
	static void CALLBACK win_event_proc(HWINEVENTHOOK hook, DWORD event, HWND window, LONG object_id,
		LONG child_id, DWORD event_thread, DWORD event_time);

	void run();
	bool create_window();
	void destroy_window();
	bool install_z_order_hooks();
	void uninstall_z_order_hooks();
	bool render_layout(const IndicatorLayout &layout, const OverlaySettings &settings);
	void apply_pending_layout();
	void request_topmost_reassertion();
	void reassert_topmost();
	void signal_initialized(bool success);

	std::thread thread_;
	std::mutex mutex_;
	std::condition_variable initialized_condition_;
	bool initialized_ = false;
	bool initialized_success_ = false;
	DWORD thread_id_ = 0;
	HWND window_ = nullptr;
	IndicatorLayout pending_layout_;
	OverlaySettings pending_settings_;
	HWINEVENTHOOK foreground_event_hook_ = nullptr;
	HWINEVENTHOOK object_event_hook_ = nullptr;
	std::atomic_bool topmost_reassertion_pending_ = false;
};
