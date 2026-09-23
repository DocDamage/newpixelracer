# Pixel Racer Track Editor — v0.4-dev Handoff

## Source basis

Continue from this package. Do not rebuild from scratch.

Engine target: **Unreal Engine 5.8.3**

Project: `PixelRacer/PixelRacer.uproject`

Plugin: `PixelRacer/Plugins/PixelRacerTools/`

The approved architecture and v0.1–v0.3 functionality remain authoritative.

## What this pass completed

### 1. Real manifest-backed Asset Browser

New files:

- `Plugins/PixelRacerTools/Source/PixelRacerTrackEditor/Private/SPixelRacerAssetBrowser.h`
- `Plugins/PixelRacerTools/Source/PixelRacerTrackEditor/Private/SPixelRacerAssetBrowser.cpp`

Behavior:

- Loads `SourceArt/WheelsInPixels/PixelRacerAssetPack_v2.json`.
- Loads additional `PixelRacerAssetPack_v2.json` files recursively from `SourceArt/Imported/`.
- Uses source PNGs as Slate thumbnail images.
- Search supports name/path/pack.
- Filters: All, Tiles, Scenery, Vehicles, VFX.
- Browser is virtualized through `SListView`.
- Rescan does not require an editor restart.
- Selection callback is wired into `SPixelRacerTrackCanvas`.

Portable ID rule:

- Wheels in Pixels retains its historical `relative/path.png` IDs so existing starter data does not need migration.
- Imported packs use `packId:relative/path.png` IDs to avoid collisions.

Selection behavior:

- `tileset` → update `ActiveTile`, switch to Tile Paint.
- `environment_piece` → update `ActivePiece`, switch to Piece Placement.
- vehicle/VFX/sprite roles remain selectable for upcoming import/preview systems but are not incorrectly placed as track geometry.

Drag/drop:

- tileset drop → one tile placement at the drop cell.
- environment-piece drop → one snapped piece placement.
- both operations use the existing edit session transaction/undo model.

### 2. Procedural-zone authoring expansion

Rectangle authoring remains.

Added:

- Polygon drawing.
- Freehand drawing.
- open-shape previews.
- Enter to commit polygon.
- right-click/Esc cancel.
- zone vertex hit testing.
- zone vertex dragging.
- selected-zone outline/handle rendering.
- selected-zone deletion.
- undo/redo/autosave integration for committed edits.

Still pending:

- insert/delete individual polygon vertices as dedicated operations,
- move whole zone without moving vertices one-by-one,
- generated-scene-object rules per preset,
- regeneration controls,
- explicit generated/manual override visualization,
- preservation tests against real asset-backed generation.

### 3. Validation hardening

`Tools/quick_check.py` now also checks:

- v2 manifest schema and pack id,
- unique Wheels portable asset IDs,
- every v2 source path exists,
- required manifest roles exist,
- Asset Browser source files exist,
- browser manifest/user-pack behavior is present,
- canvas drag/drop integration is present,
- Json module dependency exists.

Current result: **0 errors, 0 warnings**.

A C++ delimiter-balance check was also run against the TrackEditor sources during packaging.

## Critical limitation

No Unreal Engine 5.8.3 installation is available in this environment.

Therefore this pass does **not** claim:

- successful UHT,
- successful UBT compile,
- plugin load inside UE,
- rendered Slate thumbnails in a running UE editor,
- PIE/runtime behavior.

The first UE 5.8.3 machine must run `Tools/BuildEditorOnce.bat`. Fix actual compile/API issues directly rather than suppressing them.

The Slate/JSON API choices used in the browser were checked against Epic's UE 5.8 API documentation before packaging, but a real engine compile is still the authoritative test.

## Exact next development order

1. Run the real UE 5.8.3 Editor build.
2. Fix any UHT/UBT/API diagnostics until `PixelRacerTools` loads.
3. Open **Window → Pixel Racer Track Editor**.
4. Verify the browser loads all 224 Wheels in Pixels entries, thumbnails render, search/filter works, and tile/scenery selection changes the active authoring asset.
5. Verify drag/drop placement plus Rectangle/Polygon/Freehand zones and zone-vertex editing.
6. Implement the actual Paper2D adapter and import only the first required assets:
   - track tiles,
   - barriers/grid slots,
   - Hachiroku,
   - Bologna Superbike,
   - smoke,
   - nitro.
7. Make the canvas render actual imported art instead of generic authoring primitives.
8. Build the runtime TrackDocument preview actor/system.
9. Make **Play Track** load the current authored document.
10. Add Hachiroku/Bologna selection to Play Track and reach `draw → drive → Esc → edit`.

Do not divert into career/championship/menu systems before that loop works.
