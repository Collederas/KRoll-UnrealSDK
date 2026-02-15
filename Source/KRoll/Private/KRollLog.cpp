#include "KRollLog.h"
DEFINE_LOG_CATEGORY(LogKRoll);

FCriticalSection FKRollLogOnce::Mutex;
TSet<uint64> FKRollLogOnce::Seen;