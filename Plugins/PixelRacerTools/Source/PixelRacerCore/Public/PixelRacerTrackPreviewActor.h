#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PixelRacerTrackTypes.h"
#include "PixelRacerTrackPreviewActor.generated.h"

class UCameraComponent;
class UPaperSprite;
class UPaperTileSet;
class UStaticMesh;
class USceneComponent;
class APixelRacerArcadeVehiclePawn;

struct PIXELRACERCORE_API FPixelRacerTrackPreviewAssetReference
{
    FString AssetId;
    FString DisplayName;
    FSoftObjectPath SpriteObjectPath;
    FSoftObjectPath TileSetObjectPath;
    FSoftObjectPath VehicleDefinitionObjectPath;
    FIntPoint TileSize = FIntPoint(32, 32);
    FIntPoint ImageSize = FIntPoint(64, 64);
    int32 DirectionCount = 0;
};

/** Transient world representation of a TrackDocument for the editor's PIE preview. */
UCLASS(Transient, NotBlueprintable)
class PIXELRACERCORE_API APixelRacerTrackPreviewActor final : public AActor
{
    GENERATED_BODY()

public:
    APixelRacerTrackPreviewActor();

    /** Builds a non-colliding, visual preview. Missing imported assets use simple mesh placeholders. */
    bool BuildPreview(
        const FPixelRacerTrackDocument& Document,
        const TArray<FPixelRacerTrackPreviewAssetReference>& AssetReferences,
        FString& OutWarning);

    APixelRacerArcadeVehiclePawn* SpawnPlayerVehicle(
        const FPixelRacerTrackDocument& Document,
        const FPixelRacerTrackPreviewAssetReference& VehicleReference,
        FString& OutWarning);

    FBox GetPreviewBounds() const { return PreviewBounds; }

private:
    UPROPERTY(Transient)
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(Transient)
    TObjectPtr<UCameraComponent> PreviewCamera;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMesh> PreviewPlaneMesh;

    FBox PreviewBounds = FBox(ForceInit);

    void BuildRoads(const FPixelRacerTrackDocument& Document, int32& OutRoadSegmentCount);
    void BuildTiles(
        const FPixelRacerTrackDocument& Document,
        const TMap<FString, FPixelRacerTrackPreviewAssetReference>& AssetsById,
        int32& OutRenderedTileCount,
        int32& OutPlaceholderTileCount);
    void BuildPieces(
        const FPixelRacerTrackDocument& Document,
        const TMap<FString, FPixelRacerTrackPreviewAssetReference>& AssetsById,
        int32& OutRenderedPieceCount,
        int32& OutPlaceholderPieceCount);
    void UpdatePreviewCamera();
    void IncludePointInBounds(const FVector& Point, float Padding = 0.0f);
};
