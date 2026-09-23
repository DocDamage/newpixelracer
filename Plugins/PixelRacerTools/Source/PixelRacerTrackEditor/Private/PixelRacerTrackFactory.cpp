#include "PixelRacerTrackFactory.h"

#include "PixelRacerTrackAsset.h"

UPixelRacerTrackFactory::UPixelRacerTrackFactory()
{
    SupportedClass = UPixelRacerTrackAsset::StaticClass();
    bCreateNew = true;
    bEditAfterNew = true;
}

UObject* UPixelRacerTrackFactory::FactoryCreateNew(
    UClass* Class,
    UObject* InParent,
    FName Name,
    EObjectFlags Flags,
    UObject* Context,
    FFeedbackContext* Warn)
{
    UPixelRacerTrackAsset* NewAsset = NewObject<UPixelRacerTrackAsset>(InParent, Class, Name, Flags);
    NewAsset->Document.Metadata.TrackId = Name.ToString();
    NewAsset->Document.Metadata.DisplayName = Name.ToString();
    return NewAsset;
}
