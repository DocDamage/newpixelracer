#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "PixelRacerTrackFactory.generated.h"

UCLASS()
class PIXELRACERTRACKEDITOR_API UPixelRacerTrackFactory : public UFactory
{
    GENERATED_BODY()

public:
    UPixelRacerTrackFactory();

    virtual UObject* FactoryCreateNew(
        UClass* Class,
        UObject* InParent,
        FName Name,
        EObjectFlags Flags,
        UObject* Context,
        FFeedbackContext* Warn) override;
};
