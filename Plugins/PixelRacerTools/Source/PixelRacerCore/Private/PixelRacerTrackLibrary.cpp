#include "PixelRacerTrackLibrary.h"

#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace PixelRacerValidation
{
    static void AddMessage(TArray<FPixelRacerValidationMessage>& Messages, const EPixelRacerValidationSeverity Severity, const TCHAR* Code, const FString& Message)
    {
        FPixelRacerValidationMessage& Entry = Messages.AddDefaulted_GetRef();
        Entry.Severity = Severity;
        Entry.Code = Code;
        Entry.Message = Message;
    }

    static double Cross(const FVector2D& A, const FVector2D& B)
    {
        return static_cast<double>(A.X) * static_cast<double>(B.Y) - static_cast<double>(A.Y) * static_cast<double>(B.X);
    }
}

bool UPixelRacerTrackLibrary::TrackDocumentToJson(const FPixelRacerTrackDocument& Document, FString& OutJson)
{
    return FJsonObjectConverter::UStructToJsonObjectString(
        FPixelRacerTrackDocument::StaticStruct(),
        &Document,
        OutJson,
        0,
        0);
}

bool UPixelRacerTrackLibrary::TrackDocumentFromJson(const FString& Json, FPixelRacerTrackDocument& OutDocument, FString& OutError)
{
    OutError.Reset();

    if (Json.IsEmpty())
    {
        OutError = TEXT("Track JSON is empty.");
        return false;
    }

    if (!FJsonObjectConverter::JsonObjectStringToUStruct(
        Json,
        &OutDocument,
        0,
        0))
    {
        OutError = TEXT("Track JSON could not be parsed into the Pixel Racer track schema.");
        return false;
    }

    return true;
}

bool UPixelRacerTrackLibrary::ExportTrackDocument(const FPixelRacerTrackDocument& Document, const FString& FilePath, FString& OutError)
{
    FString Json;
    if (!TrackDocumentToJson(Document, Json))
    {
        OutError = TEXT("Could not serialize track document.");
        return false;
    }

    const FString Directory = FPaths::GetPath(FilePath);
    if (!Directory.IsEmpty())
    {
        IFileManager::Get().MakeDirectory(*Directory, true);
    }

    if (!FFileHelper::SaveStringToFile(Json, *FilePath))
    {
        OutError = FString::Printf(TEXT("Could not write track file: %s"), *FilePath);
        return false;
    }

    OutError.Reset();
    return true;
}

bool UPixelRacerTrackLibrary::ImportTrackDocument(const FString& FilePath, FPixelRacerTrackDocument& OutDocument, FString& OutError)
{
    FString Json;
    if (!FFileHelper::LoadFileToString(Json, *FilePath))
    {
        OutError = FString::Printf(TEXT("Could not read track file: %s"), *FilePath);
        return false;
    }

    return TrackDocumentFromJson(Json, OutDocument, OutError);
}

bool UPixelRacerTrackLibrary::SegmentsIntersect2D(const FVector2D& A0, const FVector2D& A1, const FVector2D& B0, const FVector2D& B1, FVector2D& OutIntersection)
{
    const FVector2D R = A1 - A0;
    const FVector2D S = B1 - B0;
    const double Denominator = PixelRacerValidation::Cross(R, S);
    const FVector2D Delta = B0 - A0;

    if (FMath::IsNearlyZero(Denominator, KINDA_SMALL_NUMBER))
    {
        return false;
    }

    const double T = PixelRacerValidation::Cross(Delta, S) / Denominator;
    const double U = PixelRacerValidation::Cross(Delta, R) / Denominator;

    if (T < 0.0 || T > 1.0 || U < 0.0 || U > 1.0)
    {
        return false;
    }

    OutIntersection = A0 + R * static_cast<float>(T);
    return true;
}

bool UPixelRacerTrackLibrary::ValidateTrackDocument(const FPixelRacerTrackDocument& Document, TArray<FPixelRacerValidationMessage>& OutMessages)
{
    OutMessages.Reset();

    if (Document.SchemaVersion <= 0)
    {
        PixelRacerValidation::AddMessage(OutMessages, EPixelRacerValidationSeverity::Error, TEXT("schema.invalid"), TEXT("SchemaVersion must be greater than zero."));
    }

    if (Document.Metadata.TrackId.TrimStartAndEnd().IsEmpty())
    {
        PixelRacerValidation::AddMessage(OutMessages, EPixelRacerValidationSeverity::Error, TEXT("track.id_missing"), TEXT("TrackId is required."));
    }

    if (Document.RoadSplines.IsEmpty())
    {
        PixelRacerValidation::AddMessage(OutMessages, EPixelRacerValidationSeverity::Error, TEXT("road.none"), TEXT("Track has no road splines."));
    }

    for (int32 SplineIndex = 0; SplineIndex < Document.RoadSplines.Num(); ++SplineIndex)
    {
        const FPixelRacerRoadSpline& Spline = Document.RoadSplines[SplineIndex];
        const int32 PointCount = Spline.ControlPoints.Num();
        const int32 MinimumPoints = Spline.bClosedLoop ? 3 : 2;

        if (PointCount < MinimumPoints)
        {
            PixelRacerValidation::AddMessage(
                OutMessages,
                EPixelRacerValidationSeverity::Error,
                TEXT("road.too_few_points"),
                FString::Printf(TEXT("Road spline %d needs at least %d control points."), SplineIndex, MinimumPoints));
            continue;
        }

        for (int32 PointIndex = 0; PointIndex < PointCount; ++PointIndex)
        {
            if (Spline.ControlPoints[PointIndex].Width <= 0.0f)
            {
                PixelRacerValidation::AddMessage(
                    OutMessages,
                    EPixelRacerValidationSeverity::Error,
                    TEXT("road.invalid_width"),
                    FString::Printf(TEXT("Road spline %d point %d has a non-positive width."), SplineIndex, PointIndex));
            }
        }

        const int32 SegmentCount = Spline.bClosedLoop ? PointCount : PointCount - 1;
        for (int32 A = 0; A < SegmentCount; ++A)
        {
            const int32 A0Index = A;
            const int32 A1Index = (A + 1) % PointCount;
            const FPixelRacerTrackControlPoint& A0 = Spline.ControlPoints[A0Index];
            const FPixelRacerTrackControlPoint& A1 = Spline.ControlPoints[A1Index];

            for (int32 B = A + 1; B < SegmentCount; ++B)
            {
                const int32 B0Index = B;
                const int32 B1Index = (B + 1) % PointCount;

                const bool bAdjacent = A1Index == B0Index || B1Index == A0Index;
                const bool bClosingAdjacency = Spline.bClosedLoop && A0Index == 0 && B1Index == 0;
                if (bAdjacent || bClosingAdjacency)
                {
                    continue;
                }

                const FPixelRacerTrackControlPoint& B0 = Spline.ControlPoints[B0Index];
                const FPixelRacerTrackControlPoint& B1 = Spline.ControlPoints[B1Index];

                if (A0.ElevationLayer != B0.ElevationLayer)
                {
                    continue;
                }

                if (A0.bAllowCrossing || A1.bAllowCrossing || B0.bAllowCrossing || B1.bAllowCrossing)
                {
                    continue;
                }

                FVector2D Intersection;
                if (SegmentsIntersect2D(
                    FVector2D(A0.Location.X, A0.Location.Y),
                    FVector2D(A1.Location.X, A1.Location.Y),
                    FVector2D(B0.Location.X, B0.Location.Y),
                    FVector2D(B1.Location.X, B1.Location.Y),
                    Intersection))
                {
                    PixelRacerValidation::AddMessage(
                        OutMessages,
                        EPixelRacerValidationSeverity::Warning,
                        TEXT("road.self_intersection"),
                        FString::Printf(TEXT("Road spline %d intersects itself near %.1f, %.1f. Mark the crossing intentional or move it to another elevation layer if this is a bridge/tunnel."), SplineIndex, Intersection.X, Intersection.Y));
                }
            }
        }
    }

    if (Document.Topology == EPixelRacerTrackTopology::Circuit)
    {
        bool bHasClosedRoad = false;
        for (const FPixelRacerRoadSpline& Spline : Document.RoadSplines)
        {
            bHasClosedRoad |= Spline.bClosedLoop;
        }

        if (!bHasClosedRoad)
        {
            PixelRacerValidation::AddMessage(OutMessages, EPixelRacerValidationSeverity::Error, TEXT("circuit.not_closed"), TEXT("Circuit topology requires at least one closed road spline."));
        }
    }

    TSet<int32> SeenCheckpointOrders;
    for (const FPixelRacerCheckpoint& Checkpoint : Document.Checkpoints)
    {
        if (SeenCheckpointOrders.Contains(Checkpoint.Order))
        {
            PixelRacerValidation::AddMessage(OutMessages, EPixelRacerValidationSeverity::Error, TEXT("checkpoint.duplicate_order"), FString::Printf(TEXT("Checkpoint order %d is duplicated."), Checkpoint.Order));
        }
        SeenCheckpointOrders.Add(Checkpoint.Order);
    }

    if (Document.GridSlots.IsEmpty() && Document.Topology != EPixelRacerTrackTopology::OpenDrivingArea)
    {
        PixelRacerValidation::AddMessage(OutMessages, EPixelRacerValidationSeverity::Warning, TEXT("grid.none"), TEXT("Track has no starting grid slots."));
    }

    if (Document.Checkpoints.IsEmpty() && Document.Topology != EPixelRacerTrackTopology::OpenDrivingArea)
    {
        PixelRacerValidation::AddMessage(OutMessages, EPixelRacerValidationSeverity::Info, TEXT("checkpoint.generated_pending"), TEXT("No checkpoints are stored yet. The editor can generate them from the road spline."));
    }

    if (Document.RacingLines.IsEmpty())
    {
        PixelRacerValidation::AddMessage(OutMessages, EPixelRacerValidationSeverity::Info, TEXT("ai.generated_pending"), TEXT("No racing lines are stored yet. The editor can generate ideal/alternate/recovery lines."));
    }

    for (const FPixelRacerValidationMessage& Message : OutMessages)
    {
        if (Message.Severity == EPixelRacerValidationSeverity::Error)
        {
            return false;
        }
    }

    return true;
}

FPixelRacerSurfaceProfile UPixelRacerTrackLibrary::GetLegacySurfaceProfile(const FString& SurfaceId)
{
    FPixelRacerSurfaceProfile Result;
    Result.SurfaceId = SurfaceId.ToLower();

    if (Result.SurfaceId == TEXT("dirt"))
    {
        Result.Friction = 0.82f;
        Result.DriftMultiplier = 1.45f;
        Result.MaxSpeedMultiplier = 0.88f;
    }
    else if (Result.SurfaceId == TEXT("sand"))
    {
        Result.Friction = 0.68f;
        Result.DriftMultiplier = 1.65f;
        Result.MaxSpeedMultiplier = 0.75f;
    }
    else if (Result.SurfaceId == TEXT("grass"))
    {
        Result.Friction = 0.45f;
        Result.DriftMultiplier = 1.80f;
        Result.MaxSpeedMultiplier = 0.55f;
    }
    else if (Result.SurfaceId == TEXT("kerb"))
    {
        Result.Friction = 0.95f;
        Result.DriftMultiplier = 1.10f;
        Result.MaxSpeedMultiplier = 0.98f;
    }
    else
    {
        Result.SurfaceId = TEXT("asphalt");
        Result.Friction = 1.0f;
        Result.DriftMultiplier = 1.0f;
        Result.MaxSpeedMultiplier = 1.0f;
    }

    return Result;
}
