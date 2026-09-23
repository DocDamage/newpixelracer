# Pixel Racer — Track Editor v0.4-dev

Pixel Racer is being rebuilt in Unreal Engine 5.8.3 with the Track Editor first. The portable `FPixelRacerTrackDocument` remains the source of truth so Unreal editor authoring, the later player-facing editor, procedural generation, PIE preview, and community `.pixeltrack` sharing all use the same track representation.

## Implemented in this source milestone

Everything from v0.3-dev remains, plus:

- A real manifest-backed Slate Asset Browser is integrated into the Track Editor.
- The browser loads `SourceArt/WheelsInPixels/PixelRacerAssetPack_v2.json` directly.
- It also discovers user/imported pack manifests recursively under `SourceArt/Imported/`.
- Browser features now include:
  - source-art thumbnails,
  - search by name/path/pack,
  - All / Tiles / Scenery / Vehicles / VFX filters,
  - single-selection status,
  - rescan without restarting the editor,
  - drag/drop payloads.
- Selecting a tileset automatically makes it the active Tile Paint asset.
- Selecting an environment piece automatically makes it the active Piece Placement asset.
- Dragging a tileset or environment piece from the browser directly onto the canvas performs one transaction-aware placement.
- Existing Wheels in Pixels TrackDocument IDs remain unchanged for compatibility. Imported packs use `packId:path` portable IDs to prevent filename collisions.
- Vehicle and VFX entries are browseable/selectable now, but are deliberately not treated as road tiles or scenery pieces.
- Procedural zones now support three authoring shapes:
  - Rectangle,
  - Polygon,
  - Freehand.
- Polygon authoring uses left-click vertices and Enter to commit; right-click/Esc cancels the open polygon.
- Freehand zones are sampled while dragging and committed as one undoable edit.
- Existing zone vertices can be selected and dragged to reshape the zone.
- Selected procedural zones can be deleted.
- Selected zone outlines/vertices and open-zone previews are drawn directly on the Slate canvas.
- QuickCheck now validates the v2 manifest, required asset roles, unique Wheels asset IDs, source-file existence, Asset Browser source integration, drag/drop integration, and the editor module's Json dependency.

## Important validation status

`Tools/QuickCheck.bat` / `Tools/quick_check.py` passes with **0 errors and 0 warnings**.

The Track Editor C++/header sources also pass a local delimiter-balance/static source check performed during packaging.

The UE 5.8.3 editor target builds successfully with `Tools/BuildEditorOnce.bat`. `Tools/OpenEditor.bat` launches the project and loads `PixelRacerTools` plus its Core, TrackEditor, and RuntimeEditor modules. Live Phase B verification now confirms the tab opens, all 224 browser entries load, thumbnails/search/categories/rescan work, and basic tile/scenery placement supports Undo/Redo. Testing exposed a closed-road drawing crash; the fix has been rebuilt and the saved three-point reproduction now loads successfully. Phase B verification is complete, including user-confirmed point/width/zone dragging and selected-row tile/scenery drops with Undo/Redo. Export/reload was byte-identical. See `Docs/DEVELOPMENT_STATUS.md` for evidence. No PIE result is claimed.

## Phase B editor verification

1. Run `Tools/QuickCheck.bat`.
2. Run `Tools/BuildEditorOnce.bat` if the source has changed or a fresh UE 5.8.3 build is needed.
3. Run `Tools/OpenEditor.bat`.
4. Open **Window → Pixel Racer Track Editor**.
5. Verify Asset Browser thumbnails/search/categories.
6. Select a tileset and paint it; select a barrier/tire and place it.
7. Drag a tile/scenery browser item directly onto the canvas.
8. Verify Rectangle, Polygon, and Freehand procedural zones plus zone-vertex dragging.
9. Verify Undo/Redo and sandbox export/reload, including surface/elevation state.
10. Record and fix any v0.4 defects before starting the Paper2D importer. PIE behavior remains unverified; the current TrackDocument runtime preview is a later milestone.

## Next implementation target

Phase B passed on 2026-09-23. Begin Phase C: the actual Paper2D import/render adapter in PixelRacerTrackEditor, followed by the runtime TrackDocument preview actor. Phase C has not started. The immediate product goal remains:

`browse real art → draw/edit track → Play Track → drive authored track → Esc → edit`
