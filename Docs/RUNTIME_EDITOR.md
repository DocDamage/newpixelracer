# Player-facing runtime editor foundation

`PixelRacerRuntimeEditor` is now registered as a runtime module in `PixelRacerTools.uplugin`, not as an Unreal Editor module. The shipped game can therefore build a UMG/CommonUI track editor on the same `FPixelRacerTrackDocument` representation used by the developer Track Editor.

## Shared authoring logic

`UPixelRacerTrackAuthoringLibrary` is the canonical schema-v2 authoring API.

The older `UPixelRacerAuthoringLibrary` name is retained as a compatibility façade for the existing runtime subsystem. Its overlapping road/tile/piece/race operations delegate to the canonical library rather than maintaining a second drifting implementation.

`UPixelRacerRuntimeEditorSubsystem` currently exposes Blueprint-callable operations for:

- new community tracks,
- safe load/save under `Saved/PixelRacer/CommunityTracks`,
- track listing,
- 100-step undo/redo,
- road creation and manual point/width editing,
- tile painting/erasing,
- piece placement,
- procedural zone creation/regeneration,
- checkpoints/grid/AI generation,
- complete circuit generation,
- track validation.

The finished player-facing UMG/CommonUI shell is still deferred until the developer editor and PIE track-preview loop are stable.

The runtime editor deliberately does not accept executable code from track packages. Community tracks are portable data plus approved/source-art dependencies.
