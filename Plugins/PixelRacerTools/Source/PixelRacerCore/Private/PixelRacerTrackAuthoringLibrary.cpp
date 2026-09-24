#include "PixelRacerTrackAuthoringLibrary.h"

namespace PixelRacerAuthoring
{
    constexpr float TileGridSize = 32.0f;
    constexpr int32 MaxZonePolygonVertices = 1024;
    constexpr int64 MaxZoneCandidates = 65536;
    constexpr int64 MaxPolygonPointChecks = 10000000;
    constexpr int64 MaxRoadDistanceChecks = 10000000;

    struct FZoneGridBounds
    {
        int32 MinX = 0;
        int32 MinY = 0;
        int32 MaxX = -1;
        int32 MaxY = -1;
        int64 CandidateCount = 0;
    };

    struct FRoadSegment
    {
        FVector2D Start = FVector2D::ZeroVector;
        FVector2D End = FVector2D::ZeroVector;
        float StartHalfWidth = 0.0f;
        float EndHalfWidth = 0.0f;
    };

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

    static double Cross(const FVector2D& A, const FVector2D& B, const FVector2D& C)
    {
        return static_cast<double>(B.X - A.X) * static_cast<double>(C.Y - A.Y) -
            static_cast<double>(B.Y - A.Y) * static_cast<double>(C.X - A.X);
    }

    static bool PointOnSegment(const FVector2D& Point, const FVector2D& Start, const FVector2D& End)
    {
        constexpr double Epsilon = 0.0001;
        if (FMath::Abs(Cross(Start, End, Point)) > Epsilon)
        {
            return false;
        }

        return Point.X >= FMath::Min(Start.X, End.X) - Epsilon &&
            Point.X <= FMath::Max(Start.X, End.X) + Epsilon &&
            Point.Y >= FMath::Min(Start.Y, End.Y) - Epsilon &&
            Point.Y <= FMath::Max(Start.Y, End.Y) + Epsilon;
    }

    static bool SegmentsIntersectOrTouch(
        const FVector2D& A0,
        const FVector2D& A1,
        const FVector2D& B0,
        const FVector2D& B1)
    {
        constexpr double Epsilon = 0.0001;
        const double AB0 = Cross(A0, A1, B0);
        const double AB1 = Cross(A0, A1, B1);
        const double BA0 = Cross(B0, B1, A0);
        const double BA1 = Cross(B0, B1, A1);
        const bool bProperIntersection =
            ((AB0 > Epsilon && AB1 < -Epsilon) || (AB0 < -Epsilon && AB1 > Epsilon)) &&
            ((BA0 > Epsilon && BA1 < -Epsilon) || (BA0 < -Epsilon && BA1 > Epsilon));
        if (bProperIntersection)
        {
            return true;
        }

        return (FMath::Abs(AB0) <= Epsilon && PointOnSegment(B0, A0, A1)) ||
            (FMath::Abs(AB1) <= Epsilon && PointOnSegment(B1, A0, A1)) ||
            (FMath::Abs(BA0) <= Epsilon && PointOnSegment(A0, B0, B1)) ||
            (FMath::Abs(BA1) <= Epsilon && PointOnSegment(A1, B0, B1));
    }

    static bool ValidatePolygon(
        const TArray<FVector2D>& Polygon,
        FVector2D& OutMin,
        FVector2D& OutMax,
        FString& OutError)
    {
        if (Polygon.Num() < 3)
        {
            OutError = TEXT("Procedural zone polygon needs at least three points.");
            return false;
        }
        if (Polygon.Num() > MaxZonePolygonVertices)
        {
            OutError = FString::Printf(
                TEXT("Procedural zone polygon exceeds the %d-vertex limit."),
                MaxZonePolygonVertices);
            return false;
        }

        OutMin = FVector2D(FLT_MAX, FLT_MAX);
        OutMax = FVector2D(-FLT_MAX, -FLT_MAX);
        double TwiceArea = 0.0;
        for (int32 Index = 0; Index < Polygon.Num(); ++Index)
        {
            const FVector2D& Point = Polygon[Index];
            const FVector2D& Next = Polygon[(Index + 1) % Polygon.Num()];
            if (!FMath::IsFinite(Point.X) || !FMath::IsFinite(Point.Y))
            {
                OutError = TEXT("Procedural zone polygon contains a non-finite point.");
                return false;
            }
            if (FVector2D::DistSquared(Point, Next) <= KINDA_SMALL_NUMBER)
            {
                OutError = TEXT("Procedural zone polygon contains a degenerate edge.");
                return false;
            }

            OutMin.X = FMath::Min(OutMin.X, Point.X);
            OutMin.Y = FMath::Min(OutMin.Y, Point.Y);
            OutMax.X = FMath::Max(OutMax.X, Point.X);
            OutMax.Y = FMath::Max(OutMax.Y, Point.Y);
            TwiceArea += static_cast<double>(Point.X) * static_cast<double>(Next.Y) -
                static_cast<double>(Next.X) * static_cast<double>(Point.Y);
        }

        if (FMath::Abs(TwiceArea) <= KINDA_SMALL_NUMBER)
        {
            OutError = TEXT("Procedural zone polygon has no usable area.");
            return false;
        }

        for (int32 A = 0; A < Polygon.Num(); ++A)
        {
            const int32 ANext = (A + 1) % Polygon.Num();
            for (int32 B = A + 1; B < Polygon.Num(); ++B)
            {
                const int32 BNext = (B + 1) % Polygon.Num();
                if (A == B || ANext == B || BNext == A)
                {
                    continue;
                }
                if (SegmentsIntersectOrTouch(Polygon[A], Polygon[ANext], Polygon[B], Polygon[BNext]))
                {
                    OutError = TEXT("Procedural zone polygon intersects itself.");
                    return false;
                }
            }
        }
        return true;
    }

    static bool PointInsidePolygon(const FVector2D& Point, const TArray<FVector2D>& Polygon)
    {
        bool bInside = false;
        for (int32 Index = 0, Previous = Polygon.Num() - 1; Index < Polygon.Num(); Previous = Index++)
        {
            const FVector2D& A = Polygon[Previous];
            const FVector2D& B = Polygon[Index];
            if (PointOnSegment(Point, A, B))
            {
                return true;
            }

            const bool bCrossesRay = (A.Y > Point.Y) != (B.Y > Point.Y);
            if (bCrossesRay)
            {
                const double IntersectionX = static_cast<double>(B.X - A.X) *
                    static_cast<double>(Point.Y - A.Y) / static_cast<double>(B.Y - A.Y) + A.X;
                if (static_cast<double>(Point.X) < IntersectionX)
                {
                    bInside = !bInside;
                }
            }
        }
        return bInside;
    }

    static bool BuildGridBounds(
        const FVector2D& Min,
        const FVector2D& Max,
        const float Spacing,
        FZoneGridBounds& OutBounds,
        FString& OutError)
    {
        const double HalfSpacing = static_cast<double>(Spacing) * 0.5;
        const double MinX = FMath::CeilToDouble((static_cast<double>(Min.X) - HalfSpacing) / Spacing);
        const double MinY = FMath::CeilToDouble((static_cast<double>(Min.Y) - HalfSpacing) / Spacing);
        const double MaxX = FMath::FloorToDouble((static_cast<double>(Max.X) - HalfSpacing) / Spacing);
        const double MaxY = FMath::FloorToDouble((static_cast<double>(Max.Y) - HalfSpacing) / Spacing);
        const auto IsGridCoordinateSupported = [](const double Coordinate)
        {
            return Coordinate >= MIN_int32 && Coordinate <= MAX_int32;
        };
        if (!IsGridCoordinateSupported(MinX) || !IsGridCoordinateSupported(MinY) ||
            !IsGridCoordinateSupported(MaxX) || !IsGridCoordinateSupported(MaxY))
        {
            OutError = TEXT("Procedural zone bounds exceed the supported grid range.");
            return false;
        }

        OutBounds.MinX = static_cast<int32>(MinX);
        OutBounds.MinY = static_cast<int32>(MinY);
        OutBounds.MaxX = static_cast<int32>(MaxX);
        OutBounds.MaxY = static_cast<int32>(MaxY);
        if (OutBounds.MaxX < OutBounds.MinX || OutBounds.MaxY < OutBounds.MinY)
        {
            OutBounds.CandidateCount = 0;
            return true;
        }

        const int64 Width = static_cast<int64>(OutBounds.MaxX) - OutBounds.MinX + 1;
        const int64 Height = static_cast<int64>(OutBounds.MaxY) - OutBounds.MinY + 1;
        if (Width > MaxZoneCandidates || Height > MaxZoneCandidates || Width > MaxZoneCandidates / Height)
        {
            OutError = FString::Printf(
                TEXT("Procedural zone exceeds the %lld-candidate generation budget."),
                MaxZoneCandidates);
            return false;
        }
        OutBounds.CandidateCount = Width * Height;
        return true;
    }

    static bool ValidatePointInPolygonBudget(
        const FZoneGridBounds& Bounds,
        const int32 PolygonVertexCount,
        FString& OutError)
    {
        if (Bounds.CandidateCount > 0 && PolygonVertexCount > 0 &&
            Bounds.CandidateCount > MaxPolygonPointChecks / PolygonVertexCount)
        {
            OutError = TEXT("Procedural zone exceeds the point-in-polygon work budget.");
            return false;
        }
        return true;
    }

    static FString ZoneTag(const FGuid& ZoneId)
    {
        return FString::Printf(TEXT("procedural_zone:%s"), *ZoneId.ToString(EGuidFormats::Digits));
    }

    static uint32 HashZoneCell(const FGuid& ZoneId, const int32 Seed, const FIntPoint& Cell, const uint32 Salt)
    {
        const FString Key = FString::Printf(
            TEXT("%s|%d|%d|%d|%u"),
            *ZoneId.ToString(EGuidFormats::Digits),
            Seed,
            Cell.X,
            Cell.Y,
            Salt);
        return FCrc::StrCrc32(*Key);
    }

    static FGuid MakeStablePlacementId(const FGuid& ZoneId, const int32 Seed, const FIntPoint& Cell)
    {
        return FGuid(
            HashZoneCell(ZoneId, Seed, Cell, 0x13579bdfu),
            HashZoneCell(ZoneId, Seed, Cell, 0x2468ace0u),
            HashZoneCell(ZoneId, Seed, Cell, 0x9e3779b9u),
            HashZoneCell(ZoneId, Seed, Cell, 0x7f4a7c15u));
    }

    static FGuid MakeStableZoneId(const FPixelRacerProceduralZone& Zone, const int32 ZoneIndex)
    {
        uint32 Hashes[4] = {
            FCrc::StrCrc32(*FString::Printf(TEXT("zone|%d|%d|%d"), ZoneIndex, Zone.Seed, Zone.LayerIndex)),
            FCrc::StrCrc32(*Zone.RulePreset),
            FCrc::StrCrc32(*Zone.AssetId),
            0x811c9dc5u
        };
        for (const FVector2D& Point : Zone.Polygon)
        {
            Hashes[3] = HashCombineFast(Hashes[3], GetTypeHash(Point));
        }
        Hashes[0] |= 1u;
        return FGuid(Hashes[0], Hashes[1], Hashes[2], Hashes[3]);
    }

    static bool ShouldRemoveTileForZone(const FPixelRacerTilePlacement& Tile, const FGuid& ZoneId)
    {
        return ZoneId.IsValid() && Tile.SourceZoneId == ZoneId && Tile.bGenerated && !Tile.bManualOverride;
    }

    static bool ShouldRemovePieceForZone(
        const FPixelRacerPiecePlacement& Piece,
        const FGuid& ZoneId,
        const FString& LegacyTag)
    {
        const bool bOwned = ZoneId.IsValid() &&
            (Piece.SourceZoneId == ZoneId ||
                (!Piece.SourceZoneId.IsValid() && Piece.Tags.Contains(LegacyTag)));
        return bOwned && Piece.bGenerated && !Piece.bManualOverride && !Piece.bLocked;
    }

    static void SuppressCell(FPixelRacerTrackDocument& Document, const FGuid& ZoneId, const FIntPoint& Cell)
    {
        if (!ZoneId.IsValid())
        {
            return;
        }
        FPixelRacerProceduralZone* Zone = Document.ProceduralZones.FindByPredicate([&](const FPixelRacerProceduralZone& Candidate)
        {
            return Candidate.Id == ZoneId;
        });
        if (Zone != nullptr)
        {
            Zone->SuppressedCells.AddUnique(Cell);
        }
    }

    static FIntPoint WorldToCell(const FVector2D& Location, const float Spacing)
    {
        return FIntPoint(
            FMath::FloorToInt(Location.X / Spacing),
            FMath::FloorToInt(Location.Y / Spacing));
    }

    static bool IsPointNearRoad(const FVector2D& Point, const TArray<FRoadSegment>& RoadSegments, const float Clearance)
    {
        for (const FRoadSegment& Segment : RoadSegments)
        {
            const FVector2D Delta = Segment.End - Segment.Start;
            const float LengthSquared = Delta.SizeSquared();
            if (LengthSquared <= KINDA_SMALL_NUMBER)
            {
                continue;
            }
            const float Alpha = FMath::Clamp(FVector2D::DotProduct(Point - Segment.Start, Delta) / LengthSquared, 0.0f, 1.0f);
            const FVector2D Closest = Segment.Start + Delta * Alpha;
            const float ExclusionRadius = FMath::Lerp(Segment.StartHalfWidth, Segment.EndHalfWidth, Alpha) + Clearance;
            if (FVector2D::DistSquared(Point, Closest) <= FMath::Square(ExclusionRadius))
            {
                return true;
            }
        }
        return false;
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
    for (const FPixelRacerTilePlacement& Tile : Document.Tiles)
    {
        if (Tile.Cell == Cell && Tile.LayerIndex == LayerIndex && Tile.SourceZoneId.IsValid())
        {
            PixelRacerAuthoring::SuppressCell(Document, Tile.SourceZoneId, Tile.Cell);
        }
    }
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
    for (const FPixelRacerPiecePlacement& Piece : Document.Pieces)
    {
        if (!Piece.bLocked && Piece.SourceZoneId.IsValid() &&
            (LayerIndex == INDEX_NONE || Piece.LayerIndex == LayerIndex) &&
            FVector::DistSquared2D(Piece.Transform.GetLocation(), Location) <= RadiusSquared)
        {
            PixelRacerAuthoring::SuppressCell(Document, Piece.SourceZoneId, Piece.SourceZoneCell);
        }
    }
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
    FVector2D Min;
    FVector2D Max;
    FString ValidationError;
    PixelRacerAuthoring::FZoneGridBounds Bounds;
    if (!PixelRacerAuthoring::ValidatePolygon(Polygon, Min, Max, ValidationError) ||
        !PixelRacerAuthoring::BuildGridBounds(Min, Max, PixelRacerAuthoring::TileGridSize, Bounds, ValidationError) ||
        !PixelRacerAuthoring::ValidatePointInPolygonBudget(Bounds, Polygon.Num(), ValidationError))
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
    if (!Document.ProceduralZones.IsValidIndex(ZoneIndex))
    {
        return false;
    }

    FVector2D Min;
    FVector2D Max;
    FString ValidationError;
    PixelRacerAuthoring::FZoneGridBounds Bounds;
    const FPixelRacerProceduralZone& Zone = Document.ProceduralZones[ZoneIndex];
    const float GridSpacing = Zone.bGenerateTiles ? PixelRacerAuthoring::TileGridSize : Zone.Spacing;
    if (!FMath::IsFinite(GridSpacing) || GridSpacing <= KINDA_SMALL_NUMBER ||
        !PixelRacerAuthoring::ValidatePolygon(Polygon, Min, Max, ValidationError) ||
        !PixelRacerAuthoring::BuildGridBounds(Min, Max, GridSpacing, Bounds, ValidationError) ||
        !PixelRacerAuthoring::ValidatePointInPolygonBudget(Bounds, Polygon.Num(), ValidationError))
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

    const FGuid ZoneId = Document.ProceduralZones[ZoneIndex].Id;
    if (ZoneId.IsValid())
    {
        for (int32 OtherZoneIndex = 0; OtherZoneIndex < Document.ProceduralZones.Num(); ++OtherZoneIndex)
        {
            if (OtherZoneIndex != ZoneIndex && Document.ProceduralZones[OtherZoneIndex].Id == ZoneId)
            {
                return false;
            }
        }
    }
    const FString LegacyTag = PixelRacerAuthoring::ZoneTag(ZoneId);
    Document.Tiles.RemoveAll([&](const FPixelRacerTilePlacement& Tile)
    {
        return PixelRacerAuthoring::ShouldRemoveTileForZone(Tile, ZoneId);
    });
    Document.Pieces.RemoveAll([&](const FPixelRacerPiecePlacement& Piece)
    {
        return PixelRacerAuthoring::ShouldRemovePieceForZone(Piece, ZoneId, LegacyTag);
    });
    Document.ProceduralZones.RemoveAt(ZoneIndex);
    return true;
}

bool UPixelRacerTrackAuthoringLibrary::GenerateProceduralZone(
    FPixelRacerTrackDocument& Document,
    const int32 ZoneIndex,
    int32& OutGeneratedCount,
    FString& OutError)
{
    OutGeneratedCount = 0;
    OutError.Reset();
    if (!Document.ProceduralZones.IsValidIndex(ZoneIndex))
    {
        OutError = TEXT("Procedural zone index is out of range.");
        return false;
    }

    const FPixelRacerProceduralZone Zone = Document.ProceduralZones[ZoneIndex];
    if (Zone.AssetId.TrimStartAndEnd().IsEmpty())
    {
        OutError = TEXT("Procedural zone needs an asset before it can generate placements.");
        return false;
    }
    if (!FMath::IsFinite(Zone.Spacing) || Zone.Spacing <= KINDA_SMALL_NUMBER)
    {
        OutError = TEXT("Procedural zone spacing must be a finite positive value.");
        return false;
    }
    if (!FMath::IsFinite(Zone.Density) || Zone.Density < 0.0f || Zone.Density > 1.0f)
    {
        OutError = TEXT("Procedural zone density must be finite and between zero and one.");
        return false;
    }
    if (!FMath::IsFinite(Zone.RoadClearance) || Zone.RoadClearance < 0.0f)
    {
        OutError = TEXT("Procedural zone road clearance must be a finite non-negative value.");
        return false;
    }
    if (Zone.bGenerateTiles && Zone.TileIndex < 0)
    {
        OutError = TEXT("Procedural zone tile index cannot be negative.");
        return false;
    }

    FVector2D PolygonMin;
    FVector2D PolygonMax;
    if (!PixelRacerAuthoring::ValidatePolygon(Zone.Polygon, PolygonMin, PolygonMax, OutError))
    {
        return false;
    }

    const float GridSpacing = Zone.bGenerateTiles ? PixelRacerAuthoring::TileGridSize : Zone.Spacing;
    PixelRacerAuthoring::FZoneGridBounds Bounds;
    if (!PixelRacerAuthoring::BuildGridBounds(PolygonMin, PolygonMax, GridSpacing, Bounds, OutError))
    {
        return false;
    }
    if (!PixelRacerAuthoring::ValidatePointInPolygonBudget(Bounds, Zone.Polygon.Num(), OutError))
    {
        return false;
    }

    TArray<PixelRacerAuthoring::FRoadSegment> RoadSegments;
    if (Zone.bAvoidRoads)
    {
        for (const FPixelRacerRoadSpline& Spline : Document.RoadSplines)
        {
            const int32 PointCount = Spline.ControlPoints.Num();
            if (PointCount < 2)
            {
                continue;
            }
            const int32 SegmentCount = Spline.bClosedLoop && PointCount > 2 ? PointCount : PointCount - 1;
            for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
            {
                const FPixelRacerTrackControlPoint& Start = Spline.ControlPoints[SegmentIndex];
                const FPixelRacerTrackControlPoint& End = Spline.ControlPoints[(SegmentIndex + 1) % PointCount];
                if (!FMath::IsFinite(Start.Location.X) || !FMath::IsFinite(Start.Location.Y) ||
                    !FMath::IsFinite(End.Location.X) || !FMath::IsFinite(End.Location.Y) ||
                    !FMath::IsFinite(Start.Width) || !FMath::IsFinite(End.Width) ||
                    Start.Width <= 0.0f || End.Width <= 0.0f)
                {
                    OutError = TEXT("Road avoidance encountered a non-finite point or invalid road width.");
                    return false;
                }

                if (FVector::DistSquared2D(Start.Location, End.Location) <= KINDA_SMALL_NUMBER)
                {
                    continue;
                }
                PixelRacerAuthoring::FRoadSegment& Segment = RoadSegments.AddDefaulted_GetRef();
                Segment.Start = FVector2D(Start.Location.X, Start.Location.Y);
                Segment.End = FVector2D(End.Location.X, End.Location.Y);
                Segment.StartHalfWidth = Start.Width * 0.5f;
                Segment.EndHalfWidth = End.Width * 0.5f;
            }
        }
        if (Bounds.CandidateCount > 0 && RoadSegments.Num() > 0 &&
            Bounds.CandidateCount > PixelRacerAuthoring::MaxRoadDistanceChecks / RoadSegments.Num())
        {
            OutError = TEXT("Procedural zone and road complexity exceed the road-avoidance budget.");
            return false;
        }
    }

    const FGuid ZoneId = Zone.Id.IsValid() ? Zone.Id : PixelRacerAuthoring::MakeStableZoneId(Zone, ZoneIndex);
    for (int32 OtherZoneIndex = 0; OtherZoneIndex < Document.ProceduralZones.Num(); ++OtherZoneIndex)
    {
        if (OtherZoneIndex != ZoneIndex && Document.ProceduralZones[OtherZoneIndex].Id == ZoneId)
        {
            OutError = TEXT("Procedural zone id conflicts with another zone.");
            return false;
        }
    }
    const FString LegacyTag = PixelRacerAuthoring::ZoneTag(ZoneId);
    TSet<FIntPoint> OccupiedCells;
    TSet<FGuid> OccupiedPieceIds;
    for (const FPixelRacerPiecePlacement& Piece : Document.Pieces)
    {
        if (!PixelRacerAuthoring::ShouldRemovePieceForZone(Piece, ZoneId, LegacyTag) && Piece.Id.IsValid())
        {
            OccupiedPieceIds.Add(Piece.Id);
        }
    }
    if (Zone.bGenerateTiles)
    {
        for (const FPixelRacerTilePlacement& Tile : Document.Tiles)
        {
            if (Tile.LayerIndex == Zone.LayerIndex && !PixelRacerAuthoring::ShouldRemoveTileForZone(Tile, ZoneId))
            {
                OccupiedCells.Add(Tile.Cell);
            }
        }
        for (const FPixelRacerPiecePlacement& Piece : Document.Pieces)
        {
            if (Piece.LayerIndex == Zone.LayerIndex &&
                !PixelRacerAuthoring::ShouldRemovePieceForZone(Piece, ZoneId, LegacyTag))
            {
                const FVector Location = Piece.Transform.GetLocation();
                if (FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y))
                {
                    OccupiedCells.Add(PixelRacerAuthoring::WorldToCell(FVector2D(Location.X, Location.Y), GridSpacing));
                }
                if (Piece.SourceZoneId == ZoneId)
                {
                    OccupiedCells.Add(Piece.SourceZoneCell);
                }
            }
        }
    }
    else
    {
        for (const FPixelRacerTilePlacement& Tile : Document.Tiles)
        {
            if (Tile.LayerIndex == Zone.LayerIndex && !PixelRacerAuthoring::ShouldRemoveTileForZone(Tile, ZoneId))
            {
                const FVector2D TileCenter(
                    (Tile.Cell.X + 0.5f) * PixelRacerAuthoring::TileGridSize,
                    (Tile.Cell.Y + 0.5f) * PixelRacerAuthoring::TileGridSize);
                OccupiedCells.Add(PixelRacerAuthoring::WorldToCell(TileCenter, GridSpacing));
            }
        }
        for (const FPixelRacerPiecePlacement& Piece : Document.Pieces)
        {
            if (Piece.LayerIndex == Zone.LayerIndex &&
                !PixelRacerAuthoring::ShouldRemovePieceForZone(Piece, ZoneId, LegacyTag))
            {
                const FVector Location = Piece.Transform.GetLocation();
                if (FMath::IsFinite(Location.X) && FMath::IsFinite(Location.Y))
                {
                    OccupiedCells.Add(PixelRacerAuthoring::WorldToCell(FVector2D(Location.X, Location.Y), GridSpacing));
                }
                if (Piece.SourceZoneId == ZoneId)
                {
                    OccupiedCells.Add(Piece.SourceZoneCell);
                }
            }
        }
    }

    TSet<FIntPoint> SuppressedCells;
    for (const FIntPoint& SuppressedCell : Zone.SuppressedCells)
    {
        SuppressedCells.Add(SuppressedCell);
    }
    TArray<FPixelRacerTilePlacement> GeneratedTiles;
    TArray<FPixelRacerPiecePlacement> GeneratedPieces;
    const int32 ReserveCount = static_cast<int32>(FMath::Min<int64>(Bounds.CandidateCount, MAX_int32));
    if (Zone.bGenerateTiles)
    {
        GeneratedTiles.Reserve(ReserveCount);
    }
    else
    {
        GeneratedPieces.Reserve(ReserveCount);
    }

    for (int64 CellY64 = Bounds.MinY; CellY64 <= Bounds.MaxY; ++CellY64)
    {
        for (int64 CellX64 = Bounds.MinX; CellX64 <= Bounds.MaxX; ++CellX64)
        {
            const FIntPoint Cell(static_cast<int32>(CellX64), static_cast<int32>(CellY64));
            if (SuppressedCells.Contains(Cell) || OccupiedCells.Contains(Cell))
            {
                continue;
            }

            const FVector2D Candidate(
                (static_cast<double>(Cell.X) + 0.5) * GridSpacing,
                (static_cast<double>(Cell.Y) + 0.5) * GridSpacing);
            if (!PixelRacerAuthoring::PointInsidePolygon(Candidate, Zone.Polygon))
            {
                continue;
            }

            const double UnitDensity = static_cast<double>(PixelRacerAuthoring::HashZoneCell(ZoneId, Zone.Seed, Cell, 0x51ed270bu)) /
                (static_cast<double>(MAX_uint32) + 1.0);
            const float FootprintClearance = Zone.bGenerateTiles
                ? PixelRacerAuthoring::TileGridSize * UE_SQRT_2 * 0.5f
                : 0.0f;
            if (UnitDensity >= Zone.Density ||
                (Zone.bAvoidRoads && PixelRacerAuthoring::IsPointNearRoad(
                    Candidate,
                    RoadSegments,
                    Zone.RoadClearance + FootprintClearance)))
            {
                continue;
            }

            if (Zone.bGenerateTiles)
            {
                FPixelRacerTilePlacement& Tile = GeneratedTiles.AddDefaulted_GetRef();
                Tile.Cell = Cell;
                Tile.AssetId = Zone.AssetId;
                Tile.TileIndex = Zone.TileIndex;
                Tile.LayerIndex = Zone.LayerIndex;
                Tile.bGenerated = true;
                Tile.bManualOverride = false;
                Tile.SourceZoneId = ZoneId;
            }
            else
            {
                const FGuid PlacementId = PixelRacerAuthoring::MakeStablePlacementId(ZoneId, Zone.Seed, Cell);
                if (OccupiedPieceIds.Contains(PlacementId))
                {
                    continue;
                }
                FPixelRacerPiecePlacement& Piece = GeneratedPieces.AddDefaulted_GetRef();
                Piece.Id = PlacementId;
                Piece.AssetId = Zone.AssetId;
                const float Yaw = static_cast<float>(PixelRacerAuthoring::HashZoneCell(ZoneId, Zone.Seed, Cell, 0x94d049bbu) % 36000u) / 100.0f;
                Piece.Transform = FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Candidate, 0.0f));
                Piece.LayerIndex = Zone.LayerIndex;
                Piece.bGenerated = true;
                Piece.bManualOverride = false;
                Piece.bLocked = false;
                Piece.Tags = { TEXT("procedural"), LegacyTag, Zone.RulePreset };
                Piece.SourceZoneId = ZoneId;
                Piece.SourceZoneCell = Cell;
            }
        }
    }

    Document.ProceduralZones[ZoneIndex].Id = ZoneId;
    Document.Tiles.RemoveAll([&](const FPixelRacerTilePlacement& Tile)
    {
        return PixelRacerAuthoring::ShouldRemoveTileForZone(Tile, ZoneId);
    });
    Document.Pieces.RemoveAll([&](const FPixelRacerPiecePlacement& Piece)
    {
        return PixelRacerAuthoring::ShouldRemovePieceForZone(Piece, ZoneId, LegacyTag);
    });
    OutGeneratedCount = GeneratedTiles.Num() + GeneratedPieces.Num();
    Document.Tiles.Append(MoveTemp(GeneratedTiles));
    Document.Pieces.Append(MoveTemp(GeneratedPieces));
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
