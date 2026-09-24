#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "SPixelRacerTrackCanvas.h"
#include "PixelRacerTrackAuthoringLibrary.h"
#include "PixelRacerTrackLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPixelRacerZoneEditorTest,
    "PixelRacer.TrackEditor.ProceduralZones.TransactionsAndCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPixelRacerZoneEditorTest::RunTest(const FString& Parameters)
{
    // Exercise the actual Slate action path without touching the user's autosave.
    const TSharedRef<SPixelRacerTrackCanvas> Canvas = SNew(SPixelRacerTrackCanvas).EnableAutosave(false);
    const TSharedRef<SPixelRacerAssetBrowser> Browser = SNew(SPixelRacerAssetBrowser);
    const TArray<FPixelRacerAssetPreviewData> Catalog = Browser->GetPreviewData();
    const auto* TileEntry = Catalog.FindByPredicate([](const FPixelRacerAssetPreviewData& Entry) { return Entry.AssetId == TEXT("Tilesets/grass.png"); });
    const auto* SceneryEntry = Catalog.FindByPredicate([](const FPixelRacerAssetPreviewData& Entry) { return Entry.AssetId == TEXT("Enviroment/barrier_red.png"); });
    if (!TestNotNull(TEXT("Real grass tileset is available"), TileEntry) || !TestNotNull(TEXT("Real scenery is available"), SceneryEntry)) return false;
    const FPixelRacerAssetPreviewData Tiles = *TileEntry;
    const FPixelRacerAssetPreviewData Scenery = *SceneryEntry;
    Canvas->SetAssetPreviewData({Tiles, Scenery});
    for (const FVector& Point : {FVector(-800, -800, 0), FVector(1600, -800, 0), FVector(1600, 800, 0), FVector(-800, 800, 0)})
        UPixelRacerTrackAuthoringLibrary::AddRoadControlPoint(Canvas->GetDocument(), 0, Point, 160.0f, TEXT("asphalt"));
    auto Snapshot = [&]()
    {
        FString Json;
        UPixelRacerTrackLibrary::TrackDocumentToJson(Canvas->GetDocument(), Json);
        return Json;
    };
    auto AddZone = [&](float X)
    {
        return UPixelRacerTrackAuthoringLibrary::AddProceduralZone(Canvas->GetDocument(),
            {FVector2D(X, 0), FVector2D(X + 320, 0), FVector2D(X + 320, 320), FVector2D(X, 320)},
            TEXT("Grassland"), 0, 42);
    };
    const int32 FirstZone = AddZone(0);
    Canvas->SelectZone(FirstZone);
    Canvas->SetActiveAsset(Tiles.AssetId, Tiles.Role);
    FString Error;
    if (!TestTrue(TEXT("Assigns catalog tile asset to selected zone"), Canvas->UseSelectedAssetForZone(Error))) return false;
    Canvas->EditSelectedZone([](FPixelRacerProceduralZone& Zone) { Zone.Density = 1.0f; Zone.bAvoidRoads = false; });
    const FString Before = Snapshot();
    int32 Count = 0;
    if (!TestTrue(TEXT("Generates zone through editor action"), Canvas->GenerateZones(false, Count, Error)))
    {
        AddError(Error);
        return false;
    }
    TestTrue(TEXT("Generation creates tiles"), Count > 0 && Canvas->GetDocument().Tiles.Num() > 0);
    const FString Generated = Snapshot();
    TestTrue(TEXT("Generation can be undone once"), Canvas->Undo());
    TestEqual(TEXT("One undo restores complete pre-generation document"), Snapshot(), Before);
    TestTrue(TEXT("Generation can be redone once"), Canvas->Redo());
    TestEqual(TEXT("One redo restores generated document"), Snapshot(), Generated);

    Canvas->SelectZone(FirstZone);
    const FIntPoint ManualCell = Canvas->GetDocument().Tiles[0].Cell;
    UPixelRacerTrackAuthoringLibrary::PaintTile(Canvas->GetDocument(), ManualCell, Tiles.AssetId, 0, 0, 3);
    TestTrue(TEXT("Regenerates after a manual paint override"), Canvas->GenerateZones(false, Count, Error));
    const auto* ManualTile = Canvas->GetDocument().Tiles.FindByPredicate([&](const FPixelRacerTilePlacement& Tile) { return Tile.Cell == ManualCell; });
    TestTrue(TEXT("Manual tile choice survives editor regeneration"), ManualTile && ManualTile->bManualOverride && ManualTile->TileIndex == 3);

    const int32 SecondZone = AddZone(640);
    Canvas->SelectZone(SecondZone);
    Canvas->SetActiveAsset(Scenery.AssetId, Scenery.Role);
    TestTrue(TEXT("Assigns scenery catalog asset"), Canvas->UseSelectedAssetForZone(Error));
    Canvas->EditSelectedZone([](FPixelRacerProceduralZone& Zone) { Zone.Density = 1.0f; Zone.bAvoidRoads = false; });
    const FString BeforeAll = Snapshot();
    Canvas->SetAssetPreviewData({Tiles});
    TestFalse(TEXT("Catalog rescan removing a used asset blocks all-zone generation"), Canvas->GenerateZones(true, Count, Error));
    TestTrue(TEXT("Missing catalog asset has an actionable error"), Error.Contains(TEXT("Reassign")));
    TestEqual(TEXT("Failure in second zone rolls back first-zone work"), Snapshot(), BeforeAll);
    TestEqual(TEXT("Failed all-zone operation reports no placements"), Count, 0);
    Canvas->SetAssetPreviewData({Tiles, Scenery});
    const auto SavedRoads = Canvas->GetDocument().RoadSplines;
    Canvas->GetDocument().RoadSplines.Reset();
    const FString WithoutRoad = Snapshot();
    TestFalse(TEXT("Track details require a usable primary road"), Canvas->GenerateZones(true, Count, Error));
    TestTrue(TEXT("Missing road explains the required action"), Error.Contains(TEXT("primary road")));
    TestEqual(TEXT("Missing road leaves zone outputs unchanged"), Snapshot(), WithoutRoad);
    TestEqual(TEXT("Missing road reports no generated placements"), Count, 0);
    Canvas->GetDocument().RoadSplines = SavedRoads;
    if (!TestTrue(TEXT("Whole-track detail generation uses refreshed catalog"), Canvas->GenerateZones(true, Count, Error))) return false;
    TestTrue(TEXT("Whole-track details include scenery"), Canvas->GetDocument().Pieces.Num() > 0);
    TestTrue(TEXT("Whole-track details include checkpoints, grid, and racing lines"),
        Canvas->GetDocument().Checkpoints.Num() > 0 && Canvas->GetDocument().GridSlots.Num() == 8 && Canvas->GetDocument().RacingLines.Num() == 4);
    const FString GeneratedAll = Snapshot();
    TestTrue(TEXT("Whole-track generation is one undo step"), Canvas->Undo());
    TestEqual(TEXT("Whole-track undo restores all zones and placements"), Snapshot(), BeforeAll);
    TestTrue(TEXT("Whole-track generation redo"), Canvas->Redo());
    TestEqual(TEXT("Whole-track redo restores all zones and placements"), Snapshot(), GeneratedAll);

    Canvas->SelectZone(FirstZone);
    Canvas->EditSelectedZone([&](FPixelRacerProceduralZone& Zone) { Zone.TileIndex = Tiles.TileCount; });
    const FString InvalidIndex = Snapshot();
    TestFalse(TEXT("Out-of-range tileset cell is rejected"), Canvas->GenerateZones(false, Count, Error));
    TestEqual(TEXT("Invalid tile index never changes the document"), Snapshot(), InvalidIndex);

    // Capture the actual Slate rendering of real manifest-backed assets. This
    // preview owns its document and has autosave disabled; it cannot alter the
    // live editor sandbox. The short-lived window is always closed afterward.
    Canvas->EditSelectedZone([](FPixelRacerProceduralZone& Zone) { Zone.TileIndex = 0; });
    const TSharedRef<SWindow> PreviewWindow = SNew(SWindow)
        .Title(FText::FromString(TEXT("Pixel Racer generation validation")))
        .ClientSize(FVector2D(1000, 650))
        [ Canvas ];
    FSlateApplication::Get().AddWindow(PreviewWindow);
    ON_SCOPE_EXIT { FSlateApplication::Get().RequestDestroyWindow(PreviewWindow); };
    FSlateApplication::Get().ForceRedrawWindow(PreviewWindow);
    TArray<FColor> Pixels;
    FIntVector Size;
    if (TestTrue(TEXT("Captures generated tile and scenery canvas"), FSlateApplication::Get().TakeScreenshot(Canvas, Pixels, Size)))
    {
        TArray64<uint8> Png;
        FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Png);
        const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PixelRacer/Validation/procedural_zone_preview.png"));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        TestTrue(TEXT("Saves generation preview evidence"), FFileHelper::SaveArrayToFile(Png, *Path));
        AddInfo(Path);
    }
    return true;
}
#endif
