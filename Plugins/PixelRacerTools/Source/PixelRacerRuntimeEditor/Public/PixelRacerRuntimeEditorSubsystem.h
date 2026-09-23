#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PixelRacerTrackTypes.h"
#include "PixelRacerRuntimeEditorSubsystem.generated.h"

UCLASS(BlueprintType)
class PIXELRACERRUNTIMEEDITOR_API UPixelRacerRuntimeEditorSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category="Pixel Racer|Runtime Editor")
    FPixelRacerTrackDocument WorkingDocument;

    UPROPERTY(BlueprintReadOnly, Category="Pixel Racer|Runtime Editor")
    bool bDirty = false;

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor")
    void NewTrack(const FString& TrackId, const FString& DisplayName);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor")
    bool LoadUserTrack(const FString& FileName, FString& OutError);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor")
    bool SaveUserTrack(const FString& FileName, FString& OutSavedPath, FString& OutError);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor")
    TArray<FString> ListUserTracks() const;

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor")
    bool Undo();

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor")
    bool Redo();

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Road")
    int32 AddRoadSpline(const FString& Name, bool bClosedLoop = true);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Road")
    bool AddRoadPoint(int32 SplineIndex, FVector Location, float Width, const FString& SurfaceId);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Road")
    bool MoveRoadPoint(int32 SplineIndex, int32 PointIndex, FVector Location);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Road")
    bool SetRoadPointWidth(int32 SplineIndex, int32 PointIndex, float Width);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Tile")
    bool PaintTile(FIntPoint Grid, const FString& TilesetId, int32 TileIndex, int32 LayerIndex, float RotationDegrees = 0.0f);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Tile")
    bool EraseTile(FIntPoint Grid, int32 LayerIndex);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Piece")
    FGuid PlacePiece(const FString& AssetId, FTransform Transform, int32 LayerIndex);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Zone")
    int32 AddRectangularZone(FVector2D A, FVector2D B, const FString& RulePreset, int32 LayerIndex, int32 Seed);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Zone")
    int32 RegenerateZone(int32 ZoneIndex, const TArray<FString>& AssetIds);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Race")
    bool GenerateRaceData(int32 SplineIndex = 0);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Generation")
    bool GenerateCompleteCircuit(FVector2D Center, float Radius, int32 ControlPointCount, int32 Seed, float RoadWidth, const FString& SurfaceId);

    UFUNCTION(BlueprintCallable, Category="Pixel Racer|Runtime Editor|Validation")
    bool ValidateWorkingTrack(TArray<FPixelRacerValidationMessage>& OutMessages) const;

private:
    void PushUndoSnapshot();
    void MarkEdited();
    FString ResolveUserTrackPath(const FString& FileName) const;

    TArray<FPixelRacerTrackDocument> UndoStack;
    TArray<FPixelRacerTrackDocument> RedoStack;
};
