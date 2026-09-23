#include "PixelRacerTrackPreviewActor.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "PaperTileLayer.h"
#include "PaperTileMapComponent.h"
#include "PaperTileSet.h"
#include "PixelRacerArcadeVehiclePawn.h"
#include "PixelRacerVehicleDefinition.h"
#include "UObject/ConstructorHelpers.h"

namespace PixelRacerTrackPreview
{
    constexpr float GridCellSize = 32.0f;
    constexpr float TileLayerSeparation = 12.0f;
    constexpr float RoadLayerSeparation = 64.0f;
    constexpr int32 MaxTileMapDimension = 1024;

    struct FTileForMap
    {
        const FPixelRacerTilePlacement* Placement = nullptr;
        UPaperTileSet* TileSet = nullptr;
    };

    struct FTileMapBucket
    {
        int32 LayerIndex = 0;
        FIntPoint TileSize = FIntPoint::ZeroValue;
        TArray<FTileForMap> Tiles;
    };

    static uint8 MakeTileTransformFlags(const FPixelRacerTilePlacement& Tile)
    {
        uint8 Flags = 0;
        const int32 Rotation = (Tile.RotationSteps % 4 + 4) % 4;
        switch (Rotation)
        {
        case 1: Flags = 1 | 4; break; // 90 degrees
        case 2: Flags = 1 | 2; break; // 180 degrees
        case 3: Flags = 2 | 4; break; // 270 degrees
        default: break;
        }

        if (Tile.bFlipX)
        {
            Flags ^= 1;
        }
        if (Tile.bFlipY)
        {
            Flags ^= 2;
        }
        return Flags;
    }
}

APixelRacerTrackPreviewActor::APixelRacerTrackPreviewActor()
{
    PrimaryActorTick.bCanEverTick = false;
    bFindCameraComponentWhenViewTarget = true;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
    SetRootComponent(SceneRoot);

    PreviewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("PreviewCamera"));
    PreviewCamera->SetupAttachment(SceneRoot);
    PreviewCamera->SetFieldOfView(60.0f);
    PreviewCamera->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    PreviewCamera->bAutoActivate = true;

    static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
    if (PlaneMesh.Succeeded())
    {
        PreviewPlaneMesh = PlaneMesh.Object;
    }
}

bool APixelRacerTrackPreviewActor::BuildPreview(
    const FPixelRacerTrackDocument& Document,
    const TArray<FPixelRacerTrackPreviewAssetReference>& AssetReferences,
    FString& OutWarning)
{
    OutWarning.Reset();
    PreviewBounds = FBox(ForceInit);

    TMap<FString, FPixelRacerTrackPreviewAssetReference> AssetsById;
    AssetsById.Reserve(AssetReferences.Num());
    for (const FPixelRacerTrackPreviewAssetReference& Reference : AssetReferences)
    {
        if (!Reference.AssetId.IsEmpty())
        {
            AssetsById.Add(Reference.AssetId, Reference);
        }
    }

    int32 RoadSegmentCount = 0;
    int32 RenderedTileCount = 0;
    int32 PlaceholderTileCount = 0;
    int32 RenderedPieceCount = 0;
    int32 PlaceholderPieceCount = 0;

    BuildRoads(Document, RoadSegmentCount);
    BuildTiles(Document, AssetsById, RenderedTileCount, PlaceholderTileCount);
    BuildPieces(Document, AssetsById, RenderedPieceCount, PlaceholderPieceCount);
    UpdatePreviewCamera();

    if (PlaceholderTileCount > 0 || PlaceholderPieceCount > 0)
    {
        OutWarning = FString::Printf(
            TEXT("Preview built: %d road segment(s), %d imported tile(s), %d tile placeholder(s), %d imported piece(s), %d piece placeholder(s). Import missing Paper2D assets for full visuals."),
            RoadSegmentCount,
            RenderedTileCount,
            PlaceholderTileCount,
            RenderedPieceCount,
            PlaceholderPieceCount);
    }
    return RoadSegmentCount > 0 || RenderedTileCount > 0 || PlaceholderTileCount > 0 || RenderedPieceCount > 0 || PlaceholderPieceCount > 0;
}

APixelRacerArcadeVehiclePawn* APixelRacerTrackPreviewActor::SpawnPlayerVehicle(
    const FPixelRacerTrackDocument& Document,
    const FPixelRacerTrackPreviewAssetReference& VehicleReference,
    FString& OutWarning)
{
    OutWarning.Reset();
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        OutWarning = TEXT("The PIE world is unavailable; the selected vehicle could not be spawned.");
        return nullptr;
    }

    UPixelRacerVehicleDefinition* VehicleDefinition = VehicleReference.VehicleDefinitionObjectPath.IsValid()
        ? Cast<UPixelRacerVehicleDefinition>(VehicleReference.VehicleDefinitionObjectPath.TryLoad())
        : nullptr;
    if (VehicleDefinition == nullptr)
    {
        OutWarning = TEXT("The selected vehicle definition is unavailable. Import its PNG in the Asset Browser first.");
        return nullptr;
    }
    if (VehicleDefinition->DirectionalSprites.Num() != VehicleReference.DirectionCount ||
        VehicleDefinition->DirectionalSprites.IsEmpty() ||
        VehicleDefinition->DirectionalSprites.Contains(nullptr))
    {
        OutWarning = FString::Printf(
            TEXT("The selected vehicle's directional sprites are incomplete. Reimport '%s' in the Asset Browser first."),
            *VehicleReference.DisplayName);
        return nullptr;
    }

    const FPixelRacerRoadSpline* StartSpline = nullptr;
    for (const FPixelRacerRoadSpline& Spline : Document.RoadSplines)
    {
        if (Spline.ControlPoints.Num() >= 2)
        {
            StartSpline = &Spline;
            break;
        }
    }
    if (StartSpline == nullptr)
    {
        OutWarning = TEXT("Add at least two points to a road before driving the current track.");
        return nullptr;
    }

    const FPixelRacerTrackControlPoint& FirstRoadPoint = StartSpline->ControlPoints[0];
    FVector SpawnLocation = FirstRoadPoint.Location;
    float SpawnYaw = FMath::RadiansToDegrees(FMath::Atan2(
        StartSpline->ControlPoints[1].Location.Y - FirstRoadPoint.Location.Y,
        StartSpline->ControlPoints[1].Location.X - FirstRoadPoint.Location.X));

    if (!Document.GridSlots.IsEmpty())
    {
        const FPixelRacerGridSlot* StartSlot = &Document.GridSlots[0];
        for (const FPixelRacerGridSlot& GridSlot : Document.GridSlots)
        {
            if (GridSlot.Order < StartSlot->Order)
            {
                StartSlot = &GridSlot;
            }
        }
        SpawnLocation = StartSlot->Location;
        SpawnYaw = StartSlot->YawDegrees;
    }
    SpawnLocation.Z += FirstRoadPoint.ElevationLayer * PixelRacerTrackPreview::RoadLayerSeparation + 24.0f;

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.ObjectFlags |= RF_Transient;
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APixelRacerArcadeVehiclePawn* VehiclePawn = World->SpawnActor<APixelRacerArcadeVehiclePawn>(
        SpawnLocation,
        FRotator(0.0f, SpawnYaw, 0.0f),
        SpawnParameters);
    if (VehiclePawn == nullptr)
    {
        OutWarning = TEXT("Unreal could not spawn the selected vehicle in the PIE world.");
        return nullptr;
    }

    VehiclePawn->VehicleDefinition = VehicleDefinition;
    VehiclePawn->SpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VehiclePawn->SpriteComponent->SetRelativeRotation(FRotator(0.0f, 0.0f, -90.0f));
    return VehiclePawn;
}

void APixelRacerTrackPreviewActor::BuildRoads(const FPixelRacerTrackDocument& Document, int32& OutRoadSegmentCount)
{
    OutRoadSegmentCount = 0;
    if (PreviewPlaneMesh == nullptr)
    {
        return;
    }

    const FVector PlaneSize = PreviewPlaneMesh->GetBoundingBox().GetSize();
    const float MeshWidth = FMath::Max(PlaneSize.X, 1.0f);
    const float MeshHeight = FMath::Max(PlaneSize.Y, 1.0f);

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
            const FVector2D StartXY(Start.Location.X, Start.Location.Y);
            const FVector2D EndXY(End.Location.X, End.Location.Y);
            const FVector2D Delta = EndXY - StartXY;
            const float Length = Delta.Size();
            if (Length <= KINDA_SMALL_NUMBER)
            {
                continue;
            }

            UStaticMeshComponent* Segment = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
            if (Segment == nullptr)
            {
                continue;
            }
            Segment->SetStaticMesh(PreviewPlaneMesh);
            Segment->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Segment->SetGenerateOverlapEvents(false);
            Segment->SetCastShadow(false);
            Segment->SetupAttachment(SceneRoot);
            AddInstanceComponent(Segment);
            Segment->RegisterComponent();

            const float Width = FMath::Max((FMath::Abs(Start.Width) + FMath::Abs(End.Width)) * 0.5f, 1.0f);
            const float Elevation = (Start.ElevationLayer + End.ElevationLayer) * 0.5f * PixelRacerTrackPreview::RoadLayerSeparation;
            const float Z = (Start.Location.Z + End.Location.Z) * 0.5f + Elevation + 8.0f;
            Segment->SetWorldLocation(FVector((StartXY.X + EndXY.X) * 0.5f, (StartXY.Y + EndXY.Y) * 0.5f, Z));
            Segment->SetWorldRotation(FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X)), 0.0f));
            Segment->SetWorldScale3D(FVector(Length / MeshWidth, Width / MeshHeight, 0.04f));

            IncludePointInBounds(Start.Location, Width * 0.5f);
            IncludePointInBounds(End.Location, Width * 0.5f);
            ++OutRoadSegmentCount;
        }
    }
}

void APixelRacerTrackPreviewActor::BuildTiles(
    const FPixelRacerTrackDocument& Document,
    const TMap<FString, FPixelRacerTrackPreviewAssetReference>& AssetsById,
    int32& OutRenderedTileCount,
    int32& OutPlaceholderTileCount)
{
    using namespace PixelRacerTrackPreview;
    OutRenderedTileCount = 0;
    OutPlaceholderTileCount = 0;

    auto AddPlaceholderTile = [this, &OutPlaceholderTileCount](const FPixelRacerTilePlacement& Tile)
    {
        if (PreviewPlaneMesh == nullptr)
        {
            return;
        }

        UStaticMeshComponent* Placeholder = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
        if (Placeholder == nullptr)
        {
            return;
        }

        Placeholder->SetStaticMesh(PreviewPlaneMesh);
        Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Placeholder->SetCastShadow(false);
        Placeholder->SetupAttachment(SceneRoot);
        AddInstanceComponent(Placeholder);
        Placeholder->RegisterComponent();
        Placeholder->SetWorldLocation(FVector(
            Tile.Cell.X * GridCellSize + GridCellSize * 0.5f,
            Tile.Cell.Y * GridCellSize + GridCellSize * 0.5f,
            Tile.LayerIndex * TileLayerSeparation));
        Placeholder->SetWorldRotation(FRotator(0.0f, Tile.RotationSteps * 90.0f, 0.0f));
        Placeholder->SetWorldScale3D(FVector(GridCellSize / 100.0f, GridCellSize / 100.0f, 0.04f));
        ++OutPlaceholderTileCount;
        IncludePointInBounds(Placeholder->GetComponentLocation(), GridCellSize * 0.5f);
    };

    TArray<FTileMapBucket> Buckets;
    for (const FPixelRacerTilePlacement& Tile : Document.Tiles)
    {
        const FPixelRacerTrackPreviewAssetReference* Reference = AssetsById.Find(Tile.AssetId);
        UPaperTileSet* TileSet = Reference != nullptr && Reference->TileSetObjectPath.IsValid()
            ? Cast<UPaperTileSet>(Reference->TileSetObjectPath.TryLoad())
            : nullptr;
        const FIntPoint TileSize = TileSet != nullptr ? TileSet->GetTileSize() : FIntPoint::ZeroValue;
        if (TileSet == nullptr || TileSize.X <= 0 || TileSize.Y <= 0)
        {
            AddPlaceholderTile(Tile);
            continue;
        }

        FTileMapBucket* Bucket = Buckets.FindByPredicate([&Tile, &TileSize](const FTileMapBucket& Candidate)
        {
            return Candidate.LayerIndex == Tile.LayerIndex && Candidate.TileSize == TileSize;
        });
        if (Bucket == nullptr)
        {
            Bucket = &Buckets.AddDefaulted_GetRef();
            Bucket->LayerIndex = Tile.LayerIndex;
            Bucket->TileSize = TileSize;
        }
        Bucket->Tiles.Add({&Tile, TileSet});
    }

    for (const FTileMapBucket& Bucket : Buckets)
    {
        if (Bucket.Tiles.IsEmpty())
        {
            continue;
        }

        FIntPoint MinCell(MAX_int32, MAX_int32);
        FIntPoint MaxCell(MIN_int32, MIN_int32);
        for (const FTileForMap& Entry : Bucket.Tiles)
        {
            MinCell.X = FMath::Min(MinCell.X, Entry.Placement->Cell.X);
            MinCell.Y = FMath::Min(MinCell.Y, Entry.Placement->Cell.Y);
            MaxCell.X = FMath::Max(MaxCell.X, Entry.Placement->Cell.X);
            MaxCell.Y = FMath::Max(MaxCell.Y, Entry.Placement->Cell.Y);
        }

        const int32 MapWidth = MaxCell.X - MinCell.X + 1;
        const int32 MapHeight = MaxCell.Y - MinCell.Y + 1;
        if (MapWidth <= 0 || MapHeight <= 0 || MapWidth > MaxTileMapDimension || MapHeight > MaxTileMapDimension)
        {
            for (const FTileForMap& Entry : Bucket.Tiles)
            {
                AddPlaceholderTile(*Entry.Placement);
            }
            continue;
        }

        UPaperTileMapComponent* TileMap = NewObject<UPaperTileMapComponent>(this, NAME_None, RF_Transient);
        if (TileMap == nullptr)
        {
            for (const FTileForMap& Entry : Bucket.Tiles)
            {
                AddPlaceholderTile(*Entry.Placement);
            }
            continue;
        }
        TileMap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        TileMap->SetCastShadow(false);
        TileMap->SetupAttachment(SceneRoot);
        AddInstanceComponent(TileMap);
        TileMap->RegisterComponent();
        TileMap->CreateNewTileMap(MapWidth, MapHeight, Bucket.TileSize.X, Bucket.TileSize.Y, 1.0f, true);
        TileMap->SetTileMapColor(FLinearColor::White);

        for (const FTileForMap& Entry : Bucket.Tiles)
        {
            const int32 TileIndex = Entry.Placement->TileIndex;
            if (TileIndex < 0 || TileIndex >= Entry.TileSet->GetTileCount())
            {
                AddPlaceholderTile(*Entry.Placement);
                continue;
            }

            FPaperTileInfo TileInfo;
            TileInfo.TileSet = Entry.TileSet;
            TileInfo.PackedTileIndex = TileIndex;
            TileInfo.SetFlagsAsIndex(MakeTileTransformFlags(*Entry.Placement));
            TileMap->SetTile(
                Entry.Placement->Cell.X - MinCell.X,
                Entry.Placement->Cell.Y - MinCell.Y,
                0,
                TileInfo);
            ++OutRenderedTileCount;
        }

        const FVector LocalFirstTileCenter = TileMap->GetTileCenterPosition(0, 0, 0, false);
        const FVector TileMapScale(
            GridCellSize / static_cast<float>(Bucket.TileSize.X),
            1.0f,
            GridCellSize / static_cast<float>(Bucket.TileSize.Y));
        const FRotator TileMapPlaneRotation(0.0f, 0.0f, -90.0f);
        TileMap->SetRelativeScale3D(TileMapScale);
        TileMap->SetRelativeRotation(TileMapPlaneRotation);
        const FVector DesiredCenter(
            MinCell.X * GridCellSize + GridCellSize * 0.5f,
            MinCell.Y * GridCellSize + GridCellSize * 0.5f,
            Bucket.LayerIndex * TileLayerSeparation);
        const FVector RotatedLocalFirstTileCenter = TileMapPlaneRotation.RotateVector(LocalFirstTileCenter * TileMapScale);
        TileMap->SetRelativeLocation(DesiredCenter - RotatedLocalFirstTileCenter);

        for (const FTileForMap& Entry : Bucket.Tiles)
        {
            IncludePointInBounds(FVector(
                Entry.Placement->Cell.X * GridCellSize + GridCellSize * 0.5f,
                Entry.Placement->Cell.Y * GridCellSize + GridCellSize * 0.5f,
                Bucket.LayerIndex * TileLayerSeparation), GridCellSize * 0.5f);
        }
    }
}

void APixelRacerTrackPreviewActor::BuildPieces(
    const FPixelRacerTrackDocument& Document,
    const TMap<FString, FPixelRacerTrackPreviewAssetReference>& AssetsById,
    int32& OutRenderedPieceCount,
    int32& OutPlaceholderPieceCount)
{
    using namespace PixelRacerTrackPreview;
    OutRenderedPieceCount = 0;
    OutPlaceholderPieceCount = 0;

    for (const FPixelRacerPiecePlacement& Piece : Document.Pieces)
    {
        const FPixelRacerTrackPreviewAssetReference* Reference = AssetsById.Find(Piece.AssetId);
        UPaperSprite* Sprite = Reference != nullptr && Reference->SpriteObjectPath.IsValid()
            ? Cast<UPaperSprite>(Reference->SpriteObjectPath.TryLoad())
            : nullptr;
        const FVector Location = Piece.Transform.GetLocation() + FVector(0.0f, 0.0f, Piece.LayerIndex * TileLayerSeparation + 20.0f);
        const FVector2D ImageSize = Reference != nullptr
            ? FVector2D(Reference->ImageSize)
            : FVector2D(64.0f, 64.0f);
        const FVector Scale = Piece.Transform.GetScale3D().GetAbs();

        if (Sprite != nullptr)
        {
            UPaperSpriteComponent* SpriteComponent = NewObject<UPaperSpriteComponent>(this, NAME_None, RF_Transient);
            if (SpriteComponent != nullptr)
            {
                SpriteComponent->SetSprite(Sprite);
                SpriteComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                SpriteComponent->SetCastShadow(false);
                SpriteComponent->SetupAttachment(SceneRoot);
                AddInstanceComponent(SpriteComponent);
                SpriteComponent->RegisterComponent();
                SpriteComponent->SetWorldLocation(Location);
                const FQuat SpritePlaneAlignment = FRotator(-90.0f, 0.0f, 90.0f).Quaternion();
                const FQuat TrackYaw(FVector::UpVector, FMath::DegreesToRadians(Piece.Transform.Rotator().Yaw));
                SpriteComponent->SetWorldRotation(TrackYaw * SpritePlaneAlignment);
                const float PixelToWorldScale = FMath::Max(Sprite->GetPixelsPerUnrealUnit(), 0.01f) * (GridCellSize / 64.0f);
                SpriteComponent->SetRelativeScale3D(FVector(
                    PixelToWorldScale * Scale.X,
                    0.1f,
                    PixelToWorldScale * Scale.Y));
                ++OutRenderedPieceCount;
            }
        }
        else if (PreviewPlaneMesh != nullptr)
        {
            UStaticMeshComponent* Placeholder = NewObject<UStaticMeshComponent>(this, NAME_None, RF_Transient);
            if (Placeholder != nullptr)
            {
                Placeholder->SetStaticMesh(PreviewPlaneMesh);
                Placeholder->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                Placeholder->SetCastShadow(false);
                Placeholder->SetupAttachment(SceneRoot);
                AddInstanceComponent(Placeholder);
                Placeholder->RegisterComponent();
                Placeholder->SetWorldLocation(Location);
                Placeholder->SetWorldRotation(FRotator(0.0f, Piece.Transform.Rotator().Yaw, 0.0f));
                Placeholder->SetWorldScale3D(FVector(
                    FMath::Max(ImageSize.X, 8.0f) * (GridCellSize / 64.0f) / 100.0f * Scale.X,
                    FMath::Max(ImageSize.Y, 8.0f) * (GridCellSize / 64.0f) / 100.0f * Scale.Y,
                    0.04f));
                ++OutPlaceholderPieceCount;
            }
        }

        IncludePointInBounds(Location, FMath::Max(ImageSize.X, ImageSize.Y) * (GridCellSize / 128.0f));
    }
}

void APixelRacerTrackPreviewActor::UpdatePreviewCamera()
{
    const FVector Center = PreviewBounds.IsValid ? PreviewBounds.GetCenter() : FVector::ZeroVector;
    const FVector Extent = PreviewBounds.IsValid ? PreviewBounds.GetExtent() : FVector(256.0f, 256.0f, 0.0f);
    double AspectRatio = 16.0 / 9.0;
    if (const UWorld* World = GetWorld())
    {
        if (UGameViewportClient* ViewportClient = World->GetGameViewport())
        {
            FVector2D ViewportSize = FVector2D::ZeroVector;
            ViewportClient->GetViewportSize(ViewportSize);
            if (ViewportSize.X > 0 && ViewportSize.Y > 0)
            {
                AspectRatio = static_cast<double>(ViewportSize.X) / static_cast<double>(ViewportSize.Y);
            }
        }
    }

    const double HorizontalHalfFov = FMath::DegreesToRadians(static_cast<double>(PreviewCamera->FieldOfView * 0.5f));
    const double VerticalHalfFov = FMath::Atan(FMath::Tan(HorizontalHalfFov) / FMath::Max(AspectRatio, 0.01));
    const double HorizontalDistance = Extent.X / FMath::Max(FMath::Tan(HorizontalHalfFov), 0.01);
    const double VerticalDistance = Extent.Y / FMath::Max(FMath::Tan(VerticalHalfFov), 0.01);
    const double CameraHeight = FMath::Max(HorizontalDistance, VerticalDistance) * 1.15 + 100.0;
    PreviewCamera->SetRelativeLocation(FVector(Center.X, Center.Y, Center.Z + CameraHeight));
    PreviewCamera->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
}

void APixelRacerTrackPreviewActor::IncludePointInBounds(const FVector& Point, const float Padding)
{
    const FVector Extent(Padding, Padding, FMath::Max(Padding, 8.0f));
    PreviewBounds += Point - Extent;
    PreviewBounds += Point + Extent;
}
