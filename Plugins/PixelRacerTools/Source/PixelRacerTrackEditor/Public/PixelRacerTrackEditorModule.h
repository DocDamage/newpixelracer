#pragma once

#include "Modules/ModuleManager.h"
#include "Input/Reply.h"
#include "PixelRacerTrackTypes.h"

class SDockTab;
class FSpawnTabArgs;
class SPixelRacerTrackCanvas;
class SPixelRacerAssetBrowser;

class FPixelRacerTrackEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    TSharedRef<SDockTab> SpawnTrackEditorTab(const FSpawnTabArgs& Args);

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
    FReply HandleOpenSourceArt();
    FReply HandleOpenStarterTracks();
    FReply HandlePlayTrack();
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
    EPixelRacerAuthoringMode ActiveMode = EPixelRacerAuthoringMode::Spline;

    static const FName TrackEditorTabName;
};
