#include "linux-overlay-renderer.hpp"

#include <obs-module.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <utility>
#include <vector>

#include <QByteArray>
#include <QColor>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QImage>
#include <QMetaObject>
#include <QPaintEvent>
#include <QPainter>
#include <QRect>
#include <QScreen>
#include <QString>
#include <QSocketNotifier>
#include <QTemporaryFile>
#include <QThread>
#include <QWidget>

#include <overlay-renderer-utils.hpp>
#include <plugin-support.h>

#ifdef OBS_STATUS_INDICATORS_HAVE_WAYLAND_LAYER_SHELL
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include <wlr-layer-shell-unstable-v1-client-protocol.h>
#endif

class LinuxOverlaySurface {
public:
	virtual ~LinuxOverlaySurface() = default;

	virtual void set_layout(const IndicatorLayout &layout) = 0;
	virtual void set_settings(const OverlaySettings &settings) = 0;
	virtual QObject *qt_object() = 0;
};

namespace {

QImage render_indicator_surface(const IndicatorLayout &layout, const OverlaySettings &settings)
{
	const int entry_count = static_cast<int>(layout.entries.size());
	const bool horizontal = settings.orientation == OverlayOrientation::Horizontal;
	const int primary_size = entry_count * settings.indicator_size +
		(entry_count - 1) * settings.gap;
	const int width = horizontal ? primary_size : settings.indicator_size;
	const int height = horizontal ? settings.indicator_size : primary_size;

	QImage surface(width, height, QImage::Format_ARGB32_Premultiplied);
	surface.fill(Qt::transparent);
	QPainter painter(&surface);
	// Wayland compositors generally do not implement QWidget::setWindowOpacity.
	// Apply opacity to the pixels so the setting behaves consistently everywhere.
	painter.setOpacity(static_cast<qreal>(settings.opacity) / 100.0);
	for (int index = 0; index < entry_count; ++index) {
		const int left = horizontal ? index * (settings.indicator_size + settings.gap) : 0;
		const int top = horizontal ? 0 : index * (settings.indicator_size + settings.gap);
		QImage tile(settings.indicator_size, settings.indicator_size,
			QImage::Format_ARGB32_Premultiplied);
		tile.fill(QColor::fromRgba(settings.background_color));
		overlay_renderer_detail::render_lucide_icon(tile, layout.entries[index], settings.icon_color);
		painter.drawImage(left, top, tile);
	}
	painter.end();
	return surface;
}

class LinuxOverlayWindow final : public QWidget, public LinuxOverlaySurface {
public:
	LinuxOverlayWindow()
	{
		setAttribute(Qt::WA_TranslucentBackground);
		setAttribute(Qt::WA_NoSystemBackground);
		setAttribute(Qt::WA_ShowWithoutActivating);
		setAttribute(Qt::WA_TransparentForMouseEvents);
		setAutoFillBackground(false);
		setFocusPolicy(Qt::NoFocus);
		setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
			Qt::WindowDoesNotAcceptFocus | Qt::WindowTransparentForInput);
		setWindowTitle(QStringLiteral("OBS Status Indicators"));

		QGuiApplication *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
		connect(application, &QGuiApplication::primaryScreenChanged, this,
			[this](QScreen *screen) {
				connect_screen(screen);
				render();
			});
		connect_screen(QGuiApplication::primaryScreen());
	}

	void set_layout(const IndicatorLayout &layout) override
	{
		layout_ = layout;
		render();
	}

	void set_settings(const OverlaySettings &settings) override
	{
		settings_ = normalize_overlay_settings(settings);
		render();
	}

	QObject *qt_object() override { return this; }

protected:
	void paintEvent(QPaintEvent *event) override
	{
		UNUSED_PARAMETER(event);
		QPainter painter(this);
		painter.drawImage(0, 0, surface_);
	}

private:
	void connect_screen(QScreen *screen)
	{
		if (screen_ == screen)
			return;

		if (screen_geometry_connection_)
			disconnect(screen_geometry_connection_);
		screen_ = screen;
		if (screen_) {
			screen_geometry_connection_ = connect(screen_, &QScreen::geometryChanged, this,
				[this](const QRect &) { render(); });
		}
	}

	void render()
	{
		if (layout_.entries.empty()) {
			hide();
			return;
		}

		if (!screen_) {
			obs_log(LOG_WARNING, "Linux overlay has no primary screen");
			hide();
			return;
		}

		surface_ = render_indicator_surface(layout_, settings_);
		const int width = surface_.width();
		const int height = surface_.height();
		const QRect geometry = screen_->geometry();
		const bool right = settings_.origin == OverlayOrigin::TopRight ||
			settings_.origin == OverlayOrigin::BottomRight;
		const bool bottom = settings_.origin == OverlayOrigin::BottomLeft ||
			settings_.origin == OverlayOrigin::BottomRight;
		int x = settings_.offset;
		int y = settings_.offset;
		if (right)
			x = std::max(0, geometry.width() - width - settings_.offset);
		if (bottom)
			y = std::max(0, geometry.height() - height - settings_.offset);
		setGeometry(geometry.left() + x, geometry.top() + y, width, height);
		show();
		raise();
		update();
	}

	IndicatorLayout layout_;
	OverlaySettings settings_;
	QImage surface_;
	QScreen *screen_ = nullptr;
	QMetaObject::Connection screen_geometry_connection_;
};

#ifdef OBS_STATUS_INDICATORS_HAVE_WAYLAND_LAYER_SHELL

class LinuxWaylandOverlay final : public QObject, public LinuxOverlaySurface {
public:
	LinuxWaylandOverlay()
	{
		display_ = wl_display_connect(nullptr);
		if (!display_) {
			obs_log(LOG_WARNING, "Linux overlay could not connect to Wayland display");
			return;
		}

		registry_ = wl_display_get_registry(display_);
		if (!registry_)
			return;
		static const wl_registry_listener listener = {registry_global, registry_global_remove};
		wl_registry_add_listener(registry_, &listener, this);
		if (wl_display_roundtrip(display_) < 0 || !compositor_ || !shm_ || !layer_shell_) {
			obs_log(LOG_WARNING, "Wayland layer-shell is unavailable (compositor, shm, or protocol missing)");
			return;
		}

		surface_ = wl_compositor_create_surface(compositor_);
		if (!surface_)
			return;

		layer_surface_ = zwlr_layer_shell_v1_get_layer_surface(layer_shell_, surface_, nullptr, 3,
			"obs-status-indicators");
		if (!layer_surface_)
			return;

		static const zwlr_layer_surface_v1_listener layer_listener = {layer_configure, layer_closed};
		zwlr_layer_surface_v1_add_listener(layer_surface_, &layer_listener, this);
		if (zwlr_layer_shell_v1_get_version(layer_shell_) >= 2)
			zwlr_layer_surface_v1_set_layer(layer_surface_, 3);
		zwlr_layer_surface_v1_set_keyboard_interactivity(layer_surface_, 0);
		zwlr_layer_surface_v1_set_exclusive_zone(layer_surface_, 0);
		// The initial commit must describe a valid layer surface before the
		// compositor can send its first configure event.
		zwlr_layer_surface_v1_set_size(layer_surface_, 1, 1);
		zwlr_layer_surface_v1_set_anchor(layer_surface_, 1u | 4u);

		// An empty input region keeps the visual overlay from intercepting the desktop.
		wl_region *input_region = wl_compositor_create_region(compositor_);
		if (input_region) {
			wl_surface_set_input_region(surface_, input_region);
			wl_region_destroy(input_region);
		}

		wl_surface_commit(surface_);
		if (wl_display_roundtrip(display_) < 0 || !configured_)
			return;

		socket_notifier_ = new (std::nothrow) QSocketNotifier(wl_display_get_fd(display_),
			QSocketNotifier::Read, this);
		if (!socket_notifier_)
			return;
		connect(socket_notifier_, &QSocketNotifier::activated, this,
			[this](qintptr) { dispatch_wayland(); });
		valid_ = true;
	}

	~LinuxWaylandOverlay() override
	{
		if (socket_notifier_)
			socket_notifier_->setEnabled(false);
		for (auto &buffer : buffers_)
			buffer.reset();
		if (layer_surface_)
			zwlr_layer_surface_v1_destroy(layer_surface_);
		if (surface_)
			wl_surface_destroy(surface_);
		if (layer_shell_) {
			if (zwlr_layer_shell_v1_get_version(layer_shell_) >= 3)
				zwlr_layer_shell_v1_destroy(layer_shell_);
			else
				wl_proxy_destroy(reinterpret_cast<wl_proxy *>(layer_shell_));
		}
		if (shm_)
			wl_shm_destroy(shm_);
		if (compositor_)
			wl_compositor_destroy(compositor_);
		if (registry_)
			wl_registry_destroy(registry_);
		if (display_)
			wl_display_disconnect(display_);
	}

	bool valid() const { return valid_; }

	void set_layout(const IndicatorLayout &layout) override
	{
		layout_ = layout;
		render();
	}

	void set_settings(const OverlaySettings &settings) override
	{
		settings_ = normalize_overlay_settings(settings);
		render();
	}

	QObject *qt_object() override { return this; }

private:
	struct Buffer {
		wl_buffer *buffer = nullptr;
		void *mapping = nullptr;
		size_t mapping_size = 0;
		int fd = -1;
		bool released = false;

		static void released_callback(void *data, wl_buffer *)
		{
			static_cast<Buffer *>(data)->released = true;
		}

		~Buffer()
		{
			if (buffer)
				wl_buffer_destroy(buffer);
			if (mapping && mapping != MAP_FAILED)
				munmap(mapping, mapping_size);
			if (fd >= 0)
				close(fd);
		}
	};

	static void registry_global(void *data, wl_registry *registry, uint32_t name,
		const char *interface, uint32_t version)
	{
		auto *self = static_cast<LinuxWaylandOverlay *>(data);
		if (std::strcmp(interface, "wl_compositor") == 0 && !self->compositor_)
			self->compositor_ = static_cast<wl_compositor *>(wl_registry_bind(
				registry, name, &wl_compositor_interface, std::min(version, 4u)));
		else if (std::strcmp(interface, "wl_shm") == 0 && !self->shm_)
			self->shm_ = static_cast<wl_shm *>(wl_registry_bind(registry, name, &wl_shm_interface, 1));
		else if (std::strcmp(interface, "zwlr_layer_shell_v1") == 0 && !self->layer_shell_)
			self->layer_shell_ = static_cast<zwlr_layer_shell_v1 *>(wl_registry_bind(
				registry, name, &zwlr_layer_shell_v1_interface, std::min(version, 5u)));
	}

	static void registry_global_remove(void *, wl_registry *, uint32_t) {}

	static void layer_configure(void *data, zwlr_layer_surface_v1 *layer_surface, uint32_t serial,
		uint32_t, uint32_t)
	{
		auto *self = static_cast<LinuxWaylandOverlay *>(data);
		zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
		self->configured_ = true;
		self->render();
	}

	static void layer_closed(void *data, zwlr_layer_surface_v1 *)
	{
		static_cast<LinuxWaylandOverlay *>(data)->closed_ = true;
	}

	void dispatch_wayland()
	{
		if (!display_ || wl_display_dispatch(display_) < 0) {
			if (!closed_)
				obs_log(LOG_WARNING, "Wayland overlay display connection closed");
			if (socket_notifier_)
				socket_notifier_->setEnabled(false);
		}
	}

	void cleanup_released_buffers()
	{
		buffers_.erase(std::remove_if(buffers_.begin(), buffers_.end(),
			[](const std::unique_ptr<Buffer> &buffer) { return buffer->released; }), buffers_.end());
	}

	std::unique_ptr<Buffer> create_buffer(const QImage &image)
	{
		const int stride = image.bytesPerLine();
		const size_t size = static_cast<size_t>(stride) * static_cast<size_t>(image.height());
		QTemporaryFile file;
		if (!file.open() || !file.resize(static_cast<qint64>(size)))
			return nullptr;
		const int fd = dup(file.handle());
		if (fd < 0)
			return nullptr;
		void *mapping = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (mapping == MAP_FAILED) {
			close(fd);
			return nullptr;
		}
		for (int row = 0; row < image.height(); ++row)
			std::memcpy(static_cast<char *>(mapping) + static_cast<size_t>(row) * stride,
				image.constScanLine(row), static_cast<size_t>(stride));

		wl_shm_pool *pool = wl_shm_create_pool(shm_, fd, static_cast<int>(size));
		if (!pool) {
			munmap(mapping, size);
			close(fd);
			return nullptr;
		}
		wl_buffer *wayland_buffer = wl_shm_pool_create_buffer(pool, 0, image.width(), image.height(),
			stride, WL_SHM_FORMAT_ARGB8888);
		wl_shm_pool_destroy(pool);
		if (!wayland_buffer) {
			munmap(mapping, size);
			close(fd);
			return nullptr;
		}

		auto buffer = std::make_unique<Buffer>();
		buffer->buffer = wayland_buffer;
		buffer->mapping = mapping;
		buffer->mapping_size = size;
		buffer->fd = fd;
		static const wl_buffer_listener buffer_listener = {Buffer::released_callback};
		wl_buffer_add_listener(buffer->buffer, &buffer_listener, buffer.get());
		return buffer;
	}

	void render()
	{
		if (!valid_ || !surface_ || !layer_surface_ || closed_)
			return;
		if (layout_.entries.empty()) {
			if (mapped_) {
				wl_surface_attach(surface_, nullptr, 0, 0);
				wl_surface_commit(surface_);
				mapped_ = false;
				configured_ = false;
			}
			return;
		}

		surface_image_ = render_indicator_surface(layout_, settings_);
		const uint32_t anchor = settings_.origin == OverlayOrigin::TopLeft
			? 1u | 4u
			: settings_.origin == OverlayOrigin::TopRight ? 1u | 8u
			: settings_.origin == OverlayOrigin::BottomLeft ? 2u | 4u : 2u | 8u;
		const bool right = settings_.origin == OverlayOrigin::TopRight ||
			settings_.origin == OverlayOrigin::BottomRight;
		const bool bottom = settings_.origin == OverlayOrigin::BottomLeft ||
			settings_.origin == OverlayOrigin::BottomRight;
		if (!configured_) {
			zwlr_layer_surface_v1_set_size(layer_surface_,
				static_cast<uint32_t>(surface_image_.width()),
				static_cast<uint32_t>(surface_image_.height()));
			zwlr_layer_surface_v1_set_anchor(layer_surface_, anchor);
			zwlr_layer_surface_v1_set_exclusive_zone(layer_surface_, 0);
			zwlr_layer_surface_v1_set_margin(layer_surface_, bottom ? 0 : settings_.offset,
				right ? settings_.offset : 0, bottom ? settings_.offset : 0,
				right ? 0 : settings_.offset);
			wl_surface_commit(surface_);
			if (wl_display_flush(display_) < 0 && errno != EAGAIN)
				obs_log(LOG_WARNING, "Wayland overlay could not flush remap requests");
			return;
		}
		zwlr_layer_surface_v1_set_size(layer_surface_, static_cast<uint32_t>(surface_image_.width()),
			static_cast<uint32_t>(surface_image_.height()));
		zwlr_layer_surface_v1_set_anchor(layer_surface_, anchor);
		zwlr_layer_surface_v1_set_margin(layer_surface_, bottom ? 0 : settings_.offset,
			right ? settings_.offset : 0, bottom ? settings_.offset : 0,
			right ? 0 : settings_.offset);

		auto buffer = create_buffer(surface_image_);
		if (!buffer) {
			obs_log(LOG_WARNING, "Wayland overlay could not allocate a shared-memory buffer");
			return;
		}
		wl_surface_attach(surface_, buffer->buffer, 0, 0);
		wl_surface_damage_buffer(surface_, 0, 0, surface_image_.width(), surface_image_.height());
		wl_surface_commit(surface_);
		buffers_.push_back(std::move(buffer));
		mapped_ = true;
		cleanup_released_buffers();
		if (wl_display_flush(display_) < 0 && errno != EAGAIN)
			obs_log(LOG_WARNING, "Wayland overlay could not flush display requests");
	}

	wl_display *display_ = nullptr;
	wl_registry *registry_ = nullptr;
	wl_compositor *compositor_ = nullptr;
	wl_shm *shm_ = nullptr;
	zwlr_layer_shell_v1 *layer_shell_ = nullptr;
	wl_surface *surface_ = nullptr;
	zwlr_layer_surface_v1 *layer_surface_ = nullptr;
	QSocketNotifier *socket_notifier_ = nullptr;
	IndicatorLayout layout_;
	OverlaySettings settings_;
	QImage surface_image_;
	std::vector<std::unique_ptr<Buffer>> buffers_;
	bool configured_ = false;
	bool mapped_ = false;
	bool closed_ = false;
	bool valid_ = false;
};

#endif

} // namespace

LinuxOverlayRenderer::~LinuxOverlayRenderer()
{
	stop();
}

bool LinuxOverlayRenderer::start()
{
	QGuiApplication *application = qobject_cast<QGuiApplication *>(QCoreApplication::instance());
	if (!application) {
		obs_log(LOG_ERROR, "Linux overlay requires an active Qt GUI application");
		return false;
	}

	if (QThread::currentThread() != application->thread()) {
		bool success = false;
		if (!QMetaObject::invokeMethod(application, [this, &success] { success = start(); },
			Qt::BlockingQueuedConnection)) {
			obs_log(LOG_ERROR, "Linux overlay could not enter the Qt GUI thread");
			return false;
		}
		return success;
	}

	return start_on_gui_thread();
}

bool LinuxOverlayRenderer::start_on_gui_thread()
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (window_)
		return true;

	if (!QGuiApplication::primaryScreen()) {
		obs_log(LOG_ERROR, "Linux overlay requires a primary screen");
		return false;
	}

	const QByteArray platform = QGuiApplication::platformName().toUtf8();
#ifdef OBS_STATUS_INDICATORS_HAVE_WAYLAND_LAYER_SHELL
	if (platform == "wayland") {
		auto *wayland_window = new (std::nothrow) LinuxWaylandOverlay();
		if (wayland_window && wayland_window->valid()) {
			window_ = wayland_window;
			obs_log(LOG_INFO, "Linux overlay uses Wayland layer-shell on Qt platform '%s'",
				platform.constData());
			return true;
		}
		delete wayland_window;
		obs_log(LOG_WARNING, "Wayland layer-shell setup failed; using Qt window fallback");
	}
#endif

	window_ = new (std::nothrow) LinuxOverlayWindow();
	if (!window_) {
		obs_log(LOG_ERROR, "Linux overlay window allocation failed");
		return false;
	}

	obs_log(LOG_INFO, "Linux overlay window ready on Qt platform '%s'", platform.constData());
	obs_log(LOG_INFO, "Linux overlay window behavior is managed by the desktop compositor");
	return true;
}

void LinuxOverlayRenderer::stop()
{
	QCoreApplication *application = QCoreApplication::instance();
	if (application && QThread::currentThread() != application->thread()) {
		if (!QMetaObject::invokeMethod(application, [this] { stop(); }, Qt::BlockingQueuedConnection))
			obs_log(LOG_WARNING, "Linux overlay could not enter the Qt GUI thread for shutdown");
		return;
	}

	stop_on_gui_thread();
}

void LinuxOverlayRenderer::stop_on_gui_thread()
{
	LinuxOverlaySurface *window = nullptr;
	{
		std::lock_guard<std::mutex> lock(mutex_);
		window = window_;
		window_ = nullptr;
	}
	delete window;
}

void LinuxOverlayRenderer::update_layout(const IndicatorLayout &layout)
{
	std::lock_guard<std::mutex> lock(mutex_);
	if (!window_)
		return;

	LinuxOverlaySurface *window = window_;
	QMetaObject::invokeMethod(window->qt_object(), [window, layout] { window->set_layout(layout); },
		Qt::QueuedConnection);
}

void LinuxOverlayRenderer::update_settings(const OverlaySettings &settings)
{
	const OverlaySettings normalized = normalize_overlay_settings(settings);
	std::lock_guard<std::mutex> lock(mutex_);
	if (!window_)
		return;

	LinuxOverlaySurface *window = window_;
	QMetaObject::invokeMethod(window->qt_object(), [window, normalized] { window->set_settings(normalized); },
		Qt::QueuedConnection);
}
