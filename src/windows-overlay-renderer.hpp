#pragma once

#include <windows.h>

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

private:
	static constexpr int kWidth = 176;
	static constexpr int kHeight = 48;
	static constexpr int kMargin = 8;

	static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam);

	void run();
	bool create_window();
	void destroy_window();
	bool render_marker();
	void signal_initialized(bool success);

	std::thread thread_;
	std::mutex mutex_;
	std::condition_variable initialized_condition_;
	bool initialized_ = false;
	bool initialized_success_ = false;
	DWORD thread_id_ = 0;
	HWND window_ = nullptr;
};
