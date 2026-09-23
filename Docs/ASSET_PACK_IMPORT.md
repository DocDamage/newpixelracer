# Asset pack ingestion

`Tools/import_asset_pack.py` accepts a folder or ZIP of PNG source art. `Tools/ScanAssetPack.bat` is the Windows drag/drop entry point.

The scanner:

- safely rejects ZIP path traversal,
- preserves source art separately from generated Unreal assets,
- records PNG dimensions,
- classifies vehicle sprite sheets, tilesets, environment pieces and VFX sheets,
- detects common 16-direction vehicle layouts,
- detects 64×64 tile grids,
- writes `PixelRacerAssetPack_v2.json` portable metadata.

This step does not compile Unreal. Paper2D+ is the preferred optional tool for converting/slicing that source art into Unreal-authored sprite assets; Pixel Racer remains loadable without it.

The existing Wheels in Pixels source pack scans to 224 PNG assets: 195 vehicle sheets, 20 tilesets, 7 environment pieces and 2 VFX sheets.

## v0.4-dev editor discovery

The Track Editor Asset Browser now reads these v2 manifests directly.

- Built-in pack: `SourceArt/WheelsInPixels/PixelRacerAssetPack_v2.json`
- Imported packs: `SourceArt/Imported/**/PixelRacerAssetPack_v2.json`

After running `Tools/ScanAssetPack.bat` / `Tools/import_asset_pack.py`, press **Rescan** in the Asset Browser; restarting Unreal should not be necessary.

For backwards compatibility, Wheels in Pixels continues to use the relative PNG path as its TrackDocument `AssetId`. Imported packs are scoped as `packId:relative/path.png` so two packs can contain the same filename without colliding.
