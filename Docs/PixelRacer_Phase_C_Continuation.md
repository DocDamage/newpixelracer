# Pixel Racer — Phase C continuation review

> Historical review of `2246894`. Current source at `042893d` already includes
> the texture object-path correction and conservative generated-output inventory
> and pruning. Do not reapply the accompanying patch or treat section 2 as
> unimplemented. The MCP connection was repaired on 2026-09-23; use
> `DEVELOPMENT_STATUS.md` for current validation results and remaining gates.

## Baseline and scope

Repository: `DocDamage/newpixelracer`  
Branch inspected: `main`  
Commit: `224689441373df6a34f3d2b47e488db34d546e90`  
User workspace: `<local PixelRacer workspace>`

GitHub's main branch matched the supplied commit when inspected. The user's local clean-worktree statement was not independently checked. `Docs/DEVELOPMENT_STATUS.md` records the successful Unreal 5.8.3 editor build and QuickCheck result of zero errors and zero warnings. Those are baseline results, not new results from this review.

This review produced one proposed source patch and an acceptance/implementation plan. It did not modify GitHub, access the Windows workspace, run Unreal Editor, implement stale-slice deletion, or complete Phase C.

## 1. Concrete source correction: texture object path

File: `Plugins/PixelRacerTools/Source/PixelRacerTrackEditor/Private/SPixelRacerAssetBrowser.cpp`  
Function: `BuildImportedAssetPaths`  
Baseline source line: 139  
Baseline Git blob: `602a8ff46f83428367ae6db9f015a42b382dfd02`

`PackagePath` is the destination directory, while `AssetName` is passed separately as the texture import's destination name. The current texture object-path expression omits the texture's package basename:

```text
Current:  <destination-directory>.<asset-name>
Expected: <destination-directory>/<asset-name>.<asset-name>
```

The generated sprite, TileSet and vehicle-definition paths already use the latter structure. The incorrect texture string is copied into browser metadata and returned in import status messages. This finding does not establish that current tile/scenery rendering is broken: those paths can use the separately constructed sprite/TileSet references.

The accompanying `PixelRacer_Texture_Object_Path_Fix.patch` changes only this expression. It does not rename or move existing assets, change portable asset IDs, change texture import settings, or touch the TrackDocument schema.

### Applying the proposed patch

Save the patch in the project root. From PowerShell:

```powershell
Set-Location '<path-to-PixelRacer>'
git status --short
git rev-parse HEAD
git apply --check .\PixelRacer_Texture_Object_Path_Fix.patch
```

Only after the check succeeds:

```powershell
git apply .\PixelRacer_Texture_Object_Path_Fix.patch
git diff --check
git diff -- Plugins/PixelRacerTools/Source/PixelRacerTrackEditor/Private/SPixelRacerAssetBrowser.cpp
.\Tools\QuickCheck.bat
.\Tools\BuildEditorOnce.bat
```

Inspect each exit status and do not record build success unless the build actually succeeds. Do not reset the workspace or overwrite newer work to make the patch apply. A failed context check requires comparing the current file, not forcing the patch. No commit or push is part of this review.

### Regression acceptance

Import one representative source asset. Compare the browser's expected `TextureObjectPath` with the returned imported texture's `GetPathName()` and verify they match. Resolve the expected path to the same texture. Repeat after Rescan and an editor restart. Confirm that the corrected path is shown in the successful-import status and that no duplicate texture package was created.

The patch's syntax and context were tested against the exact retrieved 27-line source excerpt, with padding to preserve its original line numbers. Application, exact resulting content, rejection of duplicate application, and reverse-application checking passed on that excerpt fixture. This is not a full-checkout test, a C++ compile, a QuickCheck run, or an Unreal runtime test. See `validation_receipt.json`.

## 2. Next implementation: safe stale generated-sprite cleanup

This section is a proposed design, not implemented functionality.

Keep the import and cleanup adapter in `PixelRacerTrackEditor`. Do not introduce editor asset-management dependencies into `PixelRacerCore` or the runtime editor.

### Required ownership and ordering

Record a durable generated-output inventory per source asset, including source pack identity, normalized relative source path, importer ownership/version, generated object paths, and output kind. A matching filename prefix alone is not proof of ownership.

Calculate the expected generated-output set from validated metadata. The stale set is the previously owned output set minus the expected output set, never a broad folder or wildcard deletion.

Complete and save the new texture/sprites and any dependent definition before cleanup. `CreateOrUpdateVehicleDefinition` currently replaces and saves `DirectionalSprites` after loading the current expected sprites; retain that ordering. Refactor the role-specific successful returns in `ImportPixelArtAssets` through a single finalization step so a failed frame or definition save cannot accidentally enter cleanup.

A failed or partial import must not prune old assets. Cleanup is not proof of an atomic import: existing assets can already have been updated earlier in the import. Report partial import and cleanup results separately.

### Deletion safeguards

Before removing a candidate, verify importer ownership, source identity, expected class, and exact generated location. Check saved asset dependencies and applicable in-memory references, including other vehicle definitions, animation assets, open/unsaved documents, and active previews. Preserve referenced or ambiguous candidates and report why they were retained.

Do not use force-delete as a shortcut. Do not manipulate `.uasset` files directly. Disable destructive cleanup during PIE and avoid invalidating the editor's undo state silently. Use appropriate editor asset-deletion facilities and handle source-control, read-only, save and deletion failures explicitly. Keep retained candidates in the inventory so later imports can report or retry them.

The baseline importer did not write this ownership inventory. Existing untagged sprites require an explicit migration/review path; do not automatically label every similarly named sprite as owned and delete it. This legacy case is part of completion, not something to hide behind a new-only success test.

### Required regression cases

| Case | Required result |
|---|---|
| Reimport unchanged source | Stable object paths and asset count; no duplicates or deletion. |
| Valid vehicle direction-count decrease | Current definition saved with the new sprite set before unreferenced owned extras are pruned. |
| Valid VFX frame-count decrease | Only proven-owned obsolete frames are eligible. |
| Direction/frame count increase | New frames created; existing expected frames preserved at their paths. |
| Metadata validation failure | No cleanup and a clear error. |
| Partial import or save failure | No pruning of previously valid output; explicit partial-result status. |
| Stale sprite referenced elsewhere | Sprite retained with a reference warning. |
| Manual asset with a similar name | Never deleted by prefix matching. |
| Legacy untagged generated output | Explicit review/migration status, not silent deletion. |
| Cleanup blocked by permissions/source control | Import and cleanup outcomes distinguished; pending outputs retained in the inventory. |
| Role change | All previous output kinds accounted for; references and manual work protected. |

Use a disposable test pack. The importer requires exact dimensions: vehicle sheets are a single row whose width equals direction count times cell width; VFX frame count equals columns times rows. Count-reduction tests must supply consistent source images and metadata. Changing only a count can correctly fail validation rather than exercise cleanup.

## 3. Live canvas and driving acceptance

Use one small TrackDocument containing a closed road, generated grid slots, several cells from an imported TileSet, imported scenery, and one selected/imported vehicle. Keep the saved baseline unchanged and record the tested document and selected asset IDs.

**Canvas:** verify the intended cell rather than the entire atlas, crisp pixels at representative zoom levels, scenery region/pivot/scale, tile rotation and flips, layer order, selected-cell cycling, and refreshed content after reimport/Rescan. Distinguish source-PNG fallback from actual imported Paper2D rendering. A plausible-looking canvas is not sufficient evidence that imported asset references resolved.

**PIE:** start from Play Track and confirm that the current document—not an unrelated starter map—is shown; the selected vehicle is spawned and possessed; its start transform matches the lowest-order grid slot or the documented road fallback; throttle, reverse, steering, drift and reset respond; and the camera behaves as intended. Stop and restart PIE twice, then repeat with a different selected vehicle. Verify that a missing/unimported definition produces a clear warning instead of a false success.

**Orientation check:** `UpdateDirectionalSprite` selects a frame from actor yaw, while the sprite component is attached to the actor and the preview setup applies a relative flat rotation. Check whether inherited yaw plus a pre-oriented frame produces a double-heading effect. This is a source-review concern, not a reproduced defect. Compare straight driving and 90/180/270-degree headings with the actual source sheet; establish frame-zero direction and frame order before choosing a fix. Do not guess an offset or rotate the entire pawn merely to hide a visual mismatch.

**Collision scope:** the preview roads and selected vehicle sprite explicitly disable collision. A successful driving demonstration therefore does not establish barrier collisions, terrain/surface physics, elevation traversal or a complete race simulation. Test and describe only the implemented preview behavior.

## 4. Exit criteria and next milestone

Keep Phase C open until the updated source builds in UE 5.8.3, QuickCheck passes on that exact source, imported canvas rendering and PIE driving have actual live evidence, and stale-slice cleanup—including legacy and reference-retention cases—has passed its tests.

Record the exact commit, test inputs, pass/fail outcomes, log locations, screenshots where relevant, and remaining limitations in `Docs/DEVELOPMENT_STATUS.md`. Never carry the baseline successful-build statement forward as validation of changed C++.

Once Phase C is accepted, continue to asset-backed procedural-zone generation/regeneration with manual-override preservation, followed by one-click generation and partial regeneration. Do not replace the current editor architecture or import code from donor repositories merely to expand this bounded acceptance pass.

## Source basis

All repository observations above refer to the pinned baseline, not an assumed later branch state:

- `Docs/DEVELOPMENT_STATUS.md`: baseline implementation and validation record.
- `SPixelRacerAssetBrowser.cpp`: path construction, strict metadata checks, role-specific import ordering, saved vehicle definitions and status handling.
- `PixelRacerTrackEditorModule.cpp`: Play Track snapshot, preview actor creation and possession.
- `PixelRacerTrackPreviewActor.cpp`: spawn selection, flat preview setup and collision-disabled roads.
- `PixelRacerArcadeVehiclePawn.cpp`: camera, input binding, movement and yaw-based directional frame selection.

The deletion design is intentionally conservative. Epic's UE 5.8 `UEditorAssetLibrary::DeleteAsset` documentation describes that particular operation as force deletion that does not check references and may clear undo history; it is not a safe default for this task.
