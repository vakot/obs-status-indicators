#include "windows-overlay-renderer.hpp"

#include <obs-module.h>

#include <algorithm>
#include <cstdint>

#include <plugin-support.h>

namespace {
constexpr wchar_t kWindowClassName[] = L"OBSStatusIndicatorsOverlay";

const wchar_t *label_for(IndicatorKind kind)
{
	switch (kind) {
	case IndicatorKind::Paused:
		return L"PAUSED";
	case IndicatorKind::Recording:
		return L"REC";
	case IndicatorKind::ReplayBuffer:
		return L"REPLAY";
	case IndicatorKind::Microphone:
		return L"MIC";
	case IndicatorKind::Saving:
		return L"SAVING";
	}
	return L"";
}
}

WindowsOverlayRenderer::~WindowsOverlayRenderer()
{
	stop();
}

bool WindowsOverlayRenderer::start()
{
	std::unique_lock<std::mutex> lock(mutex_);
	if (thread_.joinable())
		return initialized_success_;

	initialized_ = false;
	initialized_success_ = false;
	thread_ = std::thread(&WindowsOverlayRenderer::run, this);
	initialized_condition_.wait(lock, [this] { return initialized_; });
	const bool success = initialized_success_;
	lock.unlock();

	if (!success)
		stop();
	return success;
}

void WindowsOverlayRenderer::stop()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!thread_.joinable())
			return;
		if (thread_id_ != 0)
			PostThreadMessageW(thread_id_, WM_QUIT, 0, 0);
	}

	thread_.join();
	std::lock_guard<std::mutex> lock(mutex_);
	thread_id_ = 0;
	window_ = nullptr;
	initialized_ = false;
	initialized_success_ = false;
}

void WindowsOverlayRenderer::update_layout(const IndicatorLayout &layout)
{
	DWORD thread_id = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		pending_layout_ = layout;
		thread_id = thread_id_;
	}

	if (thread_id != 0)
		PostThreadMessageW(thread_id, kUpdateLayoutMessage, 0, 0);
}

void WindowsOverlayRenderer::run()
{
	{
		std::lock_guard<std::mutex> lock(mutex_);
		thread_id_ = GetCurrentThreadId();
	}

	const bool created = create_window();
	signal_initialized(created);
	if (!created) {
		destroy_window();
		return;
	}

	MSG message;
	while (GetMessageW(&message, nullptr, 0, 0) > 0) {
		if (message.message == kUpdateLayoutMessage)
			apply_pending_layout();
		else
			DispatchMessageW(&message);
	}

	destroy_window();
}

bool WindowsOverlayRenderer::create_window()
{
	const HINSTANCE instance = GetModuleHandleW(nullptr);
	WNDCLASSEXW window_class = {};
	window_class.cbSize = sizeof(window_class);
	window_class.hInstance = instance;
	window_class.lpfnWndProc = window_proc;
	window_class.lpszClassName = kWindowClassName;
	window_class.hCursor = LoadCursorW(nullptr, IDC_ARROW);

	if (!RegisterClassExW(&window_class) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
		obs_log(LOG_ERROR, "overlay class registration failed: %lu", GetLastError());
		return false;
	}

	window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
		kWindowClassName, L"OBS Status Indicators", WS_POPUP, 0, 0, kWidth, kRowHeight, nullptr,
		nullptr, instance, this);
	if (!window_) {
		obs_log(LOG_ERROR, "overlay window creation failed: %lu", GetLastError());
		return false;
	}

	IndicatorLayout initial_layout;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		initial_layout = pending_layout_;
	}
	if (!render_layout(initial_layout))
		return false;

	if (!SetWindowDisplayAffinity(window_, WDA_EXCLUDEFROMCAPTURE)) {
		obs_log(LOG_WARNING, "capture exclusion unavailable: %lu", GetLastError());
	} else {
		obs_log(LOG_INFO, "capture exclusion enabled for overlay");
	}

	const int screen_width = GetSystemMetrics(SM_CXSCREEN);
	const int screen_height = GetSystemMetrics(SM_CYSCREEN);
	const int x = screen_width - kWidth - kMargin > 0 ? screen_width - kWidth - kMargin : 0;
	const int y = screen_height - kRowHeight - kMargin > 0 ? screen_height - kRowHeight - kMargin : 0;
	SetWindowPos(window_, HWND_TOPMOST, x, y, kWidth,
		kRowHeight, SWP_NOACTIVATE | SWP_HIDEWINDOW);
	obs_log(LOG_INFO, "overlay window ready at primary-display bottom-right");
	return true;
}

void WindowsOverlayRenderer::destroy_window()
{
	if (window_)
		DestroyWindow(window_);
	window_ = nullptr;
	UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
}

bool WindowsOverlayRenderer::render_layout(const IndicatorLayout &layout)
{
	const int row_count = static_cast<int>(layout.entries.size());
	const int height = row_count > 0 ? row_count * kRowHeight : kRowHeight;
	const int width = kWidth;
	HDC screen_dc = GetDC(nullptr);
	HDC memory_dc = CreateCompatibleDC(screen_dc);
	if (!screen_dc || !memory_dc) {
		if (memory_dc)
			DeleteDC(memory_dc);
		if (screen_dc)
			ReleaseDC(nullptr, screen_dc);
		obs_log(LOG_ERROR, "overlay surface creation failed: %lu", GetLastError());
		return false;
	}

	BITMAPINFO bitmap_info = {};
	bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
	bitmap_info.bmiHeader.biWidth = width;
	bitmap_info.bmiHeader.biHeight = -height;
	bitmap_info.bmiHeader.biPlanes = 1;
	bitmap_info.bmiHeader.biBitCount = 32;
	bitmap_info.bmiHeader.biCompression = BI_RGB;

	void *pixels = nullptr;
	HBITMAP bitmap = CreateDIBSection(memory_dc, &bitmap_info, DIB_RGB_COLORS, &pixels, nullptr, 0);
	if (!bitmap || !pixels) {
		if (bitmap)
			DeleteObject(bitmap);
		DeleteDC(memory_dc);
		ReleaseDC(nullptr, screen_dc);
		obs_log(LOG_ERROR, "overlay bitmap creation failed: %lu", GetLastError());
		return false;
	}

	const auto background = static_cast<std::uint32_t>(0xFF20252B);
	std::fill_n(static_cast<std::uint32_t *>(pixels), width * height, background);
	const HGDIOBJ previous_bitmap = SelectObject(memory_dc, bitmap);
	for (int index = 0; index < row_count; ++index) {
		if (layout.entries[index].muted) {
			const auto muted_background = static_cast<std::uint32_t>(0xFFC04040);
			std::fill_n(static_cast<std::uint32_t *>(pixels) + index * width * kRowHeight,
				width * kRowHeight, muted_background);
		}
	}

	SetBkMode(memory_dc, TRANSPARENT);
	SetTextColor(memory_dc, RGB(255, 255, 255));
	HFONT font = CreateFontW(18, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
		OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
	if (font) {
		const HGDIOBJ previous_font = SelectObject(memory_dc, font);
		for (int index = 0; index < row_count; ++index) {
			RECT text_rect = {16, index * kRowHeight, width - 8, (index + 1) * kRowHeight};
			DrawTextW(memory_dc, label_for(layout.entries[index].kind), -1, &text_rect,
				DT_SINGLELINE | DT_VCENTER);
		}
		SelectObject(memory_dc, previous_font);
		DeleteObject(font);
	}

	POINT destination = {};
	RECT window_rect = {};
	GetWindowRect(window_, &window_rect);
	destination.x = window_rect.left;
	destination.y = window_rect.top;
	SIZE size = {width, height};
	POINT source = {0, 0};
	BLENDFUNCTION blend = {AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
	const BOOL updated = UpdateLayeredWindow(window_, screen_dc, &destination, &size, memory_dc,
		&source, 0, &blend, ULW_ALPHA);

	SelectObject(memory_dc, previous_bitmap);
	DeleteObject(bitmap);
	DeleteDC(memory_dc);
	ReleaseDC(nullptr, screen_dc);

	if (!updated) {
		obs_log(LOG_ERROR, "overlay paint failed: %lu", GetLastError());
		return false;
	}

	const int screen_width = GetSystemMetrics(SM_CXSCREEN);
	const int screen_height = GetSystemMetrics(SM_CYSCREEN);
	const int x = screen_width - width - kMargin > 0 ? screen_width - width - kMargin : 0;
	const int y = screen_height - height - kMargin > 0 ? screen_height - height - kMargin : 0;
	SetWindowPos(window_, HWND_TOPMOST, x, y, width, height, SWP_NOACTIVATE);
	ShowWindow(window_, row_count > 0 ? SW_SHOWNOACTIVATE : SW_HIDE);
	return true;
}

void WindowsOverlayRenderer::apply_pending_layout()
{
	IndicatorLayout layout;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		layout = pending_layout_;
	}

	if (!render_layout(layout))
		obs_log(LOG_WARNING, "overlay layout update failed");
}

void WindowsOverlayRenderer::signal_initialized(bool success)
{
	std::lock_guard<std::mutex> lock(mutex_);
	initialized_success_ = success;
	initialized_ = true;
	initialized_condition_.notify_one();
}

LRESULT CALLBACK WindowsOverlayRenderer::window_proc(HWND window, UINT message, WPARAM wparam,
	LPARAM lparam)
{
	if (message == WM_NCCREATE) {
		const auto *create = reinterpret_cast<CREATESTRUCTW *>(lparam);
		SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
	}

	switch (message) {
	case WM_NCHITTEST:
		return HTTRANSPARENT;
	case WM_MOUSEACTIVATE:
		return MA_NOACTIVATE;
	case WM_ERASEBKGND:
		return 1;
	case WM_CLOSE:
		DestroyWindow(window);
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		return DefWindowProcW(window, message, wparam, lparam);
	}
}
