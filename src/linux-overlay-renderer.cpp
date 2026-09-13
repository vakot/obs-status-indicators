#include "linux-overlay-renderer.hpp"

#include <obs-module.h>

#include <algorithm>
#include <cstdint>
#include <new>
#include <utility>

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
#include <QThread>
#include <QWidget>

#include <overlay-renderer-utils.hpp>
#include <plugin-support.h>

class LinuxOverlayWindow final : public QWidget {
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

	void set_layout(const IndicatorLayout &layout)
	{
		layout_ = layout;
		render();
	}

	void set_settings(const OverlaySettings &settings)
	{
		settings_ = normalize_overlay_settings(settings);
		render();
	}

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
		const int entry_count = static_cast<int>(layout_.entries.size());
		if (entry_count == 0) {
			hide();
			return;
		}

		if (!screen_) {
			obs_log(LOG_WARNING, "Linux overlay has no primary screen");
			hide();
			return;
		}

		const bool horizontal = settings_.orientation == OverlayOrientation::Horizontal;
		const int primary_size = entry_count * settings_.indicator_size +
			(entry_count - 1) * settings_.gap;
		const int width = horizontal ? primary_size : settings_.indicator_size;
		const int height = horizontal ? settings_.indicator_size : primary_size;

		QImage surface(width, height, QImage::Format_ARGB32_Premultiplied);
		surface.fill(Qt::transparent);
		QPainter painter(&surface);
		for (int index = 0; index < entry_count; ++index) {
			const int left = horizontal ? index * (settings_.indicator_size + settings_.gap) : 0;
			const int top = horizontal ? 0 : index * (settings_.indicator_size + settings_.gap);
			QImage tile(settings_.indicator_size, settings_.indicator_size,
				QImage::Format_ARGB32_Premultiplied);
			tile.fill(QColor::fromRgba(settings_.background_color));
			overlay_renderer_detail::render_lucide_icon(tile, layout_.entries[index],
				settings_.icon_color);
			painter.drawImage(left, top, tile);
		}
		painter.end();
		surface_ = std::move(surface);

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
		setWindowOpacity(static_cast<qreal>(settings_.opacity) / 100.0);
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

	window_ = new (std::nothrow) LinuxOverlayWindow();
	if (!window_) {
		obs_log(LOG_ERROR, "Linux overlay window allocation failed");
		return false;
	}

	const QByteArray platform = QGuiApplication::platformName().toUtf8();
	obs_log(LOG_INFO, "Linux overlay window ready on Qt platform '%s'", platform.constData());
	obs_log(LOG_INFO, "Linux overlay capture exclusion and absolute z-order are compositor-controlled");
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
	LinuxOverlayWindow *window = nullptr;
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

	LinuxOverlayWindow *window = window_;
	QMetaObject::invokeMethod(window, [window, layout] { window->set_layout(layout); },
		Qt::QueuedConnection);
}

void LinuxOverlayRenderer::update_settings(const OverlaySettings &settings)
{
	const OverlaySettings normalized = normalize_overlay_settings(settings);
	std::lock_guard<std::mutex> lock(mutex_);
	if (!window_)
		return;

	LinuxOverlayWindow *window = window_;
	QMetaObject::invokeMethod(window, [window, normalized] { window->set_settings(normalized); },
		Qt::QueuedConnection);
}
