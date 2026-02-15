#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "KRollBindingCache.h"
#include "KRollBindingWorldSubsystem.generated.h"

class UKRollSubsystem;

UCLASS()
class KROLL_API UKRollBindingWorldSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintCallable, Category="KRoll")
	void ApplyManual(AActor* Actor);

private:
	FDelegateHandle ActorSpawnedHandle;

	TObjectPtr<UKRollSubsystem> KRoll = nullptr;
	TUniquePtr<FKRollBindingCache> Cache;

	TSet<TWeakObjectPtr<AActor>> DeferredActors;

	void HandleActorSpawned(AActor* Actor);
	void ScheduleInitNextTick(AActor* Actor);
	void TryInitActorNow(AActor* Actor);

	UFUNCTION()
	void OnConfigReady();

	void ApplyActorAndComponents(AActor* Actor);
	void ApplyAttributeSets(AActor* Actor);
};
