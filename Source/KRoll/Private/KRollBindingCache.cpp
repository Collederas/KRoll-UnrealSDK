#include "KRollBindingCache.h"

#include "GameFramework/Actor.h"
#include "Components/ActorComponent.h"
#include "KRollLog.h"

// GAS attribute data type
#include "AttributeSet.h" // FGameplayAttributeData

static constexpr TCHAR META_KRollKey[]      = TEXT("KRollKey");
static constexpr TCHAR META_KRollDefault[]  = TEXT("KRollDefault");
static constexpr TCHAR META_KRollScale[]    = TEXT("KRollScale");
static constexpr TCHAR META_KRollClampMin[] = TEXT("KRollClampMin");
static constexpr TCHAR META_KRollClampMax[] = TEXT("KRollClampMax");

const FKRollClassBindings& FKRollBindingCache::GetOrBuildActorBindings(UClass* ActorClass)
{
	static FKRollClassBindings Empty;
	if (!ActorClass)
	{
		return Empty;
	}

	if (FKRollClassBindings* Existing = ActorCache.Find(ActorClass))
	{
		return *Existing;
	}

	FKRollClassBindings Built;

	// Actor properties
	CollectBindingsForClass(ActorClass, Built.ActorBindings, /*bAllowGameplayAttributeData*/ false);

	// Component properties: discover component classes from the actor CDO
	const AActor* CDO = Cast<AActor>(ActorClass->GetDefaultObject());
	if (CDO)
	{
		TArray<UActorComponent*> Components;
		CDO->GetComponents(Components);

		for (UActorComponent* Comp : Components)
		{
			if (!Comp)
			{
				continue;
			}

			UClass* CompClass = Comp->GetClass();
			TArray<FKRollPropertyBinding>& CompBindings = Built.ComponentBindings.FindOrAdd(CompClass);

			CollectBindingsForClass(CompClass, CompBindings, /*bAllowGameplayAttributeData*/ false);
		}
	}

	ActorCache.Add(ActorClass, MoveTemp(Built));
	return *ActorCache.Find(ActorClass);
}

const TArray<FKRollPropertyBinding>& FKRollBindingCache::GetOrBuildAttributeSetBindings(UClass* AttributeSetClass)
{
	static TArray<FKRollPropertyBinding> Empty;
	if (!AttributeSetClass)
	{
		return Empty;
	}

	if (TArray<FKRollPropertyBinding>* Existing = AttributeSetCache.Find(AttributeSetClass))
	{
		return *Existing;
	}

	TArray<FKRollPropertyBinding> Built;
	CollectBindingsForClass(AttributeSetClass, Built, /*bAllowGameplayAttributeData*/ true);

	AttributeSetCache.Add(AttributeSetClass, MoveTemp(Built));
	return *AttributeSetCache.Find(AttributeSetClass);
}

void FKRollBindingCache::CollectBindingsForClass(
	UClass* Class,
	TArray<FKRollPropertyBinding>& Out,
	bool bAllowGameplayAttributeData
)
{
	for (TFieldIterator<FProperty> It(Class, EFieldIteratorFlags::IncludeSuper); It; ++It)
	{
		FProperty* Prop = *It;
		FKRollPropertyBinding Binding;
		if (TryBuildBindingFromProperty(Prop, Binding, bAllowGameplayAttributeData))
		{
			Out.Add(MoveTemp(Binding));
		}
	}
}

bool FKRollBindingCache::TryBuildBindingFromProperty(
	FProperty* Prop,
	FKRollPropertyBinding& Out,
	bool bAllowGameplayAttributeData
)
{
	if (!Prop || !Prop->HasMetaData(META_KRollKey))
	{
		return false;
	}

	const FString KeyStr = Prop->GetMetaData(META_KRollKey);
	if (KeyStr.IsEmpty())
	{
		return false;
	}

	Out.Property = Prop;
	Out.KeyTemplate = FName(*KeyStr);

	ParseTransformMeta(Prop, Out.Transform);

	// Supported primitive kinds
	if (CastField<FBoolProperty>(Prop))
	{
		Out.Kind = EKRollValueKind::Bool;
		return true;
	}
	if (CastField<FIntProperty>(Prop))
	{
		Out.Kind = EKRollValueKind::Int32;
		return true;
	}
	if (CastField<FFloatProperty>(Prop))
	{
		Out.Kind = EKRollValueKind::Float;
		return true;
	}

	// AttributeData only when building for AttributeSet classes
	if (bAllowGameplayAttributeData)
	{
		if (FStructProperty* StructProp = CastField<FStructProperty>(Prop))
		{
			if (StructProp->Struct == FGameplayAttributeData::StaticStruct())
			{
				Out.Kind = EKRollValueKind::GameplayAttributeData;
				return true;
			}
		}
	}

	const UClass* Owner = Prop->GetOwnerClass();
	const uint64 Key = FKRollLogOnce::MakeKey(Owner, Prop, KROLL_REASON_UNSUPPORTED_PROP);
	if (FKRollLogOnce::ShouldLog(Key))
	{
		UE_LOG(LogKRoll, Warning,
			   TEXT("KRoll: unsupported property type for binding on %s.%s (property type: %s). Supported: bool/int32/float%s"),
			   Owner ? *Owner->GetName() : TEXT("UnknownClass"),
			   *Prop->GetName(),
			   *Prop->GetClass()->GetName(),
			   bAllowGameplayAttributeData ? TEXT("/GameplayAttributeData") : TEXT(""));
	}
	return false;
}

void FKRollBindingCache::ParseTransformMeta(FProperty* Prop, FKRollTransform& Out)
{
	// Scale (optional)
	ParseDoubleMeta(Prop, META_KRollScale, Out.Scale);

	// Clamp (optional)
	ParseOptionalDoubleMeta(Prop, META_KRollClampMin, Out.ClampMin);
	ParseOptionalDoubleMeta(Prop, META_KRollClampMax, Out.ClampMax);

	// Default (optional)
	ParseOptionalDoubleMeta(Prop, META_KRollDefault, Out.DefaultValue);
}

bool FKRollBindingCache::ParseOptionalDoubleMeta(FProperty* Prop, const TCHAR* MetaKey, TOptional<double>& OutVal)
{
	if (!Prop || !Prop->HasMetaData(MetaKey))
	{
		return false;
	}

	const FString V = Prop->GetMetaData(MetaKey);
	if (V.IsEmpty())
	{
		return false;
	}

	OutVal = FCString::Atod(*V);
	return true;
}

bool FKRollBindingCache::ParseDoubleMeta(FProperty* Prop, const TCHAR* MetaKey, double& OutVal)
{
	if (!Prop || !Prop->HasMetaData(MetaKey))
	{
		return false;
	}

	const FString V = Prop->GetMetaData(MetaKey);
	if (V.IsEmpty())
	{
		return false;
	}

	OutVal = FCString::Atod(*V);
	return true;
}
