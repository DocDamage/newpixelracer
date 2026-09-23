#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PixelRacerTrackTypes.h"
#include "PixelRacerTrackAsset.generated.h"

UCLASS(BlueprintType)
class PIXELRACERCORE_API UPixelRacerTrackAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Racer")
    FPixelRacerTrackDocument Document;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
