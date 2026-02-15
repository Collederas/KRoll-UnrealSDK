#include "KRollSubsystem.h"
#include "KRollSettings.h"
#include "KRollLog.h"

#include "HttpModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

void UKRollSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UKRollSettings* Settings = GetDefault<UKRollSettings>();
	if (Settings && Settings->bAutoFetchOnInit)
	{
		FetchConfigs();
	}
}

void UKRollSubsystem::Deinitialize()
{
	{
		FWriteScopeLock Lock(CacheLock);
		Cache.Empty();
	}

	{
		FWriteScopeLock Lock(MetaLock);
		bHasSnapshotMeta = false;
		SnapshotMeta = FKRollSnapshotMeta{};
	}

	bIsReady = false;

	if (ActiveRequest.IsValid())
	{
		ActiveRequest->CancelRequest();
	}
	ActiveRequest.Reset();

	Super::Deinitialize();
}

void UKRollSubsystem::FetchConfigs()
{
	const UKRollSettings* Settings = GetDefault<UKRollSettings>();
	if (!Settings || Settings->Host.IsEmpty() || Settings->ApiKey.IsEmpty())
	{
		return; // misconfigured SDK → safe no-op
	}

	// Cancel any in-flight request (MVP)
	if (ActiveRequest.IsValid())
	{
		ActiveRequest->CancelRequest();
		ActiveRequest.Reset();
	}

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();

	// TODO: sanitize/normalize host (e.g. remove trailing slash)
	const FString Url = Settings->Host / TEXT("client/config/fetch");

	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	// Auth header
	Request->SetHeader(TEXT("X-API-Key"), Settings->ApiKey);

	// TODO: body may include app/version/environment; keep "{}" for MVP
	Request->SetContentAsString(TEXT("{}"));

	Request->OnProcessRequestComplete().BindUObject(this, &UKRollSubsystem::OnFetchResponse);

	ActiveRequest = Request;
	Request->ProcessRequest();
}

bool UKRollSubsystem::ParseRootObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutRoot)
{
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	return FJsonSerializer::Deserialize(Reader, OutRoot) && OutRoot.IsValid();
}

FString UKRollSubsystem::JoinPath(const FString& Prefix, const FString& Key)
{
	if (Prefix.IsEmpty()) return Key;
	if (Key.IsEmpty()) return Prefix;
	return Prefix + TEXT(".") + Key;
}

void UKRollSubsystem::FlattenJsonObject(
	const TSharedPtr<FJsonObject>& Obj,
	const FString& Prefix,
	TMap<FName, TSharedPtr<FJsonValue>>& Out
)
{
	if (!Obj.IsValid())
	{
		return;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Obj->Values)
	{
		if (!Pair.Value.IsValid())
		{
			continue;
		}

		const FString NewPrefix = JoinPath(Prefix, Pair.Key);
		FlattenJsonValue(Pair.Value, NewPrefix, Out);
	}
}

void UKRollSubsystem::FlattenJsonValue(
	const TSharedPtr<FJsonValue>& Val,
	const FString& Prefix,
	TMap<FName, TSharedPtr<FJsonValue>>& Out
)
{
	if (!Val.IsValid())
	{
		return;
	}

	switch (Val->Type)
	{
		case EJson::Object:
		{
			FlattenJsonObject(Val->AsObject(), Prefix, Out);
			return;
		}

		// If arrays exist, we keep them addressable as a whole value at Prefix.
		case EJson::Array:
		case EJson::String:
		case EJson::Number:
		case EJson::Boolean:
		case EJson::Null:
		default:
		{
			Out.Add(FName(*Prefix), Val);
			return;
		}
	}
}

bool UKRollSubsystem::ParseSnapshotMeta(const TSharedPtr<FJsonObject>& RootObj, FKRollSnapshotMeta& OutMeta)
{
	if (!RootObj.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* MetaObjPtr = nullptr;
	if (!RootObj->TryGetObjectField(TEXT("meta"), MetaObjPtr) || !MetaObjPtr || !MetaObjPtr->IsValid())
	{
		return false; // meta is optional
	}

	const TSharedPtr<FJsonObject>& MetaObj = *MetaObjPtr;

	OutMeta = FKRollSnapshotMeta{};

	OutMeta.SchemaVersion = MetaObj->GetIntegerField(TEXT("schema_version"));
	OutMeta.ActiveSnapshotId = MetaObj->GetStringField(TEXT("active_snapshot_id"));
	OutMeta.ActiveSnapshotHash = MetaObj->GetStringField(TEXT("active_snapshot_hash"));
	OutMeta.Label = MetaObj->GetStringField(TEXT("label"));

	// published_at is ISO 8601. If parse fails, keep default (min) and still treat meta as present.
	const FString PublishedAtStr = MetaObj->GetStringField(TEXT("published_at"));
	FDateTime::ParseIso8601(*PublishedAtStr, OutMeta.PublishedAt);

	return OutMeta.IsValid();
}

bool UKRollSubsystem::BuildCacheFromEnvelope(const TSharedPtr<FJsonObject>& RootObj, TMap<FName, TSharedPtr<FJsonValue>>& OutCache)
{
	if (!RootObj.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* ValuesObjPtr = nullptr;
	if (!RootObj->TryGetObjectField(TEXT("values"), ValuesObjPtr) || !ValuesObjPtr || !ValuesObjPtr->IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>& ValuesObj = *ValuesObjPtr;

	for (const TPair<FString, TSharedPtr<FJsonValue>>& It : ValuesObj->Values)
	{
		if (It.Key.IsEmpty() || !It.Value.IsValid())
		{
			continue;
		}

		OutCache.Add(FName(*It.Key), It.Value);
	}

	return true;
}

FKRollSnapshotMeta UKRollSubsystem::GetSnapshotMeta() const
{
	FReadScopeLock Lock(MetaLock);
	return SnapshotMeta;
}

void UKRollSubsystem::OnFetchResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	ActiveRequest.Reset();

	if (!bSuccess || !Response.IsValid())
	{
		return; // keep previous cache
	}

	const int32 Code = Response->GetResponseCode();
	if (Code < 200 || Code >= 300)
	{
		return; // keep previous cache
	}

	TSharedPtr<FJsonObject> RootObj;
	if (!ParseRootObject(Response->GetContentAsString(), RootObj))
	{
		return; // keep previous cache
	}

	// Build new cache from envelope
	TMap<FName, TSharedPtr<FJsonValue>> NewCache;
	if (!BuildCacheFromEnvelope(RootObj, NewCache))
	{
		return; // keep previous cache
	}

	// Parse meta (optional)
	FKRollSnapshotMeta NewMeta;
	const bool bParsedMeta = ParseSnapshotMeta(RootObj, NewMeta);

	{
		FWriteScopeLock Lock(CacheLock);
		Cache = MoveTemp(NewCache);
	}

	{
		FWriteScopeLock Lock(MetaLock);
		bHasSnapshotMeta = bParsedMeta;
		SnapshotMeta = bParsedMeta ? NewMeta : FKRollSnapshotMeta{};
	}

	bIsReady = true;

	// Log once when we become ready (useful for ops/telemetry)
	if (bHasSnapshotMeta)
	{
		UE_LOG(LogKRoll, Log, TEXT("KRoll ready: snapshot_id=%s hash=%s published_at=%s label=%s"),
			   *SnapshotMeta.ActiveSnapshotId,
			   *SnapshotMeta.ActiveSnapshotHash,
			   *SnapshotMeta.PublishedAt.ToIso8601(),
			   *SnapshotMeta.Label
			   );
	}
	else
	{
		UE_LOG(LogKRoll, Log, TEXT("KRoll ready: snapshot meta missing (values loaded)"));
	}

	OnConfigReady.Broadcast();
}

TSharedPtr<FJsonValue> UKRollSubsystem::GetJson(FName Key) const
{
	FReadScopeLock Lock(CacheLock);
	const TSharedPtr<FJsonValue>* Found = Cache.Find(Key);
	return Found ? *Found : nullptr;
}

bool UKRollSubsystem::GetBool(FName Key, bool& OutValue) const
{
	TSharedPtr<FJsonValue> V = GetJson(Key);
	if (!V.IsValid())
	{
		return false;
	}

	if (V->Type == EJson::Boolean)
	{
		OutValue = V->AsBool();
		return true;
	}

	const UKRollSettings* Settings = GetDefault<UKRollSettings>();
	if (Settings && Settings->bAllowTypeCoercion)
	{
		if (V->Type == EJson::Number)
		{
			OutValue = (V->AsNumber() != 0.0);
			return true;
		}
		if (V->Type == EJson::String)
		{
			const FString S = V->AsString().ToLower();
			if (S == TEXT("true") || S == TEXT("1") || S == TEXT("yes") || S == TEXT("y"))
			{
				OutValue = true; return true;
			}
			if (S == TEXT("false") || S == TEXT("0") || S == TEXT("no") || S == TEXT("n"))
			{
				OutValue = false; return true;
			}
		}
	}

	return false;
}

bool UKRollSubsystem::GetNumber(FName Key, double& OutValue) const
{
	TSharedPtr<FJsonValue> V = GetJson(Key);
	if (!V.IsValid())
	{
		return false;
	}

	if (V->Type == EJson::Number)
	{
		OutValue = V->AsNumber();
		return true;
	}

	const UKRollSettings* Settings = GetDefault<UKRollSettings>();
	if (Settings && Settings->bAllowTypeCoercion)
	{
		if (V->Type == EJson::Boolean)
		{
			OutValue = V->AsBool() ? 1.0 : 0.0;
			return true;
		}
		if (V->Type == EJson::String)
		{
			const FString& S = V->AsString();
			double Parsed = 0.0;

			if (LexTryParseString(Parsed, *S))
			{
				OutValue = Parsed;
				return true;
			}
		}
	}

	return false;
}

bool UKRollSubsystem::GetString(FName Key, FString& OutValue) const
{
	TSharedPtr<FJsonValue> V = GetJson(Key);
	if (!V.IsValid())
	{
		return false;
	}

	if (V->Type == EJson::String)
	{
		OutValue = V->AsString();
		return true;
	}

	const UKRollSettings* Settings = GetDefault<UKRollSettings>();
	if (Settings && Settings->bAllowTypeCoercion)
	{
		if (V->Type == EJson::Number)
		{
			OutValue = FString::SanitizeFloat(V->AsNumber());
			return true;
		}
		if (V->Type == EJson::Boolean)
		{
			OutValue = V->AsBool() ? TEXT("true") : TEXT("false");
			return true;
		}
	}

	return false;
}
