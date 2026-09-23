#include "SPixelRacerAssetBrowser.h"

#include "Brushes/SlateDynamicImageBrush.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "InputCoreTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

namespace PixelRacerAssetBrowser
{
    static FString RoleLabel(const FString& Role)
    {
        if (Role == TEXT("tileset")) return TEXT("Tiles");
        if (Role == TEXT("environment_piece")) return TEXT("Scenery");
        if (Role == TEXT("vehicle_sprite_sheet")) return TEXT("Vehicle");
        if (Role == TEXT("vfx_sheet")) return TEXT("VFX");
        return TEXT("Sprite");
    }

    static FString MakeDisplayName(const FString& RelativePath)
    {
        FString Base = FPaths::GetBaseFilename(RelativePath);
        Base.ReplaceInline(TEXT("_"), TEXT(" "));
        return Base;
    }

    static FString MakePortableAssetId(const FString& PackId, const FString& RelativePath)
    {
        // Preserve the original v0.1/v0.2 Wheels-in-Pixels ids so existing
        // TrackDocument data does not need a migration. User packs are scoped
        // by pack id to prevent collisions between identical filenames.
        if (PackId == TEXT("wheels_in_pixels"))
        {
            return RelativePath;
        }
        return FString::Printf(TEXT("%s:%s"), *PackId, *RelativePath);
    }
}

class SPixelRacerAssetRow final : public STableRow<TSharedPtr<FPixelRacerAssetBrowserItem>>
{
public:
    SLATE_BEGIN_ARGS(SPixelRacerAssetRow) {}
        SLATE_ARGUMENT(TSharedPtr<FPixelRacerAssetBrowserItem>, Item)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable)
    {
        Item = InArgs._Item;
        check(Item.IsValid());

        if (!Item->ThumbnailBrush.IsValid() && FPaths::FileExists(Item->SourceFile))
        {
            Item->ThumbnailBrush = MakeShared<FSlateDynamicImageBrush>(
                FName(*Item->SourceFile),
                FVector2D(54.0f, 54.0f),
                FLinearColor::White,
                ESlateBrushTileType::NoTile,
                ESlateBrushImageType::FullColor);
        }

        const FString Metadata = FString::Printf(
            TEXT("%s  •  %dx%d  •  %s"),
            *PixelRacerAssetBrowser::RoleLabel(Item->Role),
            Item->Width,
            Item->Height,
            *Item->PackDisplayName);

        STableRow::Construct(
            STableRow::FArguments()
            .Padding(FMargin(2.0f))
            .OnDragDetected(this, &SPixelRacerAssetRow::HandleDragDetected)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 6.0f, 0.0f)
                [
                    SNew(SBox)
                    .WidthOverride(58.0f)
                    .HeightOverride(58.0f)
                    [
                        SNew(SBorder)
                        .Padding(2.0f)
                        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                        [
                            SNew(SImage)
                            .Image(Item->ThumbnailBrush.IsValid() ? Item->ThumbnailBrush.Get() : FAppStyle::GetBrush("Icons.Image"))
                        ]
                    ]
                ]
                + SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Item->DisplayName))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Metadata))
                        .ColorAndOpacity(FSlateColor::UseSubduedForeground())
                    ]
                ]
            ],
            OwnerTable);
    }

    FReply HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
    {
        if (Item.IsValid() && MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
        {
            return FReply::Handled().BeginDragDrop(FPixelRacerAssetDragDropOp::New(Item->AssetId, Item->Role, Item->DisplayName));
        }
        return FReply::Unhandled();
    }

private:
    TSharedPtr<FPixelRacerAssetBrowserItem> Item;
};

TSharedRef<FPixelRacerAssetDragDropOp> FPixelRacerAssetDragDropOp::New(
    const FString& InAssetId,
    const FString& InAssetRole,
    const FString& InDisplayName)
{
    TSharedRef<FPixelRacerAssetDragDropOp> Operation = MakeShared<FPixelRacerAssetDragDropOp>();
    Operation->AssetId = InAssetId;
    Operation->AssetRole = InAssetRole;
    Operation->DefaultHoverText = FText::Format(
        NSLOCTEXT("PixelRacerAssetBrowser", "DragAssetFmt", "Place {0}"),
        FText::FromString(InDisplayName));
    Operation->CurrentHoverText = Operation->DefaultHoverText;
    Operation->Construct();
    return Operation;
}

void SPixelRacerAssetBrowser::Construct(const FArguments& InArgs)
{
    OnAssetChosen = InArgs._OnAssetChosen;

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1.0f)
            [
                SNew(STextBlock)
                .Text(NSLOCTEXT("PixelRacerAssetBrowser", "Header", "ASSET BROWSER"))
                .Font(FAppStyle::GetFontStyle(TEXT("HeadingExtraSmall")))
            ]
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SButton)
                .Text(NSLOCTEXT("PixelRacerAssetBrowser", "Refresh", "Rescan"))
                .ToolTipText(NSLOCTEXT("PixelRacerAssetBrowser", "RefreshTip", "Reload Wheels in Pixels and imported Pixel Racer asset-pack manifests."))
                .OnClicked_Lambda([this]()
                {
                    ReloadManifests();
                    return FReply::Handled();
                })
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 4.0f)
        [
            SAssignNew(SearchBox, SSearchBox)
            .HintText(NSLOCTEXT("PixelRacerAssetBrowser", "SearchHint", "Search name, path, pack..."))
            .OnTextChanged(this, &SPixelRacerAssetBrowser::HandleSearchChanged)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 4.0f)
        [
            SNew(SWrapBox)
            .UseAllottedSize(true)
            .InnerSlotPadding(FVector2D(2.0f, 2.0f))
            + SWrapBox::Slot()
            [ SNew(SButton).Text(NSLOCTEXT("PixelRacerAssetBrowser", "AllFilter", "All")).OnClicked(this, &SPixelRacerAssetBrowser::SetRoleFilter, FString(TEXT("all"))) ]
            + SWrapBox::Slot()
            [ SNew(SButton).Text(NSLOCTEXT("PixelRacerAssetBrowser", "TileFilter", "Tiles")).OnClicked(this, &SPixelRacerAssetBrowser::SetRoleFilter, FString(TEXT("tileset"))) ]
            + SWrapBox::Slot()
            [ SNew(SButton).Text(NSLOCTEXT("PixelRacerAssetBrowser", "SceneryFilter", "Scenery")).OnClicked(this, &SPixelRacerAssetBrowser::SetRoleFilter, FString(TEXT("environment_piece"))) ]
            + SWrapBox::Slot()
            [ SNew(SButton).Text(NSLOCTEXT("PixelRacerAssetBrowser", "VehicleFilter", "Vehicles")).OnClicked(this, &SPixelRacerAssetBrowser::SetRoleFilter, FString(TEXT("vehicle_sprite_sheet"))) ]
            + SWrapBox::Slot()
            [ SNew(SButton).Text(NSLOCTEXT("PixelRacerAssetBrowser", "VfxFilter", "VFX")).OnClicked(this, &SPixelRacerAssetBrowser::SetRoleFilter, FString(TEXT("vfx_sheet"))) ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return GetFilterText(); })
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ]
        + SVerticalBox::Slot().FillHeight(1.0f)
        [
            SNew(SBox)
            .MinDesiredHeight(240.0f)
            [
                SAssignNew(ListView, SListView<FAssetItemPtr>)
                .ListItemsSource(&FilteredItems)
                .SelectionMode(ESelectionMode::Single)
                .OnGenerateRow(this, &SPixelRacerAssetBrowser::GenerateAssetRow)
                .OnSelectionChanged(this, &SPixelRacerAssetBrowser::HandleSelectionChanged)
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            SNew(STextBlock)
            .AutoWrapText(true)
            .Text_Lambda([this]() { return GetBrowserStatusText(); })
            .ColorAndOpacity(FSlateColor::UseSubduedForeground())
        ]
    ];

    ReloadManifests();
}

void SPixelRacerAssetBrowser::ReloadManifests()
{
    AllItems.Reset();
    SelectedItem.Reset();

    int32 LoadedManifestCount = 0;
    TArray<FString> Errors;

    const FString WheelsManifest = FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("SourceArt/WheelsInPixels/PixelRacerAssetPack_v2.json"));
    FString Error;
    if (LoadManifest(WheelsManifest, Error))
    {
        ++LoadedManifestCount;
    }
    else
    {
        Errors.Add(Error);
    }

    const FString ImportedRoot = FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt/Imported"));
    TArray<FString> ImportedManifests;
    IFileManager::Get().FindFilesRecursive(
        ImportedManifests,
        *ImportedRoot,
        TEXT("PixelRacerAssetPack_v2.json"),
        true,
        false,
        false);

    for (const FString& ManifestPath : ImportedManifests)
    {
        Error.Reset();
        if (LoadManifest(ManifestPath, Error))
        {
            ++LoadedManifestCount;
        }
        else if (!Error.IsEmpty())
        {
            Errors.Add(Error);
        }
    }

    AllItems.Sort([](const FAssetItemPtr& A, const FAssetItemPtr& B)
    {
        if (A->Role != B->Role)
        {
            return A->Role < B->Role;
        }
        if (A->PackId != B->PackId)
        {
            return A->PackId < B->PackId;
        }
        return A->RelativePath < B->RelativePath;
    });

    LastLoadMessage = FString::Printf(TEXT("%d asset(s) from %d pack(s)"), AllItems.Num(), LoadedManifestCount);
    if (!Errors.IsEmpty())
    {
        LastLoadMessage += FString::Printf(TEXT("; %d manifest error(s)"), Errors.Num());
    }

    ApplyFilters();
}

bool SPixelRacerAssetBrowser::LoadManifest(const FString& ManifestPath, FString& OutError)
{
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *ManifestPath))
    {
        OutError = FString::Printf(TEXT("Could not read asset manifest: %s"), *ManifestPath);
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutError = FString::Printf(TEXT("Invalid asset manifest JSON: %s"), *ManifestPath);
        return false;
    }

    FString PackId;
    FString PackDisplayName;
    if (!RootObject->TryGetStringField(TEXT("packId"), PackId) || PackId.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Asset manifest has no packId: %s"), *ManifestPath);
        return false;
    }
    RootObject->TryGetStringField(TEXT("displayName"), PackDisplayName);
    if (PackDisplayName.IsEmpty())
    {
        PackDisplayName = PackId;
    }

    const TArray<TSharedPtr<FJsonValue>>* AssetValues = nullptr;
    if (!RootObject->TryGetArrayField(TEXT("assets"), AssetValues) || AssetValues == nullptr)
    {
        OutError = FString::Printf(TEXT("Asset manifest has no assets array: %s"), *ManifestPath);
        return false;
    }

    const FString SourceRoot = FPaths::GetPath(ManifestPath);
    for (const TSharedPtr<FJsonValue>& AssetValue : *AssetValues)
    {
        const TSharedPtr<FJsonObject>* AssetObjectPtr = nullptr;
        if (!AssetValue.IsValid() || !AssetValue->TryGetObject(AssetObjectPtr) || AssetObjectPtr == nullptr || !AssetObjectPtr->IsValid())
        {
            continue;
        }

        const TSharedPtr<FJsonObject>& AssetObject = *AssetObjectPtr;
        FString RelativePath;
        FString Role;
        if (!AssetObject->TryGetStringField(TEXT("path"), RelativePath) || RelativePath.IsEmpty())
        {
            continue;
        }
        AssetObject->TryGetStringField(TEXT("role"), Role);
        if (Role.IsEmpty())
        {
            Role = TEXT("sprite");
        }

        const FString SourceFile = FPaths::Combine(SourceRoot, RelativePath);
        if (!FPaths::FileExists(SourceFile))
        {
            continue;
        }

        FAssetItemPtr Item = MakeShared<FPixelRacerAssetBrowserItem>();
        Item->PackId = PackId;
        Item->PackDisplayName = PackDisplayName;
        Item->RelativePath = RelativePath;
        Item->SourceFile = FPaths::ConvertRelativePathToFull(SourceFile);
        Item->Role = Role;
        Item->DisplayName = PixelRacerAssetBrowser::MakeDisplayName(RelativePath);
        Item->AssetId = PixelRacerAssetBrowser::MakePortableAssetId(PackId, RelativePath);

        double Number = 0.0;
        if (AssetObject->TryGetNumberField(TEXT("width"), Number)) Item->Width = static_cast<int32>(Number);
        if (AssetObject->TryGetNumberField(TEXT("height"), Number)) Item->Height = static_cast<int32>(Number);
        if (AssetObject->TryGetNumberField(TEXT("directionCount"), Number)) Item->DirectionCount = static_cast<int32>(Number);
        if (AssetObject->TryGetNumberField(TEXT("cellWidth"), Number)) Item->CellWidth = static_cast<int32>(Number);
        if (AssetObject->TryGetNumberField(TEXT("cellHeight"), Number)) Item->CellHeight = static_cast<int32>(Number);
        if (AssetObject->TryGetNumberField(TEXT("tileWidth"), Number)) Item->TileWidth = static_cast<int32>(Number);
        if (AssetObject->TryGetNumberField(TEXT("tileHeight"), Number)) Item->TileHeight = static_cast<int32>(Number);
        if (AssetObject->TryGetNumberField(TEXT("tileCount"), Number)) Item->TileCount = static_cast<int32>(Number);

        AllItems.Add(Item);
    }

    return true;
}

void SPixelRacerAssetBrowser::ApplyFilters()
{
    FilteredItems.Reset();
    const FString SearchLower = SearchText.ToLower();

    for (const FAssetItemPtr& Item : AllItems)
    {
        if (!Item.IsValid())
        {
            continue;
        }
        if (RoleFilter != TEXT("all") && Item->Role != RoleFilter)
        {
            continue;
        }

        if (!SearchLower.IsEmpty())
        {
            const FString Haystack = FString::Printf(
                TEXT("%s %s %s %s"),
                *Item->DisplayName,
                *Item->RelativePath,
                *Item->PackId,
                *Item->PackDisplayName).ToLower();
            if (!Haystack.Contains(SearchLower))
            {
                continue;
            }
        }
        FilteredItems.Add(Item);
    }

    if (ListView.IsValid())
    {
        ListView->RequestListRefresh();
    }
}

void SPixelRacerAssetBrowser::HandleSearchChanged(const FText& NewText)
{
    SearchText = NewText.ToString();
    ApplyFilters();
}

void SPixelRacerAssetBrowser::HandleSelectionChanged(FAssetItemPtr Item, ESelectInfo::Type SelectInfo)
{
    SelectedItem = Item;
    if (Item.IsValid() && OnAssetChosen.IsBound())
    {
        OnAssetChosen.Execute(Item->AssetId, Item->Role);
    }
}

TSharedRef<ITableRow> SPixelRacerAssetBrowser::GenerateAssetRow(FAssetItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable)
{
    return SNew(SPixelRacerAssetRow, OwnerTable)
        .Item(Item);
}

FReply SPixelRacerAssetBrowser::SetRoleFilter(FString InRoleFilter)
{
    RoleFilter = MoveTemp(InRoleFilter);
    ApplyFilters();
    return FReply::Handled();
}

FText SPixelRacerAssetBrowser::GetBrowserStatusText() const
{
    if (SelectedItem.IsValid())
    {
        return FText::FromString(FString::Printf(
            TEXT("Selected: %s\n%s"),
            *SelectedItem->DisplayName,
            *SelectedItem->AssetId));
    }
    return FText::FromString(LastLoadMessage);
}

FText SPixelRacerAssetBrowser::GetFilterText() const
{
    const FString Label = RoleFilter == TEXT("all") ? TEXT("All") : PixelRacerAssetBrowser::RoleLabel(RoleFilter);
    return FText::FromString(FString::Printf(TEXT("%s: %d shown / %d loaded"), *Label, FilteredItems.Num(), AllItems.Num()));
}

FString SPixelRacerAssetBrowser::GetSelectedAssetId() const
{
    return SelectedItem.IsValid() ? SelectedItem->AssetId : FString();
}

FString SPixelRacerAssetBrowser::GetSelectedRole() const
{
    return SelectedItem.IsValid() ? SelectedItem->Role : FString();
}
