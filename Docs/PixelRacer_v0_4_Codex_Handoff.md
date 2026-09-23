# Pixel Racer — Unreal Engine 5.8.3 Track Editor
## Codex Development Handoff — Continue from v0.4-dev

> Progress update (2026-09-23): Phases A and B are complete. The original baseline below is retained for context; current verification and fixes are recorded in [DEVELOPMENT_STATUS.md](DEVELOPMENT_STATUS.md). Phase C is next and has not started.

### Mission
Continue development of the existing Pixel Racer Unreal Engine 5.8.3 project from the supplied **v0.4-dev** source. Do not restart the project, replace the architecture, or spend time replanning already-approved decisions.

The immediate product goal is still:

`browse real art → draw/edit track → Play Track → drive authored track → Esc → edit`

Everything that does not materially advance that loop is secondary.

---

## 1. Authoritative source

Use the supplied archive:

`PixelRacer_TrackEditor_v0_4_DEV_Source.zip`

Expected SHA-256:

`6d0b1601b5dbe7410ff68541be18a860fe25f142d1b3d9ea2a683eb4b2beaad8`

Project:

`PixelRacer/PixelRacer.uproject`

Plugin:

`PixelRacer/Plugins/PixelRacerTools/`

Engine target:

**Unreal Engine 5.8.3**

Do not rebuild from scratch. Do not substitute a different engine version unless required only to diagnose an API change; the deliverable target remains 5.8.3.

---

## 2. Non-negotiable architecture

1. `FPixelRacerTrackDocument` schema v2 is the authoritative track representation.
2. Do not make a normal Unreal Level the source of truth for tracks.
3. Spline/freehand roads, tile painting, and individual piece placement are all first-class authoring workflows and must remain mixable in one track.
4. Portable community-track data remains JSON-based and data-only. No arbitrary executable code in track packages.
5. Paper2D is enabled. Paper2D+ may enhance authoring later, but **must remain optional**. The project must open without Paper2D+.
6. The developer editor and future player-facing editor must share the same `TrackDocument` and authoring operations. Do not fork a second runtime editing logic stack.
7. Generated content must carry generated/manual-override state, and regeneration must preserve manual edits.
8. Keep existing elevation-layer architecture. Do not permanently lock the system to flat ground even though current art is mostly flat.
9. Do not spend this milestone on career, championships, garage/menu polish, Steam Workshop, or unrelated game systems.
10. Ordinary iteration should remain lightweight: edit → validate → PIE → Esc → edit. Do not require cooking/packaging/full suites for routine authoring changes.
11. Never report a successful Unreal build, plugin load, or PIE result unless it actually happened on the current source.

---

## 3. Current plugin/module state

`PixelRacerTools.uplugin` version: **0.4.0-dev**

Registered modules:

- `PixelRacerCore` — Runtime
- `PixelRacerTrackEditor` — Editor
- `PixelRacerRuntimeEditor` — Runtime

The RuntimeEditor registration mismatch from v0.2 is already fixed. Do not remove the module merely because the player-facing UI is unfinished.

Current project plugins enabled in `PixelRacer.uproject` include:

- Modeling Tools Editor Mode
- Paper2D
- Enhanced Input
- PCG
- CommonUI
- PixelRacerTools

---

## 4. Current implementation — preserve it

### TrackDocument / core

Schema v2 currently stores:

- metadata
- topology
- infinite/chunked canvas settings
- road splines/control points
- tile placements
- piece placements
- procedural zones
- checkpoints
- starting-grid slots
- racing lines
- surface profiles
- environment profile
- generation settings

Road control points currently store:

- location
- width
- speed scale
- surface ID
- elevation layer
- manual-width flag
- crossing permission

Tile and piece placements already contain generated/manual-override state.

### Canonical authoring API

The canonical API is:

`Plugins/PixelRacerTools/Source/PixelRacerCore/Public/PixelRacerTrackAuthoringLibrary.h`

`UPixelRacerTrackAuthoringLibrary` currently owns the real schema-v2 operations for:

- add/move/insert/delete road points
- point width/surface/elevation/crossing changes
- paint/erase tiles
- place/erase pieces
- bake spline to pieces
- add/update/remove procedural zones
- generate checkpoints
- generate grid slots
- generate racing lines

There is also:

`PixelRacerAuthoringLibrary.*`

That older API is intentionally retained as a **compatibility façade** for existing RuntimeEditor callers and delegates to the canonical library where possible. Do not allow these APIs to drift into two independent implementations. Prefer new core functionality in `UPixelRacerTrackAuthoringLibrary`, then adapt legacy wrappers only when compatibility requires it.

### Track Editor canvas

Current Slate editor already supports:

- spline point creation
- point hit testing/selection
- point dragging
- Shift+click insertion near a segment
- point deletion
- draggable per-point width handles
- surface assignment
- elevation changes
- tile painting/erasing
- piece placement/erasing
- checkpoint/grid/racing-line generation
- spline-to-piece baking
- rectangle procedural zones
- polygon procedural zones
- freehand procedural zones
- procedural-zone vertex selection/dragging
- procedural-zone deletion
- undo/redo
- autosave
- sandbox export/reload
- normal PIE launch

Undo/redo session history is currently bounded to the existing 100-step behavior.

Autosaves go under:

`Saved/PixelRacer/Autosaves/`

Sandbox export goes to:

`Saved/PixelRacer/Tracks/editor_sandbox.pixeltrack.json`

### Asset Browser

`SPixelRacerAssetBrowser` is already implemented and integrated.

It loads:

`SourceArt/WheelsInPixels/PixelRacerAssetPack_v2.json`

and recursively discovers additional v2 manifests under:

`SourceArt/Imported/`

Current browser functionality:

- thumbnails from source PNGs
- search by name/path/pack
- filters: All / Tiles / Scenery / Vehicles / VFX
- virtualized `SListView`
- rescan without editor restart
- tile/scenery selection changes active authoring asset
- tile/scenery drag/drop onto the canvas
- vehicle/VFX entries are browseable but are not incorrectly placed as geometry

Portable asset-ID rule:

- Wheels in Pixels retains historical `relative/path.png` IDs for compatibility.
- Imported packs use `packId:relative/path.png`.

Do not break this ID rule without a migration strategy.

### Wheels in Pixels manifest

Current v2 manifest has **224 assets**:

- 195 vehicle sprite sheets
- 20 tilesets
- 7 environment pieces
- 2 VFX sheets

Useful metadata already exists. Examples:

- tilesets can include `tileWidth`, `tileHeight`, `columns`, `rows`, `tileCount`
- vehicle sheets can include `directionCount`, `cellWidth`, `cellHeight`

The v2 pack authoring defaults identify a 64×64 tile size and Paper2D+ as optional.

Starter vehicle definitions already exist for:

- `Hachiroku_Drifter`
- `Bologna_Superbike`

Both currently use 16-direction source sheets with 46×54 cells in their starter metadata.

---

## 5. Current validation state

Before this handoff, the v0.4-dev package had:

- `Tools/quick_check.py`: **0 errors, 0 warnings**
- Python tools: `py_compile` passed
- TrackEditor source delimiter/static packaging check: passed
- all 224 manifest source paths present
- required roles present
- unique Wheels portable asset IDs checked
- Asset Browser source/integration guardrails checked
- Json module dependency checked

However:

**No real Unreal Engine 5.8.3 UHT/UBT/editor compile has been run on this package yet.**

That is the first hard gate.

---

# 6. Execute this next — do not skip ahead

## Phase A — Real UE 5.8.3 compile and editor-load validation

From the extracted `PixelRacer` folder:

1. Run:
   `Tools\QuickCheck.bat`
2. Confirm it remains 0 errors / 0 warnings before changing code.
3. If UE 5.8 is not at the default path, set:
   `UE58_ROOT=<actual UE 5.8.3 install folder>`
4. Run:
   `Tools\BuildEditorOnce.bat`
5. Fix every project/plugin UHT, UBT, C++, Slate, JSON, Paper2D, or module API error found by the actual engine.
6. Do **not** suppress warnings/errors merely to make the build green.
7. Rebuild until the editor target succeeds.
8. Run:
   `Tools\OpenEditor.bat`
9. Verify `PixelRacerTools` loads.
10. Open:
    **Window → Pixel Racer Track Editor**

If the first compile exposes an Unreal 5.8.3 API difference, fix the implementation for 5.8.3 and record the exact error + fix in `Docs/DEVELOPMENT_STATUS.md`.

### Phase A acceptance criteria

- Real UE 5.8.3 build completes successfully.
- No project/plugin compile errors.
- PixelRacerTools loads.
- Track Editor tab opens.
- Existing v0.4 functionality still works.
- No feature regression is hidden by weakening QuickCheck.

---

## Phase B — Verify v0.4 inside the actual editor

Before adding Paper2D import code, manually verify:

1. Asset Browser loads all 224 Wheels entries.
2. Thumbnails display.
3. Search works.
4. category filters work.
5. Rescan works.
6. selecting a tileset activates Tile Paint with the expected portable asset ID.
7. selecting scenery activates Piece Placement.
8. tile/scenery drag/drop produces one undoable placement.
9. spline point drag/insert/delete works.
10. width handles work.
11. surface/elevation edits survive export/reload.
12. Rectangle/Polygon/Freehand zones work.
13. procedural-zone vertex dragging works.
14. Undo/Redo works across those operations.
15. sandbox export/reload round-trips without losing new schema-v2 state.

Fix v0.4 defects before layering new systems on top.

---

## Phase C — Implement the actual Paper2D import adapter

This is the next feature milestone.

### Architectural rule

Asset **import/creation** uses editor-only Unreal APIs, so keep the importing implementation in `PixelRacerTrackEditor` rather than putting UnrealEd/AssetTools dependencies into `PixelRacerCore`.

`PixelRacerCore` may contain runtime-safe asset-reference/lookup structures if needed, but must not gain editor-only dependencies.

If direct Paper2D APIs are used from `PixelRacerTrackEditor`, add explicit module dependencies rather than relying on accidental transitive linkage.

### Required first-pass assets

Import only enough to unlock the editor-driving loop first:

1. track tilesets
2. barriers
3. starting-grid art
4. Hachiroku Drifter
5. Bologna Superbike
6. smoke VFX
7. nitro VFX

Do not bulk-build every possible asset type before these work.

### Import requirements

Implement source PNG → Unreal asset generation with:

- deterministic destination paths
- stable mapping from portable `AssetId` to generated Unreal asset path(s)
- non-destructive source-art handling
- nearest-neighbor/pixel-safe texture filtering
- no unwanted mip/blur behavior for the intended top-down pixel presentation
- transparent sprite handling
- sprite slicing
- pivots
- reimport/update behavior
- generated thumbnails where useful
- tileset definitions from manifest tile metadata
- directional vehicle-frame extraction from manifest metadata
- clear error reporting for malformed/unsupported assets

Suggested generated content root:

`/Game/PixelRacer/Generated/...`

or another deterministic Pixel Racer-owned path. Do not write generated assets back into `SourceArt`.

### Tileset requirements

Use manifest metadata when available:

- `tileWidth`
- `tileHeight`
- `columns`
- `rows`
- `tileCount`

Do not hardcode all sheets to one geometry if the manifest says otherwise.

### Vehicle-sheet requirements

For Hachiroku and Bologna, honor:

- 16 directions
- 46×54 frame cells from current starter metadata/manifest

Verify actual direction order visually. Do not guess and permanently encode the orientation without checking the source sheet.

### Phase C acceptance criteria

- One-click or explicit editor action can import/reimport the required starter assets.
- Re-running import is idempotent or predictably updates existing generated assets rather than creating duplicates.
- Generated assets resolve from the existing portable IDs.
- Pixel filtering/pivots/slicing are correct in UE.
- Hachiroku and Bologna expose all expected directional frames.
- No Paper2D+ dependency is introduced.
- Project still opens when Paper2D+ is absent.

---

## Phase D — Render real imported art in the Track Editor canvas

The current canvas still primarily visualizes authoring primitives. Replace that limitation incrementally.

Required behavior:

1. Resolve the active/placed `AssetId` through the new generated-asset mapping.
2. Draw real imported tile/scenery art where practical in the Slate authoring canvas.
3. Preserve useful debug overlays for:
   - road centerline
   - control points
   - width handles
   - checkpoints
   - grid slots
   - racing lines
   - procedural zones
   - selection state
4. Do not distort source pixel art to force arbitrary curves.
5. Keep placement snapping and TrackDocument coordinates authoritative.
6. Missing assets must degrade visibly and safely, not crash or silently disappear.

### Phase D acceptance criteria

A user can browse a real Wheels asset, place/paint it, and see recognizable source art in the editor while TrackDocument remains the only authored data source.

---

## Phase E — Runtime TrackDocument preview system

`Play Track` currently requests ordinary PIE. It does **not** yet instantiate the currently edited TrackDocument into the play world.

Implement a runtime preview path that consumes the same `FPixelRacerTrackDocument`.

Recommended separation:

- runtime-safe preview builder/components in `PixelRacerCore`
- editor handoff/PIE setup in `PixelRacerTrackEditor`

The runtime preview must be able to instantiate, at minimum:

- tiles
- pieces/scenery
- basic road representation needed for driving/collision
- barriers where imported
- checkpoints
- starting grid
- racing-line debug visualization
- surface identities for vehicle friction/tuning

Do not make the runtime preview depend on editor-only classes.

### Current edited-document handoff

Before PIE starts, provide the runtime preview system a deterministic snapshot of the current editor document. Use a clean handoff mechanism such as a transient subsystem/object, saved sandbox snapshot, or other runtime-safe bridge. Do not introduce a second parallel track format.

### Phase E acceptance criteria

- Edit track.
- Click **Play Track**.
- PIE loads the current track, not an unrelated blank/default level representation.
- Exit PIE with Esc.
- Return to the editor with the authored document intact.
- Make another edit immediately without cooking or packaging.

---

## Phase F — Hachiroku + Bologna Play Track selection

Once runtime preview works, integrate the two starter vehicles.

Required:

- Hachiroku Drifter selectable for Play Track.
- Bologna Superbike selectable for Play Track.
- correct directional sprite switching.
- keyboard support.
- Xbox-style gamepad support through Unreal input mappings.
- PlayStation-style gamepad support through Unreal mappings where available.
- simcade handling.
- surface friction response on asphalt/dirt/grass/sand where the authored surface exists.
- motorcycle tuning must not simply duplicate the heavier car behavior.

Use existing vehicle definitions/stats as the seed; do not discard them.

### Phase F acceptance criteria

The complete target loop works:

`browse real art → draw/edit → choose vehicle → Play Track → drive current track → Esc → edit`

Do not move to career/menu work until this is reliable.

---

## Phase G — Finish procedural-zone generation after the loop works

The geometry-authoring side exists, but asset-backed rule generation is not finished.

Complete:

- preset-specific asset pools/rules for current feasible presets
- regeneration
- generated-object tagging by zone
- preservation of `bManualOverride`
- deleting/moving a generated result should be protectable as a manual override
- visible generated/manual state in editor
- move whole zone
- insert/delete individual polygon vertices
- deterministic seeded output

Initial presets remain:

- Grassland
- Barrier Edge
- Crowd
- Parking

Only generate visuals that available asset packs genuinely support. Current Wheels in Pixels art is not a full SimCity scenery library.

### Critical regeneration test

1. Generate zone content.
2. Manually move/delete selected generated objects and mark/protect the manual change.
3. Regenerate the zone.
4. Protected manual edits must survive.

---

# 7. Key source files to read before editing

### Core schema

`Plugins/PixelRacerTools/Source/PixelRacerCore/Public/PixelRacerTrackTypes.h`

### Canonical authoring API

`Plugins/PixelRacerTools/Source/PixelRacerCore/Public/PixelRacerTrackAuthoringLibrary.h`

`Plugins/PixelRacerTools/Source/PixelRacerCore/Private/PixelRacerTrackAuthoringLibrary.cpp`

### Compatibility façade — do not fork from canonical behavior

`Plugins/PixelRacerTools/Source/PixelRacerCore/Public/PixelRacerAuthoringLibrary.h`

`Plugins/PixelRacerTools/Source/PixelRacerCore/Private/PixelRacerAuthoringLibrary.cpp`

### JSON / validation

`Plugins/PixelRacerTools/Source/PixelRacerCore/Public/PixelRacerTrackLibrary.h`

`Plugins/PixelRacerTools/Source/PixelRacerCore/Private/PixelRacerTrackLibrary.cpp`

### Vehicle foundation

`Plugins/PixelRacerTools/Source/PixelRacerCore/Public/PixelRacerArcadeVehiclePawn.h`

`Plugins/PixelRacerTools/Source/PixelRacerCore/Private/PixelRacerArcadeVehiclePawn.cpp`

`Plugins/PixelRacerTools/Source/PixelRacerCore/Public/PixelRacerVehicleDefinition.h`

### Editor session/history

`Plugins/PixelRacerTools/Source/PixelRacerTrackEditor/Private/PixelRacerTrackEditorSession.*`

### Main editor module

`Plugins/PixelRacerTools/Source/PixelRacerTrackEditor/Private/PixelRacerTrackEditorModule.cpp`

### Canvas

`Plugins/PixelRacerTools/Source/PixelRacerTrackEditor/Private/SPixelRacerTrackCanvas.*`

### Asset Browser

`Plugins/PixelRacerTools/Source/PixelRacerTrackEditor/Private/SPixelRacerAssetBrowser.*`

### Runtime editor source

`Plugins/PixelRacerTools/Source/PixelRacerRuntimeEditor/`

### Asset catalog

`SourceArt/WheelsInPixels/PixelRacerAssetPack_v2.json`

### Starter vehicles

`Plugins/PixelRacerTools/Resources/StarterVehicles/`

### Validation/workflow

`Tools/quick_check.py`

`Tools/QuickCheck.bat`

`Tools/BuildEditorOnce.bat`

`Tools/OpenEditor.bat`

---

# 8. Development behavior for Codex

- Inspect the existing implementation before changing architecture.
- Make focused cumulative changes rather than replacing working systems.
- Prefer compile-driven fixes over speculative rewrites.
- Keep public/core APIs small and runtime-safe.
- Keep editor-only dependencies out of runtime modules.
- Extend QuickCheck when adding new deterministic source/data invariants.
- Do not turn QuickCheck into a fake substitute for UBT/UHT/PIE.
- After meaningful C++ changes, run the real editor build.
- After editor interaction changes, open UE and actually exercise them.
- Preserve source art and attribution/license files.
- Preserve `.pixeltrack`/TrackDocument portability.
- Preserve imported-pack support and portable ID rules.
- Do not silently delete compatibility APIs unless every caller has been migrated and the removal is deliberate.
- Do not commit generated `Intermediate`, `Binaries`, `Saved`, DDC, or other machine-local build output unless the repository explicitly requires it.

When you encounter a defect, fix the underlying code and record the concrete result. Do not disable validation or remove a feature just to make a check pass.

---

# 9. Validation required after each milestone

At minimum:

### Always

`Tools\QuickCheck.bat`

### After C++/module changes

`Tools\BuildEditorOnce.bat`

### After editor UI/interaction changes

Open Unreal and verify the affected interaction in **Window → Pixel Racer Track Editor**.

### After serialization/schema-affecting changes

- export sandbox
- reload exported sandbox
- confirm no data loss
- confirm starter tracks still load

### After Paper2D importer changes

- import
- inspect generated assets
- reimport
- verify no duplicate explosion
- verify pixel filtering/pivots/slicing

### After runtime-preview changes

- click Play Track
- confirm current document appears in PIE
- exit with Esc
- edit again immediately

---

# 10. Documentation to update while working

Keep these truthful and cumulative:

- `README_PIXEL_RACER.md`
- `Docs/DEVELOPMENT_STATUS.md`
- `Docs/TRACK_EDITOR_CONTROLS.md` when controls change
- `Docs/ASSET_PACK_IMPORT.md` when import behavior changes
- add the next handoff/package receipt when creating a new source milestone

Do not claim a milestone is complete until its acceptance criteria were actually exercised at the appropriate level.

---

# 11. Definition of the next meaningful milestone

The next meaningful milestone is not simply “Paper2D code exists.”

It is reached when, on a real UE 5.8.3 installation, the user can:

1. Open Pixel Racer Track Editor.
2. Browse real Wheels in Pixels assets.
3. Draw and edit a spline road.
4. Change local road width/surface/elevation.
5. Paint real tiles.
6. Place real scenery/track pieces.
7. Author a procedural region.
8. Generate checkpoints/grid/racing lines.
9. Choose Hachiroku or Bologna.
10. Click **Play Track**.
11. Drive the currently authored track in PIE.
12. Press Esc.
13. Return to the editor.
14. Make another edit immediately.
15. Repeat without cooking or packaging.

Until that works reliably, keep work centered on this editor-driving loop.
