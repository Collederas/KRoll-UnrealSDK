#include "KRollBindingWorldSubsystem.h"

#include "KRollSubsystem.h"
#include "KRollBindingApplier.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Components/ActorComponent.h"

// GAS
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

namespace
{
void AddUniqueActor(TArray<AActor*>& InOutActors, AActor* Actor)
{
	if (Actor)
	{
		InOutActors.AddUnique(Actor);
	}
}

AActor* ResolvePawnLikeContext(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (APawn* Pawn = Cast<APawn>(Actor))
	{
		return Pawn;
	}

	if (AController* Controller = Cast<AController>(Actor))
	{
		if (APawn* Pawn = Controller->GetPawn())
		{
			return Pawn;
		}
	}

	if (APlayerState* PlayerState = Cast<APlayerState>(Actor))
	{
		if (APawn* Pawn = PlayerState->GetPawn())
		{
			return Pawn;
		}
	}

	return Actor;
}

UAbilitySystemComponent* FindASCOnActor(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (IAbilitySystemInterface* Interface = Cast<IAbilitySystemInterface>(Actor))
	{
		if (UAbilitySystemComponent* ASC = Interface->GetAbilitySystemComponent())
		{
			return ASC;
		}
	}

	return Actor->FindComponentByClass<UAbilitySystemComponent>();
}

UAbilitySystemComponent* ResolveASCAndKeyContext(AActor* SourceActor, AActor*& OutKeyContext)
{
	OutKeyContext = ResolvePawnLikeContext(SourceActor);

	TArray<AActor*> Candidates;
	Candidates.Reserve(8);
	AddUniqueActor(Candidates, OutKeyContext);
	AddUniqueActor(Candidates, SourceActor);

	if (APawn* Pawn = Cast<APawn>(SourceActor))
	{
		AddUniqueActor(Candidates, Pawn->GetPlayerState());
		AddUniqueActor(Candidates, Pawn->GetController());
	}

	if (AController* Controller = Cast<AController>(SourceActor))
	{
		AddUniqueActor(Candidates, Controller->GetPawn());
		AddUniqueActor(Candidates, Controller->GetPlayerState<APlayerState>());
	}

	if (APlayerState* PlayerState = Cast<APlayerState>(SourceActor))
	{
		AddUniqueActor(Candidates, PlayerState->GetPawn());
	}

	AddUniqueActor(Candidates, SourceActor ? SourceActor->GetOwner() : nullptr);

	for (AActor* Candidate : Candidates)
	{
		UAbilitySystemComponent* ASC = FindASCOnActor(Candidate);
		if (!ASC)
		{
			continue;
		}

		if (AActor* Avatar = Cast<AActor>(ASC->GetAvatarActor()))
		{
			OutKeyContext = ResolvePawnLikeContext(Avatar);
		}
		else if (!OutKeyContext)
		{
			OutKeyContext = ResolvePawnLikeContext(Candidate);
		}

		return ASC;
	}

	return nullptr;
}
}

void UKRollBindingWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (UGameInstance* GI = World->GetGameInstance())
	{
		KRoll = GI->GetSubsystem<UKRollSubsystem>();
	}

	Cache = MakeUnique<FKRollBindingCache>();

	ActorSpawnedHandle = World->AddOnActorSpawnedHandler(
		FOnActorSpawned::FDelegate::CreateUObject(this, &UKRollBindingWorldSubsystem::HandleActorSpawned)
	);

	if (KRoll)
	{
		KRoll->OnConfigReady.AddDynamic(this, &UKRollBindingWorldSubsystem::OnConfigReady);
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->HasAuthority())
		{
			continue;
		}
		ScheduleInitNextTick(Actor);
	}
}

void UKRollBindingWorldSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->RemoveOnActorSpawnedHandler(ActorSpawnedHandle);
	}

	if (KRoll)
	{
		KRoll->OnConfigReady.RemoveDynamic(this, &UKRollBindingWorldSubsystem::OnConfigReady);
	}

	DeferredActors.Empty();
	Cache.Reset();
	KRoll = nullptr;

	Super::Deinitialize();
}

void UKRollBindingWorldSubsystem::HandleActorSpawned(AActor* Actor)
{
	if (!Actor || !Actor->HasAuthority())
	{
		return;
	}

	ScheduleInitNextTick(Actor);
}

void UKRollBindingWorldSubsystem::ScheduleInitNextTick(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	TWeakObjectPtr<AActor> WeakActor = Actor;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick([this, WeakActor]()
													 {
														 AActor* A = WeakActor.Get();
														 if (!A || !A->HasAuthority())
														 {
															 return;
														 }
														 TryInitActorNow(A);
													 });
	}
}

void UKRollBindingWorldSubsystem::TryInitActorNow(AActor* Actor)
{
	if (!Actor || !KRoll || !Cache.IsValid())
	{
		return;
	}

	if (!KRoll->IsReady())
	{
		DeferredActors.Add(Actor);
		return;
	}

	ApplyActorAndComponents(Actor);
	ApplyAttributeSets(Actor);
}

void UKRollBindingWorldSubsystem::OnConfigReady()
{
	if (!KRoll || !KRoll->IsReady())
	{
		return;
	}

	TArray<TWeakObjectPtr<AActor>> Work;
	Work.Reserve(DeferredActors.Num());

	for (const TWeakObjectPtr<AActor>& W : DeferredActors)
	{
		Work.Add(W);
	}
	DeferredActors.Empty();

	for (const TWeakObjectPtr<AActor>& W : Work)
	{
		AActor* Actor = W.Get();
		if (!Actor || !Actor->HasAuthority())
		{
			continue;
		}

		TryInitActorNow(Actor);
	}
}

void UKRollBindingWorldSubsystem::ApplyManual(AActor* Actor)
{
	if (!Actor || !Actor->HasAuthority())
	{
		return;
	}

	TryInitActorNow(Actor);
}

void UKRollBindingWorldSubsystem::ApplyActorAndComponents(AActor* Actor)
{
	const FKRollClassBindings& Bindings = Cache->GetOrBuildActorBindings(Actor->GetClass());

	FKRollBindingApplier::ApplyBindings(Actor, Bindings.ActorBindings, KRoll, Actor);

	TArray<UActorComponent*> Components;
	Actor->GetComponents(Components);

	for (UActorComponent* Comp : Components)
	{
		if (!Comp)
		{
			continue;
		}

		if (const TArray<FKRollPropertyBinding>* CompBindings = Bindings.ComponentBindings.Find(Comp->GetClass()))
		{
			FKRollBindingApplier::ApplyBindings(Comp, *CompBindings, KRoll, Actor);
		}
	}
}

void UKRollBindingWorldSubsystem::ApplyAttributeSets(AActor* Actor)
{
	AActor* KeyContextActor = Actor;
	UAbilitySystemComponent* ASC = ResolveASCAndKeyContext(Actor, KeyContextActor);
	if (!ASC)
	{
		return;
	}

	const TArray<UAttributeSet*>& Sets = ASC->GetSpawnedAttributes();

	if (Sets.Num() == 0)
	{
		return;
	}

	for (UAttributeSet* Set : Sets)
	{
		if (!Set)
		{
			continue;
		}

		const TArray<FKRollPropertyBinding>& SetBindings = Cache->GetOrBuildAttributeSetBindings(Set->GetClass());
		FKRollBindingApplier::ApplyAttributeSetBindings(Set, ASC, SetBindings, KRoll, KeyContextActor ? KeyContextActor : Actor);
	}
}
