# Phase C importer validation through MCP

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
