#pragma once

#include "indicator-state.hpp"

#include <vector>

enum class IndicatorKind {
	Paused,
	Recording,
	ReplayBuffer,
	Microphone,
	Saving,
};

struct IndicatorEntry {
	IndicatorKind kind;

	bool operator==(const IndicatorEntry &other) const { return kind == other.kind; }
	bool operator!=(const IndicatorEntry &other) const { return !(*this == other); }
};

struct IndicatorLayout {
	std::vector<IndicatorEntry> entries;

	bool operator==(const IndicatorLayout &other) const { return entries == other.entries; }
	bool operator!=(const IndicatorLayout &other) const { return !(*this == other); }
};

class IndicatorController {
public:
	static IndicatorLayout build_layout(const IndicatorState &state);
};
