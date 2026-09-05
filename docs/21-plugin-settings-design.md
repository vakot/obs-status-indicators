# Post-MVP plugin settings design

## Settings schema

The plugin stores one JSON object in `settings.json`:

| Key | Type | Values/default | Meaning |
|---|---|---|---|
| `origin` | string | `top-left` | One of `top-left`, `top-right`, `bottom-left`, `bottom-right`; the primary display corner used as the anchor. |
| `orientation` | string | `vertical` | `vertical` stacks indicators downward/upward from the selected corner; `horizontal` stacks left/right from the selected corner. |
| `offset` | integer | `0` | Equal distance in pixels from both display edges forming the selected corner. |
| `gap` | integer | `8` | Pixel gap between adjacent indicator tiles. |
| `background_color` | unsigned 32-bit RGBA | black at 50% alpha | Tile background color including alpha. |
| `icon_color` | unsigned 32-bit RGBA | white at 50% alpha | Lucide stroke color including alpha. |

Colors use Qt's `QColor::rgba()` representation (`0xAARRGGBB`) and are saved as JSON integers. The renderer uses per-pixel alpha with `UpdateLayeredWindow`'s source alpha set to 255 so the configured background and icon alpha values are preserved independently.

## Defaults and validation

Defaults preserve the current post-MVP appearance: top-left origin, vertical orientation, zero offset, 8px gap, black 50% background, and white 50% icon color.

The loader must validate values from disk rather than trusting JSON:

- unknown origin falls back to `top-left`;
- unknown orientation falls back to `vertical`;
- offset is clamped to `0..4096`;
- gap is clamped to `0..512`;
- missing or malformed color values fall back to their defaults.

The dialog uses the same bounds and writes only normalized values.

## Dialog behavior

The Tools menu contains `OBS Status Indicators Settings`. The dialog is modeless and parented to the OBS main window. It contains:

- an origin combo box with four corner choices;
- an orientation combo box with vertical/horizontal choices;
- offset and gap spin boxes with pixel suffixes;
- background and icon color buttons opening `QColorDialog` with alpha-channel support;
- Apply, OK, and Cancel actions.

Apply saves the normalized snapshot and requests a renderer update immediately. OK applies and closes. Cancel discards unsaved edits.

## Renderer mapping

The renderer continues to use the primary display. Given the final window width/height and the configured offset:

| Origin | X | Y |
|---|---:|---:|
| top-left | `offset` | `offset` |
| top-right | `screen_width - window_width - offset` | `offset` |
| bottom-left | `offset` | `screen_height - window_height - offset` |
| bottom-right | `screen_width - window_width - offset` | `screen_height - window_height - offset` |

The orientation determines whether the layout dimensions are `tile_count × tile_size + gaps` by `tile_size`, or the inverse. Coordinates are clamped at zero when the configured offset and window size exceed the primary display bounds.

## Implementation phases

1. Persist and normalize the settings model.
2. Add the Qt Tools-menu dialog and localized labels.
3. Pass settings snapshots to the renderer and apply layout/origin/color behavior.
4. Verify persistence, live Apply behavior, all corner/orientation combinations, alpha, and clean unload.
