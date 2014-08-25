#include "belle-sip/wakelock.h"

extern unsigned long wake_lock_acquire(const char *tag);
extern void wake_lock_release(unsigned long id);
