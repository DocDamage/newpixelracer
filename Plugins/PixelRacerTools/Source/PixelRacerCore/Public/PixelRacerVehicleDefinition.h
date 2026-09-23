#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PixelRacerVehicleDefinition.generated.h"

class UPaperSprite;

UENUM(BlueprintType)
enum class EPixelRacerVehicleType : uint8
{
    Car,
    Motorcycle
};

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerVehicleStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="0.0", ClampMax="100.0"))
    float Speed = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="0.0", ClampMax="100.0"))
    float Acceleration = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="0.0", ClampMax="100.0"))
    float Handling = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="0.0", ClampMax="100.0"))
    float Drift = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="0.0", ClampMax="100.0"))
    float Boost = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle", meta=(ClampMin="0.0", ClampMax="100.0"))
    float Weight = 60.0f;
};

UCLASS(BlueprintType)
class PIXELRACERCORE_API UPixelRacerVehicleDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
    FString VehicleId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
    FString DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
    EPixelRacerVehicleType Type = EPixelRacerVehicleType::Car;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
    FString Category;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
    FString SourceSpriteSheet;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
    int32 DirectionCount = 16;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
    FIntPoint SpriteCellSize = FIntPoint(46, 54);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
    FPixelRacerVehicleStats Stats;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
    TArray<TObjectPtr<UPaperSprite>> DirectionalSprites;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
    TArray<FString> ColorVariants;
};
