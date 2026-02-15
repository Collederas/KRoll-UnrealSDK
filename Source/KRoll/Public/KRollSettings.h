#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "KRollSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="KRoll"))
class KROLL_API UKRollSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	// Shows up under Project Settings -> KRoll
	UPROPERTY(Config, EditAnywhere, Category="Connection")
	FString Host;

	UPROPERTY(Config, EditAnywhere, Category="Connection")
	FString ApiKey;

	// If true, FetchConfigs() called automatically on subsystem Initialize
	UPROPERTY(Config, EditAnywhere, Category="Behavior")
	bool bAutoFetchOnInit = false;

	// If true, allow mild coercions (e.g., "true"/"1" -> bool, numeric strings -> number)
	UPROPERTY(Config, EditAnywhere, Category="Behavior")
	bool bAllowTypeCoercion = true;
};
