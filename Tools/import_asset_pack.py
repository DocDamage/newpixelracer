#!/usr/bin/env python3
"""Pixel Racer source-art pack scanner/importer.

This intentionally prepares source art + portable metadata instead of creating
.uasset files. That keeps the ingestion step fast and lets Paper2D+/Unreal do the
actual sprite authoring/reimport work when available.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import shutil
import struct
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def safe_id(value: str) -> str:
    cleaned = ''.join(ch if ch.isalnum() or ch in '-_' else '_' for ch in value.strip())
    return cleaned.strip('_') or 'imported_pack'


def png_size(path: Path) -> tuple[int, int] | None:
    try:
        with path.open('rb') as f:
            header = f.read(24)
        if len(header) >= 24 and header[:8] == b'\x89PNG\r\n\x1a\n' and header[12:16] == b'IHDR':
            return struct.unpack('>II', header[16:24])
    except OSError:
        pass
    return None


def classify(path: Path, width: int, height: int) -> dict:
    lower_parts = [p.lower() for p in path.parts]
    name = path.name.lower()
    data: dict = {'role': 'sprite'}

    if 'cars' in lower_parts or 'bikes' in lower_parts:
        data['role'] = 'vehicle_sprite_sheet'
        if width % 46 == 0 and width // 46 == 16:
            data.update(directionCount=16, cellWidth=46, cellHeight=height)
        elif width % 32 == 0 and width // 32 == 16:
            data.update(directionCount=16, cellWidth=32, cellHeight=height)
        elif width % 16 == 0:
            data.update(directionCount=16, cellWidth=width // 16, cellHeight=height)
    elif 'tilesets' in lower_parts or 'tile' in name or 'track' in name:
        data['role'] = 'tileset'
        if width % 64 == 0 and height % 64 == 0:
            cols, rows = width // 64, height // 64
            data.update(tileWidth=64, tileHeight=64, columns=cols, rows=rows, tileCount=cols * rows)
    elif 'vfx' in lower_parts:
        data['role'] = 'vfx_sheet'
    elif 'environment' in lower_parts or 'enviroment' in lower_parts:
        data['role'] = 'environment_piece'
    return data


def scan(root: Path, pack_id: str, display_name: str) -> dict:
    assets = []
    for file in sorted(root.rglob('*.png')):
        size = png_size(file)
        if not size:
            continue
        w, h = size
        rel = file.relative_to(root).as_posix()
        entry = {'path': rel, 'width': w, 'height': h}
        entry.update(classify(Path(rel), w, h))
        assets.append(entry)

    return {
        'schemaVersion': 2,
        'packId': pack_id,
        'displayName': display_name,
        'sourceFormat': 'png-folder',
        'assetCount': len(assets),
        'assets': assets,
        'authoring': {
            'preferredSpriteTool': 'Paper2DPlus',
            'paper2DPlusOptional': True,
            'defaultTileSize': 64,
            'supportsRuntimeTrackReferences': True,
        },
    }


def safe_extract(zf: zipfile.ZipFile, dest: Path) -> None:
    dest = dest.resolve()
    for member in zf.infolist():
        target = (dest / member.filename).resolve()
        if target != dest and dest not in target.parents:
            raise ValueError(f'Unsafe ZIP member: {member.filename}')
    zf.extractall(dest)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument('source', type=Path, help='Folder or ZIP containing source art')
    ap.add_argument('--pack-id', default='', help='Portable pack id; defaults to source name')
    ap.add_argument('--display-name', default='', help='Display name; defaults to source name')
    ap.add_argument('--output-dir', type=Path, default=None, help='Destination source-art directory')
    ap.add_argument('--manifest-only', action='store_true', help='Scan in place without copying source art')
    args = ap.parse_args()

    source = args.source.resolve()
    if not source.exists():
        raise SystemExit(f'Source does not exist: {source}')

    pack_id = safe_id(args.pack_id or source.stem)
    display_name = args.display_name or source.stem.replace('_', ' ')
    default_dest = ROOT / 'SourceArt' / 'Imported' / pack_id
    output_dir = (args.output_dir or default_dest).resolve()

    with tempfile.TemporaryDirectory(prefix='pixelracer_pack_') as temp:
        scan_root = source
        if source.is_file():
            if source.suffix.lower() != '.zip':
                raise SystemExit('Only folders and ZIP archives are supported.')
            scan_root = Path(temp) / 'extract'
            scan_root.mkdir(parents=True)
            with zipfile.ZipFile(source) as zf:
                safe_extract(zf, scan_root)
            # Flatten one wrapper folder when the ZIP has exactly one top-level directory.
            children = [p for p in scan_root.iterdir() if p.name != '__MACOSX']
            if len(children) == 1 and children[0].is_dir():
                scan_root = children[0]

        manifest = scan(scan_root, pack_id, display_name)
        if args.manifest_only:
            output_dir.mkdir(parents=True, exist_ok=True)
        else:
            if output_dir.exists():
                shutil.rmtree(output_dir)
            shutil.copytree(scan_root, output_dir)

        manifest_path = output_dir / 'PixelRacerAssetPack_v2.json'
        manifest_path.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
        print(f'Pixel Racer asset pack: {manifest["assetCount"]} PNG asset(s)')
        print(f'Manifest: {manifest_path}')
        roles = {}
        for asset in manifest['assets']:
            roles[asset['role']] = roles.get(asset['role'], 0) + 1
        for role, count in sorted(roles.items()):
            print(f'  {role}: {count}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
