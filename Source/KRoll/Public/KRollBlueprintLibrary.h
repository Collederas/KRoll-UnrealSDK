#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "KRollBlueprintLibrary.generated.h"

UCLASS()
class KROLL_API UKRollBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="KRoll")
    static void FetchConfigs();

	UFUNCTION(BlueprintPure, Category="KRoll")
	static bool GetBool(const FString& Key, bool DefaultValue);

	UFUNCTION(BlueprintPure, Category="KRoll")
	static FString GetString(const FString& Key, const FString& DefaultValue);

	UFUNCTION(BlueprintPure, Category="KRoll")
	static float GetNumber(const FString& Key, float DefaultValue);

	UFUNCTION(BlueprintPure, Category="KRoll")
	static FString GetJson(const FString& Key);

};
