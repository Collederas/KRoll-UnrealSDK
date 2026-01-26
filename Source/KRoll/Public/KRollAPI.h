#pragma once

#include "CoreMinimal.h"

class UKRollSubsystem;

class KROLL_API FKRollAPI
{
public:
    static void FetchConfigs();
	static bool GetBool(const FString& Key, bool DefaultValue);
	static FString GetString(const FString& Key, const FString& DefaultValue);
	static double GetNumber(const FString& Key, double DefaultValue);
	static TSharedPtr<FJsonValue> GetJson(const FString& Key);

private:
    static UKRollSubsystem* Resolve();
};
