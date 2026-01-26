#include "KRollBlueprintLibrary.h"
#include "KRollAPI.h"

void UKRollBlueprintLibrary::FetchConfigs()
{
    FKRollAPI::FetchConfigs();
}

bool UKRollBlueprintLibrary::GetBool(const FString& Key, bool DefaultValue)
{
    return FKRollAPI::GetBool(Key, DefaultValue);
}

FString UKRollBlueprintLibrary::GetString(const FString& Key, const FString& DefaultValue)
{
    return FKRollAPI::GetString(Key, DefaultValue);
}

float UKRollBlueprintLibrary::GetNumber(const FString& Key, float DefaultValue)
{
    return (float)FKRollAPI::GetNumber(Key, DefaultValue);
}

FString UKRollBlueprintLibrary::GetJson(const FString& Key)
{
    const TSharedPtr<FJsonValue> Json = FKRollAPI::GetJson(Key);
    if (!Json.IsValid())
        return TEXT("");

    FString Out;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Json.ToSharedRef(), TEXT(""), Writer);
    return Out;
}


