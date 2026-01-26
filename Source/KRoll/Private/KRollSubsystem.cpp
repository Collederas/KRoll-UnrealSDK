#include "KRollSubsystem.h"
#include "KRollSettings.h"

#include "HttpModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"


void UKRollSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    bIsInitialized = true;

	const UKRollSettings* Settings = GetDefault<UKRollSettings>();
    if (Settings->bAutoFetchOnGameStart)
    {
        FetchConfigs();
    }
}
void UKRollSubsystem::Deinitialize()
{
	bIsInitialized = false;
    ConfigCache.Empty();
    Super::Deinitialize();
}

void UKRollSubsystem::FetchConfigs()
{
    const UKRollSettings* Settings = GetDefault<UKRollSettings>();
    if (!Settings || Settings->Host.IsEmpty() || Settings->ApiKey.IsEmpty())
    {
        return; // misconfigured SDK → safe no-op
    }

    FHttpRequestRef Request = FHttpModule::Get().CreateRequest();

	// TODO: sanitize, normalize, do good things here...
    const FString Url = Settings->Host / TEXT("api/client/config/fetch");

    Request->SetURL(Url);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

    // Auth header
    Request->SetHeader(TEXT("X-API-Key"), Settings->ApiKey);

    Request->SetContentAsString(TEXT("{}"));

    Request->OnProcessRequestComplete().BindUObject(
        this, &UKRollSubsystem::OnFetchResponse
    );

    Request->ProcessRequest();
}


void UKRollSubsystem::OnFetchResponse(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bSuccess)
{
	if (!bIsInitialized)
	{
		UE_LOG(LogTemp, Warning, TEXT("KrollSubsystem is deinitialized on FetchResponse. Aborted fetch."));
		return;
	}

    if (!bSuccess || !Response.IsValid())
    {
        return;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Response->GetContentAsString());

    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        return;
    }

    ConfigCache.Empty();

    for (const auto& Pair : Root->Values)
    {
        ConfigCache.Add(Pair.Key, Pair.Value);
    }
}


bool UKRollSubsystem::GetBool(const FString& Key, bool DefaultValue) const
{
    if (const TSharedPtr<FJsonValue>* V = ConfigCache.Find(Key))
    {
        return (*V)->Type == EJson::Boolean ? (*V)->AsBool() : DefaultValue;
    }
    return DefaultValue;
}

FString UKRollSubsystem::GetString(const FString& Key, const FString& DefaultValue) const
{
    if (const TSharedPtr<FJsonValue>* V = ConfigCache.Find(Key))
    {
        return (*V)->Type == EJson::String ? (*V)->AsString() : DefaultValue;
    }
    return DefaultValue;
}

double UKRollSubsystem::GetNumber(const FString& Key, double DefaultValue) const
{
    if (const TSharedPtr<FJsonValue>* V = ConfigCache.Find(Key))
    {
        return (*V)->Type == EJson::Number ? (*V)->AsNumber() : DefaultValue;
    }
    return DefaultValue;
}

TSharedPtr<FJsonValue> UKRollSubsystem::GetJson(const FString& Key) const
{
    if (const TSharedPtr<FJsonValue>* V = ConfigCache.Find(Key))
    {
        return *V;
    }
    return nullptr;
}

