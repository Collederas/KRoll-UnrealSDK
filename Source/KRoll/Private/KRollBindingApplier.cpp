#include "KRollBindingApplier.h"

#include "KRollSubsystem.h"
#include "KRollKeyResolver.h"
#include "KRollLog.h"

// GAS
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"

namespace
{
static constexpr int32 AttributeSetSeedPasses = 2;

double ApplyTransformForBinding(double V, const FKRollTransform& T)
{
	V *= T.Scale;

	if (T.ClampMin.IsSet())
	{
		V = FMath::Max(V, *T.ClampMin);
	}
	if (T.ClampMax.IsSet())
	{
		V = FMath::Min(V, *T.ClampMax);
	}
	return V;
}

bool ResolveNumericBindingValue(
	const FKRollPropertyBinding& Binding,
	const UKRollSubsystem* KRollSubsystem,
	const AActor* KeyContextActor,
	const UObject* FallbackContext,
	double& OutValue
)
{
	if (!Binding.Property || Binding.KeyTemplate.IsNone() || !KRollSubsystem || !KRollSubsystem->IsReady())
	{
		return false;
	}

	const FName ResolvedKey =
		FKRollKeyResolver::ResolveKey(
			KeyContextActor ? static_cast<const UObject*>(KeyContextActor) : FallbackContext,
			Binding.KeyTemplate
	);
	if (ResolvedKey.IsNone())
	{
		return false;
	}

	const FString KeyStr = ResolvedKey.ToString();
	if (KeyStr.Contains(TEXT("{")))
	{
		const UClass* Owner = FallbackContext ? FallbackContext->GetClass() : nullptr;
		const uint64 LogKey = FKRollLogOnce::MakeKey(Owner, Binding.Property, KROLL_REASON_UNRESOLVED_TOKEN);
		if (FKRollLogOnce::ShouldLog(LogKey))
		{
			UE_LOG(LogKRoll, Warning,
				   TEXT("KRoll: unresolved token in key template \"%s\" for %s.%s -> \"%s\""),
				   *Binding.KeyTemplate.ToString(),
				   Owner ? *Owner->GetName() : TEXT("UnknownClass"),
				   Binding.Property ? *Binding.Property->GetName() : TEXT("UnknownProp"),
				   *KeyStr);
		}
		return false;
	}

	double Num = 0.0;
	const bool bFound = KRollSubsystem->GetNumber(ResolvedKey, Num);
	if (!bFound)
	{
		if (!Binding.Transform.DefaultValue.IsSet())
		{
			return false;
		}
		Num = *Binding.Transform.DefaultValue;
	}

	OutValue = ApplyTransformForBinding(Num, Binding.Transform);
	return true;
}
}

bool FKRollBindingApplier::ReadBool(const UKRollSubsystem* KRoll, FName Key, bool& Out)
{
	return KRoll && KRoll->IsReady() && KRoll->GetBool(Key, Out);
}

bool FKRollBindingApplier::ReadNumber(const UKRollSubsystem* KRoll, FName Key, double& Out)
{
	return KRoll && KRoll->IsReady() && KRoll->GetNumber(Key, Out);
}

double FKRollBindingApplier::ApplyTransform(double V, const FKRollTransform& T)
{
	V *= T.Scale;

	if (T.ClampMin.IsSet())
	{
		V = FMath::Max(V, *T.ClampMin);
	}
	if (T.ClampMax.IsSet())
	{
		V = FMath::Min(V, *T.ClampMax);
	}
	return V;
}

void FKRollBindingApplier::WriteBool(FProperty* Prop, void* Container, bool V)
{
	if (FBoolProperty* BP = CastField<FBoolProperty>(Prop))
	{
		BP->SetPropertyValue(Container, V);
	}
}

void FKRollBindingApplier::WriteInt32(FProperty* Prop, void* Container, int32 V)
{
	if (FIntProperty* IP = CastField<FIntProperty>(Prop))
	{
		IP->SetPropertyValue(Container, V);
	}
}

void FKRollBindingApplier::WriteFloat(FProperty* Prop, void* Container, float V)
{
	if (FFloatProperty* FP = CastField<FFloatProperty>(Prop))
	{
		FP->SetPropertyValue(Container, V);
	}
}

void FKRollBindingApplier::ApplyBindings(
	UObject* Target,
	const TArray<FKRollPropertyBinding>& Bindings,
	const UKRollSubsystem* KRollSubsystem,
	const AActor* KeyContextActor
)
{
	if (!Target || !KRollSubsystem || !KRollSubsystem->IsReady())
	{
		return;
	}

	for (const FKRollPropertyBinding& B : Bindings)
	{
		if (!B.Property || B.KeyTemplate.IsNone())
		{
			continue;
		}

		const FName ResolvedKey =
			FKRollKeyResolver::ResolveKey(
				KeyContextActor ? static_cast<const UObject*>(KeyContextActor) : Target,
				B.KeyTemplate
		);
		if (ResolvedKey.IsNone())
		{
			continue;
		}

		const FString KeyStr = ResolvedKey.ToString();
		if (KeyStr.Contains(TEXT("{")))
		{
			const UClass* Owner = Target->GetClass();
			const uint64 LogKey = FKRollLogOnce::MakeKey(Owner, B.Property, KROLL_REASON_UNRESOLVED_TOKEN);
			if (FKRollLogOnce::ShouldLog(LogKey))
			{
				UE_LOG(LogKRoll, Warning,
					   TEXT("KRoll: unresolved token in key template \"%s\" for %s.%s -> \"%s\""),
					   *B.KeyTemplate.ToString(),
					   Owner ? *Owner->GetName() : TEXT("UnknownClass"),
					   B.Property ? *B.Property->GetName() : TEXT("UnknownProp"),
					   *KeyStr);
			}
			continue;
		}

		void* ValuePtr = B.Property->ContainerPtrToValuePtr<void>(Target);
		if (!ValuePtr)
		{
			continue;
		}

		switch (B.Kind)
		{
			case EKRollValueKind::Bool:
			{
				bool V = false;
				const bool bFound = ReadBool(KRollSubsystem, ResolvedKey, V);
				if (!bFound)
				{
					if (!B.Transform.DefaultValue.IsSet())
					{
						continue;
					}
					V = (*B.Transform.DefaultValue != 0.0);
				}
				WriteBool(B.Property, ValuePtr, V);
				break;
			}

			case EKRollValueKind::Int32:
			{
				double Num = 0.0;
				const bool bFound = ReadNumber(KRollSubsystem, ResolvedKey, Num);
				if (!bFound)
				{
					if (!B.Transform.DefaultValue.IsSet())
					{
						continue;
					}
					Num = *B.Transform.DefaultValue;
				}
				Num = ApplyTransform(Num, B.Transform);
				const int32 V = FMath::RoundToInt(Num);
				WriteInt32(B.Property, ValuePtr, V);
				break;
			}

			case EKRollValueKind::Float:
			{
				double Num = 0.0;
				const bool bFound = ReadNumber(KRollSubsystem, ResolvedKey, Num);
				if (!bFound)
				{
					if (!B.Transform.DefaultValue.IsSet())
					{
						continue;
					}
					Num = *B.Transform.DefaultValue;
				}
				Num = ApplyTransform(Num, B.Transform);
				WriteFloat(B.Property, ValuePtr, (float)Num);
				break;
			}

			case EKRollValueKind::GameplayAttributeData:
				// AttributeSet values must be initialized through ASC. Use ApplyAttributeSetBindings().
				break;

			default:
				break;
		}
	}
}

void FKRollBindingApplier::ApplyAttributeSetBindings(
	UAttributeSet* AttributeSet,
	UAbilitySystemComponent* ASC,
	const TArray<FKRollPropertyBinding>& Bindings,
	const UKRollSubsystem* KRollSubsystem,
	const AActor* KeyContextActor
)
{
	if (!AttributeSet || !ASC || !KRollSubsystem || !KRollSubsystem->IsReady())
	{
		return;
	}

	bool bWroteAny = false;

	for (const FKRollPropertyBinding& B : Bindings)
	{
		if (B.Kind != EKRollValueKind::GameplayAttributeData || !B.Property)
		{
			continue;
		}

		double Num = 0.0;
		if (!ResolveNumericBindingValue(B, KRollSubsystem, KeyContextActor, AttributeSet, Num))
		{
			continue;
		}

		// DataTable-style seeding: write directly into the AttributeSet memory.
		// This bypasses ASC->SetNumericAttributeBase and therefore bypasses PreAttributeBaseChange clamps.
		if (FStructProperty* StructProp = CastField<FStructProperty>(B.Property))
		{
			// We only cache GameplayAttributeData properties under this kind.
			if (StructProp->Struct == TBaseStructure<FGameplayAttributeData>::Get())
			{
				FGameplayAttributeData* DataPtr = StructProp->ContainerPtrToValuePtr<FGameplayAttributeData>(AttributeSet);
				if (DataPtr)
				{
					const float V = static_cast<float>(Num);
					DataPtr->SetBaseValue(V);
					DataPtr->SetCurrentValue(V);
					bWroteAny = true;
				}
			}
		}
	}

	if (!bWroteAny)
	{
		return;
	}

	// Optional “nudge” to get state out to clients sooner.
	// This does not rebuild aggregators or fire change delegates (matching init semantics),
	// but helps replication cadence in practice.
	if (AActor* Owner = ASC->GetOwner())
	{
		Owner->ForceNetUpdate();
	}

	ASC->ForceReplication();
}
