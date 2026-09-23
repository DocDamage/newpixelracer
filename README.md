# Pixel Racer

Pixel Racer is an Unreal Engine track-authoring project: build a top-down racing track from pixel art, edit it, play the same authored track, then return to editing. Its portable `FPixelRacerTrackDocument` is the single source of truth for editor authoring, future player-facing editing, procedural generation, PIE preview, and JSON `.pixeltrack` sharing.

## Repository status

This GitHub repository is currently a landing page for `DocDamage/newpixelracer`. The active Unreal source, assets, and detailed development notes are being maintained in a local workspace while publication is prepared. Paths below describe that complete local workspace and are intentionally shown as code rather than links to files that are not yet published here.

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

From the full local workspace:

```powershell
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

**Phase B is still pending.** Before implementing the Paper2D importer, manually exercise the existing editor: browser thumbnails/search/categories/rescan and 224 entries; tile and scenery selection and drag/drop; road and width editing; surface/elevation export/reload; all zone tools and vertex dragging; Undo/Redo; and sandbox round-tripping. Fix any v0.4 defects found there first.

After that gate passes, the next milestone is the Paper2D import/render adapter: deterministic source-PNG imports, pixel-safe textures, sprite slicing and pivots, reimport behavior, and starter tiles, scenery, vehicles, and VFX. Runtime preview and the complete edit → play → edit loop follow.
