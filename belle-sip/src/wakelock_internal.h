/*
 * Copyright (c) 2012-2019 Belledonne Communications SARL.
 *
 * This file is part of belle-sip.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

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
