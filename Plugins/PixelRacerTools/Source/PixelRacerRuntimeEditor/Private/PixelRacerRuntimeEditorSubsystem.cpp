#include "PixelRacerRuntimeEditorSubsystem.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "PixelRacerAuthoringLibrary.h"
#include "PixelRacerTrackLibrary.h"

void UPixelRacerRuntimeEditorSubsystem::NewTrack(const FString& TrackId, const FString& DisplayName)
{
    WorkingDocument = FPixelRacerTrackDocument();
    WorkingDocument.Metadata.TrackId = TrackId.IsEmpty() ? TEXT("community_track") : TrackId;
    WorkingDocument.Metadata.DisplayName = DisplayName.IsEmpty() ? TEXT("Community Track") : DisplayName;
    WorkingDocument.Metadata.Author = TEXT("Player");
    UPixelRacerAuthoringLibrary::EnsureDefaultLayers(WorkingDocument);
    UndoStack.Reset();
    RedoStack.Reset();
    bDirty = true;
}

FString UPixelRacerRuntimeEditorSubsystem::ResolveUserTrackPath(const FString& FileName) const
{
    FString SafeName = FPaths::GetBaseFilename(FPaths::GetCleanFilename(FileName));
    if (SafeName.IsEmpty())
    {
        SafeName = WorkingDocument.Metadata.TrackId.IsEmpty() ? TEXT("community_track") : WorkingDocument.Metadata.TrackId;
    }
    SafeName.ReplaceInline(TEXT(".."), TEXT("_"));
    SafeName.ReplaceInline(TEXT("/"), TEXT("_"));
    SafeName.ReplaceInline(TEXT("\\"), TEXT("_"));
    SafeName.ReplaceInline(TEXT(":"), TEXT("_"));
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PixelRacer/CommunityTracks"), SafeName + TEXT(".pixeltrack.json"));
}

bool UPixelRacerRuntimeEditorSubsystem::LoadUserTrack(const FString& FileName, FString& OutError)
{
    FPixelRacerTrackDocument Loaded;
    if (!UPixelRacerTrackLibrary::ImportTrackDocument(ResolveUserTrackPath(FileName), Loaded, OutError))
    {
        return false;
    }

    WorkingDocument = MoveTemp(Loaded);
    UndoStack.Reset();
    RedoStack.Reset();
    bDirty = false;
    return true;
}

bool UPixelRacerRuntimeEditorSubsystem::SaveUserTrack(const FString& FileName, FString& OutSavedPath, FString& OutError)
{
    OutSavedPath = ResolveUserTrackPath(FileName);
    if (!UPixelRacerTrackLibrary::ExportTrackDocument(WorkingDocument, OutSavedPath, OutError))
    {
        return false;
    }
    bDirty = false;
    return true;
}

TArray<FString> UPixelRacerRuntimeEditorSubsystem::ListUserTracks() const
{
    TArray<FString> Files;
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PixelRacer/CommunityTracks"));
    IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, TEXT("*.pixeltrack.json")), true, false);
    Files.Sort();
    return Files;
}

void UPixelRacerRuntimeEditorSubsystem::PushUndoSnapshot()
{
    UndoStack.Add(WorkingDocument);
    if (UndoStack.Num() > 100)
    {
        UndoStack.RemoveAt(0);
    }
    RedoStack.Reset();
}

void UPixelRacerRuntimeEditorSubsystem::MarkEdited()
{
    bDirty = true;
}

bool UPixelRacerRuntimeEditorSubsystem::Undo()
{
    if (UndoStack.IsEmpty())
    {
        return false;
    }
    RedoStack.Add(WorkingDocument);
    WorkingDocument = UndoStack.Pop(EAllowShrinking::No);
    MarkEdited();
    return true;
}

bool UPixelRacerRuntimeEditorSubsystem::Redo()
{
    if (RedoStack.IsEmpty())
    {
        return false;
    }
    UndoStack.Add(WorkingDocument);
    WorkingDocument = RedoStack.Pop(EAllowShrinking::No);
    MarkEdited();
    return true;
}

int32 UPixelRacerRuntimeEditorSubsystem::AddRoadSpline(const FString& Name, bool bClosedLoop)
{
    PushUndoSnapshot();
    const int32 Result = UPixelRacerAuthoringLibrary::AddRoadSpline(WorkingDocument, Name, bClosedLoop);
    MarkEdited();
    return Result;
}

bool UPixelRacerRuntimeEditorSubsystem::AddRoadPoint(int32 SplineIndex, FVector Location, float Width, const FString& SurfaceId)
{
    PushUndoSnapshot();
    const bool bResult = UPixelRacerAuthoringLibrary::AddRoadPoint(WorkingDocument, SplineIndex, Location, Width, SurfaceId);
    if (!bResult) UndoStack.Pop(EAllowShrinking::No); else MarkEdited();
    return bResult;
}

bool UPixelRacerRuntimeEditorSubsystem::MoveRoadPoint(int32 SplineIndex, int32 PointIndex, FVector Location)
{
    PushUndoSnapshot();
    const bool bResult = UPixelRacerAuthoringLibrary::MoveRoadPoint(WorkingDocument, SplineIndex, PointIndex, Location);
    if (!bResult) UndoStack.Pop(EAllowShrinking::No); else MarkEdited();
    return bResult;
}

bool UPixelRacerRuntimeEditorSubsystem::SetRoadPointWidth(int32 SplineIndex, int32 PointIndex, float Width)
{
    PushUndoSnapshot();
    const bool bResult = UPixelRacerAuthoringLibrary::SetRoadPointWidth(WorkingDocument, SplineIndex, PointIndex, Width);
    if (!bResult) UndoStack.Pop(EAllowShrinking::No); else MarkEdited();
    return bResult;
}

bool UPixelRacerRuntimeEditorSubsystem::PaintTile(FIntPoint Grid, const FString& TilesetId, int32 TileIndex, int32 LayerIndex, float RotationDegrees)
{
    PushUndoSnapshot();
    const bool bResult = UPixelRacerAuthoringLibrary::PaintTile(WorkingDocument, Grid, TilesetId, TileIndex, LayerIndex, RotationDegrees);
    if (!bResult) UndoStack.Pop(EAllowShrinking::No); else MarkEdited();
    return bResult;
}

bool UPixelRacerRuntimeEditorSubsystem::EraseTile(FIntPoint Grid, int32 LayerIndex)
{
    PushUndoSnapshot();
    const bool bResult = UPixelRacerAuthoringLibrary::EraseTile(WorkingDocument, Grid, LayerIndex);
    if (!bResult) UndoStack.Pop(EAllowShrinking::No); else MarkEdited();
    return bResult;
}

FGuid UPixelRacerRuntimeEditorSubsystem::PlacePiece(const FString& AssetId, FTransform Transform, int32 LayerIndex)
{
    PushUndoSnapshot();
    const FGuid Id = UPixelRacerAuthoringLibrary::PlacePiece(WorkingDocument, AssetId, Transform, LayerIndex);
    MarkEdited();
    return Id;
}

int32 UPixelRacerRuntimeEditorSubsystem::AddRectangularZone(FVector2D A, FVector2D B, const FString& RulePreset, int32 LayerIndex, int32 Seed)
{
    PushUndoSnapshot();
    FPixelRacerProceduralZone& Zone = WorkingDocument.ProceduralZones.AddDefaulted_GetRef();
    Zone.Id = FGuid::NewGuid();
    Zone.RulePreset = RulePreset.IsEmpty() ? TEXT("Trackside") : RulePreset;
    Zone.LayerIndex = LayerIndex;
    Zone.Seed = Seed;
    Zone.Polygon = { A, FVector2D(B.X, A.Y), B, FVector2D(A.X, B.Y) };
    MarkEdited();
    return WorkingDocument.ProceduralZones.Num() - 1;
}

int32 UPixelRacerRuntimeEditorSubsystem::RegenerateZone(int32 ZoneIndex, const TArray<FString>& AssetIds)
{
    PushUndoSnapshot();
    const int32 Count = UPixelRacerAuthoringLibrary::RegenerateProceduralZone(WorkingDocument, ZoneIndex, AssetIds);
    if (Count == 0 && !WorkingDocument.ProceduralZones.IsValidIndex(ZoneIndex))
    {
        UndoStack.Pop(EAllowShrinking::No);
        return 0;
    }
    MarkEdited();
    return Count;
}

bool UPixelRacerRuntimeEditorSubsystem::GenerateRaceData(int32 SplineIndex)
{
    if (!WorkingDocument.RoadSplines.IsValidIndex(SplineIndex))
    {
        return false;
    }

    PushUndoSnapshot();
    UPixelRacerAuthoringLibrary::GenerateCheckpoints(WorkingDocument, SplineIndex, 550.0f);
    UPixelRacerAuthoringLibrary::GenerateGridSlots(WorkingDocument, SplineIndex, 8);
    UPixelRacerAuthoringLibrary::GenerateRacingLines(WorkingDocument, SplineIndex);
    MarkEdited();
    return true;
}

bool UPixelRacerRuntimeEditorSubsystem::GenerateCompleteCircuit(FVector2D Center, float Radius, int32 ControlPointCount, int32 Seed, float RoadWidth, const FString& SurfaceId)
{
    PushUndoSnapshot();
    const bool bResult = UPixelRacerAuthoringLibrary::GenerateCompleteCircuit(WorkingDocument, Center, Radius, ControlPointCount, Seed, RoadWidth, SurfaceId);
    if (!bResult) UndoStack.Pop(EAllowShrinking::No); else MarkEdited();
    return bResult;
}

bool UPixelRacerRuntimeEditorSubsystem::ValidateWorkingTrack(TArray<FPixelRacerValidationMessage>& OutMessages) const
{
    return UPixelRacerTrackLibrary::ValidateTrackDocument(WorkingDocument, OutMessages);
}
