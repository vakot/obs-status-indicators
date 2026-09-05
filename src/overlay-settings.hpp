#pragma once

#include <cstdint>

inline constexpr int kMinIndicatorSize = 16;
inline constexpr int kMaxIndicatorSize = 256;
inline constexpr int kDefaultIndicatorSize = 48;

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
	int offset = 4;
	int gap = 4;
	int indicator_size = kDefaultIndicatorSize;
	std::uint32_t background_color = 0xFF000000;
	std::uint32_t icon_color = 0xFFFFFFFF;
	int opacity = 65;
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

	if (settings.indicator_size < kMinIndicatorSize)
		settings.indicator_size = kMinIndicatorSize;
	else if (settings.indicator_size > kMaxIndicatorSize)
		settings.indicator_size = kMaxIndicatorSize;

	if (settings.opacity < 1)
		settings.opacity = 1;
	else if (settings.opacity > 100)
		settings.opacity = 100;

	settings.background_color = 0xFF000000 | (settings.background_color & 0x00FFFFFF);
	settings.icon_color = 0xFF000000 | (settings.icon_color & 0x00FFFFFF);

	return settings;
}

OverlaySettings load_overlay_settings();
bool save_overlay_settings(const OverlaySettings &settings);
