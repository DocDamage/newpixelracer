#include "PixelRacerValidationDriverComponent.h"

#include "PixelRacerArcadeVehiclePawn.h"

UPixelRacerValidationDriverComponent::UPixelRacerValidationDriverComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

bool UPixelRacerValidationDriverComponent::StartValidation(const FPixelRacerRacingLine& RacingLine)
{
    APixelRacerArcadeVehiclePawn* Vehicle = Cast<APixelRacerArcadeVehiclePawn>(GetOwner());
    if (!Vehicle || RacingLine.Points.Num() < 2)
    {
        return false;
    }

    ActiveLine = RacingLine;
    CurrentPointIndex = 0;
    Issues.Reset();
    StuckTimer = 0.0f;
    DeviationTimer = 0.0f;
    LastStuckIssuePoint = INDEX_NONE;
    LastDeviationIssuePoint = INDEX_NONE;
    bComplete = false;
    bRunning = true;
    return true;
}

bool UPixelRacerValidationDriverComponent::StartValidationFromTrack(const FPixelRacerTrackDocument& Document, EPixelRacerRacingLineKind LineKind)
{
    for (const FPixelRacerRacingLine& Line : Document.RacingLines)
    {
        if (Line.Kind == LineKind)
        {
            return StartValidation(Line);
        }
    }
    return false;
}

void UPixelRacerValidationDriverComponent::StopValidation()
{
    bRunning = false;
    if (APixelRacerArcadeVehiclePawn* Vehicle = Cast<APixelRacerArcadeVehiclePawn>(GetOwner()))
    {
        Vehicle->SetAutonomousDriveInput(0.0f, 0.0f, false);
    }
}

float UPixelRacerValidationDriverComponent::DistanceToLine2D(const FVector2D& Point) const
{
    float Best = FLT_MAX;
    for (int32 Index = 0; Index < ActiveLine.Points.Num() - 1; ++Index)
    {
        const FVector& A3 = ActiveLine.Points[Index].Location;
        const FVector& B3 = ActiveLine.Points[Index + 1].Location;
        const FVector2D A(A3.X, A3.Y);
        const FVector2D B(B3.X, B3.Y);
        const FVector2D Segment = B - A;
        const float LengthSquared = Segment.SizeSquared();
        const float Alpha = LengthSquared <= KINDA_SMALL_NUMBER ? 0.0f : FMath::Clamp(static_cast<float>(FVector2D::DotProduct(Point - A, Segment) / LengthSquared), 0.0f, 1.0f);
        Best = FMath::Min(Best, static_cast<float>(FVector2D::Distance(Point, A + Segment * Alpha)));
    }
    return Best;
}

void UPixelRacerValidationDriverComponent::AddIssue(const FString& Code, const FString& Message, const FVector& Location)
{
    FPixelRacerValidationIssue& Issue = Issues.AddDefaulted_GetRef();
    Issue.Code = Code;
    Issue.Message = Message;
    Issue.Location = Location;
    Issue.RacingLinePoint = CurrentPointIndex;
}

void UPixelRacerValidationDriverComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bRunning || !ActiveLine.Points.IsValidIndex(CurrentPointIndex))
    {
        return;
    }

    APixelRacerArcadeVehiclePawn* Vehicle = Cast<APixelRacerArcadeVehiclePawn>(GetOwner());
    if (!Vehicle)
    {
        StopValidation();
        return;
    }

    const FVector VehicleLocation = Vehicle->GetActorLocation();
    const FPixelRacerRacingLinePoint& TargetPoint = ActiveLine.Points[CurrentPointIndex];
    const FVector ToTarget3 = TargetPoint.Location - VehicleLocation;
    const FVector2D ToTarget(ToTarget3.X, ToTarget3.Y);
    const float DistanceToTarget = ToTarget.Size();

    if (DistanceToTarget <= ArrivalRadius)
    {
        ++CurrentPointIndex;
        StuckTimer = 0.0f;
        DeviationTimer = 0.0f;
        if (CurrentPointIndex >= ActiveLine.Points.Num())
        {
            bComplete = true;
            StopValidation();
            return;
        }
    }

    if (!ActiveLine.Points.IsValidIndex(CurrentPointIndex))
    {
        return;
    }

    const FVector UpdatedTarget = ActiveLine.Points[CurrentPointIndex].Location;
    const FVector UpdatedDelta = UpdatedTarget - VehicleLocation;
    const float DesiredYaw = FMath::RadiansToDegrees(FMath::Atan2(UpdatedDelta.Y, UpdatedDelta.X));
    const float YawError = FMath::FindDeltaAngleDegrees(Vehicle->GetActorRotation().Yaw, DesiredYaw);
    const float Steer = FMath::Clamp(YawError / 50.0f, -1.0f, 1.0f);

    const float MaxSpeed = Vehicle->GetEstimatedMaxSpeed();
    const float TargetSpeed = MaxSpeed * FMath::Clamp(ActiveLine.Points[CurrentPointIndex].TargetSpeedScale, 0.2f, 1.1f);
    const float CurrentSpeed = Vehicle->GetPlanarSpeed();
    const float Throttle = CurrentSpeed > TargetSpeed * 1.08f ? -0.35f : 1.0f;
    const bool bDrift = FMath::Abs(YawError) > 32.0f && CurrentSpeed > MaxSpeed * 0.35f;
    Vehicle->SetAutonomousDriveInput(Throttle, Steer, bDrift);

    if (CurrentSpeed < 55.0f && DistanceToTarget > ArrivalRadius * 1.5f && Throttle > 0.5f)
    {
        StuckTimer += DeltaTime;
        if (StuckTimer >= StuckTimeThreshold && LastStuckIssuePoint != CurrentPointIndex)
        {
            AddIssue(TEXT("ai.stuck"), TEXT("Validation vehicle remained below the stuck-speed threshold while trying to reach this racing-line point."), VehicleLocation);
            LastStuckIssuePoint = CurrentPointIndex;
        }
    }
    else
    {
        StuckTimer = 0.0f;
    }

    const float LineDeviation = DistanceToLine2D(FVector2D(VehicleLocation.X, VehicleLocation.Y));
    if (LineDeviation > MaxLineDeviation)
    {
        DeviationTimer += DeltaTime;
        if (DeviationTimer >= 0.75f && LastDeviationIssuePoint != CurrentPointIndex)
        {
            AddIssue(TEXT("ai.off_line"), FString::Printf(TEXT("Validation vehicle moved %.0f units away from the generated racing line."), LineDeviation), VehicleLocation);
            LastDeviationIssuePoint = CurrentPointIndex;
        }
    }
    else
    {
        DeviationTimer = 0.0f;
    }
}
