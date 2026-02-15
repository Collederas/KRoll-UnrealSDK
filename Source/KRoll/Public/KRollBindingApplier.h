#pragma once

#include "CoreMinimal.h"
#include "KRollBindingTypes.h"

class UKRollSubsystem;
class UAbilitySystemComponent;
class UAttributeSet;

class KROLL_API FKRollBindingApplier
{
public:
	static void ApplyBindings(
		UObject* Target,
		const TArray<FKRollPropertyBinding>& Bindings,
		const UKRollSubsystem* KRollSubsystem,
		const AActor* KeyContextActor
	);

	static void ApplyAttributeSetBindings(
		UAttributeSet* AttributeSet,
		UAbilitySystemComponent* ASC,
		const TArray<FKRollPropertyBinding>& Bindings,
		const UKRollSubsystem* KRollSubsystem,
		const AActor* KeyContextActor
	);

private:
	static bool ReadBool(const UKRollSubsystem* KRoll, FName Key, bool& Out);
	static bool ReadNumber(const UKRollSubsystem* KRoll, FName Key, double& Out);

	static double ApplyTransform(double V, const FKRollTransform& T);

	static void WriteBool(FProperty* Prop, void* Container, bool V);
	static void WriteInt32(FProperty* Prop, void* Container, int32 V);
	static void WriteFloat(FProperty* Prop, void* Container, float V);
};
