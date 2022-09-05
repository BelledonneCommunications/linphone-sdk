/*
 * Copyright (c) 2010-2019 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2.
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

#ifndef msasync_h
#define msasync_h

#include "mediastreamer2/mscommon.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Simple API to execute tasks on a worker thread.
 * Prequisite: the user code must not destroy every MSTask it holds before the MSWorkerThread is destroyed,
 * otherwise a crash will occur.
 */
	
struct _MSWorkerThread{
	ms_thread_t thread;
	ms_cond_t cond;
	ms_mutex_t mutex;
	bctbx_list_t *tasks;
	unsigned int task_wait_count;
	bool_t running;
	bool_t inwait;
	bool_t finish_tasks;
};

typedef struct _MSWorkerThread MSWorkerThread;

typedef void (*MSTaskFunc)(void *);

typedef enum _MSTaskState{
	MSTaskInit,
	MSTaskQueued,
	MSTaskCancelled,
	MSTaskRunning,
	MSTaskDone /**< the task was completed. This is the final state even for cancelled tasks.*/
}MSTaskState;

struct _MSTask{
	MSWorkerThread *worker;
	MSTaskFunc func;
	void *data;
	MSTaskState state; /* changed only when worker->mutex is hold */
	uint64_t repeat_at;
	int repeat_interval; /* in milliseconds*/
	bool_t auto_release;
};

typedef struct _MSTask MSTask;

/*
 * Cancel a task. There is no warranty that it will be actually canceled, since the task may
 * be executing, or have been executed already.
 */
MS2_PUBLIC void ms_task_cancel(MSTask *task);
/*
 * Wait for the task to reach the MSTaskDone state.
 */
MS2_PUBLIC void ms_task_wait_completion(MSTask *task);
/* destroy automatically cancels if necessar, and waits for completion.*/
MS2_PUBLIC void ms_task_destroy(MSTask *task); 


MS2_PUBLIC MSWorkerThread * ms_worker_thread_new(void);
/* Add a task to execute. The task object is internal, not returned. For simple usages. */
MS2_PUBLIC void ms_worker_thread_add_task(MSWorkerThread *obj, MSTaskFunc fn, void *data);
/* Add a task to execute. A MSTask object is returned to the caller, that can be wait upon. It must be destroyed. */
MS2_PUBLIC MSTask * ms_worker_thread_add_waitable_task(MSWorkerThread *obj, MSTaskFunc fn, void *data);

/* Add a task that must be repeated at regular interval.
 * Note that with the current implementation, the scheduling is not accurate: granularity is 10 ms, and there is no catchup.
 */
MS2_PUBLIC MSTask * ms_worker_thread_add_repeated_task(MSWorkerThread *obj, MSTaskFunc func, void *data, int repeat_interval);

MS2_PUBLIC void ms_worker_thread_destroy(MSWorkerThread *obj, bool_t finish_tasks);


#ifdef __cplusplus
}
#endif

#endif


