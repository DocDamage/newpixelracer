#include "PixelRacerAuthoringLibrary.h"

#include "Math/RandomStream.h"
#include "PixelRacerTrackAuthoringLibrary.h"

namespace PixelRacerLegacyAuthoring
{
    static bool PointInsidePolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon)
    {
        if (Polygon.Num() < 3)
        {
            return false;
        }

        bool bInside = false;
        int32 J = Polygon.Num() - 1;
        for (int32 I = 0; I < Polygon.Num(); ++I)
        {
            const FVector2D& Pi = Polygon[I];
            const FVector2D& Pj = Polygon[J];
            const bool bCrosses = ((Pi.Y > Point.Y) != (Pj.Y > Point.Y)) &&
                (Point.X < (Pj.X - Pi.X) * (Point.Y - Pi.Y) / (Pj.Y - Pi.Y) + Pi.X);
            if (bCrosses)
            {
                bInside = !bInside;
            }
            J = I;
        }
        return bInside;
    }

    static FString ZoneTag(const FGuid& ZoneId)
    {
        return FString::Printf(TEXT("procedural_zone:%s"), *ZoneId.ToString(EGuidFormats::Digits));
    }
}

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
    (void)TileIndex; // Schema v2 addresses the source sheet through AssetId; per-cell slicing is handled by the import adapter.
    const int32 RotationSteps = FMath::RoundToInt(RotationDegrees / 90.0f);
    return UPixelRacerTrackAuthoringLibrary::PaintTile(Document, Grid, TilesetId, LayerIndex, RotationSteps);
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
    if (!Document.RoadSplines.IsValidIndex(SplineIndex) || TilesetId.IsEmpty())
    {
        return 0;
    }

    (void)TileIndex;
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
    if (!Document.ProceduralZones.IsValidIndex(ZoneIndex))
    {
        return 0;
    }

    FPixelRacerProceduralZone& Zone = Document.ProceduralZones[ZoneIndex];
    if (!Zone.Id.IsValid())
    {
        Zone.Id = FGuid::NewGuid();
    }

    const FString Tag = PixelRacerLegacyAuthoring::ZoneTag(Zone.Id);
    Document.Pieces.RemoveAll([&](const FPixelRacerPiecePlacement& Piece)
    {
        return Piece.bGenerated && !Piece.bManualOverride && Piece.Tags.Contains(Tag);
    });

    if (AssetIds.IsEmpty() || Zone.Polygon.Num() < 3)
    {
        return 0;
    }

    FVector2D Min(FLT_MAX, FLT_MAX);
    FVector2D Max(-FLT_MAX, -FLT_MAX);
    for (const FVector2D& Point : Zone.Polygon)
    {
        Min.X = FMath::Min(Min.X, Point.X);
        Min.Y = FMath::Min(Min.Y, Point.Y);
        Max.X = FMath::Max(Max.X, Point.X);
        Max.Y = FMath::Max(Max.Y, Point.Y);
    }

    FRandomStream Random(Zone.Seed);
    const float Area = FMath::Max(0.0f, (Max.X - Min.X) * (Max.Y - Min.Y));
    const int32 AttemptCount = FMath::Clamp(FMath::RoundToInt(Area / 18000.0f), 4, 256);
    int32 Count = 0;
    for (int32 Attempt = 0; Attempt < AttemptCount; ++Attempt)
    {
        const FVector2D Candidate(Random.FRandRange(Min.X, Max.X), Random.FRandRange(Min.Y, Max.Y));
        if (!PixelRacerLegacyAuthoring::PointInsidePolygon(Candidate, Zone.Polygon))
        {
            continue;
        }

        const FString& AssetId = AssetIds[Random.RandRange(0, AssetIds.Num() - 1)];
        FPixelRacerPiecePlacement& Piece = Document.Pieces.AddDefaulted_GetRef();
        Piece.Id = FGuid::NewGuid();
        Piece.AssetId = AssetId;
        Piece.Transform = FTransform(FRotator(0.0f, Random.FRandRange(0.0f, 360.0f), 0.0f), FVector(Candidate, 0.0f));
        Piece.LayerIndex = Zone.LayerIndex;
        Piece.bGenerated = true;
        Piece.bManualOverride = false;
        Piece.Tags = { TEXT("procedural"), Tag, Zone.RulePreset };
        ++Count;
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
