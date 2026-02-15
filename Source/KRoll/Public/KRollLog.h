#pragma once
#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogKRoll, Log, All);

static constexpr uint32 KROLL_REASON_UNSUPPORTED_PROP = 1;
static constexpr uint32 KROLL_REASON_BAD_META_NUMERIC = 2;
static constexpr uint32 KROLL_REASON_UNRESOLVED_TOKEN = 3;
static constexpr uint32 KROLL_REASON_MISSING_KEY      = 4;

class FKRollLogOnce
{
public:
	static bool ShouldLog(uint64 Key)
	{
		FScopeLock Lock(&Mutex);

		bool bIsSeen = false;
		Seen.Add(Key, &bIsSeen);
		return !bIsSeen;
	}

	static uint64 MakeKey(const UClass* Class, const FProperty* Prop, uint32 Reason)
	{
		const uint64 A = uint64(GetTypeHash(Class));
		const uint64 B = uint64(GetTypeHash(Prop));
		const uint64 C = uint64(Reason);
		return HashCombine(HashCombine(A, B), C);
	}

private:
	static FCriticalSection Mutex;
	static TSet<uint64> Seen;
};
