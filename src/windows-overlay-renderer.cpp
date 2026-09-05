#include "windows-overlay-renderer.hpp"

#include <obs-module.h>

#include <algorithm>
#include <atomic>
#include <cstdint>

#include <plugin-support.h>

namespace {
constexpr wchar_t kWindowClassName[] = L"OBSStatusIndicatorsOverlay";
std::atomic<WindowsOverlayRenderer *> event_hook_renderer = nullptr;
constexpr COLORREF kIconWhite = RGB(255, 255, 255);
constexpr COLORREF kMutedRed = RGB(235, 72, 72);

void draw_recording_icon(HDC device_context, const RECT &bounds)
{
	const int left = bounds.left + 8;
	const int top = bounds.top + 8;
	const int right = bounds.right - 8;
	const int bottom = bounds.bottom - 8;
	HPEN pen = CreatePen(PS_SOLID, 4, kIconWhite);
	HGDIOBJ previous_pen = SelectObject(device_context, pen);
	HGDIOBJ previous_brush = SelectObject(device_context, GetStockObject(NULL_BRUSH));
	RECT body = {left, top + 8, right - 14, bottom - 8};
	RoundRect(device_context, body.left, body.top, body.right, body.bottom, 8, 8);
	POINT lens[] = {{right - 14, top + 18}, {right, top + 12}, {right, bottom - 12}};
	Polygon(device_context, lens, 3);
	SelectObject(device_context, previous_brush);
	SelectObject(device_context, previous_pen);
	DeleteObject(pen);
}

void draw_paused_icon(HDC device_context, const RECT &bounds)
{
	const int center = (bounds.left + bounds.right) / 2;
	const int top = bounds.top + 8;
	const int bottom = bounds.bottom - 8;
	HBRUSH brush = CreateSolidBrush(kIconWhite);
	HGDIOBJ previous_brush = SelectObject(device_context, brush);
	RECT left_bar = {center - 15, top, center - 5, bottom};
	RECT right_bar = {center + 5, top, center + 15, bottom};
	RoundRect(device_context, left_bar.left, left_bar.top, left_bar.right, left_bar.bottom, 4, 4);
	RoundRect(device_context, right_bar.left, right_bar.top, right_bar.right, right_bar.bottom, 4, 4);
	SelectObject(device_context, previous_brush);
	DeleteObject(brush);
}

void draw_replay_icon(HDC device_context, const RECT &bounds)
{
	const int left = bounds.left + 8;
	const int top = bounds.top + 8;
	const int right = bounds.right - 8;
	const int bottom = bounds.bottom - 8;
	HPEN pen = CreatePen(PS_SOLID, 4, kIconWhite);
	HBRUSH brush = CreateSolidBrush(kIconWhite);
	HGDIOBJ previous_pen = SelectObject(device_context, pen);
	HGDIOBJ previous_brush = SelectObject(device_context, GetStockObject(NULL_BRUSH));
	Arc(device_context, left, top, right, bottom, right - 8, top + 12, left + 12, bottom - 8);
	SelectObject(device_context, brush);
	POINT arrow[] = {{right - 9, top + 11}, {right - 25, top + 10}, {right - 12, top + 25}};
	Polygon(device_context, arrow, 3);
	SelectObject(device_context, previous_brush);
	SelectObject(device_context, previous_pen);
	DeleteObject(brush);
	DeleteObject(pen);
}

void draw_microphone_icon(HDC device_context, const RECT &bounds, bool muted)
{
	const int center = (bounds.left + bounds.right) / 2;
	const int top = bounds.top + 8;
	const int bottom = bounds.bottom - 8;
	HPEN pen = CreatePen(PS_SOLID, 4, kIconWhite);
	HGDIOBJ previous_pen = SelectObject(device_context, pen);
	HGDIOBJ previous_brush = SelectObject(device_context, GetStockObject(NULL_BRUSH));
	RoundRect(device_context, center - 10, top, center + 10, bottom - 17, 10, 10);
	MoveToEx(device_context, center - 19, bottom - 25, nullptr);
	LineTo(device_context, center - 19, bottom - 20);
	Arc(device_context, center - 19, bottom - 28, center + 19, bottom - 2, center - 19,
		bottom - 15, center + 19, bottom - 15);
	MoveToEx(device_context, center, bottom - 2, nullptr);
	LineTo(device_context, center, bottom - 15);
	MoveToEx(device_context, center - 14, bottom - 2, nullptr);
	LineTo(device_context, center + 14, bottom - 2);
	SelectObject(device_context, previous_brush);
	SelectObject(device_context, previous_pen);
	DeleteObject(pen);

	if (muted) {
		pen = CreatePen(PS_SOLID, 5, kMutedRed);
		previous_pen = SelectObject(device_context, pen);
		MoveToEx(device_context, bounds.left + 10, bounds.bottom - 10, nullptr);
		LineTo(device_context, bounds.right - 10, bounds.top + 10);
		SelectObject(device_context, previous_pen);
		DeleteObject(pen);
	}
}

void draw_saving_icon(HDC device_context, const RECT &bounds)
{
	const int center = (bounds.left + bounds.right) / 2;
	const int top = bounds.top + 8;
	const int bottom = bounds.bottom - 8;
	HPEN pen = CreatePen(PS_SOLID, 4, kIconWhite);
	HBRUSH brush = CreateSolidBrush(kIconWhite);
	HGDIOBJ previous_pen = SelectObject(device_context, pen);
	HGDIOBJ previous_brush = SelectObject(device_context, GetStockObject(NULL_BRUSH));
	RoundRect(device_context, bounds.left + 9, top, bounds.right - 9, bottom, 5, 5);
	MoveToEx(device_context, center, top + 9, nullptr);
	LineTo(device_context, center, bottom - 16);
	SelectObject(device_context, brush);
	POINT arrow[] = {{center - 13, bottom - 19}, {center + 13, bottom - 19}, {center, bottom - 6}};
	Polygon(device_context, arrow, 3);
	SelectObject(device_context, previous_brush);
	SelectObject(device_context, previous_pen);
	DeleteObject(brush);
	DeleteObject(pen);
}

void draw_indicator_icon(HDC device_context, const IndicatorEntry &entry, const RECT &bounds)
{
	switch (entry.kind) {
	case IndicatorKind::Paused:
		draw_paused_icon(device_context, bounds);
		break;
	case IndicatorKind::Recording:
		draw_recording_icon(device_context, bounds);
		break;
	case IndicatorKind::ReplayBuffer:
		draw_replay_icon(device_context, bounds);
		break;
	case IndicatorKind::Microphone:
		draw_microphone_icon(device_context, bounds, entry.muted);
		break;
	case IndicatorKind::Saving:
		draw_saving_icon(device_context, bounds);
		break;
	}
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
		else if (message.message == kReassertTopmostMessage)
			reassert_topmost();
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
		kWindowClassName, L"OBS Status Indicators", WS_POPUP, 0, 0, kIndicatorSize, kIndicatorSize, nullptr,
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
	if (!install_z_order_hooks())
		obs_log(LOG_WARNING, "topmost order recovery hooks unavailable; overlay remains best effort");

	const int screen_height = GetSystemMetrics(SM_CYSCREEN);
	const int y = screen_height - kIndicatorSize - kMargin > 0 ? screen_height - kIndicatorSize - kMargin : 0;
	SetWindowPos(window_, HWND_TOPMOST, kMargin, y, kIndicatorSize,
		kIndicatorSize, SWP_NOACTIVATE | SWP_HIDEWINDOW);
	obs_log(LOG_INFO, "overlay window ready at primary-display bottom-left");
	return true;
}

void WindowsOverlayRenderer::destroy_window()
{
	uninstall_z_order_hooks();
	if (window_)
		DestroyWindow(window_);
	window_ = nullptr;
	UnregisterClassW(kWindowClassName, GetModuleHandleW(nullptr));
}

bool WindowsOverlayRenderer::install_z_order_hooks()
{
	WindowsOverlayRenderer *expected = nullptr;
	if (!event_hook_renderer.compare_exchange_strong(expected, this))
		return false;

	constexpr DWORD hook_flags = WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS;
	foreground_event_hook_ = SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND, nullptr,
		&WindowsOverlayRenderer::win_event_proc, 0, 0, hook_flags);
	object_event_hook_ = SetWinEventHook(EVENT_OBJECT_SHOW, EVENT_OBJECT_REORDER, nullptr,
		&WindowsOverlayRenderer::win_event_proc, 0, 0, hook_flags);

	if (!foreground_event_hook_ || !object_event_hook_) {
		obs_log(LOG_WARNING, "topmost order recovery hook registration failed: %lu", GetLastError());
		uninstall_z_order_hooks();
		return false;
	}

	obs_log(LOG_INFO, "topmost order recovery hooks enabled");
	return true;
}

void WindowsOverlayRenderer::uninstall_z_order_hooks()
{
	if (foreground_event_hook_) {
		UnhookWinEvent(foreground_event_hook_);
		foreground_event_hook_ = nullptr;
	}
	if (object_event_hook_) {
		UnhookWinEvent(object_event_hook_);
		object_event_hook_ = nullptr;
	}

	WindowsOverlayRenderer *expected = this;
	event_hook_renderer.compare_exchange_strong(expected, nullptr);
	topmost_reassertion_pending_.store(false, std::memory_order_release);
}

bool WindowsOverlayRenderer::render_layout(const IndicatorLayout &layout)
{
	const int row_count = static_cast<int>(layout.entries.size());
	const int height = row_count > 0 ? row_count * kIndicatorSize + (row_count - 1) * kIndicatorGap
						 : kIndicatorSize;
	const int width = kIndicatorSize;
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

	const auto background = static_cast<std::uint32_t>(0xFF000000);
	std::fill_n(static_cast<std::uint32_t *>(pixels), width * height, background);
	const HGDIOBJ previous_bitmap = SelectObject(memory_dc, bitmap);
	for (int index = 0; index < row_count; ++index) {
		const int top = index * (kIndicatorSize + kIndicatorGap);
		RECT indicator_rect = {0, top, width, top + kIndicatorSize};
		draw_indicator_icon(memory_dc, layout.entries[index], indicator_rect);
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

	const int screen_height = GetSystemMetrics(SM_CYSCREEN);
	const int y = screen_height - height - kMargin > 0 ? screen_height - height - kMargin : 0;
	SetWindowPos(window_, HWND_TOPMOST, kMargin, y, width, height, SWP_NOACTIVATE);
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

void WindowsOverlayRenderer::request_topmost_reassertion()
{
	DWORD thread_id = 0;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		if (!window_ || thread_id_ == 0)
			return;
		thread_id = thread_id_;
	}

	bool expected = false;
	if (!topmost_reassertion_pending_.compare_exchange_strong(expected, true,
		std::memory_order_acq_rel))
		return;

	if (!PostThreadMessageW(thread_id, kReassertTopmostMessage, 0, 0))
		topmost_reassertion_pending_.store(false, std::memory_order_release);
}

void WindowsOverlayRenderer::reassert_topmost()
{
	topmost_reassertion_pending_.store(false, std::memory_order_release);
	if (!window_)
		return;

	if (!SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE))
		obs_log(LOG_WARNING, "overlay topmost order recovery failed: %lu", GetLastError());
}

void CALLBACK WindowsOverlayRenderer::win_event_proc(HWINEVENTHOOK, DWORD event, HWND, LONG object_id, LONG,
	DWORD, DWORD)
{
	if (event != EVENT_SYSTEM_FOREGROUND &&
		(event != EVENT_OBJECT_SHOW && event != EVENT_OBJECT_HIDE && event != EVENT_OBJECT_REORDER))
		return;
	if (event != EVENT_SYSTEM_FOREGROUND && object_id != OBJID_WINDOW)
		return;

	WindowsOverlayRenderer *renderer = event_hook_renderer.load(std::memory_order_acquire);
	if (renderer)
		renderer->request_topmost_reassertion();
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
