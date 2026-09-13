#pragma once

#include "indicator-controller.hpp"

#include <obs-module.h>

#include <algorithm>
#include <cstdint>

#include <QColor>
#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QRectF>
#include <QString>
#include <QSvgRenderer>

#include <plugin-support.h>

namespace overlay_renderer_detail {

inline int icon_padding_for_size(int indicator_size)
{
	return std::max(1, (indicator_size + 3) / 6);
}

inline const char *icon_file_for(const IndicatorEntry &entry)
{
	switch (entry.kind) {
	case IndicatorKind::Paused:
		return "icons/lucide/pause.svg";
	case IndicatorKind::Recording:
		return "icons/lucide/circle-dot.svg";
	case IndicatorKind::RecordingReplay:
		return "icons/lucide/refresh-ccw-dot.svg";
	case IndicatorKind::ReplayBuffer:
		return "icons/lucide/refresh-ccw.svg";
	case IndicatorKind::Microphone:
		return entry.muted ? "icons/lucide/mic-off.svg" : "icons/lucide/mic.svg";
	case IndicatorKind::Saving:
		return "icons/lucide/save.svg";
	}
	return nullptr;
}

inline bool render_lucide_icon(QImage &tile, const IndicatorEntry &entry, std::uint32_t icon_color)
{
	const char *relative_path = icon_file_for(entry);
	char *icon_path = obs_module_file(relative_path);
	if (!icon_path) {
		obs_log(LOG_WARNING, "indicator icon path unavailable: %s", relative_path);
		return false;
	}

	const QString path = QString::fromUtf8(icon_path);
	bfree(icon_path);
	QFile icon_file(path);
	if (!icon_file.open(QIODevice::ReadOnly)) {
		obs_log(LOG_WARNING, "indicator icon open failed: %s", relative_path);
		return false;
	}

	QByteArray svg = icon_file.readAll();
	const QColor color = QColor::fromRgb(icon_color);
	const QByteArray stroke = QByteArray("stroke=\"") + color.name(QColor::HexRgb).toUtf8() + "\"";
	svg.replace("stroke=\"#ffffff\"", stroke);
	QSvgRenderer renderer(svg);
	if (!renderer.isValid()) {
		obs_log(LOG_WARNING, "indicator icon load failed: %s", relative_path);
		return false;
	}

	const int icon_padding = icon_padding_for_size(tile.width());
	QPainter painter(&tile);
	renderer.render(&painter,
		QRectF(icon_padding, icon_padding, tile.width() - 2 * icon_padding,
			tile.height() - 2 * icon_padding));
	return true;
}

} // namespace overlay_renderer_detail
