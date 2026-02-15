#pragma once

#include "CoreMinimal.h"

/**
	* Resolves KRoll key templates using lightweight tokens.
	*
	* Supported tokens (MVP):
	*  - {archetype}: Actor.KRollArchetype (FName) if present, else {class}
	*  - {class}: normalized class name
	*
	* Note: For components / attribute sets, archetype resolution is based on the outer Actor if possible.
	*/
class KROLL_API FKRollKeyResolver
{
public:
	static FName ResolveKey(const UObject* Target, const FName& KeyTemplate);

private:
	static const AActor* ResolveOwningActor(const UObject* Target);
	static FString ResolveArchetypeToken(const UObject* Target);
	static FString ResolveClassToken(const UObject* Target);
	static bool TryGetArchetypeFromActor(const AActor* Actor, FString& OutLower);
	static FString NormalizeClassName(FString In);
};
