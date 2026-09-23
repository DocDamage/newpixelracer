#include "PixelRacerTrackAuthoringLibrary.h"

namespace PixelRacerAuthoring
{
    static float SegmentLength(const FVector& A, const FVector& B)
    {
        return FVector::Dist2D(A, B);
    }

    static FVector PointAlongSegment(const FVector& A, const FVector& B, const float Alpha)
    {
        return FMath::Lerp(A, B, FMath::Clamp(Alpha, 0.0f, 1.0f));
    }

    static float YawAlongSegment(const FVector& A, const FVector& B)
    {
        const FVector Delta = B - A;
        return FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
    }

    static void ForEachSampledSplinePoint(
        const FPixelRacerRoadSpline& Spline,
        const float Spacing,
        TFunctionRef<void(const FVector&, float, float)> Callback)
    {
        if (Spline.ControlPoints.Num() < 2 || Spacing <= KINDA_SMALL_NUMBER)
        {
            return;
        }

        float DistanceUntilSample = 0.0f;
        const int32 SegmentCount = Spline.bClosedLoop ? Spline.ControlPoints.Num() : Spline.ControlPoints.Num() - 1;

        for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
        {
            const FPixelRacerTrackControlPoint& APoint = Spline.ControlPoints[SegmentIndex];
            const FPixelRacerTrackControlPoint& BPoint = Spline.ControlPoints[(SegmentIndex + 1) % Spline.ControlPoints.Num()];
            const FVector A = APoint.Location;
            const FVector B = BPoint.Location;
            const float Length = SegmentLength(A, B);
            if (Length <= KINDA_SMALL_NUMBER)
            {
                continue;
            }

            float LocalDistance = DistanceUntilSample;
            while (LocalDistance <= Length)
            {
                const float Alpha = LocalDistance / Length;
                const FVector Location = PointAlongSegment(A, B, Alpha);
                const float Width = FMath::Lerp(APoint.Width, BPoint.Width, Alpha);
                Callback(Location, YawAlongSegment(A, B), Width);
                LocalDistance += Spacing;
            }

            DistanceUntilSample = FMath::Max(0.0f, LocalDistance - Length);
        }
    }
}

FVector UPixelRacerTrackAuthoringLibrary::SnapLocationToGrid(const FVector& Location, const float GridSize)
{
    if (GridSize <= KINDA_SMALL_NUMBER)
    {
        return Location;
    }

    return FVector(
        FMath::GridSnap(Location.X, GridSize),
        FMath::GridSnap(Location.Y, GridSize),
        FMath::GridSnap(Location.Z, GridSize));
}

int32 UPixelRacerTrackAuthoringLibrary::AddRoadSpline(FPixelRacerTrackDocument& Document, const FString& Name, const bool bClosedLoop)
{
    FPixelRacerRoadSpline& Spline = Document.RoadSplines.AddDefaulted_GetRef();
    Spline.Id = FGuid::NewGuid();
    Spline.Name = Name.IsEmpty() ? FString::Printf(TEXT("Road %d"), Document.RoadSplines.Num()) : Name;
    Spline.bClosedLoop = bClosedLoop;
    return Document.RoadSplines.Num() - 1;
}

bool UPixelRacerTrackAuthoringLibrary::AddRoadControlPoint(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const FVector& Location,
    const float Width,
    const FString& SurfaceId,
    const bool bSnapToGrid,
    const float GridSize)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex))
    {
        return false;
    }

    FPixelRacerTrackControlPoint& Point = Document.RoadSplines[SplineIndex].ControlPoints.AddDefaulted_GetRef();
    Point.Location = bSnapToGrid ? SnapLocationToGrid(Location, GridSize) : Location;
    Point.Width = FMath::Max(1.0f, Width);
    Point.SurfaceId = SurfaceId.IsEmpty() ? TEXT("asphalt") : SurfaceId;
    return true;
}


bool UPixelRacerTrackAuthoringLibrary::MoveRoadControlPoint(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const int32 PointIndex,
    const FVector& NewLocation,
    const bool bSnapToGrid,
    const float GridSize)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex) ||
        !Document.RoadSplines[SplineIndex].ControlPoints.IsValidIndex(PointIndex))
    {
        return false;
    }

    Document.RoadSplines[SplineIndex].ControlPoints[PointIndex].Location =
        bSnapToGrid ? SnapLocationToGrid(NewLocation, GridSize) : NewLocation;
    return true;
}

bool UPixelRacerTrackAuthoringLibrary::InsertRoadControlPoint(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const int32 InsertBeforeIndex,
    const FVector& Location,
    const float Width,
    const FString& SurfaceId,
    const bool bSnapToGrid,
    const float GridSize)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex))
    {
        return false;
    }

    TArray<FPixelRacerTrackControlPoint>& Points = Document.RoadSplines[SplineIndex].ControlPoints;
    const int32 SafeIndex = FMath::Clamp(InsertBeforeIndex, 0, Points.Num());
    FPixelRacerTrackControlPoint Point;
    Point.Location = bSnapToGrid ? SnapLocationToGrid(Location, GridSize) : Location;
    Point.Width = FMath::Max(1.0f, Width);
    Point.SurfaceId = SurfaceId.IsEmpty() ? TEXT("asphalt") : SurfaceId;
    Point.bManualWidth = true;
    Points.Insert(Point, SafeIndex);
    return true;
}

bool UPixelRacerTrackAuthoringLibrary::DeleteRoadControlPoint(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const int32 PointIndex)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex) ||
        !Document.RoadSplines[SplineIndex].ControlPoints.IsValidIndex(PointIndex))
    {
        return false;
    }

    Document.RoadSplines[SplineIndex].ControlPoints.RemoveAt(PointIndex);
    return true;
}

bool UPixelRacerTrackAuthoringLibrary::SetRoadControlPointWidth(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const int32 PointIndex,
    const float NewWidth)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex) ||
        !Document.RoadSplines[SplineIndex].ControlPoints.IsValidIndex(PointIndex))
    {
        return false;
    }

    FPixelRacerTrackControlPoint& Point = Document.RoadSplines[SplineIndex].ControlPoints[PointIndex];
    Point.Width = FMath::Max(1.0f, NewWidth);
    Point.bManualWidth = true;
    return true;
}

bool UPixelRacerTrackAuthoringLibrary::SetRoadControlPointSurface(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const int32 PointIndex,
    const FString& SurfaceId)
{
    if (SurfaceId.IsEmpty() || !Document.RoadSplines.IsValidIndex(SplineIndex) ||
        !Document.RoadSplines[SplineIndex].ControlPoints.IsValidIndex(PointIndex))
    {
        return false;
    }

    Document.RoadSplines[SplineIndex].ControlPoints[PointIndex].SurfaceId = SurfaceId;
    return true;
}

bool UPixelRacerTrackAuthoringLibrary::SetRoadControlPointElevation(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const int32 PointIndex,
    const int32 ElevationLayer)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex) ||
        !Document.RoadSplines[SplineIndex].ControlPoints.IsValidIndex(PointIndex))
    {
        return false;
    }

    Document.RoadSplines[SplineIndex].ControlPoints[PointIndex].ElevationLayer = ElevationLayer;
    return true;
}

bool UPixelRacerTrackAuthoringLibrary::SetRoadControlPointCrossing(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const int32 PointIndex,
    const bool bAllowCrossing)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex) ||
        !Document.RoadSplines[SplineIndex].ControlPoints.IsValidIndex(PointIndex))
    {
        return false;
    }

    Document.RoadSplines[SplineIndex].ControlPoints[PointIndex].bAllowCrossing = bAllowCrossing;
    return true;
}

bool UPixelRacerTrackAuthoringLibrary::PaintTile(
    FPixelRacerTrackDocument& Document,
    const FIntPoint& Cell,
    const FString& AssetId,
    const int32 LayerIndex,
    const int32 RotationSteps,
    const int32 TileIndex)
{
    if (AssetId.IsEmpty() || TileIndex < 0)
    {
        return false;
    }

    for (FPixelRacerTilePlacement& Existing : Document.Tiles)
    {
        if (Existing.Cell == Cell && Existing.LayerIndex == LayerIndex)
        {
            Existing.AssetId = AssetId;
            Existing.TileIndex = TileIndex;
            Existing.RotationSteps = ((RotationSteps % 4) + 4) % 4;
            Existing.bManualOverride = true;
            return true;
        }
    }

    FPixelRacerTilePlacement& Tile = Document.Tiles.AddDefaulted_GetRef();
    Tile.Cell = Cell;
    Tile.AssetId = AssetId;
    Tile.TileIndex = TileIndex;
    Tile.LayerIndex = LayerIndex;
    Tile.RotationSteps = ((RotationSteps % 4) + 4) % 4;
    Tile.bManualOverride = true;
    return true;
}

bool UPixelRacerTrackAuthoringLibrary::EraseTile(FPixelRacerTrackDocument& Document, const FIntPoint& Cell, const int32 LayerIndex)
{
    const int32 Removed = Document.Tiles.RemoveAll([&](const FPixelRacerTilePlacement& Tile)
    {
        return Tile.Cell == Cell && Tile.LayerIndex == LayerIndex;
    });
    return Removed > 0;
}

FGuid UPixelRacerTrackAuthoringLibrary::PlacePiece(
    FPixelRacerTrackDocument& Document,
    const FString& AssetId,
    const FTransform& Transform,
    const int32 LayerIndex,
    const bool bGenerated)
{
    if (AssetId.IsEmpty())
    {
        return FGuid();
    }

    FPixelRacerPiecePlacement& Piece = Document.Pieces.AddDefaulted_GetRef();
    Piece.Id = FGuid::NewGuid();
    Piece.AssetId = AssetId;
    Piece.Transform = Transform;
    Piece.LayerIndex = LayerIndex;
    Piece.bGenerated = bGenerated;
    Piece.bManualOverride = !bGenerated;
    return Piece.Id;
}


int32 UPixelRacerTrackAuthoringLibrary::ErasePiecesInRadius(
    FPixelRacerTrackDocument& Document,
    const FVector& Location,
    const float Radius,
    const int32 LayerIndex)
{
    const float RadiusSquared = FMath::Square(FMath::Max(0.0f, Radius));
    return Document.Pieces.RemoveAll([&](const FPixelRacerPiecePlacement& Piece)
    {
        if (Piece.bLocked)
        {
            return false;
        }
        if (LayerIndex != INDEX_NONE && Piece.LayerIndex != LayerIndex)
        {
            return false;
        }
        return FVector::DistSquared2D(Piece.Transform.GetLocation(), Location) <= RadiusSquared;
    });
}

int32 UPixelRacerTrackAuthoringLibrary::BakeSplineToPieces(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const FString& PieceAssetId,
    const float Spacing,
    const int32 LayerIndex,
    const bool bReplacePreviousGenerated)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex) || PieceAssetId.IsEmpty() || Spacing <= KINDA_SMALL_NUMBER)
    {
        return 0;
    }

    if (bReplacePreviousGenerated)
    {
        Document.Pieces.RemoveAll([&](const FPixelRacerPiecePlacement& Piece)
        {
            return Piece.bGenerated && !Piece.bManualOverride && Piece.Tags.Contains(TEXT("baked_road"));
        });
    }

    int32 Count = 0;
    PixelRacerAuthoring::ForEachSampledSplinePoint(Document.RoadSplines[SplineIndex], Spacing,
        [&](const FVector& Location, const float Yaw, const float Width)
        {
            FPixelRacerPiecePlacement& Piece = Document.Pieces.AddDefaulted_GetRef();
            Piece.Id = FGuid::NewGuid();
            Piece.AssetId = PieceAssetId;
            Piece.Transform = FTransform(FRotator(0.0f, Yaw, 0.0f), Location, FVector(1.0f));
            Piece.LayerIndex = LayerIndex;
            Piece.bGenerated = true;
            Piece.Tags = { TEXT("baked_road") };
            ++Count;
        });
    return Count;
}


int32 UPixelRacerTrackAuthoringLibrary::AddProceduralZone(
    FPixelRacerTrackDocument& Document,
    const TArray<FVector2D>& Polygon,
    const FString& RulePreset,
    const int32 LayerIndex,
    const int32 Seed)
{
    if (Polygon.Num() < 3)
    {
        return INDEX_NONE;
    }

    FPixelRacerProceduralZone& Zone = Document.ProceduralZones.AddDefaulted_GetRef();
    Zone.Id = FGuid::NewGuid();
    Zone.RulePreset = RulePreset.IsEmpty() ? TEXT("Grassland") : RulePreset;
    Zone.Polygon = Polygon;
    Zone.LayerIndex = LayerIndex;
    Zone.Seed = Seed;
    return Document.ProceduralZones.Num() - 1;
}

bool UPixelRacerTrackAuthoringLibrary::UpdateProceduralZonePolygon(
    FPixelRacerTrackDocument& Document,
    const int32 ZoneIndex,
    const TArray<FVector2D>& Polygon)
{
    if (!Document.ProceduralZones.IsValidIndex(ZoneIndex) || Polygon.Num() < 3)
    {
        return false;
    }

    Document.ProceduralZones[ZoneIndex].Polygon = Polygon;
    return true;
}

bool UPixelRacerTrackAuthoringLibrary::RemoveProceduralZone(
    FPixelRacerTrackDocument& Document,
    const int32 ZoneIndex)
{
    if (!Document.ProceduralZones.IsValidIndex(ZoneIndex))
    {
        return false;
    }

    Document.ProceduralZones.RemoveAt(ZoneIndex);
    return true;
}

int32 UPixelRacerTrackAuthoringLibrary::GenerateGridSlots(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const int32 SlotCount,
    const bool bReplaceExisting)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex) || SlotCount <= 0)
    {
        return 0;
    }

    const FPixelRacerRoadSpline& Spline = Document.RoadSplines[SplineIndex];
    if (Spline.ControlPoints.Num() < 2)
    {
        return 0;
    }

    if (bReplaceExisting)
    {
        Document.GridSlots.Reset();
    }

    const FVector Start = Spline.ControlPoints[0].Location;
    const FVector Next = Spline.ControlPoints[1].Location;
    const FVector Tangent = (Next - Start).GetSafeNormal2D();
    const FVector Normal(-Tangent.Y, Tangent.X, 0.0f);
    const float Width = FMath::Max(80.0f, Spline.ControlPoints[0].Width);
    const float Yaw = PixelRacerAuthoring::YawAlongSegment(Start, Next);
    const float RowSpacing = 110.0f;
    const float SideOffset = Width * 0.22f;

    const int32 StartOrder = Document.GridSlots.Num();
    for (int32 Index = 0; Index < SlotCount; ++Index)
    {
        const int32 Row = Index / 2;
        const float Side = (Index % 2 == 0) ? -1.0f : 1.0f;
        FPixelRacerGridSlot& Slot = Document.GridSlots.AddDefaulted_GetRef();
        Slot.Order = StartOrder + Index;
        Slot.Location = Start - Tangent * (80.0f + Row * RowSpacing) + Normal * (Side * SideOffset);
        Slot.YawDegrees = Yaw;
    }
    return SlotCount;
}

int32 UPixelRacerTrackAuthoringLibrary::GenerateCheckpoints(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const float Spacing,
    const bool bReplaceGenerated)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex) || Spacing <= KINDA_SMALL_NUMBER)
    {
        return 0;
    }

    if (bReplaceGenerated)
    {
        Document.Checkpoints.RemoveAll([](const FPixelRacerCheckpoint& Checkpoint) { return Checkpoint.bGenerated; });
    }

    int32 NextOrder = 0;
    for (const FPixelRacerCheckpoint& Existing : Document.Checkpoints)
    {
        NextOrder = FMath::Max(NextOrder, Existing.Order + 1);
    }

    int32 Count = 0;
    PixelRacerAuthoring::ForEachSampledSplinePoint(Document.RoadSplines[SplineIndex], Spacing,
        [&](const FVector& Location, const float Yaw, const float Width)
        {
            FPixelRacerCheckpoint& Checkpoint = Document.Checkpoints.AddDefaulted_GetRef();
            Checkpoint.Id = FGuid::NewGuid();
            Checkpoint.Order = NextOrder++;
            Checkpoint.Location = Location;
            Checkpoint.HalfWidth = FMath::Max(1.0f, Width * 0.55f);
            Checkpoint.YawDegrees = Yaw;
            Checkpoint.bGenerated = true;
            ++Count;
        });
    return Count;
}

bool UPixelRacerTrackAuthoringLibrary::GenerateRacingLines(
    FPixelRacerTrackDocument& Document,
    const int32 SplineIndex,
    const bool bReplaceGenerated)
{
    if (!Document.RoadSplines.IsValidIndex(SplineIndex))
    {
        return false;
    }

    const FPixelRacerRoadSpline& Spline = Document.RoadSplines[SplineIndex];
    if (Spline.ControlPoints.Num() < 2)
    {
        return false;
    }

    if (bReplaceGenerated)
    {
        Document.RacingLines.RemoveAll([](const FPixelRacerRacingLine& Line) { return Line.bGenerated; });
    }

    auto BuildLine = [&](const EPixelRacerRacingLineKind Kind, const float NormalizedOffset)
    {
        FPixelRacerRacingLine& Line = Document.RacingLines.AddDefaulted_GetRef();
        Line.Kind = Kind;
        Line.bGenerated = true;
        Line.Points.Reserve(Spline.ControlPoints.Num());

        for (int32 Index = 0; Index < Spline.ControlPoints.Num(); ++Index)
        {
            const int32 PrevIndex = Index == 0 ? (Spline.bClosedLoop ? Spline.ControlPoints.Num() - 1 : 0) : Index - 1;
            const int32 NextIndex = Index == Spline.ControlPoints.Num() - 1 ? (Spline.bClosedLoop ? 0 : Index) : Index + 1;
            const FVector Prev = Spline.ControlPoints[PrevIndex].Location;
            const FVector Next = Spline.ControlPoints[NextIndex].Location;
            const FVector Tangent = (Next - Prev).GetSafeNormal2D();
            const FVector Normal(-Tangent.Y, Tangent.X, 0.0f);
            const float Offset = Spline.ControlPoints[Index].Width * 0.28f * NormalizedOffset;

            FPixelRacerRacingLinePoint& LinePoint = Line.Points.AddDefaulted_GetRef();
            LinePoint.Location = Spline.ControlPoints[Index].Location + Normal * Offset;
            LinePoint.TargetSpeedScale = FMath::Clamp(Spline.ControlPoints[Index].SpeedScale, 0.05f, 1.5f);
            LinePoint.BrakeAmount = FMath::Clamp(1.0f - LinePoint.TargetSpeedScale, 0.0f, 1.0f);
        }
    };

    BuildLine(EPixelRacerRacingLineKind::Ideal, 0.0f);
    BuildLine(EPixelRacerRacingLineKind::AlternateLeft, -1.0f);
    BuildLine(EPixelRacerRacingLineKind::AlternateRight, 1.0f);
    BuildLine(EPixelRacerRacingLineKind::Recovery, 0.0f);
    return true;
}
