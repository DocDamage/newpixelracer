#include "PixelRacerTrackEditorModule.h"

#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IPluginManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet2/DebuggerCommands.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PixelRacerArcadeVehiclePawn.h"
#include "PixelRacerTrackLibrary.h"
#include "PixelRacerTrackPreviewActor.h"
#include "PixelRacerVehicleDefinition.h"
#include "PlayInEditorDataTypes.h"
#include "SPixelRacerTrackCanvas.h"
#include "SPixelRacerAssetBrowser.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FPixelRacerTrackEditorModule"

namespace
{
    static void ApplyVehicleAssetSelection(
        const FPixelRacerAssetPreviewData& Preview,
        FPixelRacerTrackPreviewAssetReference& OutVehicleReference)
    {
        OutVehicleReference.AssetId = Preview.AssetId;
        OutVehicleReference.DisplayName = Preview.DisplayName;
        OutVehicleReference.DirectionCount = Preview.DirectionCount;
        OutVehicleReference.VehicleDefinitionObjectPath = FSoftObjectPath(Preview.VehicleDefinitionObjectPath);
    }
}

const FName FPixelRacerTrackEditorModule::TrackEditorTabName(TEXT("PixelRacerTrackEditor"));

void FPixelRacerTrackEditorModule::StartupModule()
{
    PostPIEStartedHandle = FEditorDelegates::PostPIEStarted.AddRaw(this, &FPixelRacerTrackEditorModule::HandlePostPIEStarted);
    EndPIEHandle = FEditorDelegates::EndPIE.AddRaw(this, &FPixelRacerTrackEditorModule::HandleEndPIE);

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        TrackEditorTabName,
        FOnSpawnTab::CreateRaw(this, &FPixelRacerTrackEditorModule::SpawnTrackEditorTab))
        .SetDisplayName(LOCTEXT("TrackEditorTabTitle", "Pixel Racer Track Editor"))
        .SetTooltipText(LOCTEXT("TrackEditorTabTooltip", "Open the Pixel Racer track editing workspace."))
        .SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"));

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FPixelRacerTrackEditorModule::RegisterMenus));
}

void FPixelRacerTrackEditorModule::ShutdownModule()
{
    if (PostPIEStartedHandle.IsValid())
    {
        FEditorDelegates::PostPIEStarted.Remove(PostPIEStartedHandle);
        PostPIEStartedHandle.Reset();
    }
    if (EndPIEHandle.IsValid())
    {
        FEditorDelegates::EndPIE.Remove(EndPIEHandle);
        EndPIEHandle.Reset();
    }
    bPreviewPending = false;
    bHasSelectedVehicle = false;
    bPendingPreviewHasVehicle = false;
    PendingPreviewAssets.Reset();
    AssetBrowser.Reset();
    TrackCanvas.Reset();
    UToolMenus::UnRegisterStartupCallback(this);
    UToolMenus::UnregisterOwner(this);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TrackEditorTabName);
}

void FPixelRacerTrackEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* WindowMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
    FToolMenuSection& Section = WindowMenu->FindOrAddSection(TEXT("PixelRacer"));
    Section.AddMenuEntry(
        TEXT("OpenPixelRacerTrackEditor"),
        LOCTEXT("OpenTrackEditorLabel", "Pixel Racer Track Editor"),
        LOCTEXT("OpenTrackEditorTooltip", "Open the Pixel Racer track editor workspace."),
        FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.GameSettings"),
        FUIAction(FExecuteAction::CreateRaw(this, &FPixelRacerTrackEditorModule::OpenTrackEditorTab)));
}

void FPixelRacerTrackEditorModule::OpenTrackEditorTab() const
{
    FGlobalTabmanager::Get()->TryInvokeTab(TrackEditorTabName);
}

TSharedRef<SDockTab> FPixelRacerTrackEditorModule::SpawnTrackEditorTab(const FSpawnTabArgs& Args)
{
    SAssignNew(TrackCanvas, SPixelRacerTrackCanvas);
    TrackCanvas->SetMode(ActiveMode);
    SAssignNew(AssetBrowser, SPixelRacerAssetBrowser)
        .OnAssetChosen(FOnPixelRacerAssetChosen::CreateRaw(this, &FPixelRacerTrackEditorModule::HandleAssetChosen))
        .OnManifestsReloaded(FSimpleDelegate::CreateRaw(this, &FPixelRacerTrackEditorModule::HandleAssetBrowserManifestsReloaded));
    TrackCanvas->SetAssetPreviewData(AssetBrowser->GetPreviewData());

    auto ModeButton = [this](const FText& Label, EPixelRacerAuthoringMode Mode)
    {
        return SNew(SButton)
            .Text(Label)
            .OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleSetMode, Mode);
    };

    const TSharedRef<SWidget> ToolPalette =
        SNew(SBorder)
        .Padding(8.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ToolsHeader", "BUILD TOOLS"))
                    .Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
                [ ModeButton(LOCTEXT("SelectTool", "Select / edit road"), EPixelRacerAuthoringMode::Select) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ ModeButton(LOCTEXT("SplineTool", "Spline / freehand road"), EPixelRacerAuthoringMode::Spline) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ ModeButton(LOCTEXT("TileTool", "Tile paint"), EPixelRacerAuthoringMode::TilePaint) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ ModeButton(LOCTEXT("PieceTool", "Piece placement"), EPixelRacerAuthoringMode::PiecePlacement) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ ModeButton(LOCTEXT("ZoneTool", "Procedural zones"), EPixelRacerAuthoringMode::ProceduralZone) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ ModeButton(LOCTEXT("AITool", "AI / checkpoints"), EPixelRacerAuthoringMode::AIEdit) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)
                [
                    SNew(SBox)
                    .HeightOverride(320.0f)
                    [ AssetBrowser.ToSharedRef() ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
                [ SNew(SSeparator) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(SButton).Text(LOCTEXT("Undo", "Undo (Ctrl+Z)")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleUndo) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(SButton).Text(LOCTEXT("Redo", "Redo (Ctrl+Y)")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleRedo) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(SButton).Text(LOCTEXT("LoadSandbox", "Reload Exported Sandbox")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleLoadSandbox) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(SButton).Text(LOCTEXT("GenerateAI", "Generate AI + Checkpoints")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleGenerateAI) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(SButton).Text(LOCTEXT("BakeRoad", "Bake Spline to Pieces")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleBakeRoad) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(SButton).Text(LOCTEXT("ExportSandbox", "Export Sandbox Track")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleExportSandbox) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(SButton).Text(LOCTEXT("ResetCanvas", "Reset Canvas")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleResetCanvas) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 4.0f)
                [ SNew(SButton).Text(LOCTEXT("OpenSourceArt", "Open Source Art")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleOpenSourceArt) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [ SNew(SButton).Text(LOCTEXT("OpenStarterTracks", "Open Starter Tracks")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleOpenStarterTracks) ]
            ]
        ];

    const TSharedRef<SWidget> Workspace =
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 6.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock)
                .Text(LOCTEXT("WorkspaceTitle", "Pixel Racer Track Editor v0.4-dev"))
                .AutoWrapText(true)
                .Font(FAppStyle::GetFontStyle(TEXT("HeadingSmall")))
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text_Lambda([this]() { return GetModeText(); })
            ]
        ]
        + SVerticalBox::Slot().FillHeight(1.0f).Padding(8.0f)
        [
            SNew(SBox)
            [ TrackCanvas.ToSharedRef() ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(8.0f, 2.0f)
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text_Lambda([this]() { return GetDocumentStatsText(); })
        ];

    const TSharedRef<SWidget> Inspector =
        SNew(SBorder)
        .Padding(10.0f)
        [
            SNew(SScrollBox)
            + SScrollBox::Slot()
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("InspectorHeader", "AUTHORING"))
                    .Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
                [
                    SNew(STextBlock)
                    .AutoWrapText(true)
                    .Text(LOCTEXT("AuthoringHelp", "Spline: left-click adds points; Shift+click near a segment inserts a point. Select/Edit: drag points or the orange width handle; right-click/Delete removes a point. Tile Paint supports left-drag paint/right-drag erase. Procedural Zones support Rectangle drag, Polygon vertices (Enter commits), and Freehand drag authoring. Pick Tiles or Scenery in the Asset Browser to switch the active paint/place asset; drag a browser item directly onto the canvas for one-shot placement."))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ActiveAssetHeader", "ACTIVE ASSET"))
                    .Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return GetActiveAssetText(); }) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PointHeader", "SELECTED ROAD POINT"))
                    .AutoWrapText(true)
                    .Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return GetSelectedPointText(); }) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [
                    SNew(SWrapBox)
                    .UseAllottedSize(true)
                    .InnerSlotPadding(FVector2D(3.0f, 3.0f))
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("AsphaltSurface", "Asphalt")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleSetSurface, FString(TEXT("asphalt"))) ]
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("DirtSurface", "Dirt")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleSetSurface, FString(TEXT("dirt"))) ]
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("GrassSurface", "Grass")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleSetSurface, FString(TEXT("grass"))) ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [
                    SNew(SWrapBox)
                    .UseAllottedSize(true)
                    .InnerSlotPadding(FVector2D(3.0f, 3.0f))
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("SandSurface", "Sand")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleSetSurface, FString(TEXT("sand"))) ]
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("ElevationDown", "Elevation -")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleAdjustElevation, -1) ]
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("ElevationUp", "Elevation +")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleAdjustElevation, 1) ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(SButton).Text(LOCTEXT("DeleteRoadPoint", "Delete Selected Point")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleDeletePoint) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ZoneShapeHeader", "PROCEDURAL ZONE SHAPE"))
                    .AutoWrapText(true)
                    .Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return GetZoneDrawModeText(); }) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [
                    SNew(SWrapBox)
                    .UseAllottedSize(true)
                    .InnerSlotPadding(FVector2D(3.0f, 3.0f))
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("ZoneRectangle", "Rectangle")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleZoneRectangle) ]
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("ZonePolygon", "Polygon")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleZonePolygon) ]
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("ZoneFreehand", "Freehand")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleZoneFreehand) ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [
                    SNew(STextBlock)
                    .AutoWrapText(true)
                    .Text(LOCTEXT("ZoneShapeHelp", "Polygon: left-click vertices, Enter to finish, right-click/Esc to cancel. Drag a zone vertex to reshape it. Delete removes the selected zone."))
                    .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(SButton).Text(LOCTEXT("DeleteSelectedZone", "Delete Selected Zone")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleDeleteZone) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
                [ BuildZoneGenerationPanel() ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("ZonePresetHeader", "PROCEDURAL ZONE PRESET"))
                    .AutoWrapText(true)
                    .Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return GetZonePresetText(); }) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 3.0f)
                [
                    SNew(SWrapBox)
                    .UseAllottedSize(true)
                    .InnerSlotPadding(FVector2D(3.0f, 3.0f))
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("GrasslandZone", "Grassland")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleSetZonePreset, FString(TEXT("Grassland"))) ]
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("BarrierZone", "Barrier Edge")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleSetZonePreset, FString(TEXT("Barrier Edge"))) ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 3.0f)
                [
                    SNew(SWrapBox)
                    .UseAllottedSize(true)
                    .InnerSlotPadding(FVector2D(3.0f, 3.0f))
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("CrowdZone", "Crowd")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleSetZonePreset, FString(TEXT("Crowd"))) ]
                    + SWrapBox::Slot()
                    [ SNew(SButton).Text(LOCTEXT("ParkingZone", "Parking")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleSetZonePreset, FString(TEXT("Parking"))) ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [ SNew(SSeparator) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ValidateStarters", "Validate Starter Track Data"))
                    .OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleValidateStarterTracks)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("PlayTrack", "Play Track (PIE)"))
                    .ToolTipText(LOCTEXT("PlayTrackTip", "Select and import a vehicle in the Asset Browser to drive this track. Use WASD or arrows to steer, Space to drift, and R to reset to the start. Starts normal in-process PIE."))
                    .OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandlePlayTrack)
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [
                    SNew(STextBlock)
                    .AutoWrapText(true)
                    .Text(LOCTEXT("WorkflowText", "Daily loop stays edit → validate → PIE → Esc → edit. Full builds remain release/core-code gates."))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 12.0f)
                [
                    SNew(STextBlock)
                    .Text(LOCTEXT("PluginHeader", "OPTIONAL INTEGRATIONS"))
                    .Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("Paper2DPlus", "Paper2D+: preferred sprite slicing/pixel-editing workflow; not a hard dependency.")) ]
                + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f)
                [ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("PaperZD", "PaperZD remains deferred until vehicle/rider animation requires it.")) ]
            ]
        ];

    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(0.30f).Padding(4.0f)
            [ ToolPalette ]
            + SHorizontalBox::Slot().FillWidth(0.48f).Padding(4.0f)
            [ Workspace ]
            + SHorizontalBox::Slot().FillWidth(0.22f).Padding(4.0f)
            [ Inspector ]
        ];
}

FReply FPixelRacerTrackEditorModule::HandleSetMode(const EPixelRacerAuthoringMode Mode)
{
    ActiveMode = Mode;
    if (TrackCanvas.IsValid())
    {
        TrackCanvas->SetMode(Mode);
    }
    return FReply::Handled();
}

FText FPixelRacerTrackEditorModule::GetModeText() const
{
    const TCHAR* Name = TEXT("Select");
    switch (ActiveMode)
    {
    case EPixelRacerAuthoringMode::Select: Name = TEXT("Select / Edit Road"); break;
    case EPixelRacerAuthoringMode::Spline: Name = TEXT("Spline / Freehand Road"); break;
    case EPixelRacerAuthoringMode::TilePaint: Name = TEXT("Tile Paint"); break;
    case EPixelRacerAuthoringMode::PiecePlacement: Name = TEXT("Piece Placement"); break;
    case EPixelRacerAuthoringMode::ProceduralZone: Name = TEXT("Procedural Zone"); break;
    case EPixelRacerAuthoringMode::AIEdit: Name = TEXT("AI / Checkpoints"); break;
    default: break;
    }
    return FText::Format(LOCTEXT("ActiveModeFmt", "Mode: {0}"), FText::FromString(Name));
}

FText FPixelRacerTrackEditorModule::GetDocumentStatsText() const
{
    if (!TrackCanvas.IsValid())
    {
        return LOCTEXT("NoDocumentStats", "No active sandbox document.");
    }
    const FPixelRacerTrackDocument& D = TrackCanvas->GetDocument();
    int32 ControlPointCount = 0;
    for (const FPixelRacerRoadSpline& Spline : D.RoadSplines)
    {
        ControlPointCount += Spline.ControlPoints.Num();
    }
    return FText::Format(
        LOCTEXT("DocumentStatsFmt", "Road points: {0}   Tiles: {1}   Pieces: {2}   Zones: {3}   Checkpoints: {4}   Grid: {5}   Racing lines: {6}"),
        FText::AsNumber(ControlPointCount), FText::AsNumber(D.Tiles.Num()), FText::AsNumber(D.Pieces.Num()),
        FText::AsNumber(D.ProceduralZones.Num()), FText::AsNumber(D.Checkpoints.Num()), FText::AsNumber(D.GridSlots.Num()), FText::AsNumber(D.RacingLines.Num()));
}

FReply FPixelRacerTrackEditorModule::HandleUndo()
{
    const bool bResult = TrackCanvas.IsValid() && TrackCanvas->Undo();
    if (!bResult)
    {
        ShowNotification(LOCTEXT("NothingToUndo", "Nothing to undo."), false);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleRedo()
{
    const bool bResult = TrackCanvas.IsValid() && TrackCanvas->Redo();
    if (!bResult)
    {
        ShowNotification(LOCTEXT("NothingToRedo", "Nothing to redo."), false);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleLoadSandbox()
{
    FString Error;
    const bool bLoaded = TrackCanvas.IsValid() && TrackCanvas->LoadSandboxTrack(Error);
    ShowNotification(bLoaded ? LOCTEXT("SandboxReloaded", "Reloaded exported sandbox track.") : FText::FromString(Error.IsEmpty() ? TEXT("Sandbox export could not be loaded.") : Error), bLoaded);
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleDeletePoint()
{
    const bool bResult = TrackCanvas.IsValid() && TrackCanvas->DeleteSelectedRoadPoint();
    if (!bResult)
    {
        ShowNotification(LOCTEXT("NoPointToDelete", "Select a road point first."), false);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleSetSurface(FString SurfaceId)
{
    const bool bResult = TrackCanvas.IsValid() && TrackCanvas->SetSelectedRoadPointSurface(SurfaceId);
    if (!bResult)
    {
        ShowNotification(LOCTEXT("NoPointForSurface", "Select a road point before changing its surface."), false);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleAdjustElevation(const int32 Delta)
{
    const bool bResult = TrackCanvas.IsValid() && TrackCanvas->AdjustSelectedRoadPointElevation(Delta);
    if (!bResult)
    {
        ShowNotification(LOCTEXT("NoPointForElevation", "Select a road point before changing elevation."), false);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleZoneRectangle()
{
    ActiveMode = EPixelRacerAuthoringMode::ProceduralZone;
    if (TrackCanvas.IsValid())
    {
        TrackCanvas->SetMode(ActiveMode);
        TrackCanvas->SetZoneDrawMode(EPixelRacerZoneDrawMode::Rectangle);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleZonePolygon()
{
    ActiveMode = EPixelRacerAuthoringMode::ProceduralZone;
    if (TrackCanvas.IsValid())
    {
        TrackCanvas->SetMode(ActiveMode);
        TrackCanvas->SetZoneDrawMode(EPixelRacerZoneDrawMode::Polygon);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleZoneFreehand()
{
    ActiveMode = EPixelRacerAuthoringMode::ProceduralZone;
    if (TrackCanvas.IsValid())
    {
        TrackCanvas->SetMode(ActiveMode);
        TrackCanvas->SetZoneDrawMode(EPixelRacerZoneDrawMode::Freehand);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleDeleteZone()
{
    const bool bDeleted = TrackCanvas.IsValid() && TrackCanvas->DeleteSelectedZone();
    if (!bDeleted)
    {
        ShowNotification(LOCTEXT("NoZoneToDelete", "Select a procedural-zone vertex first."), false);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleSetZonePreset(FString Preset)
{
    if (TrackCanvas.IsValid())
    {
        TrackCanvas->SetActiveZonePreset(Preset);
    }
    return FReply::Handled();
}

TSharedRef<SWidget> FPixelRacerTrackEditorModule::BuildZoneGenerationPanel()
{
    auto FloatSetting = [this](const FText& Label, float FPixelRacerProceduralZone::* Field,
        float Minimum, float Maximum, bool bSceneryOnly = false) -> TSharedRef<SWidget>
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [ SNew(STextBlock).Text(Label).AutoWrapText(true) ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(SNumericEntryBox<float>).AllowSpin(false).MinValue(Minimum).MaxValue(Maximum)
                .IsEnabled_Lambda([this, bSceneryOnly]()
                {
                    const auto* Zone = TrackCanvas.IsValid() ? TrackCanvas->GetSelectedZone() : nullptr;
                    return Zone && (!bSceneryOnly || !Zone->bGenerateTiles);
                })
                .Value_Lambda([this, Field]() -> TOptional<float>
                {
                    const auto* Zone = TrackCanvas.IsValid() ? TrackCanvas->GetSelectedZone() : nullptr;
                    return Zone ? TOptional<float>(Zone->*Field) : TOptional<float>();
                })
                .OnValueCommitted_Lambda([this, Field, Minimum, Maximum](float Value, ETextCommit::Type)
                {
                    if (TrackCanvas.IsValid() && FMath::IsFinite(Value))
                        TrackCanvas->EditSelectedZone([&](FPixelRacerProceduralZone& Zone) { Zone.*Field = FMath::Clamp(Value, Minimum, Maximum); });
                })
            ];
    };
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [ SNew(STextBlock).Text(LOCTEXT("ZoneGenerationHeader", "ZONE CONTENT")).Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall"))) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ SNew(STextBlock).AutoWrapText(true).Text_Lambda([this]() { return TrackCanvas->GetSelectedZoneSummary(); }) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ SNew(SButton).Text(LOCTEXT("NextZone", "Next Zone")).OnClicked_Lambda([this]()
            { TrackCanvas->SelectNextZone(); return FReply::Handled(); }) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ SNew(SButton).Text(LOCTEXT("UseZoneAsset", "Use Selected Asset")).OnClicked_Lambda([this]()
            {
                FString Error;
                const bool bSuccess = TrackCanvas->UseSelectedAssetForZone(Error);
                ShowNotification(bSuccess ? LOCTEXT("ZoneAssetAssigned", "Asset assigned. Generate the zone to apply it.") : FText::FromString(Error), bSuccess);
                return FReply::Handled();
            }) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ FloatSetting(LOCTEXT("ZoneDensity", "Density (0–1)"), &FPixelRacerProceduralZone::Density, 0.0f, 1.0f) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ FloatSetting(LOCTEXT("ZoneSpacing", "Scenery spacing (tiles use the 32-unit grid)"), &FPixelRacerProceduralZone::Spacing, 1.0f, 4096.0f, true) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ SNew(STextBlock).Text(LOCTEXT("ZoneSeed", "Seed")) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SNumericEntryBox<int32>).AllowSpin(false)
            .IsEnabled_Lambda([this]() { return TrackCanvas->GetSelectedZone() != nullptr; })
            .Value_Lambda([this]() -> TOptional<int32> { const auto* Zone = TrackCanvas->GetSelectedZone(); return Zone ? TOptional<int32>(Zone->Seed) : TOptional<int32>(); })
            .OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type)
                { TrackCanvas->EditSelectedZone([&](FPixelRacerProceduralZone& Zone) { Zone.Seed = Value; }); })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ SNew(STextBlock).Text(LOCTEXT("ZoneTileIndex", "Tileset cell index")) ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SNumericEntryBox<int32>).AllowSpin(false).MinValue(0)
            .IsEnabled_Lambda([this]() { const auto* Zone = TrackCanvas->GetSelectedZone(); return Zone && Zone->bGenerateTiles; })
            .Value_Lambda([this]() -> TOptional<int32> { const auto* Zone = TrackCanvas->GetSelectedZone(); return Zone ? TOptional<int32>(Zone->TileIndex) : TOptional<int32>(); })
            .OnValueCommitted_Lambda([this](int32 Value, ETextCommit::Type)
                { TrackCanvas->EditSelectedZone([&](FPixelRacerProceduralZone& Zone) { Zone.TileIndex = FMath::Max(0, Value); }); })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [
            SNew(SCheckBox)
            .IsEnabled_Lambda([this]() { return TrackCanvas->GetSelectedZone() != nullptr; })
            .IsChecked_Lambda([this]() { const auto* Zone = TrackCanvas->GetSelectedZone(); return Zone && Zone->bAvoidRoads ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
            .OnCheckStateChanged_Lambda([this](ECheckBoxState State)
                { TrackCanvas->EditSelectedZone([&](FPixelRacerProceduralZone& Zone) { Zone.bAvoidRoads = State == ECheckBoxState::Checked; }); })
            [ SNew(STextBlock).Text(LOCTEXT("ZoneAvoidRoads", "Keep roads clear")) ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ FloatSetting(LOCTEXT("ZoneClearance", "Extra road clearance"), &FPixelRacerProceduralZone::RoadClearance, 0.0f, 4096.0f) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ SNew(SButton).Text(LOCTEXT("RegenerateZone", "Generate / Regenerate Zone")).OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleGenerateZones, false) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ SNew(SButton).Text(LOCTEXT("RestoreZoneSlots", "Restore Erased Slots on Next Generate"))
            .IsEnabled_Lambda([this]() { const auto* Zone = TrackCanvas->GetSelectedZone(); return Zone && Zone->SuppressedCells.Num() > 0; })
            .OnClicked_Lambda([this]() { TrackCanvas->EditSelectedZone([](FPixelRacerProceduralZone& Zone) { Zone.SuppressedCells.Reset(); }); return FReply::Handled(); }) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ SNew(SButton).Text(LOCTEXT("GenerateTrackDetails", "Generate Track Details"))
            .ToolTipText(LOCTEXT("GenerateTrackDetailsHelp", "Regenerate all configured zones, checkpoints, starting grid, and racing lines from the existing road. One Undo restores the whole operation."))
            .OnClicked_Raw(this, &FPixelRacerTrackEditorModule::HandleGenerateZones, true) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 3)
        [ SNew(STextBlock).AutoWrapText(true).Text(LOCTEXT("ZoneGenerationHelp", "Manual edits and erased slots survive regeneration. Changes apply when you Generate. Export saves these settings with the track.")) ];
}

FReply FPixelRacerTrackEditorModule::HandleGenerateZones(bool bAllConfiguredZones)
{
    if (TrackCanvas.IsValid())
    {
        int32 Count = 0;
        FString Error;
        const bool bSuccess = TrackCanvas->GenerateZones(bAllConfiguredZones, Count, Error);
        ShowNotification(bSuccess
            ? FText::Format(LOCTEXT("ZoneGeneratedCount", "Generated {0} placements. Manual edits preserved; Undo restores the previous track."), FText::AsNumber(Count))
            : FText::FromString(Error), bSuccess);
    }
    return FReply::Handled();
}

void FPixelRacerTrackEditorModule::HandleAssetChosen(const FString& AssetId, const FString& Role)
{
    if (!TrackCanvas.IsValid() || !TrackCanvas->SetActiveAsset(AssetId, Role))
    {
        return;
    }

    if (Role == TEXT("tileset"))
    {
        ActiveMode = EPixelRacerAuthoringMode::TilePaint;
        TrackCanvas->SetMode(ActiveMode);
    }
    else if (Role == TEXT("environment_piece"))
    {
        ActiveMode = EPixelRacerAuthoringMode::PiecePlacement;
        TrackCanvas->SetMode(ActiveMode);
    }

    bHasSelectedVehicle = false;
    if (Role == TEXT("vehicle_sprite_sheet") && AssetBrowser.IsValid())
    {
        for (const FPixelRacerAssetPreviewData& Preview : AssetBrowser->GetPreviewData())
        {
            if (Preview.AssetId == AssetId && Preview.Role == Role)
            {
                ApplyVehicleAssetSelection(Preview, SelectedVehicleReference);
                bHasSelectedVehicle = SelectedVehicleReference.VehicleDefinitionObjectPath.IsValid();
                break;
            }
        }
    }
}

void FPixelRacerTrackEditorModule::HandleAssetBrowserManifestsReloaded()
{
    bHasSelectedVehicle = false;
    if (TrackCanvas.IsValid() && AssetBrowser.IsValid())
    {
        TrackCanvas->SetAssetPreviewData(AssetBrowser->GetPreviewData());
    }
}

FText FPixelRacerTrackEditorModule::GetActiveAssetText() const
{
    return TrackCanvas.IsValid()
        ? FText::FromString(TrackCanvas->GetActiveAssetSummary())
        : LOCTEXT("NoActiveAsset", "No asset selected.");
}

FText FPixelRacerTrackEditorModule::GetSelectedPointText() const
{
    return TrackCanvas.IsValid() ? TrackCanvas->GetSelectedRoadPointSummary() : LOCTEXT("NoSelectedPoint", "No road point selected.");
}

FText FPixelRacerTrackEditorModule::GetZoneDrawModeText() const
{
    return TrackCanvas.IsValid()
        ? FText::Format(LOCTEXT("ZoneDrawModeFmt", "Shape: {0}"), FText::FromString(TrackCanvas->GetZoneDrawModeName()))
        : LOCTEXT("NoZoneDrawMode", "Shape: Rectangle");
}

FText FPixelRacerTrackEditorModule::GetZonePresetText() const
{
    return TrackCanvas.IsValid()
        ? FText::Format(LOCTEXT("ZonePresetFmt", "Active: {0}"), FText::FromString(TrackCanvas->GetActiveZonePreset()))
        : LOCTEXT("NoZonePreset", "Active: Grassland");
}

FReply FPixelRacerTrackEditorModule::HandleGenerateAI()
{
    if (TrackCanvas.IsValid())
    {
        TrackCanvas->GenerateAI();
        ShowNotification(LOCTEXT("GeneratedAI", "Generated checkpoints and four editable racing lines from the primary road."), true);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleBakeRoad()
{
    const int32 Count = TrackCanvas.IsValid() ? TrackCanvas->BakeRoad() : 0;
    ShowNotification(FText::Format(LOCTEXT("BakeSummary", "Baked {0} road pieces from the primary spline."), FText::AsNumber(Count)), Count > 0);
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleResetCanvas()
{
    if (TrackCanvas.IsValid())
    {
        TrackCanvas->ResetDocument();
        ShowNotification(LOCTEXT("CanvasReset", "Sandbox track reset."), true);
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleExportSandbox()
{
    if (!TrackCanvas.IsValid())
    {
        return FReply::Handled();
    }

    const FString ExportDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PixelRacer/Tracks"));
    const FString ExportPath = FPaths::Combine(ExportDir, TEXT("editor_sandbox.pixeltrack.json"));
    FString Error;
    const bool bSaved = UPixelRacerTrackLibrary::ExportTrackDocument(TrackCanvas->GetDocument(), ExportPath, Error);
    ShowNotification(bSaved ? FText::Format(LOCTEXT("ExportedSandbox", "Exported sandbox track to {0}"), FText::FromString(ExportPath)) : FText::FromString(Error), bSaved);
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleValidateStarterTracks()
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PixelRacerTools"));
    if (!Plugin.IsValid())
    {
        ShowNotification(LOCTEXT("PluginMissing", "PixelRacerTools plugin path could not be resolved."), false);
        return FReply::Handled();
    }

    const FString StarterTrackDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/StarterTracks"));
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *FPaths::Combine(StarterTrackDir, TEXT("*.pixeltrack.json")), true, false);

    int32 ValidCount = 0;
    int32 ErrorCount = 0;
    int32 WarningCount = 0;

    for (const FString& File : Files)
    {
        FString Json;
        const FString FullPath = FPaths::Combine(StarterTrackDir, File);
        if (!FFileHelper::LoadFileToString(Json, *FullPath))
        {
            ++ErrorCount;
            continue;
        }

        FPixelRacerTrackDocument Document;
        FString ParseError;
        if (!UPixelRacerTrackLibrary::TrackDocumentFromJson(Json, Document, ParseError))
        {
            ++ErrorCount;
            continue;
        }

        TArray<FPixelRacerValidationMessage> Messages;
        const bool bValid = UPixelRacerTrackLibrary::ValidateTrackDocument(Document, Messages);
        ValidCount += bValid ? 1 : 0;
        ErrorCount += bValid ? 0 : 1;
        for (const FPixelRacerValidationMessage& Message : Messages)
        {
            WarningCount += Message.Severity == EPixelRacerValidationSeverity::Warning ? 1 : 0;
        }
    }

    const FText Summary = FText::Format(
        LOCTEXT("ValidationSummary", "Starter tracks: {0} valid, {1} invalid, {2} warnings."),
        FText::AsNumber(ValidCount), FText::AsNumber(ErrorCount), FText::AsNumber(WarningCount));
    ShowNotification(Summary, ErrorCount == 0 && Files.Num() > 0);
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleOpenSourceArt()
{
    FPlatformProcess::ExploreFolder(*FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt"))));
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandleOpenStarterTracks()
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PixelRacerTools"));
    if (Plugin.IsValid())
    {
        FPlatformProcess::ExploreFolder(*FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/StarterTracks")));
    }
    return FReply::Handled();
}

FReply FPixelRacerTrackEditorModule::HandlePlayTrack()
{
    bPreviewPending = TrackCanvas.IsValid();
    bPendingPreviewHasVehicle = TrackCanvas.IsValid() && bHasSelectedVehicle;
    if (bPendingPreviewHasVehicle)
    {
        UPixelRacerVehicleDefinition* VehicleDefinition = Cast<UPixelRacerVehicleDefinition>(
            SelectedVehicleReference.VehicleDefinitionObjectPath.TryLoad());
        if (VehicleDefinition == nullptr ||
            VehicleDefinition->DirectionalSprites.Num() != SelectedVehicleReference.DirectionCount ||
            VehicleDefinition->DirectionalSprites.Contains(nullptr))
        {
            bPreviewPending = false;
            bPendingPreviewHasVehicle = false;
            ShowNotification(
                FText::Format(
                    LOCTEXT("VehicleDefinitionNeedsImport", "Import {0} in the Asset Browser before driving."),
                    FText::FromString(SelectedVehicleReference.DisplayName)),
                false);
            return FReply::Handled();
        }
    }
    PendingPreviewAssets.Reset();
    if (bPendingPreviewHasVehicle)
    {
        PendingPreviewVehicleReference = SelectedVehicleReference;
    }
    if (bPreviewPending)
    {
        PendingPreviewDocument = TrackCanvas->GetDocument();
        if (AssetBrowser.IsValid())
        {
            const TArray<FPixelRacerAssetPreviewData> PreviewData = AssetBrowser->GetPreviewData();
            PendingPreviewAssets.Reserve(PreviewData.Num());
            for (const FPixelRacerAssetPreviewData& Preview : PreviewData)
            {
                FPixelRacerTrackPreviewAssetReference& Reference = PendingPreviewAssets.AddDefaulted_GetRef();
                Reference.AssetId = Preview.AssetId;
                Reference.DisplayName = Preview.DisplayName;
                Reference.SpriteObjectPath = FSoftObjectPath(Preview.SpriteObjectPath);
                Reference.TileSetObjectPath = FSoftObjectPath(Preview.TileSetObjectPath);
                Reference.TileSize = FIntPoint(Preview.TileWidth, Preview.TileHeight);
                Reference.ImageSize = FIntPoint(Preview.Width, Preview.Height);
                Reference.DirectionCount = Preview.DirectionCount;
                Reference.VehicleDefinitionObjectPath = FSoftObjectPath(Preview.VehicleDefinitionObjectPath);
            }
        }
    }

    if (FPlayWorldCommands::GlobalPlayWorldActions.IsValid() && FPlayWorldCommands::Get().PlayInViewport.IsValid())
    {
        const bool bExecuted = FPlayWorldCommands::GlobalPlayWorldActions->TryExecuteAction(FPlayWorldCommands::Get().PlayInViewport.ToSharedRef());
        if (!bExecuted)
        {
            bPreviewPending = false;
            bPendingPreviewHasVehicle = false;
            PendingPreviewAssets.Reset();
        }
        ShowNotification(bExecuted ? LOCTEXT("PIEStarted", "Play In Editor requested.") : LOCTEXT("PIEUnavailable", "Play In Editor could not start in the current editor state."), bExecuted);
    }
    else
    {
        bPreviewPending = false;
        bPendingPreviewHasVehicle = false;
        PendingPreviewAssets.Reset();
        ShowNotification(LOCTEXT("PIECommandsUnavailable", "Play In Editor commands are not available yet."), false);
    }
    return FReply::Handled();
}

void FPixelRacerTrackEditorModule::HandlePostPIEStarted(const bool bIsSimulating)
{
    if (!bPreviewPending)
    {
        return;
    }
    bPreviewPending = false;

    if (GEngine == nullptr)
    {
        UE_LOG(LogTemp, Warning, TEXT("Pixel Racer could not access the PIE engine while creating the TrackDocument preview."));
        return;
    }

    int32 PreviewWorldCount = 0;
    for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
    {
        UWorld* PIEWorld = WorldContext.World();
        if (WorldContext.WorldType != EWorldType::PIE || PIEWorld == nullptr)
        {
            continue;
        }

        FActorSpawnParameters SpawnParameters;
        SpawnParameters.ObjectFlags |= RF_Transient;
        SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        APixelRacerTrackPreviewActor* PreviewActor = PIEWorld->SpawnActor<APixelRacerTrackPreviewActor>(
            FVector::ZeroVector,
            FRotator::ZeroRotator,
            SpawnParameters);
        if (PreviewActor == nullptr)
        {
            continue;
        }

        FString Warning;
        if (!PreviewActor->BuildPreview(PendingPreviewDocument, PendingPreviewAssets, Warning))
        {
            PreviewActor->Destroy();
            continue;
        }

        APixelRacerArcadeVehiclePawn* VehiclePawn = nullptr;
        if (bPendingPreviewHasVehicle)
        {
            FString VehicleWarning;
            VehiclePawn = PreviewActor->SpawnPlayerVehicle(PendingPreviewDocument, PendingPreviewVehicleReference, VehicleWarning);
            if (!VehicleWarning.IsEmpty())
            {
                UE_LOG(LogTemp, Warning, TEXT("%s"), *VehicleWarning);
            }
        }

        if (APlayerController* PlayerController = PIEWorld->GetFirstPlayerController())
        {
            if (VehiclePawn != nullptr)
            {
                PlayerController->Possess(VehiclePawn);
            }
            else
            {
                PlayerController->SetViewTarget(PreviewActor);
            }
        }
        if (!Warning.IsEmpty())
        {
            UE_LOG(LogTemp, Warning, TEXT("%s"), *Warning);
        }
        ++PreviewWorldCount;
    }

    if (PreviewWorldCount == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("Pixel Racer could not create a PIE preview world for the current TrackDocument."));
    }
}

void FPixelRacerTrackEditorModule::HandleEndPIE(const bool bWasSimulating)
{
    bPreviewPending = false;
    bPendingPreviewHasVehicle = false;
    PendingPreviewAssets.Reset();
}

void FPixelRacerTrackEditorModule::ShowNotification(const FText& Text, const bool bSuccess) const
{
    FNotificationInfo Info(Text);
    Info.ExpireDuration = 4.0f;
    Info.bUseLargeFont = false;
    Info.Image = FAppStyle::GetBrush(bSuccess ? TEXT("Icons.Success") : TEXT("Icons.Warning"));
    FSlateNotificationManager::Get().AddNotification(Info);
}

IMPLEMENT_MODULE(FPixelRacerTrackEditorModule, PixelRacerTrackEditor)

#undef LOCTEXT_NAMESPACE
