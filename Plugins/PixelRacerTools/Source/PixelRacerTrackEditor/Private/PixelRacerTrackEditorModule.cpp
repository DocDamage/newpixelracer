#include "PixelRacerTrackEditorModule.h"

#include "Editor.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/DebuggerCommands.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PixelRacerTrackLibrary.h"
#include "PlayInEditorDataTypes.h"
#include "SPixelRacerTrackCanvas.h"
#include "SPixelRacerAssetBrowser.h"
#include "Styling/AppStyle.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
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

const FName FPixelRacerTrackEditorModule::TrackEditorTabName(TEXT("PixelRacerTrackEditor"));

void FPixelRacerTrackEditorModule::StartupModule()
{
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
        .OnAssetChosen(FOnPixelRacerAssetChosen::CreateRaw(this, &FPixelRacerTrackEditorModule::HandleAssetChosen));

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
                    .ToolTipText(LOCTEXT("PlayTrackTip", "Starts normal in-process Play In Editor. No cook or packaged build."))
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
    if (FPlayWorldCommands::GlobalPlayWorldActions.IsValid() && FPlayWorldCommands::Get().PlayInViewport.IsValid())
    {
        const bool bExecuted = FPlayWorldCommands::GlobalPlayWorldActions->TryExecuteAction(FPlayWorldCommands::Get().PlayInViewport.ToSharedRef());
        ShowNotification(bExecuted ? LOCTEXT("PIEStarted", "Play In Editor requested.") : LOCTEXT("PIEUnavailable", "Play In Editor could not start in the current editor state."), bExecuted);
    }
    else
    {
        ShowNotification(LOCTEXT("PIECommandsUnavailable", "Play In Editor commands are not available yet."), false);
    }
    return FReply::Handled();
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
