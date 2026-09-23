# Pixel Racer development workflow

The project is intentionally optimized for **editor iteration**, not constant release builds.

## Normal track/content loop

1. Open Unreal with `Tools/OpenEditor.bat`.
2. Open **Window → Pixel Racer Track Editor**.
3. Edit track data/content.
4. Use the editor's lightweight validation.
5. Press **Play Track (PIE)**.
6. Exit PIE and continue editing.

No cook, package or full automation suite is part of this loop.

## When a C++ compile is actually needed

Compile when the underlying PixelRacerTools C++ source changes. `Tools/BuildEditorOnce.bat` is supplied for the first compile or a clean rebuild. Live Coding is enabled by default for later C++ iteration.

## Lightweight sanity check

`Tools/QuickCheck.bat` checks project descriptors, starter `.pixeltrack` JSON, starter vehicle references and the source-art manifest. It does **not** launch Unreal or compile anything.

## Release gate

Packaging, broad automation, multiplayer regression and cooked-build validation belong at release checkpoints. They should not block ordinary track design.
