# Sources, assumptions, and decisions

## Primary references

- [OBS frontend API header](https://github.com/obsproject/obs-studio/blob/master/frontend/api/obs-frontend-api.h) — event enum, state queries, output accessors, and callback registration.
- [OBS frontend API reference](https://github.com/obsproject/obs-studio/blob/master/docs/sphinx/reference-frontend-api.rst) — documented event and function semantics.
- [OBS libobs header](https://github.com/obsproject/obs-studio/blob/master/libobs/obs.h) — source lookup, source enumeration, references, and signal access.
- [OBS source API reference](https://github.com/obsproject/obs-studio/blob/master/docs/sphinx/reference-sources.rst) — source signals, mute state, references, and audio activity.
- [OBS Replay Buffer output implementation](https://github.com/obsproject/obs-studio/blob/master/plugins/obs-ffmpeg/obs-ffmpeg-mux.c) — `saved()` output signal registration.
- [OBS plugin template](https://github.com/obsproject/obs-plugintemplate) and [template CMakeLists.txt](https://github.com/obsproject/obs-plugintemplate/blob/master/CMakeLists.txt) — current plugin build conventions.
- [SetWindowDisplayAffinity](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity) — affinity scope, Windows 10 2004 support, and non-DRM caveat.
- [Windows window features](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features) — layered windows and click-through behavior.
- [Extended window styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles) — `WS_EX_LAYERED`, `WS_EX_NOACTIVATE`, `WS_EX_TOOLWINDOW`, and `WS_EX_TRANSPARENT`.
- [SetWindowPos](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowpos) — topmost and no-activate positioning.

## Decisions

| Decision | Rationale |
| --- | --- |
| Native in-process plugin | Meets the no-external-process/no-IPC requirement and removes middleware failure modes. |
| Frontend callbacks plus authoritative queries | Directly supported by OBS 32.2.1; avoids polling and startup races. |
| Replay output `saved()` signal | Distinguishes save completion from a hotkey/request. |
| Layered Win32 window | Smallest dependency-free path for alpha, topmost, click-through UI. |
| GDI/`UpdateLayeredWindow` for POC | Adequate for a few rows and simpler to verify than a new graphics framework. |
| One complete layout update | Prevents the known multi-step reflow/gap defect. |
| Capture exclusion as a measured capability | Windows and OBS capture paths do not provide a universal guarantee. |
| Primary display/fixed offset for v0.0.1 | Keeps the POC focused and removes configuration/state complexity. |

## Open questions that must be resolved before implementation freeze

- What exact microphone presentation did the previous working overlay expose: muted/unmuted, source active, audio activity, or a combination?
- What stable microphone identity is available in the prior implementation or profile: UUID, configured global input slot, source type, or name fallback?
- Which OBS plugin-template revision/toolchain should be pinned for the first build, and which minimum OBS runtime ABI will be supported?
- Does the chosen renderer’s DIB/GDI path produce acceptable text quality and affinity behavior on the target GPU/Windows builds?
- What are the observed Display/Window/Game Capture results on the final marker window?
- Which fullscreen modes can keep a topmost compositor window visible on the test hardware?

These are implementation/test gates, not reasons to introduce speculative abstractions now.
