#pragma once

#include "CoreMinimal.h"

/**
	* Supported target/value kinds for MVP bindings.
	*/
enum class EKRollValueKind : uint8
{
	Bool,
	Int32,
	Float,
	GameplayAttributeData
};

struct FKRollTransform
{
	double Scale = 1.0;
	TOptional<double> ClampMin;
	TOptional<double> ClampMax;
	TOptional<double> DefaultValue;
};

struct FKRollPropertyBinding
{
	// Property to write into (resolved at cache build time)
	FProperty* Property = nullptr;

	// KRoll key template; may contain tokens like {archetype}, {class}
	FName KeyTemplate;

	EKRollValueKind Kind = EKRollValueKind::Float;
	FKRollTransform Transform;
};
