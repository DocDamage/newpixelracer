# Pixel Racer

Pixel Racer is an Unreal Engine track-authoring project: build a top-down racing track from pixel art, edit it, play the same authored track, then return to editing. Its portable `FPixelRacerTrackDocument` is the single source of truth for editor authoring, future player-facing editing, procedural generation, PIE preview, and JSON `.pixeltrack` sharing.

## Repository status

This repository contains the **v0.4-dev** Unreal project, PixelRacerTools source, starter track/vehicle data, and source art. Phase B editor verification is complete. Generated binaries, caches, autosaves, and local test output are excluded.

## Requirements

- Windows
- Unreal Engine **5.8.3**
- Visual Studio 2022 with the Windows C++ and Unreal Engine development components
- Python 3 for the lightweight source check

The helper scripts look for Unreal at `C:\Program Files\Epic Games\UE_5.8` by default. Set `UE58_ROOT` when your 5.8.3 installation is elsewhere.

```powershell
$env:UE58_ROOT = 'D:\Epic\UE_5.8'
```

## Build and open

Clone the repository, then run the helper scripts:

```powershell
git clone https://github.com/DocDamage/newpixelracer.git
cd newpixelracer
Tools\QuickCheck.bat
Tools\BuildEditorOnce.bat
Tools\OpenEditor.bat
```

`QuickCheck` validates project and plugin descriptors, starter data, source-art manifests, and deterministic source guardrails. It does not replace an Unreal build or editor test. The project targets UE 5.8.3; the editor target has built successfully and `PixelRacerTools` has loaded in the editor.

Open the editor tab from **Window → Pixel Racer Track Editor**.

## Architecture

- `Plugins\PixelRacerTools\Source\PixelRacerCore` holds the schema-v2 TrackDocument, JSON portability, and canonical authoring operations.
- `Plugins\PixelRacerTools\Source\PixelRacerTrackEditor` provides the Slate track editor and manifest-backed asset browser.
- `Plugins\PixelRacerTools\Source\PixelRacerRuntimeEditor` is the runtime-facing editing foundation.
- `SourceArt\WheelsInPixels\PixelRacerAssetPack_v2.json` catalogs 224 Wheels in Pixels assets. Imported packs use portable `packId:path` asset IDs.

The project keeps editor-only importing code out of the runtime core. Paper2D is required; Paper2D+ remains optional.

## Implemented

- TrackDocument schema v2 with roads, tiles, pieces, procedural zones, checkpoints, grid slots, racing lines, surfaces, elevation layers, and generation settings.
- A Slate Track Editor with spline and tile/piece authoring, undo/redo, autosave, sandbox export/reload, and procedural Rectangle, Polygon, and Freehand zones.
- Asset Browser search, categories, source-PNG thumbnails, manifest rescan, active-asset selection, and tile/scenery drag-and-drop placement.
- UE 5.8.3 compile fixes, a successful editor-target build, and confirmed plugin/module loading.

## Current gate and next work

**Phase B passed on 2026-09-23**, using live editor checks and user-assisted gesture verification. The closed-road/zone drawing crash and small-window overflow were fixed and rebuilt. Export → reload → export was byte-identical, including manual width, surface/elevation state, zones, and generated driving metadata. See [development status](Docs/DEVELOPMENT_STATUS.md) and [editor controls](Docs/TRACK_EDITOR_CONTROLS.md).

Phase C has started with a selected-PNG importer in the Asset Browser. It writes deterministic assets under `/Game/PixelRacer/Imported/<pack>/<source folders>`, applies nearest filtering, disables mipmaps and texture compression, creates Paper2D TileSets from tile-grid metadata, and slices vehicle sheets into centered directional sprites. Vehicle imports also create or update a saved `UPixelRacerVehicleDefinition` asset that holds the directional sprites and starter stats. The Nitro and Smoke VFX sheets declare 64×64 frame grids and import as 56 and 15 centered Paper2D sprites. Reimport refreshes generated assets at the same paths. A per-source ownership inventory supports conservative stale-output pruning; referenced, dirty, read-only, and untracked legacy assets are retained for review. Painted tiles retain their tileset cell index (`[` and `]` cycle cells), and the authoring canvas draws imported tiles and scenery sprites with source-PNG preview fallbacks. Play Track snapshots the current TrackDocument into a transient PIE preview with road segments, Paper2D tile maps, scenery sprites, and a fitted top-down camera. Selecting an imported vehicle in the Asset Browser lets PIE load its saved definition, spawn the vehicle at the first grid slot (or first road point), and possess it for driving; use WASD/arrows, Space to drift, and R to return to the start. Missing vehicle definitions or sprite references are reported before PIE starts. UE 5.8.3 builds and QuickCheck pass; live verification of PIE, canvas rendering, and the stale-output cleanup safety cases remains.

## Source-art credits

- Wheels in Pixels: supplied under [CC0 with its fallback license](SourceArt/WheelsInPixels/license.txt).
- Racing UI Kit by Ville Seppänen: [CC BY 4.0 attribution](SourceArt/RacingUIKit/ATTRIBUTION.txt). The original PSD is included.

These asset licenses apply to their respective packs; no project-wide code license is declared.
