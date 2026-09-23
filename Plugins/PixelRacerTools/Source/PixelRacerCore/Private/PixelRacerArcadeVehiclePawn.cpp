#include "PixelRacerArcadeVehiclePawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "PaperSprite.h"
#include "PaperSpriteComponent.h"
#include "PixelRacerVehicleDefinition.h"

APixelRacerArcadeVehiclePawn::APixelRacerArcadeVehiclePawn()
{
    PrimaryActorTick.bCanEverTick = true;
    AutoPossessPlayer = EAutoReceiveInput::Player0;

    SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(SceneRoot);

    SpriteComponent = CreateDefaultSubobject<UPaperSpriteComponent>(TEXT("VehicleSprite"));
    SpriteComponent->SetupAttachment(SceneRoot);
    SpriteComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

    CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
    CameraArm->SetupAttachment(SceneRoot);
    CameraArm->TargetArmLength = 1400.0f;
    CameraArm->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    CameraArm->bDoCollisionTest = false;
    CameraArm->bUsePawnControlRotation = false;
    CameraArm->SetUsingAbsoluteRotation(true);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
    Camera->ProjectionMode = ECameraProjectionMode::Orthographic;
    Camera->OrthoWidth = 1600.0f;

    ResetTransform = FTransform::Identity;
}

void APixelRacerArcadeVehiclePawn::BeginPlay()
{
    Super::BeginPlay();
    ResetTransform = GetActorTransform();
}

void APixelRacerArcadeVehiclePawn::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (DeltaSeconds <= 0.0f)
    {
        return;
    }

    const float SpeedStat = VehicleDefinition ? VehicleDefinition->Stats.Speed : 80.0f;
    const float AccelStat = VehicleDefinition ? VehicleDefinition->Stats.Acceleration : 80.0f;
    const float HandlingStat = VehicleDefinition ? VehicleDefinition->Stats.Handling : 80.0f;
    const float DriftStat = VehicleDefinition ? VehicleDefinition->Stats.Drift : 80.0f;

    const float MaxSpeed = BaseMaxSpeed * FMath::Lerp(0.70f, 1.25f, FMath::Clamp(SpeedStat / 100.0f, 0.0f, 1.0f));
    const float Acceleration = BaseAcceleration * FMath::Lerp(0.65f, 1.35f, FMath::Clamp(AccelStat / 100.0f, 0.0f, 1.0f));
    const float Handling = FMath::Lerp(0.65f, 1.30f, FMath::Clamp(HandlingStat / 100.0f, 0.0f, 1.0f));
    const float DriftFactor = FMath::Lerp(0.45f, 0.82f, FMath::Clamp(DriftStat / 100.0f, 0.0f, 1.0f));

    const float CurrentSpeed = Velocity2D.Size();
    const float SpeedAlpha = FMath::Clamp(CurrentSpeed / FMath::Max(1.0f, MaxSpeed), 0.0f, 1.0f);
    const float SteeringAuthority = FMath::Lerp(0.35f, 1.0f, SpeedAlpha) * Handling;
    const float YawDelta = SteerInput * TurnRateDegrees * SteeringAuthority * DeltaSeconds;
    AddActorWorldRotation(FRotator(0.0f, YawDelta, 0.0f));

    const float YawRadians = FMath::DegreesToRadians(GetActorRotation().Yaw);
    const FVector2D Forward(FMath::Cos(YawRadians), FMath::Sin(YawRadians));
    const FVector2D Right(-Forward.Y, Forward.X);

    Velocity2D += Forward * (ThrottleInput * Acceleration * DeltaSeconds);

    const float ForwardSpeed = FVector2D::DotProduct(Velocity2D, Forward);
    float LateralSpeed = FVector2D::DotProduct(Velocity2D, Right);
    const float LateralDamping = bDriftHeld ? BaseGrip * DriftFactor : BaseGrip;
    LateralSpeed = FMath::FInterpTo(LateralSpeed, 0.0f, DeltaSeconds, LateralDamping);

    const float RollingDrag = ThrottleInput == 0.0f ? 1.8f : 0.35f;
    float DampedForwardSpeed = FMath::FInterpTo(ForwardSpeed, 0.0f, DeltaSeconds, RollingDrag);
    DampedForwardSpeed = FMath::Clamp(DampedForwardSpeed, -MaxSpeed * 0.35f, MaxSpeed);
    Velocity2D = Forward * DampedForwardSpeed + Right * LateralSpeed;

    const FVector Delta(Velocity2D.X * DeltaSeconds, Velocity2D.Y * DeltaSeconds, 0.0f);
    FHitResult Hit;
    AddActorWorldOffset(Delta, true, &Hit);
    if (Hit.bBlockingHit)
    {
        const FVector2D Normal(Hit.Normal.X, Hit.Normal.Y);
        Velocity2D -= Normal * FVector2D::DotProduct(Velocity2D, Normal) * 1.4f;
    }
    CameraArm->SetUsingAbsoluteRotation(!bRotateCameraWithVehicle);
    if (bRotateCameraWithVehicle)
    {
        CameraArm->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    }

    UpdateDirectionalSprite();
}

void APixelRacerArcadeVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAxis(TEXT("PixelRacer_Throttle"), this, &APixelRacerArcadeVehiclePawn::SetThrottleInput);
    PlayerInputComponent->BindAxis(TEXT("PixelRacer_Steer"), this, &APixelRacerArcadeVehiclePawn::SetSteerInput);
    PlayerInputComponent->BindAction(TEXT("PixelRacer_Drift"), IE_Pressed, this, &APixelRacerArcadeVehiclePawn::SetDriftPressed);
    PlayerInputComponent->BindAction(TEXT("PixelRacer_Drift"), IE_Released, this, &APixelRacerArcadeVehiclePawn::SetDriftReleased);
    PlayerInputComponent->BindAction(TEXT("PixelRacer_Reset"), IE_Pressed, this, &APixelRacerArcadeVehiclePawn::ResetVehicle);
}

void APixelRacerArcadeVehiclePawn::SetThrottleInput(const float Value)
{
    ThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void APixelRacerArcadeVehiclePawn::SetSteerInput(const float Value)
{
    SteerInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void APixelRacerArcadeVehiclePawn::SetAutonomousDriveInput(const float Throttle, const float Steer, const bool bDrift)
{
    SetThrottleInput(Throttle);
    SetSteerInput(Steer);
    bDriftHeld = bDrift;
}

float APixelRacerArcadeVehiclePawn::GetEstimatedMaxSpeed() const
{
    const float SpeedStat = VehicleDefinition ? VehicleDefinition->Stats.Speed : 80.0f;
    return BaseMaxSpeed * FMath::Lerp(0.70f, 1.25f, FMath::Clamp(SpeedStat / 100.0f, 0.0f, 1.0f));
}

float APixelRacerArcadeVehiclePawn::GetPlanarSpeed() const
{
    return Velocity2D.Size();
}

void APixelRacerArcadeVehiclePawn::SetDriftPressed()
{
    bDriftHeld = true;
}

void APixelRacerArcadeVehiclePawn::SetDriftReleased()
{
    bDriftHeld = false;
}

void APixelRacerArcadeVehiclePawn::ResetVehicle()
{
    SetActorTransform(ResetTransform, false, nullptr, ETeleportType::ResetPhysics);
    Velocity2D = FVector2D::ZeroVector;
    ThrottleInput = 0.0f;
    SteerInput = 0.0f;
    bDriftHeld = false;
}

void APixelRacerArcadeVehiclePawn::UpdateDirectionalSprite()
{
    if (!VehicleDefinition || VehicleDefinition->DirectionalSprites.IsEmpty())
    {
        return;
    }

    const int32 DirectionCount = VehicleDefinition->DirectionalSprites.Num();
    const float Step = 360.0f / static_cast<float>(DirectionCount);
    const float NormalizedYaw = FMath::Fmod(GetActorRotation().Yaw + 360.0f, 360.0f);
    const int32 DirectionIndex = FMath::RoundToInt(NormalizedYaw / Step) % DirectionCount;

    if (UPaperSprite* Sprite = VehicleDefinition->DirectionalSprites[DirectionIndex])
    {
        SpriteComponent->SetSprite(Sprite);
    }
}
