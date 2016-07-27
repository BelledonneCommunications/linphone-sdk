/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2014  Belledonne Communications SARL

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifndef msasync_h
#define msasync_h

#include "mediastreamer2/mscommon.h"

#ifdef __cplusplus
extern "C" {
#endif

struct _MSTask;

typedef void (*MSTaskFunc)(void *);

typedef enum _MSTaskState{
	MSTaskInit,
	MSTaskQueued,
	MSTaskRunning,
	MSTaskDone /**<the task was executed normally*/
}MSTaskState;

struct _MSTask{
	/*state and mutex are overkill for the moment - it's for later managing task cancellation*/
	ms_mutex_t mutex;
	MSTaskFunc func;
	void *data;
	MSTaskState state;
};

typedef struct _MSTask MSTask;

struct _MSWorkerThread{
	ms_thread_t thread;
	ms_cond_t cond;
	ms_mutex_t mutex;
	bctbx_list_t *tasks;
	bool_t running;
	bool_t inwait;
	bool_t finish_tasks;
};

typedef struct _MSWorkerThread MSWorkerThread;

MS2_PUBLIC MSWorkerThread * ms_worker_thread_new(void);
MS2_PUBLIC void ms_worker_thread_add_task(MSWorkerThread *obj, MSTaskFunc fn, void *data);
MS2_PUBLIC void ms_worker_thread_destroy(MSWorkerThread *obj, bool_t finish_tasks);


#ifdef __cplusplus
}
#endif

#endif


