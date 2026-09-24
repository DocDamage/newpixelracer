#pragma once

#include "CoreMinimal.h"
#include "PixelRacerTrackTypes.h"

enum class EPixelRacerEditorTool : uint8
{
    Select,
    Spline,
    TilePaint,
    PiecePlacement,
    ProceduralZone,
    Erase,
    Pan
};

struct FPixelRacerTrackEditorSession : public TSharedFromThis<FPixelRacerTrackEditorSession>
{
    FPixelRacerTrackDocument Document;
    EPixelRacerEditorTool Tool = EPixelRacerEditorTool::Select;

    FString ActiveSurfaceId = TEXT("asphalt");
    float ActiveRoadWidth = 160.0f;
    int32 ActiveSplineIndex = 0;

    FString ActiveTilesetId = TEXT("Tilesets/race_track_1.png");
    int32 ActiveTileIndex = 0;
    int32 ActiveLayerIndex = 10;
    float ActiveTileRotationDegrees = 0.0f;

    FString ActivePieceAssetId = TEXT("Enviroment/barrier_red.png");
    float ActivePieceRotationDegrees = 0.0f;

    FString ActiveZonePreset = TEXT("Trackside");
    int32 ActiveZoneLayerIndex = 40;

    int32 SelectedSplineIndex = INDEX_NONE;
    int32 SelectedPointIndex = INDEX_NONE;

    FVector2D ViewPan = FVector2D(80.0f, 60.0f);
    float ViewZoom = 0.22f;

    TArray<FPixelRacerTrackDocument> UndoStack;
    TArray<FPixelRacerTrackDocument> RedoStack;
    bool bEditOpen = false;
    bool bAutosaveEnabled = true;

    void BeginEdit();
    void EndEdit(bool bAutosave = true);
    bool Undo();
    bool Redo();
    void ResetHistory();
    void SaveAutosave() const;
    FString GetAutosavePath() const;
};
