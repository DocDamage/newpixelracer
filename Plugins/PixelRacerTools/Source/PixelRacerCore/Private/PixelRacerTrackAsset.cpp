#include "PixelRacerTrackAsset.h"

FPrimaryAssetId UPixelRacerTrackAsset::GetPrimaryAssetId() const
{
    const FName Name = Document.Metadata.TrackId.IsEmpty()
        ? GetFName()
        : FName(*Document.Metadata.TrackId);

    return FPrimaryAssetId(TEXT("PixelRacerTrack"), Name);
}
