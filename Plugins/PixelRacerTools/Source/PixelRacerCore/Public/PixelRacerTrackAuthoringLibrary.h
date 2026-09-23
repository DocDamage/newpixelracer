#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PixelRacerTrackTypes.h"
#include "PixelRacerTrackAuthoringLibrary.generated.h"

UCLASS()
class PIXELRACERCORE_API UPixelRacerTrackAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="Pixel Racer|Authoring")
    static FVector SnapLocationToGrid(const FVector& Location, float GridSize);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Spline")
    static int32 AddRoadSpline(FPixelRacerTrackDocument& Document, const FString& Name, bool bClosedLoop);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Spline")
    static bool AddRoadControlPoint(FPixelRacerTrackDocument& Document, int32 SplineIndex, const FVector& Location, float Width, const FString& SurfaceId, bool bSnapToGrid = false, float GridSize = 16.0f);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Spline")
    static bool MoveRoadControlPoint(FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 PointIndex, const FVector& NewLocation, bool bSnapToGrid = false, float GridSize = 16.0f);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Spline")
    static bool InsertRoadControlPoint(FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 InsertBeforeIndex, const FVector& Location, float Width, const FString& SurfaceId, bool bSnapToGrid = false, float GridSize = 16.0f);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Spline")
    static bool DeleteRoadControlPoint(FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 PointIndex);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Spline")
    static bool SetRoadControlPointWidth(FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 PointIndex, float NewWidth);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Spline")
    static bool SetRoadControlPointSurface(FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 PointIndex, const FString& SurfaceId);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Spline")
    static bool SetRoadControlPointElevation(FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 PointIndex, int32 ElevationLayer);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Spline")
    static bool SetRoadControlPointCrossing(FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 PointIndex, bool bAllowCrossing);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Tiles")
    static bool PaintTile(FPixelRacerTrackDocument& Document, const FIntPoint& Cell, const FString& AssetId, int32 LayerIndex = 0, int32 RotationSteps = 0, int32 TileIndex = 0);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Tiles")
    static bool EraseTile(FPixelRacerTrackDocument& Document, const FIntPoint& Cell, int32 LayerIndex = 0);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Pieces")
    static FGuid PlacePiece(FPixelRacerTrackDocument& Document, const FString& AssetId, const FTransform& Transform, int32 LayerIndex = 0, bool bGenerated = false);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Pieces")
    static int32 ErasePiecesInRadius(FPixelRacerTrackDocument& Document, const FVector& Location, float Radius, int32 LayerIndex = -1);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Bake")
    static int32 BakeSplineToPieces(FPixelRacerTrackDocument& Document, int32 SplineIndex, const FString& PieceAssetId, float Spacing = 32.0f, int32 LayerIndex = 0, bool bReplacePreviousGenerated = true);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Zones")
    static int32 AddProceduralZone(FPixelRacerTrackDocument& Document, const TArray<FVector2D>& Polygon, const FString& RulePreset, int32 LayerIndex = 0, int32 Seed = 1337);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Zones")
    static bool UpdateProceduralZonePolygon(FPixelRacerTrackDocument& Document, int32 ZoneIndex, const TArray<FVector2D>& Polygon);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Zones")
    static bool RemoveProceduralZone(FPixelRacerTrackDocument& Document, int32 ZoneIndex);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|AI")
    static int32 GenerateCheckpoints(FPixelRacerTrackDocument& Document, int32 SplineIndex = 0, float Spacing = 400.0f, bool bReplaceGenerated = true);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Race")
    static int32 GenerateGridSlots(FPixelRacerTrackDocument& Document, int32 SplineIndex = 0, int32 SlotCount = 8, bool bReplaceExisting = true);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|AI")
    static bool GenerateRacingLines(FPixelRacerTrackDocument& Document, int32 SplineIndex = 0, bool bReplaceGenerated = true);
};
