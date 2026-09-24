# Unreal MCP Bridge

Pixel Racer enables the editor-only `UnrealMCPBridge` plugin in `PixelRacer.uproject`.
The plugin source and Python tools live in `Plugins/UnrealMCPBridge`.

## Source

Restored from `https://github.com/DocDamage/ue5-mcp-bridge`, commit
`f819f65d4a9ba59580a8ff259c7f663e8348a9b3`.
The upstream plugin is version 0.1.0; it is an experimental editor tool.

Local UE 5.8 compatibility changes:

- Declare the existing `EnhancedInput` and `DataValidation` dependencies in the
  plugin descriptor.
- Explicitly construct `FString` from JSON-map keys in `DataTableTools.cpp` and
  `LevelTools.cpp`, where UE 5.8 uses `FSharedString`.
- Include `Misc/ObjectThumbnail.inl` for the inline thumbnail image setter.
- Remove the unused optional `RemoteControl` dependency: its WebSocket server
  also starts on port 30020. The MCP plugin has no RemoteControl module dependency.

The UE 5.8 editor build passed. Build output is in
`Saved/Logs/MCPBridgeBuild.log`; upstream deprecated-API warnings remain.

## Connection

The existing Codex server named `unreal-mcp-bridge` runs the local wrapper at
`%USERPROFILE%\.codex\external\ue5-mcp-bridge-mcp\ue5_mcp_bridge_server.py`.
That wrapper forwards tool calls to `127.0.0.1:30020` in the running editor.
The listener binds to loopback only. No firewall opening is needed.

The original failure was `WinError 10061` (connection refused): the wrapper was
installed, but this project did not contain the editor plugin. Restarting Codex
or changing the model cannot create the missing editor listener.

## Build and check

1. Close Unreal normally, preserving any unsaved work.
2. Run `Tools\BuildEditorOnce.bat` against UE 5.8.
3. Open `PixelRacer.uproject` in UE 5.8.
4. Check `Saved\Logs\PixelRacer.log` for
   `MCP bridge listening on 127.0.0.1:30020` and a successful Python bootstrap.
5. Call `unreal_mcp_ping`, then `unreal_mcp_call` with
   `{"method":"editor.project_name"}`. Confirm `pong: true` and `PixelRacer`.
6. Call `unreal_mcp_tools_list` to verify that editor tools are registered.

Only one editor can bind the default port. If another project owns port 30020,
close that editor normally before starting Pixel Racer and verify the returned
project name before making changes through MCP.

## Verified repair (2026-09-23)

- UE 5.8.3 build succeeded after the final descriptor change; see
  `Saved/Logs/MCPBridgeBuild-final.log`.
- Restarted editor returned `pong: true` and project name `PixelRacer` through
  the existing Codex MCP tools.
- `editor.get_selection` returned successfully, exercising a C++ handler.
- Tool enumeration reported 431 C++ handlers, 18 Python tools, and
  `python_ready: true`.
- Windows reported only the loopback listener at `127.0.0.1:30020`, owned by the
  Pixel Racer editor. The optional WebSocket listener was no longer present.

These checks validate the bridge connection and dispatch, not every registered
tool or the outstanding Pixel Racer importer/PIE acceptance scenarios.
