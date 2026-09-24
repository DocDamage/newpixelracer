# Pixel Racer development status

## Current milestone: asset-backed zone generation — 2026-09-24

- Procedural zones now generate deterministic tiles or scenery from assigned
  catalog assets, with density, seed, tile index, scenery spacing, and road clearance.
- Regeneration preserves manual/locked placements and erased-slot exclusions;
  shrinking or deleting a zone removes only its unprotected generated output.
- Editor controls assign assets, select zones, restore erased slots, and generate
  one zone or all configured zones. Generate Track Details also derives checkpoints,
  eight grid slots, and racing lines from the primary road in one undo transaction.
  Missing assets, invalid geometry, and unusable roads reject the entire operation.
- Geometry, candidate counts, and intersection work are bounded before mutation.
  Ownership and settings persist through TrackDocument JSON.
- Import success refreshes canvas preview metadata; unavailable imported preview
  objects are negatively cached until the next catalog refresh.
- Combined implementation builds successfully in UE 5.8.3. Eight core generation
  tests, the editor transaction/catalog/render test, and the expanded importer
  smoke test passed live with zero errors/warnings. QuickCheck and scoped diff
  whitespace checks pass. Independent integration review found the missing-road
  success-reporting issue; it is fixed and covered by an atomicity regression.
- Evidence: `Saved/PixelRacer/Validation/procedural_zone_acceptance.json`,
  `procedural_zone_preview.png`, and `procedural_zone_acceptance.log`;
  final corrective build: `Saved/Logs/Procedural-Zones-Acceptance-Build.log`.
  Initial acceptance exposed a rejected-polygon test fixture indexing an absent
  zone; fixture setup and indexing prerequisites are corrected.
- MCP connected, PIE stopped, SmokeFilter restored, no dirty persistent packages,
  and sandbox export/autosave hashes unchanged. No commit or push.
- Next substantial milestone: current-document Play Track/driving acceptance and
  remaining Phase C interactive stale deletion and full canvas/Rescan gestures.
  This batch verifies source-PNG canvas rendering and catalog refresh behavior;
  it does not claim the complete interactive Phase C acceptance matrix.

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
   - vehicle frame metadata creates centered directional Paper2D sprites; tile-grid metadata creates Paper2D TileSets; reimport updates generated assets at the same paths and now tracks generated outputs per source, pruning only verified stale outputs;
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
- Phase C importer/canvas adapter: in progress. Pixel-safe textures, metadata-driven vehicle and VFX sprites, Paper2D TileSets, saved vehicle definitions, image-backed tile/piece drawing, tile-cell selection, browser Rescan refresh, selected-vehicle PIE driving, and conservative stale-output pruning are implemented. Keep editor-only import dependencies in PixelRacerTrackEditor.
- Runtime TrackDocument preview actor and current-track pawn spawning are implemented in `PixelRacerCore`; live PIE verification remains.

### Phase C importer continuation — 2026-09-23

- Added a per-source generated-output inventory to the imported texture package metadata, plus owner/source/kind/version metadata on generated assets.
- A complete import saves current outputs before cleanup. Partial imports skip inventory updates and pruning. Import completion is reported separately from inventory/cleanup failures. Pruning checks inventory identity, exact output location, class and ownership tags, saved and loaded referencers, PIE, dirty packages, and read-only files; it uses Unreal's confirmed asset deletion API without force-delete helpers.
- Outputs that are referenced, dirty, read-only, ambiguous, or blocked by source control remain inventoried with a reason. Legacy name matches are reported for manual review and never adopted or deleted automatically.
- Verification on this source revision: `Tools/BuildEditorOnce.bat` succeeded; `python Tools/quick_check.py` reported 0 errors and 0 warnings; `git diff --check` passed; the focused independent cleanup review found no actionable issues.
- Live-check status: the editor process launched and responds, but the Unreal MCP bridge refuses connections; no import/delete/PIE regression was run in this turn.
- Still required in a disposable test pack and live editor: unchanged reimport, direction/frame count decreases and increases, role change, invalid metadata, partial import/save failure, reference retention, legacy untagged outputs, source-control/read-only behavior, and cancellation/retry of the delete confirmation. No runtime cleanup or PIE claim is made yet.

### Phase C live MCP importer regression — 2026-09-23

- Continued from `042893d` with the repaired bridge; confirmed UE 5.8.3, project
  `PixelRacer`, and PIE stopped. The historical continuation review predates this
  source: its proposed path fix and ownership/pruning implementation already exist.
- Added `PixelRacer.TrackEditor.AssetBrowser.ImportReimportSmoke`, which calls the
  production importer with GUID-isolated copies of the Wheels barrier and grass
  PNGs. It checks resolvable texture object paths, pixel settings and dimensions,
  generated sprite/TileSet assets, unchanged environment reimport, and rejection
  of mismatched dimensions while retaining the ownership inventory.
- The first live MCP run passed with zero errors and four missing-object warnings
  from optional first-import lookups. Changed those optional lookups to
  `LOAD_NoWarn`; required sprite lookups and explicit import failure reporting
  retain their diagnostics. Saved initial evidence in
  `Saved/PixelRacer/Validation/phase_c_mcp_before_warning_fix.json`.
- The final changed source builds successfully with `Tools/BuildEditorOnce.bat`;
  log: `Saved/Logs/PhaseC-MCP-Build-final.log`. QuickCheck: zero errors and warnings.
  Changed-file whitespace validation passes; the pre-existing trailing blank
  line in `Config/DefaultEngine.ini` was left untouched.
- Test invocation and artifact locations are in `PHASE_C_MCP_VALIDATION.md`.
- Final live MCP regression on the rebuilt source: **passed, zero errors, zero
  warnings**, completed in 0.215 seconds. Evidence:
  `Saved/PixelRacer/Validation/phase_c_mcp_final.json`. The first run's saved
  barrier texture also loaded after restart: 144×16, BGRA8, one mip, Pixels2D,
  editor-icon compression, and no streaming. The bridge requires the shorter
  package path here because its input validator caps paths at 240 characters;
  texture dimensions settled after asynchronous resource compilation.
  Focused review found no actionable regression in the optional-lookup change.
- The editor is reopened with MCP responding. Sandbox export/autosave hashes
  match their pre-test backups. The automation filter was restored to SmokeFilter.
  No commit or push was performed.
- Test artifact handling:
  Generated smoke assets are narrowly ignored by Git. The existing sandbox export
  and autosave were backed up in `Saved/PixelRacer/Validation/pre_mcp_phase_c/`
  before the editor restart.
- Phase C remains open: visual canvas/Rescan behavior, vehicle/VFX imports and
  driving in PIE, and the complete stale-output deletion/retention matrix still
  need live acceptance. This regression covers only environment and tileset
  imports, unchanged environment reimport, and dimension-mismatch rejection.

### Phase C runtime preview and vehicle PIE — 2026-09-24

- Reloaded the saved sandbox document in the Track Editor and verified the
  current-track preview spawned `PixelRacerTrackPreviewActor` and a possessed
  `PixelRacerArcadeVehiclePawn` in PIE.
- Imported `Bologna Superbike` from the Asset Browser; the PIE pawn's
  `VehicleDefinition` resolved to its generated project asset. A short `W`
  input moved the pawn, and `R` returned it to its exact starting transform.
- Captured the PIE viewport at
  `Saved/PixelRacer/Validation/phase_c_mcp_pie.png`; machine-readable MCP
  evidence is in `Saved/PixelRacer/Validation/phase_c_mcp_pie.json`. PIE was
  stopped and `pie.is_running` confirmed zero worlds afterward.
- This verifies one selected vehicle's import-to-PIE path. It does not cover the
  full VFX/vehicle metadata matrix, visual canvas/Rescan behavior, or stale-output
  deletion and retention cases. Direct MCP asset-property inspection of the
  generated vehicle definition was also blocked by the bridge's 240-character
  asset-path limit; the pawn's live property read confirmed the object reference.

### Phase C vehicle and VFX importer regression — 2026-09-24

- Extended `PixelRacer.TrackEditor.AssetBrowser.ImportReimportSmoke` to import
  the Bologna Superbike directional sheet and Smoke VFX sheet alongside the
  existing environment and tileset fixtures. It verifies all 16 vehicle sprites
  and their definition references, all 15 VFX frame sprites, their source
  textures, and stable outputs after unchanged reimports.
- Rebuilt the editor target successfully with UE 5.8.3. The live MCP automation
  run passed in 1.474 seconds with zero errors and zero warnings. Evidence:
  `Saved/PixelRacer/Validation/phase_c_mcp_import_roles.json`.
- Restored the `SmokeFilter`; PIE is stopped. `package.list_dirty` reports no
  dirty persistent packages.
- Phase C still needs the stale-output deletion/retention matrix and visual
  canvas/Rescan acceptance. Partial import/save failures and invalid vehicle/VFX
  metadata also remain outside this regression.

### Phase C stale-output reference retention case — 2026-09-24

- Extended `ImportReimportSmoke` to reimport the Smoke sheet with a smaller
  valid frame grid while a transient data asset holds loaded references to the
  three obsolete frames. The test checks those assets stay resolvable and in the
  ownership inventory with the loaded-reference reason.
- QuickCheck passes with 0 errors and 0 warnings. UnrealBuildTool compiled the
  changed translation unit, but could not relink the editor module because the
  running editor holds its DLL. The Live Coding compile hit its 180-second cap
  with `InProgress` and no patched module reported, so the new case is not yet
  live-verified. Rebuild after restarting the editor, then rerun the importer
  automation before treating this retention case as accepted.

### Phase C loaded-reference retention acceptance — 2026-09-24

- Rebuilt and reopened UE 5.8.3; MCP responds and confirms `PixelRacer`.
- The first live run retained frames 12–14 but failed the expected-reason checks:
  dirty native `/Script/SlateCore` incorrectly triggered the unsaved-content gate.
  `FindOtherDirtyPackage` now excludes `PKG_CompiledIn` packages. Real dirty
  content packages still block cleanup; loaded references remain checked.
- The test now holds its transient referencer with `TStrongObjectPtr` so garbage
  collection cannot invalidate the fixture, and failures include the actual reason.
- Final `ImportReimportSmoke` passed in 1.617 seconds with zero errors and warnings,
  verifying imports/reimports plus loaded-reference retention and inventory reasons.
  Evidence: `Saved/PixelRacer/Validation/phase_c_mcp_loaded_retention.json` and
  matching `.log`; pre-fix evidence: `phase_c_retention_before_fix.log`.
- Final build succeeded (`Saved/Logs/PhaseC-Retention-Build-noaccel.log`, invoked
  with `-NoUBA -NoUBALocal` after a stalled build). Existing MCP bridge deprecation
  warnings remain. QuickCheck: zero errors/warnings; focused review's lifetime
  finding is addressed. SmokeFilter restored; PIE stopped; no dirty persistent
  packages. No commit or push.
- Next bounded work: remaining stale-output deletion/retention cases (saved
  references, dirty/read-only outputs, ownership mismatches and partial failures),
  followed by visual canvas/Rescan acceptance. Phase C is still open.

### Phase C retention safety matrix — 2026-09-24

- Extended the production importer regression with four isolated safety cases:
  another dirty persistent package, a dirty stale sprite, a read-only stale
  sprite file, and mismatched generated-owner metadata.
- Each case asserts its exact retention reason, the retained-three summary,
  all 16 inventory entries, and all three obsolete sprites remaining resolvable
  and on disk. Scoped restoration is checked; a final reimport verifies reasons
  return to loaded-reference retention after fixture cleanup.
- UE 5.8.3 incremental build succeeded in 24.45 seconds; log:
  `Saved/Logs/PhaseC-Retention-Matrix-Build.log`. QuickCheck and scoped whitespace
  validation passed; parent review corrections were incorporated before building.
- Live `ImportReimportSmoke` passed in 2.559 seconds with zero errors/warnings.
  Evidence: `Saved/PixelRacer/Validation/phase_c_mcp_retention_matrix.json` and
  matching `.log`. MCP remains connected, PIE stopped, SmokeFilter restored,
  no dirty persistent packages, and sandbox export/autosave hashes unchanged.
- Next bounded work: saved-package reference retention and partial-import/save
  failure cases, then confirmed stale deletion and visual canvas/Rescan acceptance.
  No deletion behavior or full Phase C completion is claimed. No commit or push.

### Phase C saved-reference and partial-save recovery — 2026-09-24

- `SaveImportedAsset` now reports the exact read-only package before attempting
  Unreal's package save. A generated-frame save failure still stops import before
  inventory finalization or stale cleanup.
- Added a saved-reference fixture using the run's generated vehicle definition;
  after saving and refreshing the asset registry, all three stale VFX frames must
  record that saved package as the retention reason. Original references are
  restored and saved before the next case.
- Added a failure on current VFX frame 5: five earlier frames save, the protected
  file fails, the in-memory ownership inventory remains unchanged, and stale
  frame files survive. Restoring write access and retrying completes the import
  and refreshes all retention reasons. Scoped fixture cleanup covers early exits.
- UE 5.8.3 build succeeded in 26.56 seconds; QuickCheck passed with zero errors
  and warnings. Live `ImportReimportSmoke` passed in 6.333 seconds with zero
  errors/warnings. Evidence: `Saved/PixelRacer/Validation/phase_c_mcp_partial_save.json`
  and matching `.log`; build log: `Saved/Logs/PhaseC-Partial-Save-Build.log`.
- Editor remains connected via MCP, PIE stopped, SmokeFilter restored, no dirty
  persistent packages, and sandbox export/autosave hashes unchanged. No commit/push.
- Next: confirmed stale deletion and visual canvas/Rescan acceptance. Source
  texture and inventory-save failures remain distinct uncovered stages; do not
  claim the entire failure matrix or Phase C complete.
