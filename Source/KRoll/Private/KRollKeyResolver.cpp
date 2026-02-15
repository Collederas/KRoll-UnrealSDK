#include "KRollKeyResolver.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"

static constexpr TCHAR TOKEN_ARCHETYPE[] = TEXT("{archetype}");
static constexpr TCHAR TOKEN_CLASS[]     = TEXT("{class}");

const AActor* FKRollKeyResolver::ResolveOwningActor(const UObject* Target)
{
	if (!Target)
	{
		return nullptr;
	}

	if (const AActor* AsActor = Cast<AActor>(Target))
	{
		return AsActor;
	}

	if (const UActorComponent* AsComp = Cast<UActorComponent>(Target))
	{
		return AsComp->GetOwner();
	}

	// AttributeSets are usually Outer'd to ASC; try to walk up to an Actor if possible
	const UObject* Outer = Target->GetOuter();
	while (Outer)
	{
		if (const AActor* OuterActor = Cast<AActor>(Outer))
		{
			return OuterActor;
		}
		if (const UActorComponent* OuterComp = Cast<UActorComponent>(Outer))
		{
			return OuterComp->GetOwner();
		}
		Outer = Outer->GetOuter();
	}

	return nullptr;
}

bool FKRollKeyResolver::TryGetArchetypeFromActor(const AActor* Actor, FString& OutLower)
{
	if (!Actor)
	{
		return false;
	}

	// Convention-based property lookup: optional UPROPERTY(EditDefaultsOnly) FName KRollArchetype;
	const FProperty* Prop = Actor->GetClass()->FindPropertyByName(TEXT("KRollArchetype"));
	const FNameProperty* NameProp = CastField<FNameProperty>(Prop);
	if (!NameProp)
	{
		return false;
	}

	const void* ValuePtr = NameProp->ContainerPtrToValuePtr<void>(Actor);
	const FName Archetype = NameProp->GetPropertyValue(ValuePtr);
	if (Archetype.IsNone())
	{
		return false;
	}

	OutLower = Archetype.ToString().ToLower();
	return !OutLower.IsEmpty();
}

FString FKRollKeyResolver::NormalizeClassName(FString In)
{
	// Conservative normalization (keep stable across refactors as much as possible)
	In.RemoveFromEnd(TEXT("_C"));
	In = In.ToLower();

	// Optional light heuristics; keep minimal to avoid surprises
	In.ReplaceInline(TEXT("bp_"), TEXT(""), ESearchCase::IgnoreCase);
	In.ReplaceInline(TEXT("b_"), TEXT(""), ESearchCase::IgnoreCase);

	In.TrimStartAndEndInline();

	return In.IsEmpty() ? TEXT("unknown") : In;
}

FString FKRollKeyResolver::ResolveClassToken(const UObject* Target)
{
	if (!Target)
	{
		return TEXT("unknown");
	}

	return NormalizeClassName(Target->GetClass()->GetName());
}

FString FKRollKeyResolver::ResolveArchetypeToken(const UObject* Target)
{
	const AActor* OwnerActor = ResolveOwningActor(Target);

	FString Archetype;
	if (TryGetArchetypeFromActor(OwnerActor, Archetype))
	{
		return Archetype;
	}

	// Fallback to class token (based on owning actor if available, else target class)
	if (OwnerActor)
	{
		return NormalizeClassName(OwnerActor->GetClass()->GetName());
	}

	return ResolveClassToken(Target);
}

FName FKRollKeyResolver::ResolveKey(const UObject* Target, const FName& KeyTemplate)
{
	FString Key = KeyTemplate.ToString();
	if (Key.IsEmpty())
	{
		return NAME_None;
	}

	const FString Archetype = ResolveArchetypeToken(Target);
	const FString ClassTok  = ResolveClassToken(Target);

	Key.ReplaceInline(TOKEN_ARCHETYPE, *Archetype, ESearchCase::IgnoreCase);
	Key.ReplaceInline(TOKEN_CLASS, *ClassTok, ESearchCase::IgnoreCase);

	return FName(*Key);
}
