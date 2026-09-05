#include "windows-overlay-renderer.hpp"

#include <obs-module.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>

#include <QColor>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

#include <plugin-support.h>

namespace {
constexpr wchar_t kWindowClassName[] = L"OBSStatusIndicatorsOverlay";
std::atomic<WindowsOverlayRenderer *> event_hook_renderer = nullptr;
constexpr int kIconPadding = 8;
constexpr int kIndicatorOpacity = 204;
const char *icon_file_for(const IndicatorEntry &entry)
{
	switch (entry.kind) {
	case IndicatorKind::Paused:
		return "icons/lucide/pause.svg";
	case IndicatorKind::Recording:
		return "icons/lucide/circle-dot.svg";
	case IndicatorKind::ReplayBuffer:
		return "icons/lucide/repeat-2.svg";
	case IndicatorKind::Microphone:
		return entry.muted ? "icons/lucide/mic-off.svg" : "icons/lucide/mic.svg";
	case IndicatorKind::Saving:
		return "icons/lucide/save.svg";
	}
	return nullptr;
}

bool render_lucide_icon(QImage &tile, const IndicatorEntry &entry)
{
	const char *relative_path = icon_file_for(entry);
	char *icon_path = obs_module_file(relative_path);
	if (!icon_path) {
		obs_log(LOG_WARNING, "indicator icon path unavailable: %s", relative_path);
		return false;
	}

	const QString path = QString::fromUtf8(icon_path);
	bfree(icon_path);
	QSvgRenderer renderer(path);
	if (!renderer.isValid()) {
		obs_log(LOG_WARNING, "indicator icon load failed: %s", relative_path);
		return false;
	}

	QPainter painter(&tile);
	renderer.render(&painter,
		QRectF(kIconPadding, kIconPadding, tile.width() - 2 * kIconPadding,
			tile.height() - 2 * kIconPadding));
	return true;
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

	SetWindowPos(window_, HWND_TOPMOST, 0, 0, kIndicatorSize,
		kIndicatorSize, SWP_NOACTIVATE | SWP_HIDEWINDOW);
	obs_log(LOG_INFO, "overlay window ready at primary-display top-left");
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

	const auto background = static_cast<std::uint32_t>(0x00000000);
	std::fill_n(static_cast<std::uint32_t *>(pixels), width * height, background);
	const HGDIOBJ previous_bitmap = SelectObject(memory_dc, bitmap);
	for (int index = 0; index < row_count; ++index) {
		const int top = index * (kIndicatorSize + kIndicatorGap);
		QImage tile(kIndicatorSize, kIndicatorSize, QImage::Format_ARGB32_Premultiplied);
		tile.fill(QColor(0, 0, 0, kIndicatorOpacity));
		render_lucide_icon(tile, layout.entries[index]);
		std::memcpy(static_cast<std::uint8_t *>(pixels) +
				static_cast<size_t>(top) * width * sizeof(std::uint32_t), tile.constBits(),
				static_cast<size_t>(tile.bytesPerLine()) * tile.height());
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

	SetWindowPos(window_, HWND_TOPMOST, 0, 0, width, height, SWP_NOACTIVATE);
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
