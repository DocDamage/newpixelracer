#pragma once

#include "CoreMinimal.h"
#include "PixelRacerTrackEditorSession.h"
#include "PixelRacerTrackTypes.h"
#include "SPixelRacerAssetBrowser.h"
#include "Widgets/SCompoundWidget.h"

class UPaperSprite;
class UPaperTileSet;

enum class EPixelRacerZoneDrawMode : uint8
{
    Rectangle,
    Polygon,
    Freehand
};

class SPixelRacerTrackCanvas : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPixelRacerTrackCanvas) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    void SetMode(EPixelRacerAuthoringMode InMode);
    EPixelRacerAuthoringMode GetMode() const { return Mode; }
    FPixelRacerTrackDocument& GetDocument() { return Session.Document; }
    const FPixelRacerTrackDocument& GetDocument() const { return Session.Document; }
    void ResetDocument();
    void GenerateAI();
    int32 BakeRoad();
    bool Undo();
    bool Redo();
    bool LoadSandboxTrack(FString& OutError);

    bool HasSelectedRoadPoint() const;
    FText GetSelectedRoadPointSummary() const;
    bool SetSelectedRoadPointSurface(const FString& SurfaceId);
    bool AdjustSelectedRoadPointElevation(int32 Delta);
    bool DeleteSelectedRoadPoint();
    void SetActiveZonePreset(const FString& InPreset) { ActiveZonePreset = InPreset; }
    FString GetActiveZonePreset() const { return ActiveZonePreset; }
    void SetZoneDrawMode(EPixelRacerZoneDrawMode InMode);
    EPixelRacerZoneDrawMode GetZoneDrawMode() const { return ZoneDrawMode; }
    FString GetZoneDrawModeName() const;
    bool DeleteSelectedZone();
    bool SetActiveAsset(const FString& AssetId, const FString& Role);
    void SetAssetPreviewData(const TArray<FPixelRacerAssetPreviewData>& InPreviewData);
    FString GetActiveAssetSummary() const;

    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
    virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
    virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
    virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

private:
    enum class EDragOperation : uint8
    {
        None,
        RoadPoint,
        RoadWidth,
        TileStroke,
        ProceduralRectangle,
        ProceduralFreehand,
        ProceduralVertex
    };

    void ApplyTileAt(const FVector2D& LocalPosition, bool bErase);
    void ApplyPieceAt(const FVector2D& LocalPosition, bool bErase);
    FVector CanvasToTrack(const FVector2D& LocalPosition) const;
    FVector2D TrackToCanvas(const FVector& TrackPosition) const;

    bool HitTestRoadPoint(const FVector2D& LocalPosition, int32& OutSplineIndex, int32& OutPointIndex, float MaxDistance = 12.0f) const;
    bool HitTestSelectedWidthHandle(const FVector2D& LocalPosition) const;
    bool FindNearestRoadSegment(const FVector2D& LocalPosition, int32& OutSplineIndex, int32& OutInsertBeforeIndex, FVector& OutTrackLocation, float MaxDistance = 28.0f) const;
    FVector2D GetRoadPointNormal(int32 SplineIndex, int32 PointIndex) const;
    FVector2D GetSelectedWidthHandlePosition() const;
    FPixelRacerTrackControlPoint* GetSelectedRoadPoint();
    const FPixelRacerTrackControlPoint* GetSelectedRoadPoint() const;
    void SelectRoadPoint(int32 SplineIndex, int32 PointIndex);
    void ClearRoadPointSelection();
    bool CreateProceduralRectangle();
    bool FinishProceduralPolygon();
    bool FinishProceduralFreehand();
    bool HitTestZoneVertex(const FVector2D& LocalPosition, int32& OutZoneIndex, int32& OutVertexIndex, float MaxDistance = 10.0f) const;
    int32 GetActiveTileCount() const;
    void CycleActiveTileIndex(int32 Delta);
    TSharedPtr<FSlateDynamicImageBrush> GetPreviewSourceBrush(const FString& AssetId) const;
    UPaperSprite* LoadPreviewSprite(const FString& AssetId) const;
    UPaperTileSet* LoadPreviewTileSet(const FString& AssetId) const;
    void ClearZoneSelection();
    void CancelOpenZone();
    void FinishOpenEdit();

    EPixelRacerAuthoringMode Mode = EPixelRacerAuthoringMode::Spline;
    FPixelRacerTrackEditorSession Session;
    EDragOperation DragOperation = EDragOperation::None;
    bool bEraseStroke = false;
    float GridSize = 32.0f;
    float RoadWidth = 160.0f;
    FString ActiveSurface = TEXT("asphalt");
    FString ActiveTile = TEXT("Tilesets/race_track_1.png");
    FString ActivePiece = TEXT("Enviroment/barrier_red.png");
    FString LastSelectedAsset = TEXT("Tilesets/race_track_1.png");
    FString LastSelectedAssetRole = TEXT("tileset");
    TMap<FString, FPixelRacerAssetPreviewData> AssetPreviewData;
    mutable TMap<FString, TSharedPtr<FSlateDynamicImageBrush>> LoadedSourceBrushCache;
    mutable TMap<FString, TWeakObjectPtr<UPaperSprite>> LoadedSpriteCache;
    mutable TMap<FString, TWeakObjectPtr<UPaperTileSet>> LoadedTileSetCache;
    FString ActiveZonePreset = TEXT("Grassland");
    EPixelRacerZoneDrawMode ZoneDrawMode = EPixelRacerZoneDrawMode::Rectangle;
    int32 SelectedZoneIndex = INDEX_NONE;
    int32 SelectedZoneVertexIndex = INDEX_NONE;
    TArray<FVector2D> OpenZoneVertices;
    FVector ZoneStart = FVector::ZeroVector;
    FVector ZoneCurrent = FVector::ZeroVector;
};
