#pragma once

#include "Engine/DeveloperSettings.h"
#include "KRollSettings.generated.h"

UCLASS(config=Game, defaultconfig, meta=(DisplayName="KRoll"))
class KROLL_API UKRollSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    /** Base URL, e.g. https://api.kroll.dev */
    UPROPERTY(EditAnywhere, Config, Category="KRoll")
    FString Host;

    /** API key for client access */
    UPROPERTY(EditAnywhere, Config, Category="KRoll")
    FString ApiKey;

	UPROPERTY(config, EditAnywhere, Category="KRoll")
	bool bAutoFetchOnGameStart = false;
};
