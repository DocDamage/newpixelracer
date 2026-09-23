#pragma once

#include "CoreMinimal.h"
#include "PixelRacerTrackTypes.generated.h"

UENUM(BlueprintType)
enum class EPixelRacerTrackTopology : uint8
{
    Circuit,
    PointToPoint,
    Sprint,
    Drag,
    Branching,
    EliminationArena,
    OpenDrivingArea
};

UENUM(BlueprintType)
enum class EPixelRacerLayerKind : uint8
{
    Terrain,
    Road,
    Markings,
    Barriers,
    Scenery,
    Gameplay,
    AI,
    LightingWeather,
    Elevated1,
    Elevated2,
    Underground
};

UENUM(BlueprintType)
enum class EPixelRacerRacingLineKind : uint8
{
    Ideal,
    AlternateLeft,
    AlternateRight,
    Pit,
    Recovery
};

UENUM(BlueprintType)
enum class EPixelRacerValidationSeverity : uint8
{
    Info,
    Warning,
    Error
};

UENUM(BlueprintType)
enum class EPixelRacerAuthoringMode : uint8
{
    Select,
    Spline,
    TilePaint,
    PiecePlacement,
    ProceduralZone,
    AIEdit
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerTrackMetadata
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FString TrackId = TEXT("new_track");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FString DisplayName = TEXT("New Track");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FString Author;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FString> Tags;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerTrackControlPoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track", meta=(ClampMin="1.0"))
    float Width = 160.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track", meta=(ClampMin="0.0"))
    float SpeedScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FString SurfaceId = TEXT("asphalt");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    int32 ElevationLayer = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bManualWidth = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bAllowCrossing = false;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerRoadSpline
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FString Name = TEXT("Road");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bClosedLoop = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FPixelRacerTrackControlPoint> ControlPoints;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerTilePlacement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FIntPoint Cell = FIntPoint::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FString AssetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    int32 LayerIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    int32 RotationSteps = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bFlipX = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bFlipY = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bGenerated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bManualOverride = false;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerPiecePlacement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FString AssetId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FTransform Transform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    int32 LayerIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bGenerated = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bManualOverride = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bLocked = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FString> Tags;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerProceduralZone
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FString RulePreset = TEXT("Grassland");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FVector2D> Polygon;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    int32 LayerIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    int32 Seed = 1337;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerCheckpoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FGuid Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    int32 Order = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track", meta=(ClampMin="1.0"))
    float HalfWidth = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    float YawDegrees = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bGenerated = true;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerGridSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    int32 Order = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    float YawDegrees = 0.0f;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerRacingLinePoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track", meta=(ClampMin="0.0"))
    float TargetSpeedScale = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track", meta=(ClampMin="0.0", ClampMax="1.0"))
    float BrakeAmount = 0.0f;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerRacingLine
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    EPixelRacerRacingLineKind Kind = EPixelRacerRacingLineKind::Ideal;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bGenerated = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FPixelRacerRacingLinePoint> Points;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerEnvironmentProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    FString TimeOfDayPreset = TEXT("Midday");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    FString WeatherPreset = TEXT("Clear");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SurfaceWetness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Environment")
    bool bDynamicTime = false;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerGenerationSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
    int32 Seed = 1337;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", meta=(ClampMin="100.0"))
    float TargetTrackLength = 3000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation", meta=(ClampMin="0.0", ClampMax="1.0"))
    float Complexity = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
    bool bGenerateRoad = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
    bool bGenerateScenery = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
    bool bGenerateBarriers = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
    bool bGenerateCheckpoints = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
    bool bGenerateAILines = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
    bool bGenerateMinimap = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Generation")
    bool bGenerateThumbnail = true;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerSurfaceProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface")
    FString SurfaceId = TEXT("asphalt");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0"))
    float Friction = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0"))
    float DriftMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Surface", meta=(ClampMin="0.0"))
    float MaxSpeedMultiplier = 1.0f;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerTrackDocument
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    int32 SchemaVersion = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FPixelRacerTrackMetadata Metadata;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    EPixelRacerTrackTopology Topology = EPixelRacerTrackTopology::Circuit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    bool bInfiniteCanvas = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track", meta=(ClampMin="256"))
    int32 ChunkSize = 2048;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FPixelRacerRoadSpline> RoadSplines;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FPixelRacerTilePlacement> Tiles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FPixelRacerPiecePlacement> Pieces;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FPixelRacerProceduralZone> ProceduralZones;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FPixelRacerCheckpoint> Checkpoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FPixelRacerGridSlot> GridSlots;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FPixelRacerRacingLine> RacingLines;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    TArray<FPixelRacerSurfaceProfile> SurfaceProfiles;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FPixelRacerEnvironmentProfile Environment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Track")
    FPixelRacerGenerationSettings Generation;
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerValidationMessage
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    EPixelRacerValidationSeverity Severity = EPixelRacerValidationSeverity::Info;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    FString Code;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    FString Message;
};
