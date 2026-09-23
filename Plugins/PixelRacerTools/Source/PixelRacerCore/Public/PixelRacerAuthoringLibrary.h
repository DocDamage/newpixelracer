#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PixelRacerTrackTypes.h"
#include "PixelRacerAuthoringLibrary.generated.h"

UCLASS()
class PIXELRACERCORE_API UPixelRacerAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring")
    static void EnsureDefaultLayers(UPARAM(ref) FPixelRacerTrackDocument& Document);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Road")
    static int32 AddRoadSpline(UPARAM(ref) FPixelRacerTrackDocument& Document, const FString& Name, bool bClosedLoop);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Road")
    static bool AddRoadPoint(UPARAM(ref) FPixelRacerTrackDocument& Document, int32 SplineIndex, FVector Location, float Width, const FString& SurfaceId);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Road")
    static bool MoveRoadPoint(UPARAM(ref) FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 PointIndex, FVector NewLocation);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Road")
    static bool SetRoadPointWidth(UPARAM(ref) FPixelRacerTrackDocument& Document, int32 SplineIndex, int32 PointIndex, float NewWidth);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Tile")
    static bool PaintTile(UPARAM(ref) FPixelRacerTrackDocument& Document, FIntPoint Grid, const FString& TilesetId, int32 TileIndex, int32 LayerIndex, float RotationDegrees = 0.0f);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Tile")
    static bool EraseTile(UPARAM(ref) FPixelRacerTrackDocument& Document, FIntPoint Grid, int32 LayerIndex);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Piece")
    static FGuid PlacePiece(UPARAM(ref) FPixelRacerTrackDocument& Document, const FString& AssetId, const FTransform& Transform, int32 LayerIndex);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Piece")
    static int32 ErasePiecesInRadius(UPARAM(ref) FPixelRacerTrackDocument& Document, FVector Location, float Radius, int32 LayerIndex = -1);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Bake")
    static int32 BakeRoadSplineToPieces(UPARAM(ref) FPixelRacerTrackDocument& Document, int32 SplineIndex, const FString& PieceAssetId, float Spacing, int32 LayerIndex, bool bReplacePreviousGenerated = true);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Bake")
    static int32 BakeRoadSplineToTiles(UPARAM(ref) FPixelRacerTrackDocument& Document, int32 SplineIndex, const FString& TilesetId, int32 TileIndex, int32 LayerIndex, bool bReplacePreviousGenerated = true);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Race")
    static int32 GenerateCheckpoints(UPARAM(ref) FPixelRacerTrackDocument& Document, int32 SplineIndex = 0, float Spacing = 550.0f);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Race")
    static int32 GenerateGridSlots(UPARAM(ref) FPixelRacerTrackDocument& Document, int32 SplineIndex = 0, int32 SlotCount = 8);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|AI")
    static int32 GenerateRacingLines(UPARAM(ref) FPixelRacerTrackDocument& Document, int32 SplineIndex = 0);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Procedural")
    static int32 RegenerateProceduralZone(UPARAM(ref) FPixelRacerTrackDocument& Document, int32 ZoneIndex, const TArray<FString>& AssetIds);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Authoring|Procedural")
    static bool GenerateCompleteCircuit(UPARAM(ref) FPixelRacerTrackDocument& Document, FVector2D Center, float Radius, int32 ControlPointCount, int32 Seed, float RoadWidth, const FString& SurfaceId);
};
