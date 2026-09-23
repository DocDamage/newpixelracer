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

The next milestone is the Paper2D import/render adapter: deterministic source-PNG imports, pixel-safe textures, sprite slicing and pivots, reimport behavior, and starter tiles, scenery, vehicles, and VFX. It has not started. The canvas currently draws authoring primitives; Play Track starts ordinary PIE and does not yet instantiate the edited TrackDocument. Runtime preview and the complete edit → play → edit loop follow.

## Source-art credits

- Wheels in Pixels: supplied under [CC0 with its fallback license](SourceArt/WheelsInPixels/license.txt).
- Racing UI Kit by Ville Seppänen: [CC BY 4.0 attribution](SourceArt/RacingUIKit/ATTRIBUTION.txt). The original PSD is included.

These asset licenses apply to their respective packs; no project-wide code license is declared.
