#include "belle-sip/wakelock.h"

/**
 * @brief Acquire a wake lock.
 * Ask to Android to prevent the system sleeping. That function
 * do nothing if the wake lock system has not been initialized
 * by the function bellesip_wake_lock_init().
 * @param tag
 * @return An ID that anthentificates the taken wake lock.
 */
unsigned long wake_lock_acquire(const char *tag);

/**
 * @brief Release a wake lock.
 * Ask to Android to release a prevously aquired
 * wake lock. After calling this function, the system
 * can sleep again.
 * @param id ID of the wake lock to release.
 */
void wake_lock_release(unsigned long id);
