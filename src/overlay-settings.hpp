#pragma once

#include <cstdint>

enum class OverlayOrigin {
	TopLeft,
	TopRight,
	BottomLeft,
	BottomRight,
};

enum class OverlayOrientation {
	Vertical,
	Horizontal,
};

struct OverlaySettings {
	OverlayOrigin origin = OverlayOrigin::TopLeft;
	OverlayOrientation orientation = OverlayOrientation::Vertical;
	int offset = 0;
	int gap = 8;
	std::uint32_t background_color = 0x80000000;
	std::uint32_t icon_color = 0x80FFFFFF;
};

inline OverlaySettings default_overlay_settings()
{
	return {};
}

inline OverlaySettings normalize_overlay_settings(OverlaySettings settings)
{
	if (settings.offset < 0)
		settings.offset = 0;
	else if (settings.offset > 4096)
		settings.offset = 4096;

	if (settings.gap < 0)
		settings.gap = 0;
	else if (settings.gap > 512)
		settings.gap = 512;

	return settings;
}

OverlaySettings load_overlay_settings();
bool save_overlay_settings(const OverlaySettings &settings);
