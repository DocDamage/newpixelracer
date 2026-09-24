#include "PixelRacerAuthoringLibrary.h"

#include "Math/RandomStream.h"
#include "PixelRacerTrackAuthoringLibrary.h"

void UPixelRacerAuthoringLibrary::EnsureDefaultLayers(FPixelRacerTrackDocument& Document)
{
    // Schema v2 stores semantic layer indices directly on placements/control points.
    // Keep this legacy entry point as a compatibility shim for RuntimeEditor callers.
    Document.SchemaVersion = FMath::Max(Document.SchemaVersion, 2);

    auto EnsureSurface = [&](const TCHAR* Id, float Friction, float DriftMultiplier, float SpeedMultiplier)
    {
        const FString SurfaceId(Id);
        if (!Document.SurfaceProfiles.ContainsByPredicate([&](const FPixelRacerSurfaceProfile& Existing)
        {
            return Existing.SurfaceId.Equals(SurfaceId, ESearchCase::IgnoreCase);
        }))
        {
            FPixelRacerSurfaceProfile& Surface = Document.SurfaceProfiles.AddDefaulted_GetRef();
            Surface.SurfaceId = SurfaceId;
            Surface.Friction = Friction;
            Surface.DriftMultiplier = DriftMultiplier;
            Surface.MaxSpeedMultiplier = SpeedMultiplier;
        }
    };

    EnsureSurface(TEXT("asphalt"), 1.0f, 1.0f, 1.0f);
    EnsureSurface(TEXT("dirt"), 0.74f, 1.20f, 0.86f);
    EnsureSurface(TEXT("grass"), 0.52f, 1.35f, 0.68f);
    EnsureSurface(TEXT("sand"), 0.45f, 1.45f, 0.58f);
}

int32 UPixelRacerAuthoringLibrary::AddRoadSpline(FPixelRacerTrackDocument& Document, const FString& Name, bool bClosedLoop)
{
    EnsureDefaultLayers(Document);
    return UPixelRacerTrackAuthoringLibrary::AddRoadSpline(Document, Name, bClosedLoop);
}

bool UPixelRacerAuthoringLibrary::AddRoadPoint(FPixelRacerTrackDocument& Document, int32 SplineIndex, FVector Location, float Width, const FString& SurfaceId)
{
    return UPixelRacerTrackAuthoringLibrary::AddRoadControlPoint(Document, SplineIndex, Location, Width, SurfaceId, false, 16.0f);
}

bool UPixelRacerAuthoringLibrary::MoveRoadPoint(FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 PointIndex, FVector NewLocation)
{
    return UPixelRacerTrackAuthoringLibrary::MoveRoadControlPoint(Document, SplineIndex, PointIndex, NewLocation, false, 16.0f);
}

bool UPixelRacerAuthoringLibrary::SetRoadPointWidth(FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 PointIndex, float NewWidth)
{
    return UPixelRacerTrackAuthoringLibrary::SetRoadControlPointWidth(Document, SplineIndex, PointIndex, NewWidth);
}

bool UPixelRacerAuthoringLibrary::PaintTile(FPixelRacerTrackDocument& Document, FIntPoint Grid, const FString& TilesetId, int32 TileIndex, int32 LayerIndex, float RotationDegrees)
{
    const int32 RotationSteps = FMath::RoundToInt(RotationDegrees / 90.0f);
    return UPixelRacerTrackAuthoringLibrary::PaintTile(Document, Grid, TilesetId, LayerIndex, RotationSteps, TileIndex);
}

bool UPixelRacerAuthoringLibrary::EraseTile(FPixelRacerTrackDocument& Document, FIntPoint Grid, int32 LayerIndex)
{
    return UPixelRacerTrackAuthoringLibrary::EraseTile(Document, Grid, LayerIndex);
}

FGuid UPixelRacerAuthoringLibrary::PlacePiece(FPixelRacerTrackDocument& Document, const FString& AssetId, const FTransform& Transform, int32 LayerIndex)
{
    return UPixelRacerTrackAuthoringLibrary::PlacePiece(Document, AssetId, Transform, LayerIndex, false);
}

int32 UPixelRacerAuthoringLibrary::ErasePiecesInRadius(FPixelRacerTrackDocument& Document, FVector Location, float Radius, int32 LayerIndex)
{
    return UPixelRacerTrackAuthoringLibrary::ErasePiecesInRadius(Document, Location, Radius, LayerIndex);
}

int32 UPixelRacerAuthoringLibrary::BakeRoadSplineToPieces(FPixelRacerTrackDocument& Document, int32 SplineIndex, const FString& PieceAssetId, float Spacing, int32 LayerIndex, bool bReplacePreviousGenerated)
{
    return UPixelRacerTrackAuthoringLibrary::BakeSplineToPieces(Document, SplineIndex, PieceAssetId, Spacing, LayerIndex, bReplacePreviousGenerated);
}

int32 UPixelRacerAuthoringLibrary::BakeRoadSplineToTiles(FPixelRacerTrackDocument& Document, int32 SplineIndex, const FString& TilesetId, int32 TileIndex, int32 LayerIndex, bool bReplacePreviousGenerated)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex) || TilesetId.IsEmpty() || TileIndex < 0)
    {
        return 0;
    }

    if (bReplacePreviousGenerated)
    {
        Document.Tiles.RemoveAll([&](const FPixelRacerTilePlacement& Tile)
        {
            return Tile.bGenerated && !Tile.bManualOverride && Tile.LayerIndex == LayerIndex;
        });
    }

    const FPixelRacerRoadSpline& Spline = Document.RoadSplines[SplineIndex];
    TSet<FIntPoint> AddedCells;
    int32 Count = 0;
    for (int32 PointIndex = 0; PointIndex < Spline.ControlPoints.Num(); ++PointIndex)
    {
        const FVector& Location = Spline.ControlPoints[PointIndex].Location;
        const FIntPoint Cell(FMath::RoundToInt(Location.X / 64.0f), FMath::RoundToInt(Location.Y / 64.0f));
        if (AddedCells.Contains(Cell))
        {
            continue;
        }
        AddedCells.Add(Cell);

        FPixelRacerTilePlacement& Tile = Document.Tiles.AddDefaulted_GetRef();
        Tile.Cell = Cell;
        Tile.AssetId = TilesetId;
        Tile.TileIndex = TileIndex;
        Tile.LayerIndex = LayerIndex;
        Tile.bGenerated = true;
        Tile.bManualOverride = false;
        ++Count;
    }
    return Count;
}

int32 UPixelRacerAuthoringLibrary::GenerateCheckpoints(FPixelRacerTrackDocument& Document, int32 SplineIndex, float Spacing)
{
    return UPixelRacerTrackAuthoringLibrary::GenerateCheckpoints(Document, SplineIndex, Spacing, true);
}

int32 UPixelRacerAuthoringLibrary::GenerateGridSlots(FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 SlotCount)
{
    return UPixelRacerTrackAuthoringLibrary::GenerateGridSlots(Document, SplineIndex, SlotCount, true);
}

int32 UPixelRacerAuthoringLibrary::GenerateRacingLines(FPixelRacerTrackDocument& Document, int32 SplineIndex)
{
    return UPixelRacerTrackAuthoringLibrary::GenerateRacingLines(Document, SplineIndex, true) ? 4 : 0;
}

int32 UPixelRacerAuthoringLibrary::RegenerateProceduralZone(FPixelRacerTrackDocument& Document, int32 ZoneIndex, const TArray<FString>& AssetIds)
{
    if (!Document.ProceduralZones.IsValidIndex(ZoneIndex) || AssetIds.IsEmpty())
    {
        return 0;
    }

    TArray<FString> ValidAssetIds;
    for (const FString& AssetId : AssetIds)
    {
        if (!AssetId.TrimStartAndEnd().IsEmpty())
        {
            ValidAssetIds.Add(AssetId);
        }
    }
    if (ValidAssetIds.IsEmpty())
    {
        return 0;
    }

    FPixelRacerProceduralZone& Zone = Document.ProceduralZones[ZoneIndex];
    const FString PreviousAssetId = Zone.AssetId;
    const bool bPreviousGenerateTiles = Zone.bGenerateTiles;
    Zone.AssetId = ValidAssetIds[0];
    Zone.bGenerateTiles = false;

    int32 Count = 0;
    FString Error;
    if (!UPixelRacerTrackAuthoringLibrary::GenerateProceduralZone(Document, ZoneIndex, Count, Error))
    {
        Zone.AssetId = PreviousAssetId;
        Zone.bGenerateTiles = bPreviousGenerateTiles;
        return 0;
    }

    if (ValidAssetIds.Num() > 1)
    {
        for (FPixelRacerPiecePlacement& Piece : Document.Pieces)
        {
            if (Piece.SourceZoneId != Zone.Id || !Piece.bGenerated || Piece.bManualOverride || Piece.bLocked)
            {
                continue;
            }
            const FString SlotKey = FString::Printf(
                TEXT("%s|%d|%d|%d|palette"),
                *Zone.Id.ToString(EGuidFormats::Digits),
                Zone.Seed,
                Piece.SourceZoneCell.X,
                Piece.SourceZoneCell.Y);
            Piece.AssetId = ValidAssetIds[FCrc::StrCrc32(*SlotKey) % ValidAssetIds.Num()];
        }
    }
    return Count;
}

bool UPixelRacerAuthoringLibrary::GenerateCompleteCircuit(FPixelRacerTrackDocument& Document, FVector2D Center, float Radius, int32 ControlPointCount, int32 Seed, float RoadWidth, const FString& SurfaceId)
{
    if (Radius < 100.0f || ControlPointCount < 3)
    {
        return false;
    }

    EnsureDefaultLayers(Document);
    Document.RoadSplines.Reset();
    Document.Checkpoints.Reset();
    Document.GridSlots.Reset();
    Document.RacingLines.Reset();

    const int32 SplineIndex = UPixelRacerTrackAuthoringLibrary::AddRoadSpline(Document, TEXT("Generated Main Road"), true);
    FRandomStream Random(Seed);
    for (int32 Index = 0; Index < ControlPointCount; ++Index)
    {
        const float Angle = (2.0f * PI * static_cast<float>(Index)) / static_cast<float>(ControlPointCount);
        const float RadiusScale = Random.FRandRange(0.82f, 1.18f);
        const FVector Location(
            Center.X + FMath::Cos(Angle) * Radius * RadiusScale,
            Center.Y + FMath::Sin(Angle) * Radius * RadiusScale,
            0.0f);
        if (!UPixelRacerTrackAuthoringLibrary::AddRoadControlPoint(Document, SplineIndex, Location, RoadWidth, SurfaceId, false, 16.0f))
        {
            return false;
        }
    }

    UPixelRacerTrackAuthoringLibrary::GenerateCheckpoints(Document, SplineIndex, FMath::Max(250.0f, Radius * 0.28f), true);
    UPixelRacerTrackAuthoringLibrary::GenerateGridSlots(Document, SplineIndex, 8, true);
    UPixelRacerTrackAuthoringLibrary::GenerateRacingLines(Document, SplineIndex, true);
    Document.Generation.Seed = Seed;
    return true;
}
