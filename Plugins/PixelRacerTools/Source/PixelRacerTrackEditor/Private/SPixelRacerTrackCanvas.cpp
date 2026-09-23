#include "SPixelRacerTrackCanvas.h"

#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Layout/Clipping.h"
#include "Misc/Paths.h"
#include "PaperSprite.h"
#include "PaperTileSet.h"
#include "PixelRacerTrackAuthoringLibrary.h"
#include "PixelRacerTrackLibrary.h"
#include "SPixelRacerAssetBrowser.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateBrush.h"

namespace PixelRacerCanvas
{
    static FSlateBrush MakeSourceRegionBrush(
        const FSlateBrush* FallbackBrush,
        UTexture2D* Texture,
        const FIntPoint& SourceUV,
        const FIntPoint& SourceSize,
        const FIntPoint& TextureSize)
    {
        FSlateBrush Brush;
        if (Texture != nullptr)
        {
            Brush.SetResourceObject(Texture);
        }
        else if (FallbackBrush != nullptr)
        {
            Brush = *FallbackBrush;
        }

        Brush.DrawAs = ESlateBrushDrawType::Image;
        Brush.Tiling = ESlateBrushTileType::NoTile;
        Brush.TintColor = FSlateColor(FLinearColor::White);
        if (TextureSize.X > 0 && TextureSize.Y > 0 && SourceSize.X > 0 && SourceSize.Y > 0)
        {
            const FVector2f UVMin(
                static_cast<float>(SourceUV.X) / static_cast<float>(TextureSize.X),
                static_cast<float>(SourceUV.Y) / static_cast<float>(TextureSize.Y));
            const FVector2f UVMax(
                static_cast<float>(SourceUV.X + SourceSize.X) / static_cast<float>(TextureSize.X),
                static_cast<float>(SourceUV.Y + SourceSize.Y) / static_cast<float>(TextureSize.Y));
            Brush.SetUVRegion(FBox2f(UVMin, UVMax));
        }
        return Brush;
    }

    static float DistancePointToSegment(const FVector2D& Point, const FVector2D& A, const FVector2D& B, FVector2D& OutClosest)
    {
        const FVector2D Delta = B - A;
        const float Denominator = Delta.SizeSquared();
        if (Denominator <= KINDA_SMALL_NUMBER)
        {
            OutClosest = A;
            return FVector2D::Distance(Point, A);
        }

        const float Alpha = FMath::Clamp(FVector2D::DotProduct(Point - A, Delta) / Denominator, 0.0f, 1.0f);
        OutClosest = A + Delta * Alpha;
        return FVector2D::Distance(Point, OutClosest);
    }
}

void SPixelRacerTrackCanvas::Construct(const FArguments& InArgs)
{
    SetClipping(EWidgetClipping::ClipToBounds);
    ResetDocument();
    ChildSlot
    [
        SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(0.0f)
    ];
}

void SPixelRacerTrackCanvas::SetMode(const EPixelRacerAuthoringMode InMode)
{
    if (Mode == EPixelRacerAuthoringMode::ProceduralZone && InMode != EPixelRacerAuthoringMode::ProceduralZone)
    {
        CancelOpenZone();
    }
    Mode = InMode;
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SPixelRacerTrackCanvas::ResetDocument()
{
    FinishOpenEdit();
    Session.Document = FPixelRacerTrackDocument();
    Session.Document.SchemaVersion = 2;
    Session.Document.Metadata.TrackId = TEXT("editor_sandbox");
    Session.Document.Metadata.DisplayName = TEXT("Editor Sandbox");
    UPixelRacerTrackAuthoringLibrary::AddRoadSpline(Session.Document, TEXT("Main Road"), true);

    auto AddSurface = [&](const TCHAR* Id, float Friction, float Drift, float Speed)
    {
        FPixelRacerSurfaceProfile& Surface = Session.Document.SurfaceProfiles.AddDefaulted_GetRef();
        Surface.SurfaceId = Id;
        Surface.Friction = Friction;
        Surface.DriftMultiplier = Drift;
        Surface.MaxSpeedMultiplier = Speed;
    };
    AddSurface(TEXT("asphalt"), 1.0f, 1.0f, 1.0f);
    AddSurface(TEXT("dirt"), 0.74f, 1.20f, 0.86f);
    AddSurface(TEXT("grass"), 0.52f, 1.35f, 0.68f);
    AddSurface(TEXT("sand"), 0.45f, 1.45f, 0.58f);

    Session.ResetHistory();
    DragOperation = EDragOperation::None;
    OpenZoneVertices.Reset();
    ClearZoneSelection();
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SPixelRacerTrackCanvas::GenerateAI()
{
    Session.BeginEdit();
    UPixelRacerTrackAuthoringLibrary::GenerateCheckpoints(Session.Document, 0, 320.0f, true);
    UPixelRacerTrackAuthoringLibrary::GenerateGridSlots(Session.Document, 0, 8, true);
    UPixelRacerTrackAuthoringLibrary::GenerateRacingLines(Session.Document, 0, true);
    Session.EndEdit();
    Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SPixelRacerTrackCanvas::BakeRoad()
{
    Session.BeginEdit();
    const int32 Count = UPixelRacerTrackAuthoringLibrary::BakeSplineToPieces(Session.Document, 0, TEXT("Tilesets/race_track_1.png"), 48.0f, 0, true);
    Session.EndEdit();
    Invalidate(EInvalidateWidgetReason::Paint);
    return Count;
}

bool SPixelRacerTrackCanvas::Undo()
{
    FinishOpenEdit();
    const bool bResult = Session.Undo();
    if (bResult)
    {
        ClearRoadPointSelection();
        ClearZoneSelection();
        OpenZoneVertices.Reset();
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return bResult;
}

bool SPixelRacerTrackCanvas::Redo()
{
    FinishOpenEdit();
    const bool bResult = Session.Redo();
    if (bResult)
    {
        ClearRoadPointSelection();
        ClearZoneSelection();
        OpenZoneVertices.Reset();
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return bResult;
}

bool SPixelRacerTrackCanvas::LoadSandboxTrack(FString& OutError)
{
    FinishOpenEdit();
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PixelRacer/Tracks/editor_sandbox.pixeltrack.json"));
    FPixelRacerTrackDocument Loaded;
    if (!UPixelRacerTrackLibrary::ImportTrackDocument(Path, Loaded, OutError))
    {
        return false;
    }

    Session.Document = MoveTemp(Loaded);
    Session.ResetHistory();
    DragOperation = EDragOperation::None;
    OpenZoneVertices.Reset();
    ClearZoneSelection();
    Invalidate(EInvalidateWidgetReason::Paint);
    return true;
}


void SPixelRacerTrackCanvas::SetZoneDrawMode(const EPixelRacerZoneDrawMode InMode)
{
    if (ZoneDrawMode == InMode)
    {
        return;
    }
    CancelOpenZone();
    ZoneDrawMode = InMode;
    Invalidate(EInvalidateWidgetReason::Paint);
}

FString SPixelRacerTrackCanvas::GetZoneDrawModeName() const
{
    switch (ZoneDrawMode)
    {
    case EPixelRacerZoneDrawMode::Polygon: return TEXT("Polygon");
    case EPixelRacerZoneDrawMode::Freehand: return TEXT("Freehand");
    case EPixelRacerZoneDrawMode::Rectangle:
    default: return TEXT("Rectangle");
    }
}

bool SPixelRacerTrackCanvas::DeleteSelectedZone()
{
    if (!Session.Document.ProceduralZones.IsValidIndex(SelectedZoneIndex))
    {
        return false;
    }

    Session.BeginEdit();
    const bool bRemoved = UPixelRacerTrackAuthoringLibrary::RemoveProceduralZone(Session.Document, SelectedZoneIndex);
    Session.EndEdit();
    if (bRemoved)
    {
        ClearZoneSelection();
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return bRemoved;
}

bool SPixelRacerTrackCanvas::SetActiveAsset(const FString& AssetId, const FString& Role)
{
    if (AssetId.IsEmpty() || Role.IsEmpty())
    {
        return false;
    }

    LastSelectedAsset = AssetId;
    LastSelectedAssetRole = Role;

    if (Role == TEXT("tileset"))
    {
        if (ActiveTile != AssetId)
        {
            Session.ActiveTileIndex = 0;
        }
        ActiveTile = AssetId;
        Invalidate(EInvalidateWidgetReason::Paint);
        return true;
    }
    if (Role == TEXT("environment_piece"))
    {
        ActivePiece = AssetId;
        Invalidate(EInvalidateWidgetReason::Paint);
        return true;
    }

    // Vehicle/VFX selection is retained for upcoming preview/import workflows,
    // but those roles are not placed as track tiles/pieces.
    return Role == TEXT("vehicle_sprite_sheet") || Role == TEXT("vfx_sheet") || Role == TEXT("sprite");
}

void SPixelRacerTrackCanvas::SetAssetPreviewData(const TArray<FPixelRacerAssetPreviewData>& InPreviewData)
{
    AssetPreviewData.Reset();
    LoadedSourceBrushCache.Reset();
    LoadedSpriteCache.Reset();
    LoadedTileSetCache.Reset();
    for (const FPixelRacerAssetPreviewData& Preview : InPreviewData)
    {
        if (!Preview.AssetId.IsEmpty())
        {
            AssetPreviewData.Add(Preview.AssetId, Preview);
        }
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

int32 SPixelRacerTrackCanvas::GetActiveTileCount() const
{
    const FPixelRacerAssetPreviewData* Preview = AssetPreviewData.Find(ActiveTile);
    return Preview != nullptr ? FMath::Max(Preview->TileCount, 1) : 1;
}

TSharedPtr<FSlateDynamicImageBrush> SPixelRacerTrackCanvas::GetPreviewSourceBrush(const FString& AssetId) const
{
    const FPixelRacerAssetPreviewData* Preview = AssetPreviewData.Find(AssetId);
    if (Preview == nullptr)
    {
        return nullptr;
    }
    if (Preview->SourceBrush.IsValid())
    {
        return Preview->SourceBrush;
    }
    if (const TSharedPtr<FSlateDynamicImageBrush>* CachedBrush = LoadedSourceBrushCache.Find(AssetId); CachedBrush && CachedBrush->IsValid())
    {
        return *CachedBrush;
    }
    if (!FPaths::FileExists(Preview->SourceFile))
    {
        return nullptr;
    }

    TSharedPtr<FSlateDynamicImageBrush> Brush = MakeShared<FSlateDynamicImageBrush>(
        FName(*Preview->SourceFile),
        FVector2D(54.0f, 54.0f),
        FLinearColor::White,
        ESlateBrushTileType::NoTile,
        ESlateBrushImageType::FullColor);
    LoadedSourceBrushCache.Add(AssetId, Brush);
    return Brush;
}

void SPixelRacerTrackCanvas::CycleActiveTileIndex(const int32 Delta)
{
    const int32 TileCount = GetActiveTileCount();
    if (TileCount <= 1 || Delta == 0)
    {
        return;
    }
    Session.ActiveTileIndex = (Session.ActiveTileIndex + Delta + TileCount) % TileCount;
    Invalidate(EInvalidateWidgetReason::Paint);
}

UPaperSprite* SPixelRacerTrackCanvas::LoadPreviewSprite(const FString& AssetId) const
{
    const FPixelRacerAssetPreviewData* Preview = AssetPreviewData.Find(AssetId);
    if (Preview == nullptr || Preview->SpriteObjectPath.IsEmpty())
    {
        return nullptr;
    }
    if (const TWeakObjectPtr<UPaperSprite>* CachedSprite = LoadedSpriteCache.Find(AssetId); CachedSprite && CachedSprite->IsValid())
    {
        return CachedSprite->Get();
    }

    UPaperSprite* Sprite = LoadObject<UPaperSprite>(nullptr, *Preview->SpriteObjectPath);
    if (Sprite != nullptr)
    {
        LoadedSpriteCache.Add(AssetId, Sprite);
    }
    return Sprite;
}

UPaperTileSet* SPixelRacerTrackCanvas::LoadPreviewTileSet(const FString& AssetId) const
{
    const FPixelRacerAssetPreviewData* Preview = AssetPreviewData.Find(AssetId);
    if (Preview == nullptr || Preview->TileSetObjectPath.IsEmpty())
    {
        return nullptr;
    }
    if (const TWeakObjectPtr<UPaperTileSet>* CachedTileSet = LoadedTileSetCache.Find(AssetId); CachedTileSet && CachedTileSet->IsValid())
    {
        return CachedTileSet->Get();
    }

    UPaperTileSet* TileSet = LoadObject<UPaperTileSet>(nullptr, *Preview->TileSetObjectPath);
    if (TileSet != nullptr)
    {
        LoadedTileSetCache.Add(AssetId, TileSet);
    }
    return TileSet;
}

FString SPixelRacerTrackCanvas::GetActiveAssetSummary() const
{
    if (LastSelectedAssetRole == TEXT("tileset"))
    {
        return FString::Printf(
            TEXT("%s (%s), tile %d/%d  [ and ] to change"),
            *LastSelectedAsset,
            *LastSelectedAssetRole,
            Session.ActiveTileIndex + 1,
            GetActiveTileCount());
    }
    return FString::Printf(TEXT("%s (%s)"), *LastSelectedAsset, *LastSelectedAssetRole);
}

FVector SPixelRacerTrackCanvas::CanvasToTrack(const FVector2D& LocalPosition) const
{
    return FVector(LocalPosition.X, LocalPosition.Y, 0.0f);
}

FVector2D SPixelRacerTrackCanvas::TrackToCanvas(const FVector& TrackPosition) const
{
    return FVector2D(TrackPosition.X, TrackPosition.Y);
}

FPixelRacerTrackControlPoint* SPixelRacerTrackCanvas::GetSelectedRoadPoint()
{
    if (!Session.Document.RoadSplines.IsValidIndex(Session.SelectedSplineIndex))
    {
        return nullptr;
    }
    TArray<FPixelRacerTrackControlPoint>& Points = Session.Document.RoadSplines[Session.SelectedSplineIndex].ControlPoints;
    return Points.IsValidIndex(Session.SelectedPointIndex) ? &Points[Session.SelectedPointIndex] : nullptr;
}

const FPixelRacerTrackControlPoint* SPixelRacerTrackCanvas::GetSelectedRoadPoint() const
{
    if (!Session.Document.RoadSplines.IsValidIndex(Session.SelectedSplineIndex))
    {
        return nullptr;
    }
    const TArray<FPixelRacerTrackControlPoint>& Points = Session.Document.RoadSplines[Session.SelectedSplineIndex].ControlPoints;
    return Points.IsValidIndex(Session.SelectedPointIndex) ? &Points[Session.SelectedPointIndex] : nullptr;
}

bool SPixelRacerTrackCanvas::HasSelectedRoadPoint() const
{
    return GetSelectedRoadPoint() != nullptr;
}

FText SPixelRacerTrackCanvas::GetSelectedRoadPointSummary() const
{
    const FPixelRacerTrackControlPoint* Point = GetSelectedRoadPoint();
    if (!Point)
    {
        return FText::FromString(TEXT("No road point selected."));
    }

    return FText::FromString(FString::Printf(
        TEXT("Spline %d / Point %d\nWidth: %.1f\nSurface: %s\nElevation layer: %d"),
        Session.SelectedSplineIndex + 1,
        Session.SelectedPointIndex + 1,
        Point->Width,
        *Point->SurfaceId,
        Point->ElevationLayer));
}

bool SPixelRacerTrackCanvas::SetSelectedRoadPointSurface(const FString& SurfaceId)
{
    if (!HasSelectedRoadPoint() || SurfaceId.IsEmpty())
    {
        return false;
    }
    Session.BeginEdit();
    const bool bResult = UPixelRacerTrackAuthoringLibrary::SetRoadControlPointSurface(
        Session.Document, Session.SelectedSplineIndex, Session.SelectedPointIndex, SurfaceId);
    Session.EndEdit();
    if (bResult)
    {
        ActiveSurface = SurfaceId;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return bResult;
}

bool SPixelRacerTrackCanvas::AdjustSelectedRoadPointElevation(const int32 Delta)
{
    const FPixelRacerTrackControlPoint* Point = GetSelectedRoadPoint();
    if (!Point)
    {
        return false;
    }
    const int32 NewLayer = FMath::Clamp(Point->ElevationLayer + Delta, -1, 2);
    Session.BeginEdit();
    const bool bResult = UPixelRacerTrackAuthoringLibrary::SetRoadControlPointElevation(
        Session.Document, Session.SelectedSplineIndex, Session.SelectedPointIndex, NewLayer);
    Session.EndEdit();
    if (bResult)
    {
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return bResult;
}

bool SPixelRacerTrackCanvas::DeleteSelectedRoadPoint()
{
    if (!HasSelectedRoadPoint())
    {
        return false;
    }
    Session.BeginEdit();
    const bool bResult = UPixelRacerTrackAuthoringLibrary::DeleteRoadControlPoint(
        Session.Document, Session.SelectedSplineIndex, Session.SelectedPointIndex);
    Session.EndEdit();
    if (bResult)
    {
        ClearRoadPointSelection();
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    return bResult;
}

void SPixelRacerTrackCanvas::SelectRoadPoint(const int32 SplineIndex, const int32 PointIndex)
{
    Session.SelectedSplineIndex = SplineIndex;
    Session.SelectedPointIndex = PointIndex;
}

void SPixelRacerTrackCanvas::ClearRoadPointSelection()
{
    Session.SelectedSplineIndex = INDEX_NONE;
    Session.SelectedPointIndex = INDEX_NONE;
}

bool SPixelRacerTrackCanvas::HitTestRoadPoint(const FVector2D& LocalPosition, int32& OutSplineIndex, int32& OutPointIndex, const float MaxDistance) const
{
    float BestDistance = MaxDistance;
    bool bFound = false;
    for (int32 SplineIndex = 0; SplineIndex < Session.Document.RoadSplines.Num(); ++SplineIndex)
    {
        const FPixelRacerRoadSpline& Spline = Session.Document.RoadSplines[SplineIndex];
        for (int32 PointIndex = 0; PointIndex < Spline.ControlPoints.Num(); ++PointIndex)
        {
            const float Distance = FVector2D::Distance(LocalPosition, TrackToCanvas(Spline.ControlPoints[PointIndex].Location));
            if (Distance <= BestDistance)
            {
                BestDistance = Distance;
                OutSplineIndex = SplineIndex;
                OutPointIndex = PointIndex;
                bFound = true;
            }
        }
    }
    return bFound;
}

FVector2D SPixelRacerTrackCanvas::GetRoadPointNormal(const int32 SplineIndex, const int32 PointIndex) const
{
    if (!Session.Document.RoadSplines.IsValidIndex(SplineIndex))
    {
        return FVector2D(0.0f, -1.0f);
    }
    const FPixelRacerRoadSpline& Spline = Session.Document.RoadSplines[SplineIndex];
    if (!Spline.ControlPoints.IsValidIndex(PointIndex) || Spline.ControlPoints.Num() < 2)
    {
        return FVector2D(0.0f, -1.0f);
    }

    const int32 PrevIndex = PointIndex > 0 ? PointIndex - 1 : (Spline.bClosedLoop ? Spline.ControlPoints.Num() - 1 : PointIndex);
    const int32 NextIndex = PointIndex + 1 < Spline.ControlPoints.Num() ? PointIndex + 1 : (Spline.bClosedLoop ? 0 : PointIndex);
    const FVector2D Prev = TrackToCanvas(Spline.ControlPoints[PrevIndex].Location);
    const FVector2D Next = TrackToCanvas(Spline.ControlPoints[NextIndex].Location);
    const FVector2D Tangent = (Next - Prev).GetSafeNormal();
    if (Tangent.IsNearlyZero())
    {
        return FVector2D(0.0f, -1.0f);
    }
    return FVector2D(-Tangent.Y, Tangent.X);
}

FVector2D SPixelRacerTrackCanvas::GetSelectedWidthHandlePosition() const
{
    const FPixelRacerTrackControlPoint* Point = GetSelectedRoadPoint();
    if (!Point)
    {
        return FVector2D::ZeroVector;
    }
    const FVector2D PointPosition = TrackToCanvas(Point->Location);
    return PointPosition + GetRoadPointNormal(Session.SelectedSplineIndex, Session.SelectedPointIndex) * (Point->Width * 0.5f);
}

bool SPixelRacerTrackCanvas::HitTestSelectedWidthHandle(const FVector2D& LocalPosition) const
{
    return HasSelectedRoadPoint() && FVector2D::Distance(LocalPosition, GetSelectedWidthHandlePosition()) <= 10.0f;
}

bool SPixelRacerTrackCanvas::FindNearestRoadSegment(
    const FVector2D& LocalPosition,
    int32& OutSplineIndex,
    int32& OutInsertBeforeIndex,
    FVector& OutTrackLocation,
    const float MaxDistance) const
{
    float BestDistance = MaxDistance;
    bool bFound = false;
    for (int32 SplineIndex = 0; SplineIndex < Session.Document.RoadSplines.Num(); ++SplineIndex)
    {
        const FPixelRacerRoadSpline& Spline = Session.Document.RoadSplines[SplineIndex];
        if (Spline.ControlPoints.Num() < 2)
        {
            continue;
        }

        const int32 SegmentCount = Spline.bClosedLoop ? Spline.ControlPoints.Num() : Spline.ControlPoints.Num() - 1;
        for (int32 SegmentIndex = 0; SegmentIndex < SegmentCount; ++SegmentIndex)
        {
            const int32 NextIndex = (SegmentIndex + 1) % Spline.ControlPoints.Num();
            FVector2D Closest;
            const float Distance = PixelRacerCanvas::DistancePointToSegment(
                LocalPosition,
                TrackToCanvas(Spline.ControlPoints[SegmentIndex].Location),
                TrackToCanvas(Spline.ControlPoints[NextIndex].Location),
                Closest);
            if (Distance <= BestDistance)
            {
                BestDistance = Distance;
                OutSplineIndex = SplineIndex;
                OutInsertBeforeIndex = NextIndex == 0 ? Spline.ControlPoints.Num() : NextIndex;
                OutTrackLocation = CanvasToTrack(Closest);
                bFound = true;
            }
        }
    }
    return bFound;
}

void SPixelRacerTrackCanvas::ApplyTileAt(const FVector2D& LocalPosition, const bool bErase)
{
    const FIntPoint Cell(FMath::FloorToInt(LocalPosition.X / GridSize), FMath::FloorToInt(LocalPosition.Y / GridSize));
    if (bErase)
    {
        UPixelRacerTrackAuthoringLibrary::EraseTile(Session.Document, Cell, 0);
    }
    else
    {
        UPixelRacerTrackAuthoringLibrary::PaintTile(Session.Document, Cell, ActiveTile, 0, 0, Session.ActiveTileIndex);
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SPixelRacerTrackCanvas::ApplyPieceAt(const FVector2D& LocalPosition, const bool bErase)
{
    const FVector TrackLocation = CanvasToTrack(LocalPosition);
    if (bErase)
    {
        UPixelRacerTrackAuthoringLibrary::ErasePiecesInRadius(Session.Document, TrackLocation, GridSize * 0.75f, 0);
    }
    else
    {
        const FVector Snapped = UPixelRacerTrackAuthoringLibrary::SnapLocationToGrid(TrackLocation, GridSize);
        UPixelRacerTrackAuthoringLibrary::PlacePiece(Session.Document, ActivePiece, FTransform(FRotator::ZeroRotator, Snapped), 0, false);
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

bool SPixelRacerTrackCanvas::CreateProceduralRectangle()
{
    const float Width = FMath::Abs(ZoneCurrent.X - ZoneStart.X);
    const float Height = FMath::Abs(ZoneCurrent.Y - ZoneStart.Y);
    if (Width < GridSize * 0.5f || Height < GridSize * 0.5f)
    {
        return false;
    }

    const FVector2D A(ZoneStart.X, ZoneStart.Y);
    const FVector2D B(ZoneCurrent.X, ZoneCurrent.Y);
    const TArray<FVector2D> Polygon =
    {
        A,
        FVector2D(B.X, A.Y),
        B,
        FVector2D(A.X, B.Y)
    };
    const int32 NewZoneIndex = UPixelRacerTrackAuthoringLibrary::AddProceduralZone(Session.Document, Polygon, ActiveZonePreset, 0, 1337);
    if (NewZoneIndex != INDEX_NONE)
    {
        SelectedZoneIndex = NewZoneIndex;
        SelectedZoneVertexIndex = INDEX_NONE;
        return true;
    }
    return false;
}

bool SPixelRacerTrackCanvas::FinishProceduralPolygon()
{
    if (OpenZoneVertices.Num() < 3)
    {
        return false;
    }
    const int32 NewZoneIndex = UPixelRacerTrackAuthoringLibrary::AddProceduralZone(
        Session.Document, OpenZoneVertices, ActiveZonePreset, 0, 1337);
    if (NewZoneIndex == INDEX_NONE)
    {
        return false;
    }
    SelectedZoneIndex = NewZoneIndex;
    SelectedZoneVertexIndex = INDEX_NONE;
    OpenZoneVertices.Reset();
    return true;
}

bool SPixelRacerTrackCanvas::FinishProceduralFreehand()
{
    if (OpenZoneVertices.Num() < 3)
    {
        OpenZoneVertices.Reset();
        return false;
    }
    const int32 NewZoneIndex = UPixelRacerTrackAuthoringLibrary::AddProceduralZone(
        Session.Document, OpenZoneVertices, ActiveZonePreset, 0, 1337);
    OpenZoneVertices.Reset();
    if (NewZoneIndex == INDEX_NONE)
    {
        return false;
    }
    SelectedZoneIndex = NewZoneIndex;
    SelectedZoneVertexIndex = INDEX_NONE;
    return true;
}

bool SPixelRacerTrackCanvas::HitTestZoneVertex(
    const FVector2D& LocalPosition,
    int32& OutZoneIndex,
    int32& OutVertexIndex,
    const float MaxDistance) const
{
    float BestDistance = MaxDistance;
    bool bFound = false;
    for (int32 ZoneIndex = 0; ZoneIndex < Session.Document.ProceduralZones.Num(); ++ZoneIndex)
    {
        const FPixelRacerProceduralZone& Zone = Session.Document.ProceduralZones[ZoneIndex];
        for (int32 VertexIndex = 0; VertexIndex < Zone.Polygon.Num(); ++VertexIndex)
        {
            const float Distance = FVector2D::Distance(LocalPosition, Zone.Polygon[VertexIndex]);
            if (Distance <= BestDistance)
            {
                BestDistance = Distance;
                OutZoneIndex = ZoneIndex;
                OutVertexIndex = VertexIndex;
                bFound = true;
            }
        }
    }
    return bFound;
}

void SPixelRacerTrackCanvas::ClearZoneSelection()
{
    SelectedZoneIndex = INDEX_NONE;
    SelectedZoneVertexIndex = INDEX_NONE;
}

void SPixelRacerTrackCanvas::CancelOpenZone()
{
    OpenZoneVertices.Reset();
    if (DragOperation == EDragOperation::ProceduralFreehand || DragOperation == EDragOperation::ProceduralRectangle)
    {
        FinishOpenEdit();
    }
    Invalidate(EInvalidateWidgetReason::Paint);
}

void SPixelRacerTrackCanvas::FinishOpenEdit()
{
    if (Session.bEditOpen)
    {
        Session.EndEdit();
    }
    DragOperation = EDragOperation::None;
}

FReply SPixelRacerTrackCanvas::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    const FKey Button = MouseEvent.GetEffectingButton();
    if (Button != EKeys::LeftMouseButton && Button != EKeys::RightMouseButton)
    {
        return FReply::Unhandled();
    }

    const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
    bEraseStroke = Button == EKeys::RightMouseButton;

    if (Mode == EPixelRacerAuthoringMode::Select || Mode == EPixelRacerAuthoringMode::Spline)
    {
        int32 HitSpline = INDEX_NONE;
        int32 HitPoint = INDEX_NONE;

        if (Button == EKeys::RightMouseButton && HitTestRoadPoint(Local, HitSpline, HitPoint))
        {
            SelectRoadPoint(HitSpline, HitPoint);
            DeleteSelectedRoadPoint();
            return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
        }

        if (Button == EKeys::LeftMouseButton && HitTestSelectedWidthHandle(Local))
        {
            Session.BeginEdit();
            DragOperation = EDragOperation::RoadWidth;
            return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
        }

        if (Button == EKeys::LeftMouseButton && HitTestRoadPoint(Local, HitSpline, HitPoint))
        {
            SelectRoadPoint(HitSpline, HitPoint);
            Session.BeginEdit();
            DragOperation = EDragOperation::RoadPoint;
            return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
        }

        if (Button == EKeys::LeftMouseButton && Mode == EPixelRacerAuthoringMode::Spline)
        {
            if (MouseEvent.IsShiftDown())
            {
                int32 SplineIndex = INDEX_NONE;
                int32 InsertBefore = INDEX_NONE;
                FVector InsertLocation = FVector::ZeroVector;
                if (FindNearestRoadSegment(Local, SplineIndex, InsertBefore, InsertLocation))
                {
                    Session.BeginEdit();
                    const bool bInserted = UPixelRacerTrackAuthoringLibrary::InsertRoadControlPoint(
                        Session.Document, SplineIndex, InsertBefore, InsertLocation, RoadWidth, ActiveSurface, false, GridSize);
                    Session.EndEdit();
                    if (bInserted)
                    {
                        SelectRoadPoint(SplineIndex, InsertBefore);
                    }
                    Invalidate(EInvalidateWidgetReason::Paint);
                    return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
                }
            }

            Session.BeginEdit();
            const bool bAdded = UPixelRacerTrackAuthoringLibrary::AddRoadControlPoint(
                Session.Document, 0, CanvasToTrack(Local), RoadWidth, ActiveSurface, false, GridSize);
            Session.EndEdit();
            if (bAdded)
            {
                SelectRoadPoint(0, Session.Document.RoadSplines[0].ControlPoints.Num() - 1);
            }
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
        }

        ClearRoadPointSelection();
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    if (Mode == EPixelRacerAuthoringMode::TilePaint)
    {
        Session.BeginEdit();
        DragOperation = EDragOperation::TileStroke;
        ApplyTileAt(Local, bEraseStroke);
        return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    if (Mode == EPixelRacerAuthoringMode::PiecePlacement)
    {
        Session.BeginEdit();
        ApplyPieceAt(Local, bEraseStroke);
        Session.EndEdit();
        return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    if (Mode == EPixelRacerAuthoringMode::ProceduralZone)
    {
        if (Button == EKeys::RightMouseButton)
        {
            CancelOpenZone();
            ClearZoneSelection();
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled().ReleaseMouseCapture().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
        }

        int32 HitZone = INDEX_NONE;
        int32 HitVertex = INDEX_NONE;
        if (HitTestZoneVertex(Local, HitZone, HitVertex))
        {
            SelectedZoneIndex = HitZone;
            SelectedZoneVertexIndex = HitVertex;
            Session.BeginEdit();
            DragOperation = EDragOperation::ProceduralVertex;
            return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
        }

        ClearZoneSelection();
        if (ZoneDrawMode == EPixelRacerZoneDrawMode::Polygon)
        {
            const FVector TrackPoint = CanvasToTrack(Local);
            OpenZoneVertices.Add(FVector2D(TrackPoint.X, TrackPoint.Y));
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
        }

        Session.BeginEdit();
        if (ZoneDrawMode == EPixelRacerZoneDrawMode::Freehand)
        {
            OpenZoneVertices.Reset();
            const FVector TrackPoint = CanvasToTrack(Local);
            OpenZoneVertices.Add(FVector2D(TrackPoint.X, TrackPoint.Y));
            DragOperation = EDragOperation::ProceduralFreehand;
        }
        else
        {
            ZoneStart = CanvasToTrack(Local);
            ZoneCurrent = ZoneStart;
            DragOperation = EDragOperation::ProceduralRectangle;
        }
        return FReply::Handled().CaptureMouse(SharedThis(this)).SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    return FReply::Unhandled();
}

FReply SPixelRacerTrackCanvas::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    if (DragOperation == EDragOperation::None)
    {
        return FReply::Unhandled();
    }

    if (DragOperation == EDragOperation::ProceduralRectangle)
    {
        ZoneCurrent = CanvasToTrack(MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()));
        CreateProceduralRectangle();
    }
    else if (DragOperation == EDragOperation::ProceduralFreehand)
    {
        FinishProceduralFreehand();
    }

    Session.EndEdit();
    DragOperation = EDragOperation::None;
    Invalidate(EInvalidateWidgetReason::Paint);
    return FReply::Handled().ReleaseMouseCapture();
}

FReply SPixelRacerTrackCanvas::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
    const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());

    if (DragOperation == EDragOperation::TileStroke)
    {
        ApplyTileAt(Local, bEraseStroke);
        return FReply::Handled();
    }

    if (DragOperation == EDragOperation::RoadPoint && HasSelectedRoadPoint())
    {
        UPixelRacerTrackAuthoringLibrary::MoveRoadControlPoint(
            Session.Document, Session.SelectedSplineIndex, Session.SelectedPointIndex, CanvasToTrack(Local), false, GridSize);
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    if (DragOperation == EDragOperation::RoadWidth && HasSelectedRoadPoint())
    {
        const FPixelRacerTrackControlPoint* Point = GetSelectedRoadPoint();
        const float Width = Point ? FVector2D::Distance(Local, TrackToCanvas(Point->Location)) * 2.0f : RoadWidth;
        UPixelRacerTrackAuthoringLibrary::SetRoadControlPointWidth(
            Session.Document, Session.SelectedSplineIndex, Session.SelectedPointIndex, FMath::Max(16.0f, Width));
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    if (DragOperation == EDragOperation::ProceduralRectangle)
    {
        ZoneCurrent = CanvasToTrack(Local);
        Invalidate(EInvalidateWidgetReason::Paint);
        return FReply::Handled();
    }

    if (DragOperation == EDragOperation::ProceduralFreehand)
    {
        const FVector TrackPoint = CanvasToTrack(Local);
        const FVector2D NextPoint(TrackPoint.X, TrackPoint.Y);
        if (OpenZoneVertices.Num() == 0 || FVector2D::Distance(OpenZoneVertices.Last(), NextPoint) >= GridSize * 0.35f)
        {
            OpenZoneVertices.Add(NextPoint);
            Invalidate(EInvalidateWidgetReason::Paint);
        }
        return FReply::Handled();
    }

    if (DragOperation == EDragOperation::ProceduralVertex &&
        Session.Document.ProceduralZones.IsValidIndex(SelectedZoneIndex))
    {
        const FPixelRacerProceduralZone& Zone = Session.Document.ProceduralZones[SelectedZoneIndex];
        if (Zone.Polygon.IsValidIndex(SelectedZoneVertexIndex))
        {
            TArray<FVector2D> UpdatedPolygon = Zone.Polygon;
            const FVector TrackPoint = CanvasToTrack(Local);
            UpdatedPolygon[SelectedZoneVertexIndex] = FVector2D(TrackPoint.X, TrackPoint.Y);
            UPixelRacerTrackAuthoringLibrary::UpdateProceduralZonePolygon(Session.Document, SelectedZoneIndex, UpdatedPolygon);
            Invalidate(EInvalidateWidgetReason::Paint);
            return FReply::Handled();
        }
    }

    return FReply::Unhandled();
}

FReply SPixelRacerTrackCanvas::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Mode == EPixelRacerAuthoringMode::TilePaint && Key == EKeys::LeftBracket)
    {
        CycleActiveTileIndex(-1);
        return FReply::Handled();
    }
    if (Mode == EPixelRacerAuthoringMode::TilePaint && Key == EKeys::RightBracket)
    {
        CycleActiveTileIndex(1);
        return FReply::Handled();
    }
    if (InKeyEvent.IsControlDown() && Key == EKeys::Z)
    {
        const bool bRedo = InKeyEvent.IsShiftDown();
        return (bRedo ? Redo() : Undo()) ? FReply::Handled() : FReply::Unhandled();
    }
    if (InKeyEvent.IsControlDown() && Key == EKeys::Y)
    {
        return Redo() ? FReply::Handled() : FReply::Unhandled();
    }
    if (Mode == EPixelRacerAuthoringMode::ProceduralZone)
    {
        if (Key == EKeys::Enter && ZoneDrawMode == EPixelRacerZoneDrawMode::Polygon && OpenZoneVertices.Num() >= 3)
        {
            Session.BeginEdit();
            const bool bCreated = FinishProceduralPolygon();
            Session.EndEdit();
            if (bCreated)
            {
                Invalidate(EInvalidateWidgetReason::Paint);
                return FReply::Handled();
            }
        }
        if (Key == EKeys::Escape)
        {
            CancelOpenZone();
            ClearZoneSelection();
            return FReply::Handled().ReleaseMouseCapture();
        }
        if (Key == EKeys::Delete || Key == EKeys::BackSpace)
        {
            return DeleteSelectedZone() ? FReply::Handled() : FReply::Unhandled();
        }
    }
    if (Key == EKeys::Delete || Key == EKeys::BackSpace)
    {
        return DeleteSelectedRoadPoint() ? FReply::Handled() : FReply::Unhandled();
    }
    return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}


FReply SPixelRacerTrackCanvas::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    const TSharedPtr<FPixelRacerAssetDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FPixelRacerAssetDragDropOp>();
    if (!AssetOp.IsValid())
    {
        return FReply::Unhandled();
    }

    const bool bPlaceable = AssetOp->AssetRole == TEXT("tileset") || AssetOp->AssetRole == TEXT("environment_piece");
    AssetOp->CurrentHoverText = bPlaceable
        ? FText::FromString(FString::Printf(TEXT("Place %s"), *AssetOp->AssetId))
        : FText::FromString(TEXT("Select in browser; this asset type is not canvas-placeable yet"));
    return bPlaceable ? FReply::Handled() : FReply::Unhandled();
}

FReply SPixelRacerTrackCanvas::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
    const TSharedPtr<FPixelRacerAssetDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FPixelRacerAssetDragDropOp>();
    if (!AssetOp.IsValid())
    {
        return FReply::Unhandled();
    }

    const FVector2D Local = MyGeometry.AbsoluteToLocal(DragDropEvent.GetScreenSpacePosition());
    if (AssetOp->AssetRole == TEXT("tileset"))
    {
        SetActiveAsset(AssetOp->AssetId, AssetOp->AssetRole);
        Session.BeginEdit();
        ApplyTileAt(Local, false);
        Session.EndEdit();
        return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }
    if (AssetOp->AssetRole == TEXT("environment_piece"))
    {
        SetActiveAsset(AssetOp->AssetId, AssetOp->AssetRole);
        Session.BeginEdit();
        ApplyPieceAt(Local, false);
        Session.EndEdit();
        return FReply::Handled().SetUserFocus(SharedThis(this), EFocusCause::Mouse);
    }

    return FReply::Unhandled();
}

int32 SPixelRacerTrackCanvas::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
    FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
    const FVector2D Size = AllottedGeometry.GetLocalSize();
    const FLinearColor GridColor(1.0f, 1.0f, 1.0f, 0.05f);

    for (float X = 0.0f; X < Size.X; X += GridSize)
    {
        TArray<FVector2D> Line{FVector2D(X, 0.0f), FVector2D(X, Size.Y)};
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, GridColor, true, 1.0f);
    }
    for (float Y = 0.0f; Y < Size.Y; Y += GridSize)
    {
        TArray<FVector2D> Line{FVector2D(0.0f, Y), FVector2D(Size.X, Y)};
        FSlateDrawElement::MakeLines(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, GridColor, true, 1.0f);
    }

    int32 DrawLayer = LayerId + 1;
    for (const FPixelRacerTilePlacement& Tile : Session.Document.Tiles)
    {
        const FVector2D Position(Tile.Cell.X * GridSize, Tile.Cell.Y * GridSize);
        const FPixelRacerAssetPreviewData* Preview = AssetPreviewData.Find(Tile.AssetId);
        if (Preview != nullptr && Preview->TileWidth > 0 && Preview->TileHeight > 0 && Preview->Columns > 0 && Preview->Rows > 0)
        {
            const TSharedPtr<FSlateDynamicImageBrush> SourceBrush = GetPreviewSourceBrush(Tile.AssetId);
            UPaperTileSet* TileSet = LoadPreviewTileSet(Tile.AssetId);
            UTexture2D* Texture = TileSet != nullptr ? TileSet->GetTileSheetTexture() : nullptr;
            FIntPoint TileSize(Preview->TileWidth, Preview->TileHeight);
            const int32 TileCount = TileSet != nullptr ? TileSet->GetTileCount() : Preview->TileCount;
            const int32 SafeTileIndex = FMath::Clamp(Tile.TileIndex, 0, FMath::Max(TileCount - 1, 0));
            FIntPoint SourceUV(
                (SafeTileIndex % Preview->Columns) * Preview->TileWidth,
                (SafeTileIndex / Preview->Columns) * Preview->TileHeight);
            if (TileSet != nullptr && SafeTileIndex < TileSet->GetTileCount())
            {
                TileSize = TileSet->GetTileSize();
                FVector2D TileUV = FVector2D::ZeroVector;
                if (TileSet->GetTileUV(SafeTileIndex, TileUV))
                {
                    SourceUV = FIntPoint(FMath::RoundToInt(TileUV.X), FMath::RoundToInt(TileUV.Y));
                }
            }

            const FIntPoint TextureSize = Texture != nullptr
                ? Texture->GetImportedSize()
                : FIntPoint(Preview->Width, Preview->Height);
            FSlateBrush Brush = PixelRacerCanvas::MakeSourceRegionBrush(
                SourceBrush.Get(),
                Texture,
                SourceUV,
                TileSize,
                TextureSize);
            Brush.SetImageSize(FVector2D(GridSize, GridSize));
            const float Angle = FMath::DegreesToRadians(static_cast<float>(Tile.RotationSteps * 90));
            if (Texture != nullptr || SourceBrush.IsValid())
            {
                FSlateDrawElement::MakeRotatedBox(
                    OutDrawElements,
                    DrawLayer,
                    AllottedGeometry.ToPaintGeometry(FVector2D(GridSize, GridSize), FSlateLayoutTransform(Position)),
                    &Brush,
                    ESlateDrawEffect::None,
                    Angle);
            }
            else
            {
                FSlateDrawElement::MakeBox(OutDrawElements, DrawLayer, AllottedGeometry.ToPaintGeometry(FVector2D(GridSize, GridSize), FSlateLayoutTransform(Position)),
                    FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.17f, 0.20f, 0.24f, 0.9f));
            }
        }
        else
        {
            FSlateDrawElement::MakeBox(OutDrawElements, DrawLayer, AllottedGeometry.ToPaintGeometry(FVector2D(GridSize, GridSize), FSlateLayoutTransform(Position)),
                FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(0.17f, 0.20f, 0.24f, 0.9f));
        }
    }

    for (int32 SplineIndex = 0; SplineIndex < Session.Document.RoadSplines.Num(); ++SplineIndex)
    {
        const FPixelRacerRoadSpline& Spline = Session.Document.RoadSplines[SplineIndex];
        TArray<FVector2D> Points;
        for (const FPixelRacerTrackControlPoint& Point : Spline.ControlPoints)
        {
            Points.Add(TrackToCanvas(Point.Location));
        }
        if (Spline.bClosedLoop && Points.Num() > 2)
        {
            // TArray rejects adding a reference into itself, even with spare capacity.
            const FVector2D FirstPoint = Points[0];
            Points.Add(FirstPoint);
        }
        if (Points.Num() > 1)
        {
            FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer + 1, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
                FLinearColor(0.72f, 0.72f, 0.74f, 1.0f), true, 8.0f);
        }

        for (int32 PointIndex = 0; PointIndex < Spline.ControlPoints.Num(); ++PointIndex)
        {
            const FVector2D P = TrackToCanvas(Spline.ControlPoints[PointIndex].Location) - FVector2D(5.0f, 5.0f);
            const bool bSelected = SplineIndex == Session.SelectedSplineIndex && PointIndex == Session.SelectedPointIndex;
            FSlateDrawElement::MakeBox(OutDrawElements, DrawLayer + 2, AllottedGeometry.ToPaintGeometry(FVector2D(10.0f, 10.0f), FSlateLayoutTransform(P)),
                FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, bSelected ? FLinearColor(1.0f, 0.75f, 0.1f, 1.0f) : FLinearColor(0.82f, 0.82f, 0.86f, 1.0f));
        }
    }

    if (HasSelectedRoadPoint())
    {
        const FPixelRacerTrackControlPoint* Point = GetSelectedRoadPoint();
        const FVector2D Center = Point ? TrackToCanvas(Point->Location) : FVector2D::ZeroVector;
        const FVector2D Handle = GetSelectedWidthHandlePosition();
        TArray<FVector2D> WidthLine{Center, Handle};
        FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer + 3, AllottedGeometry.ToPaintGeometry(), WidthLine, ESlateDrawEffect::None,
            FLinearColor(1.0f, 0.55f, 0.1f, 0.95f), true, 2.0f);
        FSlateDrawElement::MakeBox(OutDrawElements, DrawLayer + 4,
            AllottedGeometry.ToPaintGeometry(FVector2D(10.0f, 10.0f), FSlateLayoutTransform(Handle - FVector2D(5.0f, 5.0f))),
            FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, FLinearColor(1.0f, 0.55f, 0.1f, 1.0f));
    }

    for (const FPixelRacerPiecePlacement& Piece : Session.Document.Pieces)
    {
        const FPixelRacerAssetPreviewData* Preview = AssetPreviewData.Find(Piece.AssetId);
        const TSharedPtr<FSlateDynamicImageBrush> SourceBrush = GetPreviewSourceBrush(Piece.AssetId);
        UPaperSprite* Sprite = LoadPreviewSprite(Piece.AssetId);
        UTexture2D* Texture = Sprite != nullptr ? Sprite->GetSourceTexture() : nullptr;
        if (Preview != nullptr && Preview->Width > 0 && Preview->Height > 0)
        {
            FIntPoint SourceUV(0, 0);
            FIntPoint SourceSize(Preview->Width, Preview->Height);
            if (Sprite != nullptr)
            {
                const FVector2D SpriteUV = Sprite->GetSourceUV();
                const FVector2D SpriteSize = Sprite->GetSourceSize();
                SourceUV = FIntPoint(FMath::RoundToInt(SpriteUV.X), FMath::RoundToInt(SpriteUV.Y));
                SourceSize = FIntPoint(FMath::RoundToInt(SpriteSize.X), FMath::RoundToInt(SpriteSize.Y));
            }
            const FIntPoint TextureSize = Texture != nullptr
                ? Texture->GetImportedSize()
                : FIntPoint(Preview->Width, Preview->Height);
            const FVector2D DrawSize = FVector2D(SourceSize) * (GridSize / 64.0f) * FVector2D(
                FMath::Abs(Piece.Transform.GetScale3D().X),
                FMath::Abs(Piece.Transform.GetScale3D().Y));
            const FVector2D Position = TrackToCanvas(Piece.Transform.GetLocation()) - DrawSize * 0.5f;
            FSlateBrush Brush = PixelRacerCanvas::MakeSourceRegionBrush(
                SourceBrush.Get(),
                Texture,
                SourceUV,
                SourceSize,
                TextureSize);
            Brush.SetImageSize(DrawSize);
            const float Angle = FMath::DegreesToRadians(Piece.Transform.Rotator().Yaw);
            if (Texture != nullptr || SourceBrush.IsValid())
            {
                FSlateDrawElement::MakeRotatedBox(
                    OutDrawElements,
                    DrawLayer + 5,
                    AllottedGeometry.ToPaintGeometry(DrawSize, FSlateLayoutTransform(Position)),
                    &Brush,
                    ESlateDrawEffect::None,
                    Angle);
            }
            else
            {
                const FVector2D P = TrackToCanvas(Piece.Transform.GetLocation()) - FVector2D(5.0f, 5.0f);
                FSlateDrawElement::MakeBox(OutDrawElements, DrawLayer + 5, AllottedGeometry.ToPaintGeometry(FVector2D(10.0f, 10.0f), FSlateLayoutTransform(P)),
                    FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, Piece.bGenerated ? FLinearColor::Yellow : FLinearColor::Red);
            }
        }
        else
        {
            const FVector2D P = TrackToCanvas(Piece.Transform.GetLocation()) - FVector2D(5.0f, 5.0f);
            FSlateDrawElement::MakeBox(OutDrawElements, DrawLayer + 5, AllottedGeometry.ToPaintGeometry(FVector2D(10.0f, 10.0f), FSlateLayoutTransform(P)),
                FAppStyle::GetBrush("WhiteBrush"), ESlateDrawEffect::None, Piece.bGenerated ? FLinearColor::Yellow : FLinearColor::Red);
        }
    }

    for (int32 ZoneIndex = 0; ZoneIndex < Session.Document.ProceduralZones.Num(); ++ZoneIndex)
    {
        const FPixelRacerProceduralZone& Zone = Session.Document.ProceduralZones[ZoneIndex];
        TArray<FVector2D> Polygon = Zone.Polygon;
        const bool bSelectedZone = ZoneIndex == SelectedZoneIndex;
        if (Polygon.Num() > 2)
        {
            const FVector2D FirstVertex = Polygon[0];
            Polygon.Add(FirstVertex);
            FSlateDrawElement::MakeLines(
                OutDrawElements,
                DrawLayer + 6,
                AllottedGeometry.ToPaintGeometry(),
                Polygon,
                ESlateDrawEffect::None,
                bSelectedZone ? FLinearColor(1.0f, 0.72f, 0.12f, 0.95f) : FLinearColor(0.25f, 0.9f, 0.45f, 0.75f),
                true,
                bSelectedZone ? 3.0f : 2.0f);
        }

        if (bSelectedZone)
        {
            for (int32 VertexIndex = 0; VertexIndex < Zone.Polygon.Num(); ++VertexIndex)
            {
                const bool bSelectedVertex = VertexIndex == SelectedZoneVertexIndex;
                const FVector2D P = Zone.Polygon[VertexIndex] - FVector2D(4.0f, 4.0f);
                FSlateDrawElement::MakeBox(
                    OutDrawElements,
                    DrawLayer + 7,
                    AllottedGeometry.ToPaintGeometry(FVector2D(8.0f, 8.0f), FSlateLayoutTransform(P)),
                    FAppStyle::GetBrush("WhiteBrush"),
                    ESlateDrawEffect::None,
                    bSelectedVertex ? FLinearColor(1.0f, 0.45f, 0.08f, 1.0f) : FLinearColor(1.0f, 0.82f, 0.3f, 1.0f));
            }
        }
    }

    if (DragOperation == EDragOperation::ProceduralRectangle)
    {
        const FVector2D A(ZoneStart.X, ZoneStart.Y);
        const FVector2D B(ZoneCurrent.X, ZoneCurrent.Y);
        TArray<FVector2D> Preview{A, FVector2D(B.X, A.Y), B, FVector2D(A.X, B.Y), A};
        FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer + 8, AllottedGeometry.ToPaintGeometry(), Preview, ESlateDrawEffect::None,
            FLinearColor(0.35f, 1.0f, 0.55f, 0.95f), true, 2.0f);
    }

    if (OpenZoneVertices.Num() > 1)
    {
        FSlateDrawElement::MakeLines(
            OutDrawElements,
            DrawLayer + 8,
            AllottedGeometry.ToPaintGeometry(),
            OpenZoneVertices,
            ESlateDrawEffect::None,
            FLinearColor(0.35f, 1.0f, 0.55f, 0.95f),
            true,
            2.0f);
        for (const FVector2D& Vertex : OpenZoneVertices)
        {
            FSlateDrawElement::MakeBox(
                OutDrawElements,
                DrawLayer + 9,
                AllottedGeometry.ToPaintGeometry(FVector2D(6.0f, 6.0f), FSlateLayoutTransform(Vertex - FVector2D(3.0f, 3.0f))),
                FAppStyle::GetBrush("WhiteBrush"),
                ESlateDrawEffect::None,
                FLinearColor(0.35f, 1.0f, 0.55f, 1.0f));
        }
    }

    for (const FPixelRacerRacingLine& Line : Session.Document.RacingLines)
    {
        TArray<FVector2D> Points;
        for (const FPixelRacerRacingLinePoint& Point : Line.Points)
        {
            Points.Add(TrackToCanvas(Point.Location));
        }
        if (Points.Num() > 1)
        {
            FSlateDrawElement::MakeLines(OutDrawElements, DrawLayer + 10, AllottedGeometry.ToPaintGeometry(), Points, ESlateDrawEffect::None,
                FLinearColor(0.15f, 0.75f, 1.0f, 0.8f), true, 2.0f);
        }
    }

    return DrawLayer + 11;
}
