# Pixel Racer Track Editor — v0.3-dev Handoff

## Source basis

Continue from this package. Do not rebuild from scratch.

Engine target: **Unreal Engine 5.8.3**

Project: `PixelRacer/PixelRacer.uproject`

Plugin: `PixelRacer/Plugins/PixelRacerTools/`

## What this pass completed

### RuntimeEditor activation / schema drift fix

- Registered `PixelRacerRuntimeEditor` in `PixelRacerTools.uplugin`.
- Confirmed the previous duplicate `PixelRacerAuthoringLibrary.cpp` had diverged from TrackDocument schema v2 and referenced nonexistent fields/types.
- Reworked it into a compatibility façade backed by the canonical `UPixelRacerTrackAuthoringLibrary` for overlapping operations.
- Added QuickCheck guards so the known stale schema identifiers cannot silently return.

### Canonical authoring API additions

`UPixelRacerTrackAuthoringLibrary` now exposes runtime-safe operations for:

- move road control point,
- insert road control point,
- delete road control point,
- set point width,
- set point surface,
- set point elevation layer,
- set point crossing permission,
- erase pieces in radius,
- add/update/remove procedural zones,
- generate starting grid slots.

### Spline manipulation in the Track Editor

Implemented:

- road-point hit testing,
- point selection,
- point dragging,
- Shift+click segment insertion,
- right-click / Delete point removal,
- selected-point highlight,
- per-point draggable width handle,
- surface buttons for asphalt/dirt/grass/sand,
- elevation +/- bounded to Underground through Elevated 2,
- Undo/Redo buttons,
- Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z,
- canvas edits grouped into existing autosave/undo transactions,
- reload of exported sandbox `.pixeltrack.json` for round-trip checking.

### Procedural-zone authoring — first usable pass

Implemented:

- rectangle drag authoring,
- visible zone outlines,
- stored zone polygon/preset/layer/seed,
- preset selection for:
  - Grassland,
  - Barrier Edge,
  - Crowd,
  - Parking.

Not yet implemented:

- polygon vertex editing,
- freehand zones,
- rectangle resizing after creation,
- asset-backed preset regeneration UI,
- visual protection/marking of manually overridden generated objects.

### Asset pack validation

The supplied Wheels in Pixels ZIP was rescanned successfully:

- 224 PNGs total,
- 195 vehicle sprite sheets,
- 20 tilesets,
- 7 environment pieces,
- 2 VFX sheets.

A byte-for-byte PNG comparison against the already embedded source art found:

- 0 missing,
- 0 extra,
- 0 changed.

The existing `PixelRacerAssetPack_v2.json` remains in place for the next Asset Browser pass.

## Validation performed

- `python3 Tools/quick_check.py` → **0 errors, 0 warnings**.
- Python helper scripts compile.
- ZIP integrity and clean-extraction QuickCheck are required before final delivery of this package.

## Critical limitation

No Unreal Engine 5.8.3 installation is available in the environment that produced this pass.

Therefore:

- no real Unreal Editor build has been run,
- no UHT/UBT success is claimed,
- no PIE runtime behavior is claimed from this environment.

The next machine with UE 5.8.3 must run `Tools/BuildEditorOnce.bat`. Any compiler errors should be fixed directly; do not weaken validation or remove features just to suppress them.

## Next development order

1. **Real UE 5.8.3 compile** and fix UHT/UBT/API issues if any.
2. Open **Window → Pixel Racer Track Editor** and verify the v0.3-dev spline interactions.
3. Finish procedural-zone polygon/freehand editing and regeneration behavior.
4. Build the real manifest-backed Wheels in Pixels Asset Browser:
   - thumbnails,
   - search,
   - categories,
   - click-to-select tile/piece,
   - drag/drop,
   - user packs.
5. Build the Paper2D/Paper2D+ import adapter.
6. Build the runtime TrackDocument preview actor/system.
7. Import/select the Hachiroku Drifter and Bologna Superbike in the Play workflow.
8. Reach the reliable loop: `draw → drive authored track → Esc → edit`.

Do not divert into career/championship/menu work until that loop is reliable.
