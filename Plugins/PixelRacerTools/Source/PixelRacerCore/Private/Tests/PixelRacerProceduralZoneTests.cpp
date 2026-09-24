#include "PixelRacerTrackAuthoringLibrary.h"
#include "PixelRacerTrackLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

namespace PixelRacerProceduralZoneTests
{
    static constexpr int32 ZoneLayer = 4;

    static FPixelRacerTrackDocument MakeDocument(
        const TArray<FVector2D>& Polygon,
        const bool bGenerateTiles,
        const float Spacing = 32.0f)
    {
        FPixelRacerTrackDocument Document;
        const int32 ZoneIndex = UPixelRacerTrackAuthoringLibrary::AddProceduralZone(
            Document,
            Polygon,
            TEXT("TestPreset"),
            ZoneLayer,
            12345);
        if (!Document.ProceduralZones.IsValidIndex(ZoneIndex))
        {
            return Document;
        }
        FPixelRacerProceduralZone& Zone = Document.ProceduralZones[ZoneIndex];
        Zone.Id = FGuid(0x11223344, 0x55667788, 0x99aabbcc, 0xddeeff00);
        Zone.AssetId = bGenerateTiles ? TEXT("test.tiles") : TEXT("test.scenery");
        Zone.bGenerateTiles = bGenerateTiles;
        Zone.TileIndex = 3;
        Zone.Spacing = Spacing;
        Zone.Density = 1.0f;
        Zone.bAvoidRoads = false;
        return Document;
    }

    static TArray<FVector2D> Square(const float MinX, const float MinY, const float MaxX, const float MaxY)
    {
        return {
            FVector2D(MinX, MinY),
            FVector2D(MaxX, MinY),
            FVector2D(MaxX, MaxY),
            FVector2D(MinX, MaxY)
        };
    }

    static bool Generate(FPixelRacerTrackDocument& Document, int32& OutCount, FString& OutError)
    {
        return UPixelRacerTrackAuthoringLibrary::GenerateProceduralZone(Document, 0, OutCount, OutError);
    }

    static int32 CountZonePieces(const FPixelRacerTrackDocument& Document, const FGuid& ZoneId)
    {
        int32 Count = 0;
        for (const FPixelRacerPiecePlacement& Piece : Document.Pieces)
        {
            Count += Piece.SourceZoneId == ZoneId ? 1 : 0;
        }
        return Count;
    }

    static int32 CountZoneTiles(const FPixelRacerTrackDocument& Document, const FGuid& ZoneId)
    {
        int32 Count = 0;
        for (const FPixelRacerTilePlacement& Tile : Document.Tiles)
        {
            Count += Tile.SourceZoneId == ZoneId ? 1 : 0;
        }
        return Count;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPixelRacerProceduralZoneDeterminismTest,
    "PixelRacer.ProceduralZones.DeterminismJsonEquality",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPixelRacerProceduralZoneDeterminismTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace PixelRacerProceduralZoneTests;

    FPixelRacerTrackDocument Document = MakeDocument(Square(0.0f, 0.0f, 256.0f, 256.0f), false, 32.0f);
    if (!TestTrue(TEXT("Determinism fixture contains a zone"), Document.ProceduralZones.Num() == 1)) return false;
    Document.ProceduralZones[0].Density = 0.61f;
    int32 GeneratedCount = 0;
    FString Error;
    TestTrue(TEXT("First generation succeeds"), Generate(Document, GeneratedCount, Error));
    TestTrue(TEXT("First generation produces placements"), GeneratedCount > 0);

    FString FirstJson;
    TestTrue(TEXT("First generated document serializes"), UPixelRacerTrackLibrary::TrackDocumentToJson(Document, FirstJson));
    TestTrue(TEXT("Second generation succeeds"), Generate(Document, GeneratedCount, Error));
    FString SecondJson;
    TestTrue(TEXT("Second generated document serializes"), UPixelRacerTrackLibrary::TrackDocumentToJson(Document, SecondJson));
    TestEqual(TEXT("Regeneration is byte-for-byte deterministic"), SecondJson, FirstJson);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPixelRacerProceduralZoneConcaveTest,
    "PixelRacer.ProceduralZones.ConcaveMembership",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPixelRacerProceduralZoneConcaveTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace PixelRacerProceduralZoneTests;

    const TArray<FVector2D> ConcavePolygon = {
        FVector2D(0.0f, 0.0f), FVector2D(96.0f, 0.0f), FVector2D(96.0f, 32.0f),
        FVector2D(32.0f, 32.0f), FVector2D(32.0f, 96.0f), FVector2D(0.0f, 96.0f)
    };
    FPixelRacerTrackDocument Document = MakeDocument(ConcavePolygon, true);
    int32 GeneratedCount = 0;
    FString Error;
    TestTrue(TEXT("Concave zone generation succeeds"), Generate(Document, GeneratedCount, Error));
    TestEqual(TEXT("Only cells inside the L-shaped polygon are generated"), GeneratedCount, 5);
    TestFalse(TEXT("The concave cutout remains empty"), Document.Tiles.ContainsByPredicate([](const FPixelRacerTilePlacement& Tile)
    {
        return Tile.Cell.X > 0 && Tile.Cell.Y > 0;
    }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPixelRacerProceduralZoneOverridesTest,
    "PixelRacer.ProceduralZones.OverridesLockedAndSuppression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPixelRacerProceduralZoneOverridesTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace PixelRacerProceduralZoneTests;

    FPixelRacerTrackDocument Document = MakeDocument(Square(0.0f, 0.0f, 128.0f, 128.0f), false, 64.0f);
    if (!TestTrue(TEXT("Override fixture contains a zone"), Document.ProceduralZones.Num() == 1)) return false;
    const FGuid ZoneId = Document.ProceduralZones[0].Id;
    int32 GeneratedCount = 0;
    FString Error;
    if (!TestTrue(TEXT("Initial piece generation succeeds"), Generate(Document, GeneratedCount, Error))) return false;
    TestEqual(TEXT("Initial zone has four piece slots"), GeneratedCount, 4);
    if (!TestTrue(TEXT("Initial generation produced the required override fixtures"), Document.Pieces.Num() >= 3)) return false;

    Document.Pieces[0].bManualOverride = true;
    const FGuid ManualId = Document.Pieces[0].Id;
    const FIntPoint ManualSourceCell = Document.Pieces[0].SourceZoneCell;
    Document.Pieces[0].Transform.SetTranslation(FVector(672.0f, 672.0f, 0.0f));
    Document.Pieces[1].bLocked = true;
    const FGuid LockedId = Document.Pieces[1].Id;
    const FIntPoint ErasedCell = Document.Pieces[2].SourceZoneCell;
    TestEqual(TEXT("One generated piece is erased"),
        UPixelRacerTrackAuthoringLibrary::ErasePiecesInRadius(
            Document,
            Document.Pieces[2].Transform.GetLocation(),
            1.0f,
            ZoneLayer),
        1);
    TestTrue(TEXT("Erasing records the source cell suppression"), Document.ProceduralZones[0].SuppressedCells.Contains(ErasedCell));

    TestTrue(TEXT("Regeneration with overrides succeeds"), Generate(Document, GeneratedCount, Error));
    TestEqual(TEXT("Manual, locked, and suppressed slots are not duplicated"), CountZonePieces(Document, ZoneId), 3);
    TestTrue(TEXT("Manual placement identity is retained"), Document.Pieces.ContainsByPredicate([&](const FPixelRacerPiecePlacement& Piece)
    {
        return Piece.Id == ManualId;
    }));
    TestTrue(TEXT("Locked placement identity is retained"), Document.Pieces.ContainsByPredicate([&](const FPixelRacerPiecePlacement& Piece)
    {
        return Piece.Id == LockedId;
    }));
    TestFalse(TEXT("Suppressed source cell does not return"), Document.Pieces.ContainsByPredicate([&](const FPixelRacerPiecePlacement& Piece)
    {
        return Piece.SourceZoneId == ZoneId && Piece.SourceZoneCell == ErasedCell;
    }));
    int32 ManualSourceCellCount = 0;
    for (const FPixelRacerPiecePlacement& Piece : Document.Pieces)
    {
        ManualSourceCellCount += Piece.SourceZoneId == ZoneId && Piece.SourceZoneCell == ManualSourceCell ? 1 : 0;
    }
    TestEqual(TEXT("A moved override still owns its original generator slot"), ManualSourceCellCount, 1);
    TSet<FGuid> UniqueIds;
    TSet<FIntPoint> UniqueSourceCells;
    for (const FPixelRacerPiecePlacement& Piece : Document.Pieces)
    {
        if (Piece.SourceZoneId == ZoneId)
        {
            UniqueIds.Add(Piece.Id);
            UniqueSourceCells.Add(Piece.SourceZoneCell);
        }
    }
    TestEqual(TEXT("Zone pieces retain unique stable ids"), UniqueIds.Num(), CountZonePieces(Document, ZoneId));
    TestEqual(TEXT("Zone pieces retain unique source cells"), UniqueSourceCells.Num(), CountZonePieces(Document, ZoneId));

    FPixelRacerTrackDocument TileDocument = MakeDocument(Square(0.0f, 0.0f, 64.0f, 64.0f), true);
    if (!TestTrue(TEXT("Tile override fixture contains a zone"), TileDocument.ProceduralZones.Num() == 1)) return false;
    if (!TestTrue(TEXT("Initial tile generation succeeds"), Generate(TileDocument, GeneratedCount, Error))) return false;
    if (!TestTrue(TEXT("Initial tile generation produced an erase fixture"), !TileDocument.Tiles.IsEmpty())) return false;
    const FIntPoint TileCell = TileDocument.Tiles[0].Cell;
    TestTrue(TEXT("Generated tile erases"), UPixelRacerTrackAuthoringLibrary::EraseTile(TileDocument, TileCell, ZoneLayer));
    TestTrue(TEXT("Tile erasure records suppression"), TileDocument.ProceduralZones[0].SuppressedCells.Contains(TileCell));
    TestTrue(TEXT("Tile regeneration succeeds"), Generate(TileDocument, GeneratedCount, Error));
    TestEqual(TEXT("The erased tile stays suppressed"), CountZoneTiles(TileDocument, TileDocument.ProceduralZones[0].Id), 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPixelRacerProceduralZoneIsolationTest,
    "PixelRacer.ProceduralZones.OverlapIsolation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPixelRacerProceduralZoneIsolationTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace PixelRacerProceduralZoneTests;

    FPixelRacerTrackDocument Document = MakeDocument(Square(0.0f, 0.0f, 96.0f, 96.0f), true);
    if (!TestTrue(TEXT("Isolation fixture contains its first zone"), Document.ProceduralZones.Num() == 1)) return false;
    const int32 SecondZoneIndex = UPixelRacerTrackAuthoringLibrary::AddProceduralZone(
        Document, Square(64.0f, 0.0f, 160.0f, 96.0f), TEXT("Second"), ZoneLayer, 77);
    if (!TestTrue(TEXT("Isolation fixture creates its second zone"), Document.ProceduralZones.IsValidIndex(SecondZoneIndex))) return false;
    FPixelRacerProceduralZone& SecondZone = Document.ProceduralZones[SecondZoneIndex];
    SecondZone.Id = FGuid(0xabcdef01, 0x23456789, 0xabcdef01, 0x23456789);
    SecondZone.AssetId = TEXT("other.tiles");
    SecondZone.bGenerateTiles = true;
    SecondZone.Density = 1.0f;
    SecondZone.bAvoidRoads = false;

    int32 GeneratedCount = 0;
    FString Error;
    TestTrue(TEXT("First overlapping zone generates"), Generate(Document, GeneratedCount, Error));
    TestTrue(TEXT("Second overlapping zone generates around occupied cells"),
        UPixelRacerTrackAuthoringLibrary::GenerateProceduralZone(Document, SecondZoneIndex, GeneratedCount, Error));
    const int32 SecondCount = CountZoneTiles(Document, SecondZone.Id);
    TestEqual(TEXT("Second zone owns only its unoccupied column"), SecondCount, 6);

    TestTrue(TEXT("Regenerating the first zone succeeds"), Generate(Document, GeneratedCount, Error));
    TestEqual(TEXT("Regenerating one zone leaves the other zone untouched"), CountZoneTiles(Document, SecondZone.Id), SecondCount);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPixelRacerProceduralZoneShrinkDeleteTest,
    "PixelRacer.ProceduralZones.ShrinkAndDelete",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPixelRacerProceduralZoneShrinkDeleteTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace PixelRacerProceduralZoneTests;

    FPixelRacerTrackDocument Document = MakeDocument(Square(0.0f, 0.0f, 128.0f, 128.0f), false, 64.0f);
    if (!TestTrue(TEXT("Shrink fixture contains a zone"), Document.ProceduralZones.Num() == 1)) return false;
    const FGuid ZoneId = Document.ProceduralZones[0].Id;
    int32 GeneratedCount = 0;
    FString Error;
    if (!TestTrue(TEXT("Initial generation succeeds"), Generate(Document, GeneratedCount, Error))) return false;
    if (!TestTrue(TEXT("Initial generation produced a manual-override fixture"), !Document.Pieces.IsEmpty())) return false;
    Document.Pieces[0].bManualOverride = true;
    const FGuid RetainedManualId = Document.Pieces[0].Id;

    TestTrue(TEXT("Zone polygon shrinks"),
        UPixelRacerTrackAuthoringLibrary::UpdateProceduralZonePolygon(Document, 0, Square(64.0f, 64.0f, 128.0f, 128.0f)));
    TestTrue(TEXT("Shrunk zone regenerates"), Generate(Document, GeneratedCount, Error));
    TestEqual(TEXT("Only the manual placement and one in-bounds generated placement remain"), CountZonePieces(Document, ZoneId), 2);

    TestTrue(TEXT("Zone removal succeeds"), UPixelRacerTrackAuthoringLibrary::RemoveProceduralZone(Document, 0));
    TestEqual(TEXT("Zone removal deletes non-manual generated pieces"), CountZonePieces(Document, ZoneId), 1);
    TestTrue(TEXT("Zone removal retains the manual override"), Document.Pieces.ContainsByPredicate([&](const FPixelRacerPiecePlacement& Piece)
    {
        return Piece.Id == RetainedManualId;
    }));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPixelRacerProceduralZoneInvalidAtomicTest,
    "PixelRacer.ProceduralZones.InvalidAtomicityAndBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPixelRacerProceduralZoneInvalidAtomicTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace PixelRacerProceduralZoneTests;

    FPixelRacerTrackDocument Document = MakeDocument(Square(0.0f, 0.0f, 96.0f, 96.0f), true);
    if (!TestTrue(TEXT("Invalid-input fixture starts with a valid zone"), Document.ProceduralZones.Num() == 1)) return false;
    Document.ProceduralZones[0].Polygon = {
        FVector2D(0.0f, 0.0f), FVector2D(96.0f, 96.0f),
        FVector2D(0.0f, 96.0f), FVector2D(96.0f, 0.0f)
    };
    FPixelRacerTilePlacement& Existing = Document.Tiles.AddDefaulted_GetRef();
    Existing.Cell = FIntPoint(99, 99);
    Existing.AssetId = TEXT("manual.tile");
    Existing.LayerIndex = ZoneLayer;
    Existing.bManualOverride = true;
    FString BeforeJson;
    UPixelRacerTrackLibrary::TrackDocumentToJson(Document, BeforeJson);

    int32 GeneratedCount = 9;
    FString Error;
    TestFalse(TEXT("Self-intersecting polygon is rejected"), Generate(Document, GeneratedCount, Error));
    TestEqual(TEXT("Rejected generation reports zero outputs"), GeneratedCount, 0);
    FString AfterJson;
    UPixelRacerTrackLibrary::TrackDocumentToJson(Document, AfterJson);
    TestEqual(TEXT("Invalid generation is atomic"), AfterJson, BeforeJson);

    Document.ProceduralZones[0].Polygon = Square(0.0f, 0.0f, 100000000.0f, 100000000.0f);
    UPixelRacerTrackLibrary::TrackDocumentToJson(Document, BeforeJson);
    TestFalse(TEXT("Unbounded candidate generation is rejected"), Generate(Document, GeneratedCount, Error));
    UPixelRacerTrackLibrary::TrackDocumentToJson(Document, AfterJson);
    TestEqual(TEXT("Candidate-budget rejection is atomic"), AfterJson, BeforeJson);

    FPixelRacerTrackDocument DuplicateIdDocument = MakeDocument(Square(0.0f, 0.0f, 64.0f, 64.0f), true);
    if (!TestTrue(TEXT("Duplicate-id fixture contains its first zone"), DuplicateIdDocument.ProceduralZones.Num() == 1)) return false;
    const int32 DuplicateIndex = UPixelRacerTrackAuthoringLibrary::AddProceduralZone(
        DuplicateIdDocument,
        Square(96.0f, 0.0f, 160.0f, 64.0f),
        TEXT("Duplicate"),
        ZoneLayer,
        88);
    if (!TestTrue(TEXT("Duplicate-id fixture creates its second zone"), DuplicateIdDocument.ProceduralZones.IsValidIndex(DuplicateIndex))) return false;
    DuplicateIdDocument.ProceduralZones[DuplicateIndex].Id = DuplicateIdDocument.ProceduralZones[0].Id;
    UPixelRacerTrackLibrary::TrackDocumentToJson(DuplicateIdDocument, BeforeJson);
    TestFalse(TEXT("Duplicate zone ownership id is rejected"), Generate(DuplicateIdDocument, GeneratedCount, Error));
    UPixelRacerTrackLibrary::TrackDocumentToJson(DuplicateIdDocument, AfterJson);
    TestEqual(TEXT("Duplicate-id generation rejection is atomic"), AfterJson, BeforeJson);
    TestFalse(TEXT("Duplicate zone ownership id also blocks ambiguous removal"),
        UPixelRacerTrackAuthoringLibrary::RemoveProceduralZone(DuplicateIdDocument, 0));
    UPixelRacerTrackLibrary::TrackDocumentToJson(DuplicateIdDocument, AfterJson);
    TestEqual(TEXT("Duplicate-id removal rejection is atomic"), AfterJson, BeforeJson);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPixelRacerProceduralZoneRoadAvoidanceTest,
    "PixelRacer.ProceduralZones.RoadAvoidanceIncludesClosedSegment",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPixelRacerProceduralZoneRoadAvoidanceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace PixelRacerProceduralZoneTests;

    FPixelRacerTrackDocument Document = MakeDocument(Square(-32.0f, 192.0f, 64.0f, 288.0f), true);
    if (!TestTrue(TEXT("Road-avoidance fixture contains a zone"), Document.ProceduralZones.Num() == 1)) return false;
    FPixelRacerProceduralZone& Zone = Document.ProceduralZones[0];
    Zone.bAvoidRoads = true;
    Zone.RoadClearance = 10.0f;
    const int32 SplineIndex = UPixelRacerTrackAuthoringLibrary::AddRoadSpline(Document, TEXT("Closed road"), true);
    UPixelRacerTrackAuthoringLibrary::AddRoadControlPoint(Document, SplineIndex, FVector(8.0f, 0.0f, 0.0f), 16.0f, TEXT("asphalt"));
    UPixelRacerTrackAuthoringLibrary::AddRoadControlPoint(Document, SplineIndex, FVector(1000.0f, 0.0f, 0.0f), 16.0f, TEXT("asphalt"));
    UPixelRacerTrackAuthoringLibrary::AddRoadControlPoint(Document, SplineIndex, FVector(8.0f, 1000.0f, 0.0f), 16.0f, TEXT("asphalt"));

    int32 GeneratedCount = 0;
    FString Error;
    TestTrue(TEXT("Road-aware generation succeeds"), Generate(Document, GeneratedCount, Error));
    TestEqual(TEXT("The closing road segment plus tile footprint excludes all overlapping columns"), GeneratedCount, 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPixelRacerProceduralZoneJsonRoundTripTest,
    "PixelRacer.ProceduralZones.JsonRoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FPixelRacerProceduralZoneJsonRoundTripTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    using namespace PixelRacerProceduralZoneTests;

    FPixelRacerTrackDocument Document = MakeDocument(Square(0.0f, 0.0f, 64.0f, 64.0f), false, 48.0f);
    if (!TestTrue(TEXT("Round-trip fixture contains a zone"), Document.ProceduralZones.Num() == 1)) return false;
    FPixelRacerProceduralZone& Zone = Document.ProceduralZones[0];
    Zone.Density = 1.0f;
    Zone.RoadClearance = 27.0f;
    Zone.bAvoidRoads = true;
    Zone.SuppressedCells = { FIntPoint(2, -3) };
    int32 GeneratedCount = 0;
    FString Error;
    if (!TestTrue(TEXT("Round-trip fixture generates"), Generate(Document, GeneratedCount, Error))) return false;
    if (!TestTrue(TEXT("Round-trip fixture produces an owned piece"), !Document.Pieces.IsEmpty())) return false;
    Zone.Density = 0.375f;

    FString Json;
    TestTrue(TEXT("Zone document serializes"), UPixelRacerTrackLibrary::TrackDocumentToJson(Document, Json));
    FPixelRacerTrackDocument RoundTripped;
    if (!TestTrue(TEXT("Zone document deserializes"), UPixelRacerTrackLibrary::TrackDocumentFromJson(Json, RoundTripped, Error))) return false;
    if (!TestTrue(TEXT("Round-tripped document retains zone and piece fixtures"),
        RoundTripped.ProceduralZones.Num() == 1 && !RoundTripped.Pieces.IsEmpty())) return false;
    TestEqual(TEXT("Asset id round-trips"), RoundTripped.ProceduralZones[0].AssetId, Zone.AssetId);
    TestEqual(TEXT("Generation mode round-trips"), RoundTripped.ProceduralZones[0].bGenerateTiles, Zone.bGenerateTiles);
    TestEqual(TEXT("Spacing round-trips"), RoundTripped.ProceduralZones[0].Spacing, Zone.Spacing);
    TestEqual(TEXT("Density round-trips"), RoundTripped.ProceduralZones[0].Density, Zone.Density);
    TestTrue(TEXT("Suppressed cells round-trip"), RoundTripped.ProceduralZones[0].SuppressedCells == Zone.SuppressedCells);
    TestEqual(TEXT("Generated piece ownership round-trips"), RoundTripped.Pieces[0].SourceZoneId, Zone.Id);
    TestEqual(TEXT("Generated piece source cell round-trips"), RoundTripped.Pieces[0].SourceZoneCell, Document.Pieces[0].SourceZoneCell);
    return true;
}

#endif
