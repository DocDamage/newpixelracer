#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "PixelRacerTrackTypes.h"
#include "PixelRacerValidationDriverComponent.generated.h"

USTRUCT(BlueprintType)
struct PIXELRACERCORE_API FPixelRacerValidationIssue
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    FString Code;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    FString Message;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    FVector Location = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    int32 RacingLinePoint = INDEX_NONE;
};

UCLASS(ClassGroup=(PixelRacer), meta=(BlueprintSpawnableComponent))
class PIXELRACERCORE_API UPixelRacerValidationDriverComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPixelRacerValidationDriverComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Validation", meta=(ClampMin="10.0"))
    float ArrivalRadius = 80.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Validation", meta=(ClampMin="50.0"))
    float MaxLineDeviation = 420.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Validation", meta=(ClampMin="0.1"))
    float StuckTimeThreshold = 2.5f;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    bool bRunning = false;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    bool bComplete = false;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    int32 CurrentPointIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category="Validation")
    TArray<FPixelRacerValidationIssue> Issues;

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Validation")
    bool StartValidation(const FPixelRacerRacingLine& RacingLine);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Validation")
    bool StartValidationFromTrack(const FPixelRacerTrackDocument& Document, EPixelRacerRacingLineKind LineKind = EPixelRacerRacingLineKind::Ideal);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Validation")
    void StopValidation();

private:
    float DistanceToLine2D(const FVector2D& Point) const;
    void AddIssue(const FString& Code, const FString& Message, const FVector& Location);

    FPixelRacerRacingLine ActiveLine;
    float StuckTimer = 0.0f;
    float DeviationTimer = 0.0f;
    int32 LastStuckIssuePoint = INDEX_NONE;
    int32 LastDeviationIssuePoint = INDEX_NONE;
};
