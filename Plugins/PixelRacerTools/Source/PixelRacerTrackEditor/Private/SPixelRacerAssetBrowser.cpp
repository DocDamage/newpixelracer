#include "SPixelRacerAssetBrowser.h"

#include "AssetImportTask.h"
#include "AssetToolsModule.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Containers/StringConv.h"
#include "Dom/JsonObject.h"
#include "Factories/DataAssetFactory.h"
#include "Factories/TextureFactory.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "InputCoreTypes.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/Texture2D.h"
#include "ObjectTools.h"
#include "Misc/SecureHash.h"
#include "PaperSprite.h"
#include "PaperSpriteFactory.h"
#include "PaperTileSet.h"
#include "PaperTileSetFactory.h"
#include "PixelRacerVehicleDefinition.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/AppStyle.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
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
    static FString MakeSafeAssetName(const FString& SourceName)
    {
        FString SafeName = ObjectTools::SanitizeObjectName(SourceName);
        if (SafeName.IsEmpty())
        {
            SafeName = TEXT("Asset");
        }

        // Hash UTF-8 bytes so the generated name is stable across TCHAR widths and
        // distinguish source names that sanitize to the same Unreal identifier.
        const FTCHARToUTF8 Utf8SourceName(*SourceName);
        const FSHAHash Hash = FSHA1::HashBuffer(Utf8SourceName.Get(), static_cast<uint64>(Utf8SourceName.Length()));
        SafeName += TEXT("_");
        SafeName += Hash.ToString();
        return SafeName;
    }

    static bool TryGetOptionalInt32Field(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName,
        int32& InOutValue,
        FString& OutError)
    {
        if (!Object->HasField(FieldName))
        {
            return true;
        }

        double Number = 0.0;
        if (!Object->TryGetNumberField(FieldName, Number) || !FMath::IsFinite(Number) ||
            Number < static_cast<double>(MIN_int32) || Number > static_cast<double>(MAX_int32))
        {
            OutError = FString::Printf(TEXT("Manifest field '%s' must be a 32-bit integer."), FieldName);
            return false;
        }

        const int32 ParsedValue = static_cast<int32>(Number);
        if (static_cast<double>(ParsedValue) != Number)
        {
            OutError = FString::Printf(TEXT("Manifest field '%s' must be a 32-bit integer."), FieldName);
            return false;
        }

        InOutValue = ParsedValue;
        return true;
    }

    struct FImportedAssetPaths
    {
        FString PackagePath;
        FString AssetName;
        FString TextureObjectPath;
        FString SpriteObjectPath;
        FString TileSetObjectPath;
        FString VehicleDefinitionObjectPath;
    };

    static bool BuildImportedAssetPaths(
        const FPixelRacerAssetBrowserItem& Item,
        FImportedAssetPaths& OutPaths,
        FString& OutError)
    {
        const FString SafePackId = MakeSafeAssetName(Item.PackId);

        TArray<FString> SourceDirectorySegments;
        FPaths::GetPath(Item.RelativePath).ParseIntoArray(SourceDirectorySegments, TEXT("/"), true);
        FString SafeSourceDirectory;
        for (const FString& Segment : SourceDirectorySegments)
        {
            if (!SafeSourceDirectory.IsEmpty())
            {
                SafeSourceDirectory += TEXT("/");
            }
            SafeSourceDirectory += MakeSafeAssetName(Segment);
        }

        OutPaths.PackagePath = FString::Printf(TEXT("/Game/PixelRacer/Imported/%s"), *SafePackId);
        if (!SafeSourceDirectory.IsEmpty())
        {
            OutPaths.PackagePath += TEXT("/");
            OutPaths.PackagePath += SafeSourceDirectory;
        }
        if (!FPackageName::IsValidLongPackageName(OutPaths.PackagePath))
        {
            OutError = TEXT("Could not form a valid Unreal package path from this asset manifest.");
            return false;
        }

        OutPaths.AssetName = MakeSafeAssetName(FPaths::GetBaseFilename(Item.RelativePath));
        if (OutPaths.AssetName.IsEmpty())
        {
            OutError = TEXT("The manifest source path has no asset name.");
            return false;
        }

        OutPaths.TextureObjectPath = FString::Printf(TEXT("%s.%s"), *OutPaths.PackagePath, *OutPaths.AssetName);
        const FString SpriteName = OutPaths.AssetName + TEXT("_Sprite");
        const FString TileSetName = OutPaths.AssetName + TEXT("_TileSet");
        OutPaths.SpriteObjectPath = FString::Printf(TEXT("%s/%s.%s"), *OutPaths.PackagePath, *SpriteName, *SpriteName);
        OutPaths.TileSetObjectPath = FString::Printf(TEXT("%s/%s.%s"), *OutPaths.PackagePath, *TileSetName, *TileSetName);
        const FString VehicleDefinitionName = OutPaths.AssetName + TEXT("_Vehicle");
        OutPaths.VehicleDefinitionObjectPath = FString::Printf(
            TEXT("%s/%s.%s"), *OutPaths.PackagePath, *VehicleDefinitionName, *VehicleDefinitionName);
        return true;
    }

    static bool ResolveSourceFileUnderRoot(
        const FString& SourceRoot,
        const FString& RelativePath,
        FString& OutNormalizedRelativePath,
        FString& OutSourceFile)
    {
        FString NormalizedRelativePath = RelativePath;
        NormalizedRelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
        if (NormalizedRelativePath.IsEmpty() || NormalizedRelativePath.StartsWith(TEXT("/")) ||
            NormalizedRelativePath.Contains(TEXT(":")) || !FPaths::IsRelative(NormalizedRelativePath))
        {
            return false;
        }

        if (!FPaths::CollapseRelativeDirectories(NormalizedRelativePath))
        {
            return false;
        }
        NormalizedRelativePath.ReplaceInline(TEXT("\\"), TEXT("/"));
        if (NormalizedRelativePath == TEXT("..") || NormalizedRelativePath.StartsWith(TEXT("../")))
        {
            return false;
        }

        FString FullRoot = FPaths::ConvertRelativePathToFull(SourceRoot);
        FPaths::NormalizeFilename(FullRoot);
        while (FullRoot.EndsWith(TEXT("/")))
        {
            FullRoot.LeftChopInline(1);
        }

        FString FullSourceFile = FPaths::Combine(FullRoot, NormalizedRelativePath);
        FullSourceFile = FPaths::ConvertRelativePathToFull(FullSourceFile);
        FPaths::NormalizeFilename(FullSourceFile);
        const FString RootPrefix = FullRoot + TEXT("/");
        if (!FullSourceFile.StartsWith(RootPrefix, ESearchCase::IgnoreCase))
        {
            return false;
        }

        if (IFileManager::Get().IsSymlink(*FullRoot))
        {
            return false;
        }
        FString CurrentPath = FullRoot;
        TArray<FString> PathSegments;
        NormalizedRelativePath.ParseIntoArray(PathSegments, TEXT("/"), true);
        for (const FString& Segment : PathSegments)
        {
            CurrentPath = FPaths::Combine(CurrentPath, Segment);
            if (IFileManager::Get().IsSymlink(*CurrentPath))
            {
                return false;
            }
        }

        OutNormalizedRelativePath = MoveTemp(NormalizedRelativePath);
        OutSourceFile = MoveTemp(FullSourceFile);
        return true;
    }

    static bool ReadPngDimensions(const FString& SourceFile, FIntPoint& OutDimensions)
    {
        TArray<uint8> FileBytes;
        if (!FFileHelper::LoadFileToArray(FileBytes, *SourceFile) || FileBytes.Num() < 24)
        {
            return false;
        }

        static constexpr uint8 PngSignature[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
        if (FMemory::Memcmp(FileBytes.GetData(), PngSignature, UE_ARRAY_COUNT(PngSignature)) != 0 ||
            FileBytes[12] != 'I' || FileBytes[13] != 'H' || FileBytes[14] != 'D' || FileBytes[15] != 'R')
        {
            return false;
        }

        const auto ReadBigEndian32 = [&FileBytes](int32 Offset)
        {
            return (static_cast<uint32>(FileBytes[Offset]) << 24) |
                (static_cast<uint32>(FileBytes[Offset + 1]) << 16) |
                (static_cast<uint32>(FileBytes[Offset + 2]) << 8) |
                static_cast<uint32>(FileBytes[Offset + 3]);
        };

        const uint32 Width = ReadBigEndian32(16);
        const uint32 Height = ReadBigEndian32(20);
        if (Width == 0 || Height == 0 || Width > static_cast<uint32>(MAX_int32) || Height > static_cast<uint32>(MAX_int32))
        {
            return false;
        }

        OutDimensions = FIntPoint(static_cast<int32>(Width), static_cast<int32>(Height));
        return true;
    }

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

    static bool SaveImportedAsset(UObject* Asset, FString& OutError)
    {
        if (Asset == nullptr)
        {
            OutError = TEXT("Cannot save a missing imported asset.");
            return false;
        }

        UPackage* Package = Asset->GetOutermost();
        const FString PackageFilename = FPackageName::LongPackageNameToFilename(
            Package->GetName(),
            FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFilename), true);

        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        if (!UPackage::SavePackage(Package, Asset, *PackageFilename, SaveArgs))
        {
            OutError = FString::Printf(TEXT("Could not save imported asset package: %s"), *PackageFilename);
            return false;
        }
        return true;
    }

    static bool CreateOrUpdateSprite(
        IAssetTools& AssetTools,
        UTexture2D* Texture,
        const FString& PackagePath,
        const FString& SpriteName,
        const FIntPoint& SourceUV,
        const FIntPoint& SourceSize,
        int32& InOutGeneratedAssetCount,
        FString& OutError)
    {
        const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *SpriteName, *SpriteName);
        UPaperSprite* Sprite = LoadObject<UPaperSprite>(nullptr, *ObjectPath);
        if (Sprite == nullptr)
        {
            UPaperSpriteFactory* SpriteFactory = NewObject<UPaperSpriteFactory>(GetTransientPackage());
            SpriteFactory->InitialTexture = Texture;
            SpriteFactory->bUseSourceRegion = true;
            SpriteFactory->InitialSourceUV = SourceUV;
            SpriteFactory->InitialSourceDimension = SourceSize;
            Sprite = Cast<UPaperSprite>(AssetTools.CreateAsset(
                SpriteName,
                PackagePath,
                UPaperSprite::StaticClass(),
                SpriteFactory,
                FName(TEXT("PixelRacerAssetBrowser"))));
        }
        else
        {
            Sprite->Modify();
            FSpriteAssetInitParameters InitParams;
            InitParams.Texture = Texture;
            InitParams.Offset = SourceUV;
            InitParams.Dimension = SourceSize;
            Sprite->InitializeSprite(InitParams);
        }

        if (Sprite == nullptr)
        {
            OutError = FString::Printf(TEXT("Could not create Paper2D sprite '%s'."), *SpriteName);
            return false;
        }

        Sprite->SetPivotMode(ESpritePivotMode::Center_Center, FVector2D::ZeroVector, true);
        Sprite->MarkPackageDirty();
        if (!SaveImportedAsset(Sprite, OutError))
        {
            return false;
        }

        ++InOutGeneratedAssetCount;
        return true;
    }

    static bool CreateOrUpdateTileSet(
        IAssetTools& AssetTools,
        UTexture2D* Texture,
        const FString& PackagePath,
        const FString& AssetName,
        const FIntPoint& TileSize,
        int32& InOutGeneratedAssetCount,
        FString& OutError)
    {
        const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *AssetName, *AssetName);
        UPaperTileSet* TileSet = LoadObject<UPaperTileSet>(nullptr, *ObjectPath);
        if (TileSet == nullptr)
        {
            UPaperTileSetFactory* TileSetFactory = NewObject<UPaperTileSetFactory>(GetTransientPackage());
            TileSetFactory->InitialTexture = Texture;
            TileSet = Cast<UPaperTileSet>(AssetTools.CreateAsset(
                AssetName,
                PackagePath,
                UPaperTileSet::StaticClass(),
                TileSetFactory,
                FName(TEXT("PixelRacerAssetBrowser"))));
        }

        if (TileSet == nullptr)
        {
            OutError = FString::Printf(TEXT("Could not create Paper2D tile set '%s'."), *AssetName);
            return false;
        }

        TileSet->Modify();
        TileSet->SetTileSheetTexture(Texture);
        TileSet->SetTileSize(TileSize);
        TileSet->SetMargin(FIntMargin(0));
        TileSet->SetPerTileSpacing(FIntPoint::ZeroValue);
        if (FProperty* TileSheetProperty = FindFProperty<FProperty>(UPaperTileSet::StaticClass(), TEXT("TileSheet")))
        {
            FPropertyChangedEvent TileSheetChanged(TileSheetProperty, EPropertyChangeType::ValueSet);
            TileSet->PostEditChangeProperty(TileSheetChanged);
        }
        if (FProperty* TileSizeProperty = FindFProperty<FProperty>(UPaperTileSet::StaticClass(), TEXT("TileSize")))
        {
            FPropertyChangedEvent TileSizeChanged(TileSizeProperty, EPropertyChangeType::ValueSet);
            TileSet->PostEditChangeProperty(TileSizeChanged);
        }
        TileSet->PostEditChange();
        TileSet->MarkPackageDirty();
        if (!SaveImportedAsset(TileSet, OutError))
        {
            return false;
        }

        ++InOutGeneratedAssetCount;
        return true;
    }

    static float GetVehicleStat(const TSharedPtr<FJsonObject>& Stats, const TCHAR* FieldName, const float DefaultValue)
    {
        double Value = 0.0;
        return Stats.IsValid() && Stats->TryGetNumberField(FieldName, Value) && FMath::IsFinite(Value)
            ? FMath::Clamp(static_cast<float>(Value), 0.0f, 100.0f)
            : DefaultValue;
    }

    static void ApplyVehicleDefinitionMetadata(
        const FPixelRacerAssetBrowserItem& Item,
        UPixelRacerVehicleDefinition& Definition)
    {
        Definition.VehicleId = Item.AssetId;
        Definition.DisplayName = Item.DisplayName;
        Definition.Type = Item.RelativePath.StartsWith(TEXT("Bikes/"), ESearchCase::IgnoreCase)
            ? EPixelRacerVehicleType::Motorcycle
            : EPixelRacerVehicleType::Car;
        Definition.Category = Definition.Type == EPixelRacerVehicleType::Motorcycle ? TEXT("Motorcycle") : TEXT("Car");
        Definition.SourceSpriteSheet = Item.AssetId;
        Definition.DirectionCount = Item.DirectionCount;
        Definition.SpriteCellSize = FIntPoint(Item.CellWidth, Item.CellHeight);
        Definition.Stats = FPixelRacerVehicleStats();
        Definition.ColorVariants.Reset();

        FString WheelsRoot = FPaths::ConvertRelativePathToFull(
            FPaths::Combine(FPaths::ProjectDir(), TEXT("SourceArt/WheelsInPixels")));
        FString ItemSourceRoot = FPaths::ConvertRelativePathToFull(Item.SourceRoot);
        FPaths::NormalizeFilename(WheelsRoot);
        FPaths::NormalizeFilename(ItemSourceRoot);
        if (!ItemSourceRoot.Equals(WheelsRoot, ESearchCase::IgnoreCase))
        {
            return;
        }

        TArray<FString> DefinitionFiles;
        if (const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("PixelRacerTools")); Plugin.IsValid())
        {
            IFileManager::Get().FindFilesRecursive(
                DefinitionFiles,
                *FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources/StarterVehicles")),
                TEXT("*.vehicle.json"),
                true,
                false,
                false);
        }

        const FString ItemDirectory = FPaths::GetPath(Item.RelativePath);
        const FString ItemBaseName = FPaths::GetBaseFilename(Item.RelativePath);
        for (const FString& DefinitionFile : DefinitionFiles)
        {
            FString JsonText;
            if (!FFileHelper::LoadFileToString(JsonText, *DefinitionFile))
            {
                continue;
            }

            TSharedPtr<FJsonObject> StarterDefinition;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
            if (!FJsonSerializer::Deserialize(Reader, StarterDefinition) || !StarterDefinition.IsValid())
            {
                continue;
            }

            FString SourceSpriteSheet;
            if (!StarterDefinition->TryGetStringField(TEXT("SourceSpriteSheet"), SourceSpriteSheet) ||
                !SourceSpriteSheet.RemoveFromStart(TEXT("SourceArt/WheelsInPixels/"), ESearchCase::IgnoreCase))
            {
                continue;
            }

            const FString StarterRelativeDirectory = FPaths::GetPath(SourceSpriteSheet);
            const FString StarterBaseName = FPaths::GetBaseFilename(SourceSpriteSheet);
            if (!ItemDirectory.Equals(StarterRelativeDirectory, ESearchCase::IgnoreCase) ||
                !ItemBaseName.StartsWith(StarterBaseName, ESearchCase::IgnoreCase))
            {
                continue;
            }

            FString StarterDisplayName;
            if (StarterDefinition->TryGetStringField(TEXT("DisplayName"), StarterDisplayName) && !StarterDisplayName.IsEmpty())
            {
                FString VariantSuffix = ItemBaseName;
                VariantSuffix.RemoveFromStart(StarterBaseName, ESearchCase::IgnoreCase);
                VariantSuffix.TrimStartAndEndInline();
                VariantSuffix.ReplaceInline(TEXT("_"), TEXT(" "));
                Definition.DisplayName = StarterDisplayName;
                if (!VariantSuffix.IsEmpty())
                {
                    Definition.DisplayName += TEXT(" ") + VariantSuffix;
                }
            }

            StarterDefinition->TryGetStringField(TEXT("VehicleId"), Definition.VehicleId);
            StarterDefinition->TryGetStringField(TEXT("Category"), Definition.Category);
            FString VehicleType;
            if (StarterDefinition->TryGetStringField(TEXT("Type"), VehicleType))
            {
                Definition.Type = VehicleType.Equals(TEXT("Motorcycle"), ESearchCase::IgnoreCase)
                    ? EPixelRacerVehicleType::Motorcycle
                    : EPixelRacerVehicleType::Car;
            }

            const TSharedPtr<FJsonObject>* Stats = nullptr;
            if (StarterDefinition->TryGetObjectField(TEXT("Stats"), Stats) && Stats != nullptr && Stats->IsValid())
            {
                Definition.Stats.Speed = GetVehicleStat(*Stats, TEXT("Speed"), Definition.Stats.Speed);
                Definition.Stats.Acceleration = GetVehicleStat(*Stats, TEXT("Acceleration"), Definition.Stats.Acceleration);
                Definition.Stats.Handling = GetVehicleStat(*Stats, TEXT("Handling"), Definition.Stats.Handling);
                Definition.Stats.Drift = GetVehicleStat(*Stats, TEXT("Drift"), Definition.Stats.Drift);
                Definition.Stats.Boost = GetVehicleStat(*Stats, TEXT("Boost"), Definition.Stats.Boost);
                Definition.Stats.Weight = GetVehicleStat(*Stats, TEXT("Weight"), Definition.Stats.Weight);
            }

            const TArray<TSharedPtr<FJsonValue>>* ColorValues = nullptr;
            if (StarterDefinition->TryGetArrayField(TEXT("ColorVariants"), ColorValues) && ColorValues != nullptr)
            {
                for (const TSharedPtr<FJsonValue>& ColorValue : *ColorValues)
                {
                    if (ColorValue.IsValid() && ColorValue->Type == EJson::String)
                    {
                        Definition.ColorVariants.Add(ColorValue->AsString());
                    }
                }
            }
            return;
        }
    }

    static bool CreateOrUpdateVehicleDefinition(
        IAssetTools& AssetTools,
        const FPixelRacerAssetBrowserItem& Item,
        const FString& PackagePath,
        const FString& AssetName,
        int32& InOutGeneratedAssetCount,
        FString& OutError)
    {
        const FString VehicleDefinitionName = AssetName + TEXT("_Vehicle");
        const FString ObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *VehicleDefinitionName, *VehicleDefinitionName);
        UPixelRacerVehicleDefinition* Definition = LoadObject<UPixelRacerVehicleDefinition>(nullptr, *ObjectPath);
        if (Definition == nullptr)
        {
            UDataAssetFactory* Factory = NewObject<UDataAssetFactory>(GetTransientPackage());
            Factory->DataAssetClass = UPixelRacerVehicleDefinition::StaticClass();
            Definition = Cast<UPixelRacerVehicleDefinition>(AssetTools.CreateAsset(
                VehicleDefinitionName,
                PackagePath,
                UPixelRacerVehicleDefinition::StaticClass(),
                Factory,
                FName(TEXT("PixelRacerAssetBrowser"))));
        }
        if (Definition == nullptr)
        {
            OutError = FString::Printf(TEXT("Could not create vehicle definition '%s'."), *VehicleDefinitionName);
            return false;
        }

        TArray<TObjectPtr<UPaperSprite>> DirectionalSprites;
        DirectionalSprites.Reserve(Item.DirectionCount);
        for (int32 DirectionIndex = 0; DirectionIndex < Item.DirectionCount; ++DirectionIndex)
        {
            const FString SpriteName = FString::Printf(TEXT("%s_Direction_%02d"), *AssetName, DirectionIndex);
            const FString SpriteObjectPath = FString::Printf(TEXT("%s/%s.%s"), *PackagePath, *SpriteName, *SpriteName);
            UPaperSprite* Sprite = LoadObject<UPaperSprite>(nullptr, *SpriteObjectPath);
            if (Sprite == nullptr)
            {
                OutError = FString::Printf(TEXT("Vehicle direction sprite '%s' is missing."), *SpriteName);
                return false;
            }
            DirectionalSprites.Add(Sprite);
        }

        Definition->Modify();
        ApplyVehicleDefinitionMetadata(Item, *Definition);
        Definition->DirectionalSprites = MoveTemp(DirectionalSprites);
        Definition->MarkPackageDirty();
        if (!SaveImportedAsset(Definition, OutError))
        {
            return false;
        }

        ++InOutGeneratedAssetCount;
        return true;
    }

    static bool ImportPixelArtAssets(
        const FPixelRacerAssetBrowserItem& Item,
        FString& OutObjectPath,
        int32& OutGeneratedAssetCount,
        FString& OutError)
    {
        FString RelativePath;
        FString SourceFile;
        if (!ResolveSourceFileUnderRoot(Item.SourceRoot, Item.RelativePath, RelativePath, SourceFile) ||
            !FPaths::FileExists(SourceFile) ||
            !FPaths::GetExtension(SourceFile).Equals(TEXT("png"), ESearchCase::IgnoreCase))
        {
            OutError = TEXT("The selected source is unavailable, is not a PNG, or resolves outside its asset pack.");
            return false;
        }

        FIntPoint SourceDimensions;
        if (!ReadPngDimensions(SourceFile, SourceDimensions))
        {
            OutError = TEXT("The selected source is not a readable PNG image.");
            return false;
        }

        if ((Item.Width > 0 && Item.Width != SourceDimensions.X) ||
            (Item.Height > 0 && Item.Height != SourceDimensions.Y))
        {
            OutError = TEXT("The source PNG dimensions do not match the manifest.");
            return false;
        }

        if (Item.Role == TEXT("tileset"))
        {
            if (Item.TileWidth <= 0 || Item.TileHeight <= 0 || Item.Width <= 0 || Item.Height <= 0 ||
                Item.Columns <= 0 || Item.Rows <= 0 || Item.TileCount <= 0 ||
                static_cast<int64>(Item.Columns) * Item.TileWidth != Item.Width ||
                static_cast<int64>(Item.Rows) * Item.TileHeight != Item.Height ||
                static_cast<int64>(Item.Columns) * Item.Rows != Item.TileCount)
            {
                OutError = TEXT("Tileset dimensions do not exactly match the manifest grid metadata.");
                return false;
            }
        }
        else if (Item.Role == TEXT("vehicle_sprite_sheet"))
        {
            if (Item.DirectionCount <= 0 || Item.CellWidth <= 0 || Item.CellHeight <= 0 ||
                Item.Width <= 0 || Item.Height <= 0 || Item.CellHeight != Item.Height ||
                static_cast<int64>(Item.DirectionCount) * Item.CellWidth != Item.Width)
            {
                OutError = TEXT("Vehicle frame metadata must describe an exact, single-row directional sheet.");
                return false;
            }
        }
        else if (Item.Role == TEXT("vfx_sheet"))
        {
            if (Item.FrameWidth <= 0 || Item.FrameHeight <= 0 || Item.FrameColumns <= 0 ||
                Item.FrameRows <= 0 || Item.FrameCount <= 0 || Item.Width <= 0 || Item.Height <= 0 ||
                static_cast<int64>(Item.FrameColumns) * Item.FrameWidth != Item.Width ||
                static_cast<int64>(Item.FrameRows) * Item.FrameHeight != Item.Height ||
                static_cast<int64>(Item.FrameColumns) * Item.FrameRows != Item.FrameCount)
            {
                OutError = TEXT("VFX frame metadata must describe an exact rectangular sprite grid.");
                return false;
            }
        }

        FImportedAssetPaths ImportedPaths;
        if (!BuildImportedAssetPaths(Item, ImportedPaths, OutError))
        {
            return false;
        }
        const FString& PackagePath = ImportedPaths.PackagePath;
        const FString& AssetName = ImportedPaths.AssetName;

        UTextureFactory* TextureFactory = NewObject<UTextureFactory>(GetTransientPackage());
        TextureFactory->bCreateMaterial = false;
        TextureFactory->NoCompression = true;
        TextureFactory->CompressionSettings = TC_EditorIcon;
        TextureFactory->LODGroup = TEXTUREGROUP_Pixels2D;
        TextureFactory->MipGenSettings = TMGS_NoMipmaps;

        UAssetImportTask* ImportTask = NewObject<UAssetImportTask>(GetTransientPackage());
        ImportTask->Filename = SourceFile;
        ImportTask->DestinationPath = PackagePath;
        ImportTask->DestinationName = AssetName;
        ImportTask->Factory = TextureFactory;
        ImportTask->bAutomated = true;
        ImportTask->bAsync = false;
        ImportTask->bReplaceExisting = true;
        ImportTask->bReplaceExistingSettings = true;
        ImportTask->bSave = false;

        TArray<UAssetImportTask*> ImportTasks;
        ImportTasks.Add(ImportTask);
        FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
        AssetToolsModule.Get().ImportAssetTasks(ImportTasks);

        UTexture2D* ImportedTexture = nullptr;
        for (UObject* ImportedObject : ImportTask->GetObjects())
        {
            ImportedTexture = Cast<UTexture2D>(ImportedObject);
            if (ImportedTexture != nullptr)
            {
                break;
            }
        }
        if (ImportedTexture == nullptr)
        {
            OutError = TEXT("Unreal did not return an imported texture.");
            return false;
        }

        const FIntPoint TextureSize = ImportedTexture->GetImportedSize();
        if (TextureSize != SourceDimensions)
        {
            OutError = TEXT("Unreal changed the source image dimensions during import; the texture was not saved.");
            return false;
        }

        ImportedTexture->Filter = TF_Nearest;
        ImportedTexture->MipGenSettings = TMGS_NoMipmaps;
        ImportedTexture->LODGroup = TEXTUREGROUP_Pixels2D;
        ImportedTexture->CompressionSettings = TC_EditorIcon;
        ImportedTexture->NeverStream = true;
        ImportedTexture->SRGB = true;
        ImportedTexture->PostEditChange();
        ImportedTexture->MarkPackageDirty();
        if (!SaveImportedAsset(ImportedTexture, OutError))
        {
            return false;
        }

        OutObjectPath = ImportedPaths.TextureObjectPath;

        if (Item.Role == TEXT("tileset"))
        {
            return CreateOrUpdateTileSet(
                AssetToolsModule.Get(),
                ImportedTexture,
                PackagePath,
                AssetName + TEXT("_TileSet"),
                FIntPoint(Item.TileWidth, Item.TileHeight),
                OutGeneratedAssetCount,
                OutError);
        }

        if (Item.Role == TEXT("vehicle_sprite_sheet"))
        {
            for (int32 DirectionIndex = 0; DirectionIndex < Item.DirectionCount; ++DirectionIndex)
            {
                const FString SpriteName = FString::Printf(TEXT("%s_Direction_%02d"), *AssetName, DirectionIndex);
                if (!CreateOrUpdateSprite(
                        AssetToolsModule.Get(),
                        ImportedTexture,
                        PackagePath,
                        SpriteName,
                        FIntPoint(DirectionIndex * Item.CellWidth, 0),
                        FIntPoint(Item.CellWidth, Item.CellHeight),
                        OutGeneratedAssetCount,
                        OutError))
                {
                    return false;
                }
            }
            return CreateOrUpdateVehicleDefinition(
                AssetToolsModule.Get(),
                Item,
                PackagePath,
                AssetName,
                OutGeneratedAssetCount,
                OutError);
        }

        if (Item.Role == TEXT("vfx_sheet"))
        {
            for (int32 FrameRow = 0; FrameRow < Item.FrameRows; ++FrameRow)
            {
                for (int32 FrameColumn = 0; FrameColumn < Item.FrameColumns; ++FrameColumn)
                {
                    const int32 FrameIndex = FrameRow * Item.FrameColumns + FrameColumn;
                    const FString SpriteName = FString::Printf(TEXT("%s_Frame_%03d"), *AssetName, FrameIndex);
                    if (!CreateOrUpdateSprite(
                            AssetToolsModule.Get(),
                            ImportedTexture,
                            PackagePath,
                            SpriteName,
                            FIntPoint(FrameColumn * Item.FrameWidth, FrameRow * Item.FrameHeight),
                            FIntPoint(Item.FrameWidth, Item.FrameHeight),
                            OutGeneratedAssetCount,
                            OutError))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        if (Item.Role == TEXT("environment_piece") || Item.Role == TEXT("sprite"))
        {
            return CreateOrUpdateSprite(
                AssetToolsModule.Get(),
                ImportedTexture,
                PackagePath,
                AssetName + TEXT("_Sprite"),
                FIntPoint::ZeroValue,
                TextureSize,
                OutGeneratedAssetCount,
                OutError);
        }

        return true;
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
    OnManifestsReloaded = InArgs._OnManifestsReloaded;

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
        + SVerticalBox::Slot().AutoHeight().Padding(0.0f, 4.0f, 0.0f, 0.0f)
        [
            SNew(SButton)
            .Text(NSLOCTEXT("PixelRacerAssetBrowser", "ImportTexture", "Import Selected PNG"))
            .ToolTipText(NSLOCTEXT("PixelRacerAssetBrowser", "ImportTextureTip", "Import the selected PNG with pixel-safe texture settings. Vehicle and VFX sheets create Paper2D sprites; tilesets create Paper2D tile sets."))
            .IsEnabled_Lambda([this]() { return SelectedItem.IsValid(); })
            .OnClicked(this, &SPixelRacerAssetBrowser::ImportSelectedTexture)
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
    LastActionMessage.Reset();

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
    OnManifestsReloaded.ExecuteIfBound();
}

TArray<FPixelRacerAssetPreviewData> SPixelRacerAssetBrowser::GetPreviewData() const
{
    TArray<FPixelRacerAssetPreviewData> PreviewData;
    PreviewData.Reserve(AllItems.Num());

    for (const FAssetItemPtr& Item : AllItems)
    {
        if (!Item.IsValid())
        {
            continue;
        }

        FPixelRacerAssetPreviewData& Preview = PreviewData.AddDefaulted_GetRef();
        Preview.AssetId = Item->AssetId;
        Preview.Role = Item->Role;
        Preview.DisplayName = Item->DisplayName;
        Preview.RelativePath = Item->RelativePath;
        Preview.SourceFile = Item->SourceFile;
        Preview.TextureObjectPath = Item->TextureObjectPath;
        Preview.SpriteObjectPath = Item->SpriteObjectPath;
        Preview.TileSetObjectPath = Item->TileSetObjectPath;
        Preview.VehicleDefinitionObjectPath = Item->VehicleDefinitionObjectPath;
        Preview.DirectionCount = Item->DirectionCount;
        Preview.CellWidth = Item->CellWidth;
        Preview.CellHeight = Item->CellHeight;
        Preview.Width = Item->Width;
        Preview.Height = Item->Height;
        Preview.TileWidth = Item->TileWidth;
        Preview.TileHeight = Item->TileHeight;
        Preview.Columns = Item->Columns;
        Preview.Rows = Item->Rows;
        Preview.TileCount = Item->TileCount;
        Preview.SourceBrush = Item->ThumbnailBrush;
    }

    return PreviewData;
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
    TArray<FAssetItemPtr> ManifestItems;
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

        FString NormalizedRelativePath;
        FString SourceFile;
        if (!PixelRacerAssetBrowser::ResolveSourceFileUnderRoot(
                SourceRoot,
                RelativePath,
                NormalizedRelativePath,
                SourceFile) || !FPaths::FileExists(SourceFile))
        {
            continue;
        }

        FAssetItemPtr Item = MakeShared<FPixelRacerAssetBrowserItem>();
        Item->PackId = PackId;
        Item->PackDisplayName = PackDisplayName;
        Item->RelativePath = NormalizedRelativePath;
        Item->SourceRoot = FPaths::ConvertRelativePathToFull(SourceRoot);
        Item->SourceFile = SourceFile;
        Item->Role = Role;
        Item->DisplayName = PixelRacerAssetBrowser::MakeDisplayName(Item->RelativePath);
        Item->AssetId = PixelRacerAssetBrowser::MakePortableAssetId(PackId, Item->RelativePath);

        if (!PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("width"), Item->Width, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("height"), Item->Height, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("directionCount"), Item->DirectionCount, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("cellWidth"), Item->CellWidth, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("cellHeight"), Item->CellHeight, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("tileWidth"), Item->TileWidth, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("tileHeight"), Item->TileHeight, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("columns"), Item->Columns, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("rows"), Item->Rows, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("tileCount"), Item->TileCount, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("frameWidth"), Item->FrameWidth, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("frameHeight"), Item->FrameHeight, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("frameColumns"), Item->FrameColumns, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("frameRows"), Item->FrameRows, OutError) ||
            !PixelRacerAssetBrowser::TryGetOptionalInt32Field(AssetObject, TEXT("frameCount"), Item->FrameCount, OutError))
        {
            OutError = FString::Printf(TEXT("Invalid numeric metadata in asset manifest %s: %s"), *ManifestPath, *OutError);
            return false;
        }

        PixelRacerAssetBrowser::FImportedAssetPaths ImportedPaths;
        if (!PixelRacerAssetBrowser::BuildImportedAssetPaths(*Item, ImportedPaths, OutError))
        {
            OutError = FString::Printf(TEXT("Could not build imported asset paths for manifest %s: %s"), *ManifestPath, *OutError);
            return false;
        }
        Item->TextureObjectPath = ImportedPaths.TextureObjectPath;
        Item->SpriteObjectPath = ImportedPaths.SpriteObjectPath;
        Item->TileSetObjectPath = ImportedPaths.TileSetObjectPath;
        Item->VehicleDefinitionObjectPath = ImportedPaths.VehicleDefinitionObjectPath;

        ManifestItems.Add(Item);
    }

    AllItems.Append(ManifestItems);
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
    LastActionMessage.Reset();
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

FReply SPixelRacerAssetBrowser::ImportSelectedTexture()
{
    if (!SelectedItem.IsValid())
    {
        return FReply::Handled();
    }

    FString ObjectPath;
    int32 GeneratedAssetCount = 0;
    FString Error;
    if (PixelRacerAssetBrowser::ImportPixelArtAssets(*SelectedItem, ObjectPath, GeneratedAssetCount, Error))
    {
        LastActionMessage = FString::Printf(
            TEXT("Imported pixel-safe texture and created/updated %d Paper2D asset(s): %s"),
            GeneratedAssetCount,
            *ObjectPath);
    }
    else
    {
        LastActionMessage = ObjectPath.IsEmpty()
            ? FString::Printf(TEXT("Import failed: %s"), *Error)
            : FString::Printf(
                TEXT("Partial import: saved texture %s and %d Paper2D asset(s) before a later creation failure: %s"),
                *ObjectPath,
                GeneratedAssetCount,
                *Error);
    }
    return FReply::Handled();
}

FText SPixelRacerAssetBrowser::GetBrowserStatusText() const
{
    if (SelectedItem.IsValid())
    {
        if (!LastActionMessage.IsEmpty())
        {
            return FText::FromString(FString::Printf(
                TEXT("Selected: %s\n%s\n%s"),
                *SelectedItem->DisplayName,
                *SelectedItem->AssetId,
                *LastActionMessage));
        }
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
