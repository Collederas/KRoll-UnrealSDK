#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Http.h"
#include "Dom/JsonObject.h"
#include "KRollSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FKrollConfigReadyDelegate);

class UKRollSettings;

USTRUCT(BlueprintType)
struct FKRollSnapshotMeta
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="KRoll")
	int32 SchemaVersion = 0;

	UPROPERTY(BlueprintReadOnly, Category="KRoll")
	FString ActiveSnapshotId;

	UPROPERTY(BlueprintReadOnly, Category="KRoll")
	FString ActiveSnapshotHash;

	UPROPERTY(BlueprintReadOnly, Category="KRoll")
	FDateTime PublishedAt;

	UPROPERTY(BlueprintReadOnly, Category="KRoll")
	FString Label;

	bool IsValid() const
	{
		return SchemaVersion > 0 && !ActiveSnapshotId.IsEmpty();
	}
};

UCLASS()
class KROLL_API UKRollSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="KRoll")
	void FetchConfigs();

	UFUNCTION(BlueprintPure, Category="KRoll")
	bool IsReady() const { return bIsReady; }

	// Snapshot meta (optional but recommended to surface)
	UFUNCTION(BlueprintPure, Category="KRoll")
	bool HasSnapshotMeta() const { return bHasSnapshotMeta; }

	UFUNCTION(BlueprintPure, Category="KRoll")
	FKRollSnapshotMeta GetSnapshotMeta() const;

	bool GetBool(FName Key, bool& OutValue) const;
	bool GetNumber(FName Key, double& OutValue) const;
	bool GetString(FName Key, FString& OutValue) const;
	TSharedPtr<FJsonValue> GetJson(FName Key) const;

	UPROPERTY(BlueprintAssignable, Category="KRoll")
	FKrollConfigReadyDelegate OnConfigReady;

private:
	void OnFetchResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);

	// Cache is keyed by dotted path: "characters.zombie.health"
	mutable FRWLock CacheLock;
	TMap<FName, TSharedPtr<FJsonValue>> Cache;

	bool bIsReady = false;
	TSharedPtr<IHttpRequest, ESPMode::ThreadSafe> ActiveRequest;

	// Snapshot meta is stored separately from values to avoid polluting the keyspace
	mutable FRWLock MetaLock;
	bool bHasSnapshotMeta = false;
	FKRollSnapshotMeta SnapshotMeta;

	// Helpers
	static bool ParseRootObject(const FString& JsonText, TSharedPtr<FJsonObject>& OutRoot);

	static bool ParseSnapshotMeta(const TSharedPtr<FJsonObject>& RootObj, FKRollSnapshotMeta& OutMeta);
	static bool BuildCacheFromEnvelope(const TSharedPtr<FJsonObject>& RootObj, TMap<FName, TSharedPtr<FJsonValue>>& OutCache);

	static void FlattenJsonObject(
		const TSharedPtr<FJsonObject>& Obj,
		const FString& Prefix,
		TMap<FName, TSharedPtr<FJsonValue>>& Out
	);

	static void FlattenJsonValue(
		const TSharedPtr<FJsonValue>& Val,
		const FString& Prefix,
		TMap<FName, TSharedPtr<FJsonValue>>& Out
	);

	static FString JoinPath(const FString& Prefix, const FString& Key);
};
