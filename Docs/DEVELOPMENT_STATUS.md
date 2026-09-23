# Pixel Racer development status

## v0.4-dev Asset Browser + procedural-zone editing — implemented in source

Everything from v0.3-dev remains, plus:

### Manifest-backed Asset Browser

- Added `SPixelRacerAssetBrowser` to `PixelRacerTrackEditor`.
- Loads the real Wheels in Pixels v2 manifest.
- Recursively discovers imported/user pack manifests under `SourceArt/Imported/`.
- Displays source PNG thumbnails in virtualized Slate rows.
- Search by display name, relative path, pack id, and pack display name.
- Category filters for Tiles, Scenery, Vehicles, and VFX.
- Rescan button reloads manifests without restarting the editor.
- Canvas preview metadata refreshes with the browser catalog after Rescan.
- Wheels in Pixels retains legacy path-only TrackDocument asset IDs.
- Imported packs use `packId:path` portable IDs to prevent cross-pack filename collisions.
- Selecting Tiles switches the canvas to Tile Paint and updates `ActiveTile`.
- Selecting Scenery switches the canvas to Piece Placement and updates `ActivePiece`.
- Vehicle and VFX selection is retained without misclassifying them as tile/piece placements.
- Browser drag/drop onto the canvas performs transaction-aware placement for tiles and scenery.
- Painted tiles retain their tileset cell index in the TrackDocument; `[` and `]` cycle the active tile while Tile Paint is selected.
- The canvas draws tiles from imported Paper2D TileSets when available and otherwise previews the corresponding source-PNG cell. Scenery pieces draw their imported Paper2D sprite region or source PNG, with the existing marker as a fallback.
- `PixelRacerTrackEditor.Build.cs` now explicitly depends on `Json` for manifest parsing.

### Procedural-zone editing

- Rectangle drag remains supported.
- Added Polygon drawing.
- Added Freehand region painting.
- Polygon commit: Enter.
- Polygon cancel: right-click or Escape.
- Existing zone vertices can be hit-tested, selected, and dragged.
- Selected zone has highlighted outline and vertex handles.
- Selected zone can be deleted.
- Open polygon/freehand geometry is previewed while authoring.
- All committed zone mutation remains integrated with the editor session undo/redo/autosave transaction model.

## Validation status

- `Tools/QuickCheck.bat`: **0 errors, 0 warnings** before and after the UE build fixes.
- Wheels in Pixels v2 manifest still contains **224 assets**.
- Required roles remain present: vehicle sprite sheets, tilesets, environment pieces, and VFX sheets.
- Every v2 manifest source path resolves to an embedded PNG.
- Asset Browser and drag/drop source guardrails pass QuickCheck.
- TrackEditor C++/header delimiter balance passes static packaging validation.
- Unreal Engine **5.8.3** editor target build now succeeds with `Tools/BuildEditorOnce.bat`.
- `Play Track` snapshots the current document into a transient runtime preview actor, renders road segments, imported Paper2D tiles and sprites (mesh placeholders for missing imports), and focuses a fitted top-down camera. Importing a vehicle sheet creates or updates a saved `UPixelRacerVehicleDefinition` with directional sprites and starter stats; PIE loads that asset, spawns the pawn at the first ordered grid slot (or first road point), possesses it, and supports the configured driving controls. Live PIE verification remains.
- Phase C importer, image-backed canvas, and TrackDocument PIE preview compile successfully; QuickCheck passes with **0 errors, 0 warnings** after the rescan-preview refresh fix.
- `Tools/OpenEditor.bat` launched the project; the Unreal log confirms `PixelRacerTools` mounted, and the Core, TrackEditor, and RuntimeEditor DLLs loaded. The editor process was responsive.
- The Track Editor tab opened through **Window → Pixel Racer Track Editor** on 2026-09-23 using the bundled Computer Use `sky` API. Native access now works; the Unreal MCP bridge still refuses connections.
- Phase B in-editor verification passed on 2026-09-23, combining direct editor observations with the user's final confirmation of all remaining drag gestures. No PIE result or current-document driving loop is claimed.

### UE 5.8.3 compile fixes

The first real UHT/UBT pass exposed these source issues; each was fixed before the successful rebuild:

- UHT rejected `INDEX_NONE` as the reflected default argument for `ErasePiecesInRadius`; both canonical and compatibility declarations now use the equivalent `-1` sentinel.
- UBT warned that the plugin descriptor omitted its direct Paper2D dependency; `Paper2D` is now declared in `PixelRacerTools.uplugin`.
- UE 5.8's typed `JsonObjectStringToUStruct` overload takes the output struct pointer directly; removed the obsolete `StaticStruct()` argument.
- `UPixelRacerValidationDriverComponent` referenced missing vehicle methods; added autonomous input, estimated max-speed, and planar-speed methods to `APixelRacerArcadeVehiclePawn`.
- The Slate header declares `FSlateDynamicImageBrush` as a `struct`; corrected the Asset Browser forward declaration.
- UE 5.8's `FReply` uses `SetUserFocus`, not `SetKeyboardFocus`; updated the Track Editor canvas focus replies.

## Remaining near-term work

1. Complete Phase C's Paper2D import/render adapter in `PixelRacerTrackEditor`:
   - source PNG import now writes a deterministic Unreal texture asset, applies nearest filtering, disables mipmaps/compression, and does not rescale source pixels;
   - vehicle frame metadata creates centered directional Paper2D sprites; tile-grid metadata creates Paper2D TileSets; reimport updates generated assets at the same paths, but stale slices are not pruned when metadata changes;
   - Nitro and Smoke VFX metadata now describes 64×64 grids (56 and 15 frames) and creates centered Paper2D sprites; VFX starter placement and live editor verification remain;
   - the importer and image-backed canvas rendering build successfully with Unreal 5.8.3 and pass QuickCheck; canvas rendering still needs live editor verification.
2. Live-verify the runtime TrackDocument preview and vehicle driving loop in PIE; the code path now supports selected/imported starter vehicles and current-track spawning.
3. Asset-backed procedural-zone generation/regeneration with manual-override preservation.
4. One-click Generate Track and partial regeneration.
5. Minimap/thumbnail generation.
6. Autonomous validation lap and problem markers.
7. Player-facing CommonUI editor shell using the same TrackDocument/authoring API.

## Phase B live validation — 2026-09-23 (completed)

The entries below preserve the verification sequence, including intermediate blockers that were subsequently resolved. Final acceptance is recorded at the end.

- Observed the Track Editor tab open, all 224 entries, and real source thumbnails.
- Category counts observed: Tiles 20, Scenery 7, Vehicles 195, VFX 2. Searching `smoke` narrowed VFX to one result; Rescan retained that filter and the 224-entry catalog.
- Tile/scenery selection displayed the expected portable IDs and switched authoring modes. Successful drops added one tile/piece; Ctrl+Z and Ctrl+Y removed/restored one placement. Immediate drags of an already selected row were inconsistent and need retesting after the drag callback fix.
- Adding the third point of a closed road crashed the editor. The log identifies `SPixelRacerTrackCanvas::OnPaint` at the old line 937 and UE's TArray self-reference assertion. `Points.Add(Points[0])` appended an element from its own array; the zone outline had the same defect. Both now copy the first vertex before appending it.
- Preserved the failing three-point document and crash log under `Saved/PixelRacer/Validation/phase_b_crash_repro.pixeltrack.json` and `phase_b_crash.log` (machine-local evidence, excluded from source control).
- Responsive layout fixes wrap filter/inspector button rows, reduce and wrap the title, and move the Asset Browser above the transaction controls. Asset-row dragging now uses the base STableRow drag delegate so Slate retains its selection/drag bookkeeping.
- Phase B remains open until rebuilt interactions and export/reload pass. Shift+click insertion and a curved Freehand stroke require user-assisted input because the exposed Computer Use API has no modifier-held click or multi-point drag. Phase C has not started.
- Rebuilt all three changed editor translation units with `Tools/BuildEditorOnce.bat`: **Succeeded**. QuickCheck: **0 errors, 0 warnings**. Reopened UE 5.8.3 and loaded the saved three-point crash reproduction through **Reload Exported Sandbox**: the closed triangle, two tiles, and one piece render successfully. The rebuilt layout shows VFX, Elevation +, and Freehand without truncating their buttons.

### Follow-up verification — user-assisted gestures and round trip

- User reported Shift+click insertion and curved Freehand drawing worked. The resulting document was inspected in the actual editor and preserved as `Saved/PixelRacer/Validation/phase_b_user_gestures.pixeltrack.json`: 16 road points and three zones (4, 4, and 29 vertices). One road point also has a manual width of about 120.185.
- Created a new three-vertex Polygon zone, committed with Enter, then verified Ctrl+Z removes it and Ctrl+Y restores it (zone count 3 → 4 → 3 → 4). No outline crash.
- Set road point 12 to dirt and elevation layer 1; deleted it with the inspector (16 → 15 points) and restored it with Undo (16 points).
- Generated seven checkpoints, eight grid slots, and four racing lines.
- Exported through the UI, reloaded through the UI, and exported again. The two files are byte-identical, SHA-256 `E31B4F61978445F467B4019620A920149522E444A8B247589D714158151EC3A5`. The document preserves road geometry, manual width, surface/elevation edits, asset IDs and override flags, four zones, checkpoints, grid slots, racing lines, and the remaining schema-v2 fields. Baseline saved as `Saved/PixelRacer/Validation/phase_b_before_reload.pixeltrack.json`.
- Found and fixed an additional small-window layout defect: custom road/racing-line paint escaped the canvas and covered the inspector/footer. The canvas now explicitly uses `ClipToBounds`; removed its 520 px minimum height and wrapped stats/inspector labels. Rebuild and visual acceptance pending.
- Automated selected-row drops and point drags remain inconclusive: several synthetic drag calls selected their origin but did not move/place anything. Do not claim the delegate change fully resolves dragging until a manual check distinguishes desktop input limitations from application behavior. Remaining focused checks: selected tile/scenery drop + single Undo/Redo, road point/width-handle dragging, zone-vertex dragging, and the rebuilt small-window clipping behavior. Phase C remains gated.
- Clipping-fix acceptance: UE 5.8.3 rebuild succeeded; reopened and loaded the saved document. At approximately 1000×600 the road and racing lines stop at the canvas border, the inspector is unobscured, and the footer wraps visibly. Maximized layout also checked. No additional C++ changes remain unbuilt.

### Final Phase B acceptance — 2026-09-23

The user confirmed **all remaining checks worked** on the rebuilt editor: road point and orange width-handle dragging, zone-vertex dragging, Rectangle creation, and dragging already-selected Tiles/Scenery rows onto the canvas with one-step Undo/Redo. Combined with the recorded browser, polygon/freehand, deletion, serialization, build, and resizing checks above, this completes the Phase B checklist. Synthetic-input failures are not treated as reproduced application defects after the manual confirmation.

- Phase A compile/load and Phase B editor acceptance: complete.
- Latest changed C++ source: built successfully with UE 5.8.3; QuickCheck passed with 0 errors and 0 warnings.
- Phase C importer/canvas adapter: in progress. Pixel-safe textures, metadata-driven vehicle and VFX sprites, Paper2D TileSets, saved vehicle definitions, image-backed tile/piece drawing, tile-cell selection, browser Rescan refresh, and the selected-vehicle PIE drive path are implemented; stale generated slices are not pruned, and the new runtime/canvas rendering still needs live editor verification. Keep editor-only import dependencies in PixelRacerTrackEditor.
- Runtime TrackDocument preview actor and current-track pawn spawning are implemented in `PixelRacerCore`; live PIE verification remains.
