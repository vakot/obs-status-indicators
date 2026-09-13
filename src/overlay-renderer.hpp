#pragma once

#if defined(_WIN32)
#include "windows-overlay-renderer.hpp"
using OverlayRenderer = WindowsOverlayRenderer;
#elif defined(__linux__)
#include "linux-overlay-renderer.hpp"
using OverlayRenderer = LinuxOverlayRenderer;
#else
#error "OBS Status Indicators has no overlay renderer for this platform"
#endif
