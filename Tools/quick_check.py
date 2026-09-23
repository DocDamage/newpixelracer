#!/usr/bin/env python3
"""Pixel Racer lightweight project sanity check.

This intentionally does not compile Unreal or run an automation suite. It catches
bad JSON, missing starter files, missing source art, and broken project/plugin
references in a few seconds.
"""
from __future__ import annotations

import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
PLUGIN = ROOT / "Plugins" / "PixelRacerTools"
ERRORS: list[str] = []
WARNINGS: list[str] = []


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
canvas_cpp = (PLUGIN / "Source/PixelRacerTrackEditor/Private/SPixelRacerTrackCanvas.cpp").read_text(encoding="utf-8")
editor_build = (PLUGIN / "Source/PixelRacerTrackEditor/PixelRacerTrackEditor.Build.cs").read_text(encoding="utf-8")
for expected in ("PixelRacerAssetPack_v2.json", "SourceArt/Imported", "vehicle_sprite_sheet", "environment_piece", "tileset", "vfx_sheet"):
    if expected not in asset_browser_cpp:
        ERRORS.append(f"Asset Browser source missing expected manifest behavior: {expected}")
for expected in ("OnDragOver", "OnDrop", "FPixelRacerAssetDragDropOp", "SetActiveAsset"):
    if expected not in canvas_cpp:
        ERRORS.append(f"Track canvas missing Asset Browser integration: {expected}")
if '"Json"' not in editor_build:
    ERRORS.append("PixelRacerTrackEditor.Build.cs must depend on Json for the manifest-backed Asset Browser")

print(f"Pixel Racer quick check: {len(ERRORS)} error(s), {len(WARNINGS)} warning(s)")
for warning in WARNINGS:
    print(f"  WARNING: {warning}")
for error in ERRORS:
    print(f"  ERROR: {error}")

sys.exit(1 if ERRORS else 0)
