#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonValue.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

#include "KRollSubsystem.generated.h"

UCLASS()
class KROLL_API UKRollSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void FetchConfigs();

	bool GetBool(const FString& Key, bool DefaultValue) const;
	FString GetString(const FString& Key, const FString& DefaultValue) const;
	double GetNumber(const FString& Key, double DefaultValue) const;
	TSharedPtr<FJsonValue> GetJson(const FString& Key) const;

private:
	bool bIsInitialized = false;

    void OnFetchResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);

    TMap<FString, TSharedPtr<FJsonValue>> ConfigCache;
};
