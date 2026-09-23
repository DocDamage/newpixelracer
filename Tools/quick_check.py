#!/usr/bin/env python3
"""Pixel Racer lightweight project sanity check.

This intentionally does not compile Unreal or run an automation suite. It catches
bad JSON, missing starter files, missing source art, and broken project/plugin
references in a few seconds.
"""
from __future__ import annotations

import json
from pathlib import Path
import struct
import sys
from typing import Optional

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "Plugins" / "PixelRacerTools"
ERRORS: list[str] = []
WARNINGS: list[str] = []


def png_dimensions(path: Path) -> Optional[tuple[int, int]]:
    try:
        with path.open("rb") as image:
            header = image.read(24)
        if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
            return None
        return struct.unpack(">II", header[16:24])
    except OSError:
        return None


def load_json(path: Path):
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        ERRORS.append(f"{path.relative_to(ROOT)}: {exc}")
        return None


uproject = load_json(ROOT / "PixelRacer.uproject") or {}
plugin_names = {p.get("Name") for p in uproject.get("Plugins", []) if p.get("Enabled")}
for required in {"Paper2D", "EnhancedInput", "PCG", "PixelRacerTools"}:
    if required not in plugin_names:
        ERRORS.append(f"PixelRacer.uproject: required plugin not enabled: {required}")

uplugin = load_json(PLUGIN / "PixelRacerTools.uplugin") or {}
module_names = {m.get("Name") for m in uplugin.get("Modules", [])}
for required in {"PixelRacerCore", "PixelRacerTrackEditor", "PixelRacerRuntimeEditor"}:
    if required not in module_names:
        ERRORS.append(f"PixelRacerTools.uplugin: module missing: {required}")

tracks = sorted((PLUGIN / "Resources" / "StarterTracks").glob("*.pixeltrack.json"))
if len(tracks) != 4:
    ERRORS.append(f"Expected 4 starter tracks; found {len(tracks)}")
for path in tracks:
    data = load_json(path)
    if not data:
        continue
    if data.get("SchemaVersion", 0) < 1:
        ERRORS.append(f"{path.name}: invalid schema version")
    meta = data.get("Metadata", {})
    if not meta.get("TrackId") or not meta.get("DisplayName"):
        ERRORS.append(f"{path.name}: missing track metadata")
    roads = data.get("RoadSplines", [])
    if not roads:
        ERRORS.append(f"{path.name}: no road splines")
    for i, road in enumerate(roads):
        points = road.get("ControlPoints", [])
        if road.get("bClosedLoop", False) and len(points) < 3:
            ERRORS.append(f"{path.name}: closed road {i} has fewer than 3 points")
        if any(p.get("Width", 0) <= 0 for p in points):
            ERRORS.append(f"{path.name}: road {i} has invalid width")

vehicles = sorted((PLUGIN / "Resources" / "StarterVehicles").glob("*.vehicle.json"))
if len(vehicles) != 2:
    ERRORS.append(f"Expected 2 starter vehicles; found {len(vehicles)}")
for path in vehicles:
    data = load_json(path)
    if not data:
        continue
    source = ROOT / data.get("SourceSpriteSheet", "")
    if not source.exists():
        ERRORS.append(f"{path.name}: missing source sprite sheet {source}")
    if data.get("DirectionCount") != 16:
        WARNINGS.append(f"{path.name}: starter vehicle is not configured for 16 directions")

manifest_path = ROOT / "SourceArt" / "WheelsInPixels" / "PixelRacerAssetPack.json"
manifest_v2_path = ROOT / "SourceArt" / "WheelsInPixels" / "PixelRacerAssetPack_v2.json"
manifest = load_json(manifest_path) or {}
manifest_v2 = load_json(manifest_v2_path) or {}
assets = manifest.get("assets", [])
assets_v2 = manifest_v2.get("assets", [])
if manifest_v2.get("schemaVersion") != 2:
    ERRORS.append("Wheels in Pixels v2 manifest must use schemaVersion 2")
if manifest_v2.get("packId") != "wheels_in_pixels":
    ERRORS.append("Wheels in Pixels v2 manifest packId changed unexpectedly")
if len(assets) < 200:
    ERRORS.append(f"Wheels in Pixels manifest looks incomplete ({len(assets)} assets)")
if len(assets_v2) != len(assets):
    ERRORS.append(f"Wheels in Pixels v2 manifest count mismatch ({len(assets_v2)} vs {len(assets)})")
roles_v2: dict[str, int] = {}
portable_ids: set[str] = set()
for item in assets_v2:
    role = item.get("role", "sprite")
    roles_v2[role] = roles_v2.get(role, 0) + 1
    rel = item.get("path", "")
    if not rel:
        ERRORS.append("Wheels in Pixels v2 manifest contains an asset without a path")
        continue
    if rel in portable_ids:
        ERRORS.append(f"Duplicate portable asset id in Wheels manifest: {rel}")
    portable_ids.add(rel)
    asset = ROOT / "SourceArt" / "WheelsInPixels" / rel
    if not asset.exists():
        ERRORS.append(f"Wheels v2 manifest points to missing file: {rel}")
        continue

    source_size = png_dimensions(asset)
    if source_size is None:
        ERRORS.append(f"Wheels v2 manifest source is not a readable PNG: {rel}")
        continue

    width, height = item.get("width", 0), item.get("height", 0)
    if width and height and source_size != (width, height):
        ERRORS.append(f"Wheels v2 manifest dimensions do not match the PNG: {rel}")

    if role == "tileset":
        tile_width = item.get("tileWidth", 0)
        tile_height = item.get("tileHeight", 0)
        columns = item.get("columns", 0)
        rows = item.get("rows", 0)
        tile_count = item.get("tileCount", 0)
        if not all(type(value) is int and value > 0 for value in (tile_width, tile_height, columns, rows, tile_count, width, height)):
            ERRORS.append(f"Tileset metadata is incomplete: {rel}")
        elif (columns * tile_width, rows * tile_height) != (width, height) or tile_count != columns * rows:
            ERRORS.append(f"Tileset metadata does not describe an exact grid: {rel}")

    if role == "vehicle_sprite_sheet":
        directions = item.get("directionCount", 0)
        cell_width = item.get("cellWidth", 0)
        cell_height = item.get("cellHeight", 0)
        if not all(type(value) is int and value > 0 for value in (directions, cell_width, cell_height, width, height)):
            ERRORS.append(f"Vehicle frame metadata is incomplete: {rel}")
        elif directions * cell_width != width or cell_height != height:
            ERRORS.append(f"Vehicle metadata does not describe an exact single-row sheet: {rel}")

    if role == "vfx_sheet":
        frame_width = item.get("frameWidth", 0)
        frame_height = item.get("frameHeight", 0)
        frame_columns = item.get("frameColumns", 0)
        frame_rows = item.get("frameRows", 0)
        frame_count = item.get("frameCount", 0)
        if not all(type(value) is int and value > 0 for value in (
            frame_width, frame_height, frame_columns, frame_rows, frame_count, width, height
        )):
            ERRORS.append(f"VFX frame metadata is incomplete: {rel}")
        elif (frame_columns * frame_width, frame_rows * frame_height) != (width, height) or frame_count != frame_columns * frame_rows:
            ERRORS.append(f"VFX metadata does not describe an exact sprite grid: {rel}")
for required_role, minimum in {"vehicle_sprite_sheet": 1, "tileset": 1, "environment_piece": 1, "vfx_sheet": 1}.items():
    if roles_v2.get(required_role, 0) < minimum:
        ERRORS.append(f"Wheels v2 manifest is missing role: {required_role}")
for item in assets:
    asset = ROOT / "SourceArt" / "WheelsInPixels" / item.get("path", "")
    if not asset.exists():
        ERRORS.append(f"Asset manifest points to missing file: {item.get('path')}")
        if len(ERRORS) > 25:
            break

required_cpp = [
    PLUGIN / "Source/PixelRacerCore/Public/PixelRacerTrackTypes.h",
    PLUGIN / "Source/PixelRacerCore/Private/PixelRacerTrackLibrary.cpp",
    PLUGIN / "Source/PixelRacerCore/Public/PixelRacerTrackAuthoringLibrary.h",
    PLUGIN / "Source/PixelRacerCore/Private/PixelRacerTrackAuthoringLibrary.cpp",
    PLUGIN / "Source/PixelRacerCore/Private/PixelRacerArcadeVehiclePawn.cpp",
    PLUGIN / "Source/PixelRacerCore/Public/PixelRacerTrackPreviewActor.h",
    PLUGIN / "Source/PixelRacerCore/Private/PixelRacerTrackPreviewActor.cpp",
    PLUGIN / "Source/PixelRacerCore/Public/PixelRacerAuthoringLibrary.h",
    PLUGIN / "Source/PixelRacerCore/Private/PixelRacerAuthoringLibrary.cpp",
    PLUGIN / "Source/PixelRacerTrackEditor/Private/PixelRacerTrackEditorModule.cpp",
    PLUGIN / "Source/PixelRacerTrackEditor/Private/SPixelRacerTrackCanvas.h",
    PLUGIN / "Source/PixelRacerTrackEditor/Private/SPixelRacerTrackCanvas.cpp",
    PLUGIN / "Source/PixelRacerTrackEditor/Private/SPixelRacerAssetBrowser.h",
    PLUGIN / "Source/PixelRacerTrackEditor/Private/SPixelRacerAssetBrowser.cpp",
    PLUGIN / "Source/PixelRacerTrackEditor/Private/PixelRacerTrackEditorSession.cpp",
    PLUGIN / "Source/PixelRacerRuntimeEditor/Public/PixelRacerRuntimeEditorSubsystem.h",
    PLUGIN / "Source/PixelRacerRuntimeEditor/Private/PixelRacerRuntimeEditorSubsystem.cpp",
]
for path in required_cpp:
    if not path.exists() or path.stat().st_size == 0:
        ERRORS.append(f"Missing core source file: {path.relative_to(ROOT)}")


# Guard against the schema-drift identifiers that previously made the duplicate
# authoring implementation incompatible with TrackDocument schema v2.
authoring_compat = (PLUGIN / "Source/PixelRacerCore/Private/PixelRacerAuthoringLibrary.cpp").read_text(encoding="utf-8")
for stale_identifier in ("FPixelRacerTileStamp", "Document.Layers", ".Grid ==", ".TilesetId"):
    if stale_identifier in authoring_compat:
        ERRORS.append(f"Legacy authoring schema drift returned: {stale_identifier}")

# RuntimeEditor must be registered now that its source is treated as active.
if "PixelRacerRuntimeEditor" not in module_names:
    ERRORS.append("PixelRacerRuntimeEditor source exists but module is not registered")


# v0.4-dev Asset Browser guardrails.
asset_browser_cpp = (PLUGIN / "Source/PixelRacerTrackEditor/Private/SPixelRacerAssetBrowser.cpp").read_text(encoding="utf-8")
editor_module_cpp = (PLUGIN / "Source/PixelRacerTrackEditor/Private/PixelRacerTrackEditorModule.cpp").read_text(encoding="utf-8")
preview_actor_cpp = (PLUGIN / "Source/PixelRacerCore/Private/PixelRacerTrackPreviewActor.cpp").read_text(encoding="utf-8")
canvas_cpp = (PLUGIN / "Source/PixelRacerTrackEditor/Private/SPixelRacerTrackCanvas.cpp").read_text(encoding="utf-8")
track_types_h = (PLUGIN / "Source/PixelRacerCore/Public/PixelRacerTrackTypes.h").read_text(encoding="utf-8")
authoring_cpp = (PLUGIN / "Source/PixelRacerCore/Private/PixelRacerTrackAuthoringLibrary.cpp").read_text(encoding="utf-8")
editor_build = (PLUGIN / "Source/PixelRacerTrackEditor/PixelRacerTrackEditor.Build.cs").read_text(encoding="utf-8")
for expected in ("PixelRacerAssetPack_v2.json", "SourceArt/Imported", "vehicle_sprite_sheet", "environment_piece", "tileset", "vfx_sheet"):
    if expected not in asset_browser_cpp:
        ERRORS.append(f"Asset Browser source missing expected manifest behavior: {expected}")
for expected in ("OnDragOver", "OnDrop", "FPixelRacerAssetDragDropOp", "SetActiveAsset"):
    if expected not in canvas_cpp:
        ERRORS.append(f"Track canvas missing Asset Browser integration: {expected}")
for expected in ("TileIndex", "MakeRotatedBox", "MakeSourceRegionBrush", "SetAssetPreviewData"):
    if expected not in canvas_cpp:
        ERRORS.append(f"Track canvas missing Paper2D asset rendering behavior: {expected}")
if "GetPreviewData" not in asset_browser_cpp:
    ERRORS.append("Asset Browser must expose manifest-backed canvas preview data")
if "OnManifestsReloaded.ExecuteIfBound()" not in asset_browser_cpp or "SetAssetPreviewData(AssetBrowser->GetPreviewData())" not in editor_module_cpp:
    ERRORS.append("Asset Browser manifest rescans must refresh canvas preview metadata")
for expected in ("PostPIEStarted", "HandlePostPIEStarted", "BuildPreview", "SpawnPlayerVehicle", "Possess(VehiclePawn)", "SetViewTarget"):
    if expected not in editor_module_cpp:
        ERRORS.append(f"Track Editor must spawn, possess, and focus the TrackDocument preview during PIE: {expected}")
for expected in (
    "UPaperTileMapComponent", "UPaperSpriteComponent", "BuildRoads", "UpdatePreviewCamera",
    "SpawnPlayerVehicle", "DirectionalSprites", "GridSlots", "TileMapPlaneRotation",
    "GetViewportSize", "VerticalHalfFov",
):
    if expected not in preview_actor_cpp:
        ERRORS.append(f"TrackDocument PIE preview is missing runtime rendering or driving behavior: {expected}")
for expected in (
    "UDataAssetFactory",
    "CreateOrUpdateVehicleDefinition",
    "ApplyVehicleDefinitionMetadata",
    "GeneratedInventoryKey",
    "BuildExpectedGeneratedAssets",
    "PruneStaleGeneratedAssets",
    "IsExpectedGeneratedLocation",
    "GeneratedSourceKey",
    "GeneratedKindKey",
    "FindOtherDirtyPackage",
    "ObjectTools::DeleteAssets",
    "FReferencerFinder::GetAllReferencers",
    "WaitForCompletion",
    "GeneratedOwnerKey",
    "StarterDisplayName",
    "Definition.Stats.Speed",
    "VehicleDefinitionObjectPath",
    "_Direction_%02d",
):
    if expected not in asset_browser_cpp:
        ERRORS.append(f"Asset Browser must persist imported vehicle definitions and directional sprites: {expected}")
if "ForceDeleteObjects" in asset_browser_cpp or "DeleteObjectsUnchecked" in asset_browser_cpp:
    ERRORS.append("Importer cleanup must use reference-aware editor deletion rather than force deletion")
pie_guard_offset = asset_browser_cpp.find("GEditor->PlayWorld != nullptr")
delete_offset = asset_browser_cpp.find("ObjectTools::DeleteAssets")
if pie_guard_offset < 0 or delete_offset < 0 or pie_guard_offset > delete_offset:
    ERRORS.append("Generated asset cleanup must be disabled during PIE before deleting stale assets")
import_function_offset = asset_browser_cpp.find("static bool ImportPixelArtAssets")
partial_import_stop_offset = asset_browser_cpp.find("if (!bGeneratedAssets)", import_function_offset)
cleanup_finalizer_call_offset = asset_browser_cpp.find("return FinalizeGeneratedAssetImport(", import_function_offset)
if import_function_offset < 0 or partial_import_stop_offset < 0 or cleanup_finalizer_call_offset < 0 or partial_import_stop_offset > cleanup_finalizer_call_offset:
    ERRORS.append("Partial imports must stop before generated-asset cleanup and inventory updates")
for expected in ("ApplyVehicleAssetSelection", "VehicleDefinitionObjectPath", "VehicleDefinitionNeedsImport"):
    if expected not in editor_module_cpp:
        ERRORS.append(f"Track Editor must select and preflight the imported vehicle definition: {expected}")
for expected in (
    "VehicleDefinitionObjectPath.TryLoad()",
    "DirectionalSprites",
    "VehiclePawn->VehicleDefinition = VehicleDefinition",
):
    if expected not in preview_actor_cpp:
        ERRORS.append(f"PIE preview must use the persistent vehicle definition asset: {expected}")
if "virtual void BeginPlay() override;" not in (PLUGIN / "Source/PixelRacerCore/Public/PixelRacerArcadeVehiclePawn.h").read_text(encoding="utf-8"):
    ERRORS.append("Arcade vehicle must initialize its reset transform at BeginPlay")
vehicle_pawn_cpp = (PLUGIN / "Source/PixelRacerCore/Private/PixelRacerArcadeVehiclePawn.cpp").read_text(encoding="utf-8")
if "ResetTransform = GetActorTransform();" not in vehicle_pawn_cpp or "SetActorTransform(ResetTransform" not in vehicle_pawn_cpp:
    ERRORS.append("Arcade vehicle reset must return to its initial spawn transform")
if "VehicleDefinitionNeedsImport" not in editor_module_cpp or "VehicleDefinition->DirectionalSprites.Contains(nullptr)" not in editor_module_cpp:
    ERRORS.append("Play Track must report missing selected vehicle definitions or sprites before starting PIE")
if "int32 TileIndex = 0;" not in track_types_h or "Existing.TileIndex = TileIndex;" not in authoring_cpp:
    ERRORS.append("Track TilePlacement must preserve the selected TileIndex through authoring")
if '"Json"' not in editor_build:
    ERRORS.append("PixelRacerTrackEditor.Build.cs must depend on Json for the manifest-backed Asset Browser")

for expected in (
    "ResolveSourceFileUnderRoot",
    "CollapseRelativeDirectories(NormalizedRelativePath)",
    "TryGetOptionalInt32Field",
    "FTCHARToUTF8",
    "ReadPngDimensions",
    "TF_Nearest",
    "TMGS_NoMipmaps",
    "TC_EditorIcon",
    "CreateOrUpdateSprite",
    "CreateOrUpdateTileSet",
    "frameColumns",
    "frameRows",
    "frameCount",
    "Asset generation stopped after saving texture",
    "ownership inventory could not be saved",
):
    if expected not in asset_browser_cpp:
        ERRORS.append(f"Pixel-art import guardrail missing from Asset Browser: {expected}")
for required_module in ('"Paper2D"', '"Paper2DEditor"'):
    if required_module not in editor_build:
        ERRORS.append(f"PixelRacerTrackEditor.Build.cs must depend on {required_module} for Paper2D importing")

print(f"Pixel Racer quick check: {len(ERRORS)} error(s), {len(WARNINGS)} warning(s)")
for warning in WARNINGS:
    print(f"  WARNING: {warning}")
for error in ERRORS:
    print(f"  ERROR: {error}")

sys.exit(1 if ERRORS else 0)
