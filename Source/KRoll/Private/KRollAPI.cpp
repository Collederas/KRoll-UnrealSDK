#include "KRollAPI.h"
#include "KRollSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"


class UKRollSubsystem* FKRollAPI::Resolve()
{
	if (!GEngine)
    {
        return nullptr;
    }

    const UWorld* World = GEngine->GetCurrentPlayWorld();
    if (!World)
    {
        return nullptr;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (!GameInstance)
    {
        return nullptr;
    }

    return GameInstance->GetSubsystem<UKRollSubsystem>();
}

void FKRollAPI::FetchConfigs()
{
    if (UKRollSubsystem* Subsystem = Resolve())
    {
        Subsystem->FetchConfigs();
    }
}

bool FKRollAPI::GetBool(const FString& Key, bool DefaultValue)
{
    if (const UKRollSubsystem* S = Resolve())
        return S->GetBool(Key, DefaultValue);
    return DefaultValue;
}

FString FKRollAPI::GetString(const FString& Key, const FString& DefaultValue)
{
    if (const UKRollSubsystem* S = Resolve())
        return S->GetString(Key, DefaultValue);
    return DefaultValue;
}

double FKRollAPI::GetNumber(const FString& Key, double DefaultValue)
{
    if (const UKRollSubsystem* S = Resolve())
        return S->GetNumber(Key, DefaultValue);
    return DefaultValue;
}

TSharedPtr<FJsonValue> FKRollAPI::GetJson(const FString& Key)
{
    if (const UKRollSubsystem* S = Resolve())
        return S->GetJson(Key);
    return nullptr;
}

