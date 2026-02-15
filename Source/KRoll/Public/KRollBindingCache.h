#pragma once

#include "CoreMinimal.h"
#include "KRollBindingTypes.h"

/**
	* Per-class bindings (MVP):
	*  - Actor bindings: bool/int32/float properties on the actor class
	*  - Component bindings: discovered from actor CDO's components, keyed by component class
	*  - AttributeSet bindings: built per AttributeSet class on demand
	*/
struct FKRollClassBindings
{
	TArray<FKRollPropertyBinding> ActorBindings;
	TMap<UClass*, TArray<FKRollPropertyBinding>> ComponentBindings;
};

class KROLL_API FKRollBindingCache
{
public:
	const FKRollClassBindings& GetOrBuildActorBindings(UClass* ActorClass);
	const TArray<FKRollPropertyBinding>& GetOrBuildAttributeSetBindings(UClass* AttributeSetClass);

private:
	// Cache (weak keys avoid holding classes alive on hot reload)
	TMap<TWeakObjectPtr<UClass>, FKRollClassBindings> ActorCache;
	TMap<TWeakObjectPtr<UClass>, TArray<FKRollPropertyBinding>> AttributeSetCache;

	// Builders
	static void CollectBindingsForClass(
		UClass* Class,
		TArray<FKRollPropertyBinding>& Out,
		bool bAllowGameplayAttributeData
	);

	static bool TryBuildBindingFromProperty(
		FProperty* Prop,
		FKRollPropertyBinding& Out,
		bool bAllowGameplayAttributeData
	);

	static void ParseTransformMeta(FProperty* Prop, FKRollTransform& Out);

	static bool ParseOptionalDoubleMeta(FProperty* Prop, const TCHAR* MetaKey, TOptional<double>& OutVal);
	static bool ParseDoubleMeta(FProperty* Prop, const TCHAR* MetaKey, double& OutVal);
};
