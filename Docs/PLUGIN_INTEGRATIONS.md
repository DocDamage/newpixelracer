# Pixel Racer plugin integrations

## Use now

- **Paper2D**: built-in Unreal sprite runtime.
- **PCG**: procedural scenery/generation foundation.
- **Enhanced Input**: long-term input system. The initial arcade pawn also exposes simple named mappings for immediate PIE testing.
- **Modeling Tools**: retained from the original project.

## Preferred optional integration: Paper2D+

Paper2D+ is intentionally **not** a hard dependency. Install/enable it when available and use it for sprite-sheet slicing, directional sprite authoring, reimport and pixel editing. Pixel Racer's own source-art manifest and runtime `.pixeltrack` format remain independent so player-created tracks never require an editor-only plugin.

## Deferred

- **PaperZD**: add when vehicle/rider animation graphs and notifies justify it.
- **2DFactory**: supported as an alternate content pipeline, not required.
- **Day/Night Manager**: possible backend for environment profiles later; tracks store time/weather generically.

## Deliberately not required

A dedicated 2D line-intersection plugin is unnecessary. PixelRacerCore owns the small geometry routines needed by validation so track files do not acquire a dependency for a basic operation.
