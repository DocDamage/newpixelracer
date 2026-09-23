# Pixel Racer Track Editor v0.4-dev controls

Open **Window → Pixel Racer Track Editor**.

The tools and inspector panels scroll independently. The Asset Browser appears below the mode buttons; Undo/Redo and export/reload are further down the left panel. In smaller windows, filters and inspector controls wrap, and track drawing stays inside the canvas.

## Asset Browser

The left palette now contains the manifest-backed Asset Browser.

- **All / Tiles / Scenery / Vehicles / VFX**: filter the current pack catalog.
- Search filters by asset name, relative path, pack id, or pack display name.
- **Rescan** reloads Wheels in Pixels plus manifests found under `SourceArt/Imported/`.
- Click a **Tiles** entry: it becomes the active tile and the editor switches to Tile Paint.
- Click a **Scenery** entry: it becomes the active piece and the editor switches to Piece Placement.
- Drag a tile/scenery entry directly onto the canvas for a one-shot placement.
- Vehicles and VFX can be browsed/selected but are not currently placed as track tiles or scenery.

## Select / edit road

- Left-click a road point: select it and begin dragging.
- Drag selected point: move the control point.
- Drag the orange width handle: change local road width.
- Right-click a road point: delete it.
- Delete/Backspace: delete the currently selected road point.
- Inspector buttons: set selected point surface to Asphalt, Dirt, Grass, or Sand.
- Inspector **Elevation - / +**: move through Underground (-1), Ground (0), Elevated 1 (1), Elevated 2 (2).

## Spline / freehand road

- Left-click empty canvas: add a centerline control point.
- Left-click an existing point: select/drag it.
- Shift+left-click near a road segment: insert a control point into that segment.
- **Generate AI + Checkpoints** derives checkpoints, starting grid, and generated racing lines from the primary road.
- **Bake Spline to Pieces** creates individually editable generated pieces.

## Tile paint

- Select any tileset in the Asset Browser to make it active.
- Hold/drag left mouse: paint tiles on the 32-unit authoring grid.
- Hold/drag right mouse: erase tiles.
- One drag stroke is one undo transaction.

## Piece placement

- Select an environment/scenery item in the Asset Browser to make it active.
- Left click: place the active piece snapped to the grid.
- Right click: erase unlocked pieces near the pointer on the active layer.
- Manual pieces are marked as manual overrides.

## Procedural zones

Select **Procedural zones** and choose a preset: Grassland, Barrier Edge, Crowd, or Parking.

### Rectangle

- Choose **Rectangle**.
- Left-drag the zone bounds.

### Polygon

- Choose **Polygon**.
- Left-click each vertex.
- Press **Enter** when at least three vertices exist to commit the zone.
- Right-click or **Esc** cancels the open polygon.

### Freehand

- Choose **Freehand**.
- Left-drag the region outline.
- Release to commit the zone.

### Edit existing zone

- Click/drag a visible zone vertex to reshape it.
- The selected zone is highlighted with edit handles.
- **Delete Selected Zone** or Delete/Backspace removes the selected zone while Procedural Zone mode is active.

Asset-backed preset regeneration and manual-override protection for generated scenery are still pending.

## Undo / redo

- Ctrl+Z: undo.
- Ctrl+Y: redo.
- Ctrl+Shift+Z: redo.
- Matching Undo/Redo buttons are available in the left palette.
- Edits autosave to `Saved/PixelRacer/Autosaves/` when a transaction ends.

## Export / reload

**Export Sandbox Track** writes:

`Saved/PixelRacer/Tracks/editor_sandbox.pixeltrack.json`

**Reload Exported Sandbox** imports that same file back into the canvas for round-trip testing.

## Fast iteration rule

Normal track authoring remains:

`edit → lightweight validate → PIE → Esc → edit`

No cook, package, or full automation suite is required for ordinary track changes.
