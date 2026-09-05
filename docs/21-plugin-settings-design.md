# Post-MVP plugin settings design

## Settings schema

The plugin stores one JSON object in `settings.json`:

| Key | Type | Values/default | Meaning |
|---|---|---|---|
| `origin` | string | `top-left` | One of `top-left`, `top-right`, `bottom-left`, `bottom-right`; the primary display corner used as the anchor. |
| `orientation` | string | `vertical` | `vertical` stacks indicators downward/upward from the selected corner; `horizontal` stacks left/right from the selected corner. |
| `offset` | integer | `0` | Equal distance in pixels from both display edges forming the selected corner. |
| `gap` | integer | `8` | Pixel gap between adjacent indicator tiles. |
| `background_color` | unsigned 32-bit RGB | black | Tile background color. |
| `icon_color` | unsigned 32-bit RGB | white | Lucide stroke color. |
| `opacity` | integer | `50` | Shared opacity for the complete indicator tile, from `1` to `100` percent. |

Colors use Qt's opaque `QColor::rgb()` representation (`0xFFRRGGBB`) and are saved as JSON integers. The renderer applies the single configured opacity through `UpdateLayeredWindow`'s source alpha, so the background and icon always fade together. Previously saved alpha bytes are ignored when loading colors.

## Defaults and validation

Defaults preserve the current post-MVP appearance: top-left origin, vertical orientation, zero offset, 8px gap, black background, white icon color, and 50% shared opacity.

The loader must validate values from disk rather than trusting JSON:

- unknown origin falls back to `top-left`;
- unknown orientation falls back to `vertical`;
- offset is clamped to `0..4096`;
- gap is clamped to `0..512`;
- opacity is clamped to `1..100`;
- missing or malformed color values fall back to their defaults.

The dialog uses the same bounds and writes only normalized values.

## Dialog behavior

The Tools menu contains `Status Indicators`. The dialog is modeless and parented to the OBS main window. Its controls are grouped into sections:

- `Layout`: origin and orientation controls on one row;
- `Spacing`: offset and gap spin boxes with pixel suffixes on one row;
- `Appearance`: solid background and icon color buttons on one row, followed by a shared opacity spin box with a percent suffix;
- Apply, OK, and Cancel actions.

Color buttons open `QColorDialog` without alpha-channel support.

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
4. Verify persistence, live Apply behavior, all corner/orientation combinations, shared opacity, and clean unload.
