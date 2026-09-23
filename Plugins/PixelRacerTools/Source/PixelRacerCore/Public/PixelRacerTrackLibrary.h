#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PixelRacerTrackTypes.h"
#include "PixelRacerTrackLibrary.generated.h"

UCLASS()
class PIXELRACERCORE_API UPixelRacerTrackLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Track")
    static bool TrackDocumentToJson(const FPixelRacerTrackDocument& Document, FString& OutJson);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Track")
    static bool TrackDocumentFromJson(const FString& Json, FPixelRacerTrackDocument& OutDocument, FString& OutError);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Track")
    static bool ExportTrackDocument(const FPixelRacerTrackDocument& Document, const FString& FilePath, FString& OutError);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Track")
    static bool ImportTrackDocument(const FString& FilePath, FPixelRacerTrackDocument& OutDocument, FString& OutError);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Validation")
    static bool ValidateTrackDocument(const FPixelRacerTrackDocument& Document, TArray<FPixelRacerValidationMessage>& OutMessages);

    UFUNCTION(BlueprintPure, Category="Pixel Racer|Geometry")
    static bool SegmentsIntersect2D(const FVector2D& A0, const FVector2D& A1, const FVector2D& B0, const FVector2D& B1, FVector2D& OutIntersection);

    UFUNCTION(BlueprintPure, Category="Pixel Racer|Surface")
    static FPixelRacerSurfaceProfile GetLegacySurfaceProfile(const FString& SurfaceId);
};
