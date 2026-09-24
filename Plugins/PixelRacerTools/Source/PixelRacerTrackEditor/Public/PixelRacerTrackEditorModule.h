#pragma once

#include "Modules/ModuleManager.h"
#include "Input/Reply.h"
#include "PixelRacerTrackPreviewActor.h"
#include "PixelRacerTrackTypes.h"

class SDockTab;
class FSpawnTabArgs;
class SPixelRacerTrackCanvas;
class SPixelRacerAssetBrowser;
class SWidget;

class FPixelRacerTrackEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    TSharedRef<SDockTab> SpawnTrackEditorTab(const FSpawnTabArgs& Args);
    TSharedRef<SWidget> BuildZoneGenerationPanel();
    FReply HandleGenerateZones(bool bAllConfiguredZones);

    FReply HandleValidateStarterTracks();
    FReply HandleUndo();
    FReply HandleRedo();
    FReply HandleLoadSandbox();
    FReply HandleDeletePoint();
    FReply HandleSetSurface(FString SurfaceId);
    FReply HandleAdjustElevation(int32 Delta);
    FReply HandleSetZonePreset(FString Preset);
    FReply HandleZoneRectangle();
    FReply HandleZonePolygon();
    FReply HandleZoneFreehand();
    FReply HandleDeleteZone();
    void HandleAssetChosen(const FString& AssetId, const FString& Role);
    void HandleAssetBrowserManifestsReloaded();
    FReply HandleOpenSourceArt();
    FReply HandleOpenStarterTracks();
    FReply HandlePlayTrack();
    void HandlePostPIEStarted(bool bIsSimulating);
    void HandleEndPIE(bool bWasSimulating);
    FReply HandleSetMode(EPixelRacerAuthoringMode Mode);
    FReply HandleGenerateAI();
    FReply HandleBakeRoad();
    FReply HandleResetCanvas();
    FReply HandleExportSandbox();

    FText GetModeText() const;
    FText GetDocumentStatsText() const;
    FText GetSelectedPointText() const;
    FText GetZonePresetText() const;
    FText GetZoneDrawModeText() const;
    FText GetActiveAssetText() const;
    void OpenTrackEditorTab() const;
    void ShowNotification(const FText& Text, bool bSuccess) const;

    TSharedPtr<SPixelRacerTrackCanvas> TrackCanvas;
    TSharedPtr<SPixelRacerAssetBrowser> AssetBrowser;
    FDelegateHandle PostPIEStartedHandle;
    FDelegateHandle EndPIEHandle;
    FPixelRacerTrackDocument PendingPreviewDocument;
    TArray<FPixelRacerTrackPreviewAssetReference> PendingPreviewAssets;
    FPixelRacerTrackPreviewAssetReference SelectedVehicleReference;
    FPixelRacerTrackPreviewAssetReference PendingPreviewVehicleReference;
    bool bPreviewPending = false;
    bool bHasSelectedVehicle = false;
    bool bPendingPreviewHasVehicle = false;
    EPixelRacerAuthoringMode ActiveMode = EPixelRacerAuthoringMode::Spline;

    static const FName TrackEditorTabName;
};
