#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "PixelRacerArcadeVehiclePawn.generated.h"

class UCameraComponent;
class UPaperSpriteComponent;
class USpringArmComponent;
class UPixelRacerVehicleDefinition;

UCLASS(Blueprintable)
class PIXELRACERCORE_API APixelRacerArcadeVehiclePawn : public APawn
{
    GENERATED_BODY()

public:
    APixelRacerArcadeVehiclePawn();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pixel Racer")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pixel Racer")
    TObjectPtr<UPaperSpriteComponent> SpriteComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pixel Racer")
    TObjectPtr<USpringArmComponent> CameraArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Pixel Racer")
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Racer")
    TObjectPtr<UPixelRacerVehicleDefinition> VehicleDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Racer|Camera")
    bool bRotateCameraWithVehicle = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Racer|Driving", meta=(ClampMin="100.0"))
    float BaseMaxSpeed = 2200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Racer|Driving", meta=(ClampMin="100.0"))
    float BaseAcceleration = 1500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Racer|Driving", meta=(ClampMin="0.1"))
    float BaseGrip = 7.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pixel Racer|Driving", meta=(ClampMin="1.0"))
    float TurnRateDegrees = 150.0f;

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Driving")
    void ResetVehicle();

    void SetAutonomousDriveInput(float Throttle, float Steer, bool bDrift);
    float GetEstimatedMaxSpeed() const;
    float GetPlanarSpeed() const;

private:
    void SetThrottleInput(float Value);
    void SetSteerInput(float Value);
    void SetDriftPressed();
    void SetDriftReleased();
    void UpdateDirectionalSprite();

    float ThrottleInput = 0.0f;
    float SteerInput = 0.0f;
    bool bDriftHeld = false;
    FVector2D Velocity2D = FVector2D::ZeroVector;
    FTransform ResetTransform = FTransform::Identity;
};
