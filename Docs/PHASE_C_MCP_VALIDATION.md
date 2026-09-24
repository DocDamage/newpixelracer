# Phase C importer validation through MCP

## Latest batch acceptance — 2026-09-24

The expanded importer smoke test passed in 2.983 seconds with zero errors and
warnings. It now covers a read-only source texture (zero generated outputs) and
a pre-cleanup ownership-inventory save failure. Both preserve stale frame files;
the latter checks unchanged source-package bytes on disk and requires the explicit
cleanup-skipped summary. Scoped restoration allows a successful retry afterward.
Final inventory saving after confirmed deletion remains a separate uncovered stage.

The new zone editor test passed with catalog removal/restoration, invalid tile
indices, missing-road rejection, atomic all-zone generation, and full undo/redo
document comparisons. A real Slate screenshot shows grass tiles and red barrier
sprites using manifest-backed source previews. This does not replace remaining
interactive Rescan/placement acceptance or confirmed stale deletion.

Evidence: `Saved/PixelRacer/Validation/procedural_zone_acceptance.json`,
`procedural_zone_acceptance.log`, and `procedural_zone_preview.png`. MCP remains
connected, PIE stopped, SmokeFilter restored, no persistent packages dirty, and
sandbox export/autosave hashes unchanged.

Build the editor target with `Tools/BuildEditorOnce.bat`, then open Pixel Racer
with `Tools/OpenEditor.bat`. New automation tests require the rebuilt module.

Call `unreal_mcp_ping`, then `unreal_mcp_call` with
`{"method":"editor.project_name"}`. Confirm the connected project is
`PixelRacer` before running a test. PIE must be stopped.

Enable the product-test filter (the initial filter can hide this test), then
enumerate the test:

```json
{"method":"test.set_filter_flags","args":{"flags":["ProductFilter"]}}
```

```json
{"method":"test.list_automation_specs","args":{"filter":"PixelRacer.TrackEditor.AssetBrowser","page_size":20}}
```

Run the importer regression:

```json
{"method":"test.run_single_test","args":{"test_name":"PixelRacer.TrackEditor.AssetBrowser.ImportReimportSmoke"},"timeoutMs":120000}
```

Check the inner result, not just the RPC envelope: `succeeded` and `completed`
must be true, `cap_exceeded` must be false, and `error_count` must be zero.
Retain the response and the corresponding Unreal log as validation evidence.
Afterward, restore the desired filter, for example `{"flags":["SmokeFilter"]}`
with `test.set_filter_flags`; the setting persists for the editor session.

The test uses real source PNGs copied to a unique directory under
`Saved/Automation/PixelRacerAssetBrowserImportSmoke/`. Generated assets remain
under a unique `/Game/PixelRacer/Imported/pixelracer_automation_import_smoke_*`
folder for inspection. These test outputs are excluded from source control.
It exercises the production importer rather than a separate mock import path.

Coverage includes an environment sprite and a grass TileSet, resolvable texture
object paths, preserved source dimensions, nearest filtering, disabled mipmaps,
Pixels2D texture settings, unchanged environment reimport, and rejection of
dimension-mismatched metadata while retaining the previous ownership inventory.
The temporary source copies are removed after the test; saved generated assets
are retained only for inspection, not as reusable source packs.

This is an importer check. It does not establish visual canvas correctness,
PIE driving behavior, or the full stale-output deletion/retention matrix.
Those remain separate Phase C acceptance gates in `DEVELOPMENT_STATUS.md`.

## Runtime preview and selected vehicle PIE check

After building and reopening the editor, load the saved sandbox track, select
and import a vehicle in the Asset Browser, and use **Play Track (PIE)**. Inspect
`pie.get_pawn`, `actor.get_property` for `VehicleDefinition`, and
`pie.dump_world_state` for the preview actor. Send a brief `W` press followed by
`release`, then `R` to verify driving and reset. Stop PIE with `pie.stop` and
confirm `pie.is_running` returns `running: false` and `world_count: 0`.

The 2026-09-24 live check passed for Bologna Superbike. Evidence is saved in
`Saved/PixelRacer/Validation/phase_c_mcp_pie.json` and the viewport capture in
`Saved/PixelRacer/Validation/phase_c_mcp_pie.png`. MCP direct asset-property
inspection of the generated definition was limited by the bridge's 240-character
asset path guard; the live pawn property read still confirmed the definition
reference. This does not cover the remaining canvas/Rescan or stale-output
cleanup/retention acceptance cases.

## Vehicle and VFX importer regression

The extended `ImportReimportSmoke` automation test imports four real pack roles:
environment piece, tileset, vehicle sprite sheet, and VFX sheet. For the vehicle
it checks all 16 directional sprites resolve to the imported texture and are
referenced by the vehicle definition. For Smoke VFX it checks all 15 frame
sprites resolve to the imported texture. Both roles are reimported unchanged to
verify the texture object path and generated output count remain stable; the VFX
test also verifies its ownership inventory. The environment mismatch rejection
and tileset generation checks remain in the same test.

On 2026-09-24, the test passed in UE 5.8.3 through the live bridge with zero
errors and warnings. The editor was left open, PIE stopped, and `SmokeFilter`
restored. Evidence is in
`Saved/PixelRacer/Validation/phase_c_mcp_import_roles.json`. This does not verify
stale-output deletion, retention when outputs are referenced or read-only,
source-control blocks, partial failures, or invalid vehicle/VFX metadata.

### Loaded-reference stale-output retention case

The test source now also reimports Smoke with a valid 3-by-4 frame grid after
its original 3-by-5 import. A transient vehicle definition keeps strong loaded
references to frames 12–14. The case asserts the reimport completes, the old
frame assets remain resolvable and inventoried, and each retained record stores
the loaded-reference reason. This avoids exercising Unreal's interactive delete
confirmation while checking the reference safety gate.

Live verification passed on 2026-09-24 after rebuilding and reopening UE 5.8.3:
1.617 seconds, zero errors and warnings. Evidence is saved in
`Saved/PixelRacer/Validation/phase_c_mcp_loaded_retention.json` and the matching
`.log`. The older `phase_c_mcp_import_roles.json` predates this case.

The first run exposed a production cleanup defect: dirty compiled-in script
packages such as `/Script/SlateCore` blocked cleanup before loaded references
could be checked. The dirty-content guard now excludes `PKG_CompiledIn` packages;
real unsaved content remains protected. The transient test referencer is held
with `TStrongObjectPtr` for GC safety, and failed reason checks print the actual
retention reason. This passing test verifies loaded-reference retention only;
deletion and the remaining retention/failure matrix still need acceptance.

### Dirty, read-only, and ownership retention cases

The same test now reimports the reduced Smoke grid with four isolated conditions:
an unrelated generated vehicle-sprite package marked dirty, a stale VFX sprite
marked dirty, a stale VFX file marked read-only, and a stale sprite with changed
owner metadata. Each case asserts the specific retention reason, all 16 inventory
records, and that all three obsolete sprites still resolve and exist on disk.

The strong reference holder remains alive throughout these cases to prevent
interactive deletion if a guard regresses; exact reason assertions ensure this
fallback cannot hide a failed guard. Scoped cleanup restores each fixture's state,
including on early return. A final reimport must replace the temporary guard
reasons with loaded-reference reasons. All mutations are limited to this run's
unique generated smoke assets.

Live acceptance passed on 2026-09-24 with UE 5.8.3: 2.559 seconds, zero errors and
warnings. Evidence: `Saved/PixelRacer/Validation/phase_c_mcp_retention_matrix.json`
and matching `.log`. The editor was left connected with PIE stopped, SmokeFilter
restored, no dirty persistent packages, and unchanged sandbox export/autosave
hashes. Saved-reference retention, partial failures, and confirmed deletion are
still separate pending cases.

### Saved references and partial-save recovery

The smoke test temporarily saves references to the three obsolete VFX frames in
its own generated vehicle definition, refreshes the asset registry from that
file, and requires the saved-package retention reason for each frame. It restores
and saves the original directional references afterward.

For partial failure, the test makes current frame 5 read-only and reimports the
reduced grid. Frames 0–4 must save, frame 5 must fail with its exact package path,
and the import must leave the ownership inventory unchanged without claiming
cleanup completed. All obsolete frame files must survive. The fixture restores
write access and a successful retry must save the blocked frame and return all
stale records to their loaded-reference reasons. This covers a generated-sprite
save failure, not every texture/inventory-save failure stage.

Live acceptance passed on 2026-09-24 in UE 5.8.3: 6.333 seconds with zero errors
and warnings. Evidence: `Saved/PixelRacer/Validation/phase_c_mcp_partial_save.json`
and matching `.log`. No persistent packages were left dirty and the sandbox
export/autosave hashes match their pre-test values. The inventory comparison is
in-memory; it does not claim reload-after-crash durability for failed imports.
