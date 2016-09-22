/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Belledonne Communications SARL

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


#include "mediastreamer2/msasync.h"

static void ms_task_destroy(MSTask *obj){
	ms_mutex_destroy(&obj->mutex);
	ms_free(obj);
}

MSTask * ms_task_new(MSTaskFunc func, void *data){
	MSTask *obj = ms_new0(MSTask, 1);
	obj->data = data;
	obj->func = func;
	obj->state = MSTaskInit;
	ms_mutex_init(&obj->mutex, NULL);
	return obj;
}

void ms_task_execute(MSTask *obj, int do_it){
	ms_mutex_lock(&obj->mutex);
	obj->state = MSTaskRunning;
	ms_mutex_unlock(&obj->mutex);
		
	if (do_it) obj->func(obj->data);
		
	ms_mutex_lock(&obj->mutex);
	obj->state = MSTaskDone;
	ms_mutex_unlock(&obj->mutex);
	ms_task_destroy(obj);
}


static void *ms_worker_thread_run(void *d){
	MSWorkerThread *obj = (MSWorkerThread*)d;
	
	ms_mutex_lock(&obj->mutex);
	while(obj->running || obj->tasks){ /*don't let the thread exit with unterminated tasks*/
		if (obj->tasks){
			MSTask *t = (MSTask*)obj->tasks->data;
			/*pop first element*/
			obj->tasks = bctbx_list_erase_link(obj->tasks, obj->tasks);
			
			ms_mutex_unlock(&obj->mutex);
			ms_task_execute(t, obj->running || obj->finish_tasks);
			ms_mutex_lock(&obj->mutex);
		}else{
			obj->inwait = TRUE;
			ms_cond_wait(&obj->cond, &obj->mutex);
			obj->inwait = FALSE;
		}
	}
	ms_mutex_unlock(&obj->mutex);
	return NULL;
}

MSWorkerThread * ms_worker_thread_new(void){
	MSWorkerThread *obj = ms_new0(MSWorkerThread, 1);
	ms_mutex_init(&obj->mutex, NULL);
	ms_cond_init(&obj->cond, NULL);
	obj->running = TRUE;
	ms_thread_create(&obj->thread, NULL, ms_worker_thread_run, obj);
	return obj;
}

void ms_worker_thread_add_task(MSWorkerThread *obj, MSTaskFunc func, void *data){
	MSTask *task = ms_task_new(func, data);
	ms_mutex_lock(&obj->mutex);
	obj->tasks = bctbx_list_append(obj->tasks, task);
	if (obj->inwait) ms_cond_signal(&obj->cond);
	ms_mutex_unlock(&obj->mutex);
}

void ms_worker_thread_destroy(MSWorkerThread *obj, bool_t finish_tasks){
	ms_mutex_lock(&obj->mutex);
	obj->finish_tasks = finish_tasks;
	obj->running = FALSE;
	if (obj->inwait) ms_cond_signal(&obj->cond);
	ms_mutex_unlock(&obj->mutex);
	ms_thread_join(obj->thread, NULL);
	if (obj->tasks){
		/*should never happen*/
		ms_error("Leaving %i tasks in worker thread.", (int)bctbx_list_size(obj->tasks));
	}
	ms_mutex_destroy(&obj->mutex);
	ms_cond_destroy(&obj->cond);
	ms_free(obj);
}


