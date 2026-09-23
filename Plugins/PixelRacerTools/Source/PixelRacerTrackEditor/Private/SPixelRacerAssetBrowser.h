#pragma once

#include "CoreMinimal.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"

struct FSlateDynamicImageBrush;
class SSearchBox;

struct FPixelRacerAssetBrowserItem
{
    FString PackId;
    FString PackDisplayName;
    FString AssetId;
    FString RelativePath;
    FString SourceFile;
    FString Role;
    FString DisplayName;
    int32 Width = 0;
    int32 Height = 0;
    int32 DirectionCount = 0;
    int32 CellWidth = 0;
    int32 CellHeight = 0;
    int32 TileWidth = 0;
    int32 TileHeight = 0;
    int32 TileCount = 0;
    TSharedPtr<FSlateDynamicImageBrush> ThumbnailBrush;
};

class FPixelRacerAssetDragDropOp final : public FDecoratedDragDropOp
{
public:
    DRAG_DROP_OPERATOR_TYPE(FPixelRacerAssetDragDropOp, FDecoratedDragDropOp)

    FString AssetId;
    FString AssetRole;

    static TSharedRef<FPixelRacerAssetDragDropOp> New(const FString& InAssetId, const FString& InAssetRole, const FString& InDisplayName);
};

DECLARE_DELEGATE_TwoParams(FOnPixelRacerAssetChosen, const FString& /*AssetId*/, const FString& /*Role*/);

class SPixelRacerAssetBrowser final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPixelRacerAssetBrowser) {}
        SLATE_EVENT(FOnPixelRacerAssetChosen, OnAssetChosen)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);
    void ReloadManifests();

    int32 GetLoadedAssetCount() const { return AllItems.Num(); }
    FString GetSelectedAssetId() const;
    FString GetSelectedRole() const;

private:
    using FAssetItemPtr = TSharedPtr<FPixelRacerAssetBrowserItem>;

    bool LoadManifest(const FString& ManifestPath, FString& OutError);
    void ApplyFilters();
    void HandleSearchChanged(const FText& NewText);
    void HandleSelectionChanged(FAssetItemPtr Item, ESelectInfo::Type SelectInfo);
    TSharedRef<ITableRow> GenerateAssetRow(FAssetItemPtr Item, const TSharedRef<STableViewBase>& OwnerTable);
    FReply SetRoleFilter(FString InRoleFilter);
    FText GetBrowserStatusText() const;
    FText GetFilterText() const;

    FString SearchText;
    FString RoleFilter = TEXT("all");
    FString LastLoadMessage;
    TArray<FAssetItemPtr> AllItems;
    TArray<FAssetItemPtr> FilteredItems;
    TSharedPtr<SListView<FAssetItemPtr>> ListView;
    TSharedPtr<SSearchBox> SearchBox;
    FAssetItemPtr SelectedItem;
    FOnPixelRacerAssetChosen OnAssetChosen;
};
