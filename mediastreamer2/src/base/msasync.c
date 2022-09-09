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


#include "mediastreamer2/msasync.h"

void ms_task_cancel(MSTask *task){
	if (!task->worker) return ;
	ms_mutex_lock(&task->worker->mutex);
	if (task->state != MSTaskDone){ /* task may be queued or running */
		ms_debug("msasync.c: cancelling task %p", task);
		task->state = MSTaskCancelled;
		task->repeat_interval = 0;
	}
	ms_mutex_unlock(&task->worker->mutex);
	return;
}

void ms_task_wait_completion(MSTask *task){
	if (!task->worker) return;
	ms_mutex_lock(&task->worker->mutex);
	while (task->state != MSTaskDone){
		task->worker->task_wait_count++;
		ms_debug("msasync.c: waiting for task %p", task);
		ms_cond_wait(&task->worker->cond, &task->worker->mutex);
		task->worker->task_wait_count--;
	}
	ms_mutex_unlock(&task->worker->mutex);
}

void ms_task_destroy(MSTask *obj){
	if (!obj->auto_release){
		/* Make sure it is cancelled */
		ms_task_cancel(obj);
		ms_task_wait_completion(obj);
	}
	ms_free(obj);
}

MSTask * ms_task_new(MSWorkerThread *worker, MSTaskFunc func, void *data, int repeat_interval, bool_t auto_release){
	MSTask *obj = ms_new0(MSTask, 1);
	obj->worker = worker;
	obj->data = data;
	obj->func = func;
	obj->state = MSTaskInit;
	obj->repeat_interval = repeat_interval;
	obj->auto_release = auto_release;
	return obj;
}

static bool_t ms_worker_thread_run_task(MSWorkerThread *obj, MSTask *task, int do_it){
	bool_t drop = TRUE;
	task->state = MSTaskRunning;
	if (do_it) {
		ms_mutex_unlock(&obj->mutex);
		task->func(task->data);
		ms_mutex_lock(&obj->mutex);
	}
	if (obj->running && task->state == MSTaskRunning && task->repeat_interval != 0){
		/* This tasks needs to be repeated */
		task->state = MSTaskQueued;
		drop = FALSE; 
	}else{
		/* the task is one-shot, cancelled or the worker is exiting */
		task->state = MSTaskDone;
	}
	return drop;
}

/* prequisite: the worker's mutex is held when calling this function */
static bool_t ms_worker_thread_process_task(MSWorkerThread *obj, MSTask *task, uint64_t curtime, int do_it){
	bool_t drop = TRUE;
	if (task->state == MSTaskQueued){
		if (task->repeat_interval != 0){
			if (task->repeat_at == 0) task->repeat_at = curtime;
			if (curtime >= task->repeat_at){
				drop = ms_worker_thread_run_task(obj, task, do_it);
				task->repeat_at += task->repeat_interval;
			}
		}else{
			
			drop = ms_worker_thread_run_task(obj, task, do_it);
		}
	}else if (task->state == MSTaskCancelled){
		task->state = MSTaskDone;
	}
	if (task->auto_release) ms_task_destroy(task);
	return drop;
}


static void *ms_worker_thread_run(void *d){
	MSWorkerThread *obj = (MSWorkerThread*)d;
	
	ms_mutex_lock(&obj->mutex);
	while(obj->running || obj->tasks){ /*don't let the thread exit with unterminated tasks*/
		uint64_t curtime = 0;
		bctbx_list_t *tasks = obj->tasks;
		
		if (tasks){
			bctbx_list_t *it;
			obj->tasks = NULL; /* don't let the task list to be modified while we are iterating over it */
			
			for(it = tasks; it != NULL; ){
				MSTask *t = (MSTask*)it->data;
				bctbx_list_t *next = it->next;
				
				if (curtime == 0 && t->repeat_interval != 0){
					curtime = ms_get_cur_time_ms();
				}

				if (ms_worker_thread_process_task(obj, t, curtime, obj->running || obj->finish_tasks) == TRUE){
					tasks = bctbx_list_erase_link(tasks, it);
				}
				it = next;
			}
			/* signal threads waiting task completion */
			if (obj->task_wait_count != 0){
				ms_mutex_unlock(&obj->mutex);
				ms_debug("msasync.c: worker is signaling a thread waiting for completion.");
				ms_cond_broadcast(&obj->cond);
				ms_mutex_lock(&obj->mutex);
			}
		}
		if (obj->tasks) {
			/* New tasks may have been queued to obj->tasks */
			obj->tasks = bctbx_list_concat(obj->tasks, tasks);
			/* The loop must continue */
		}else if (tasks){
			/* No new tasks, but there are repeatable tasks (not dequeued). Go to sleep for a while. */
			obj->tasks = tasks;
		}
		if (obj->running){
			if (tasks){
				ms_debug("msasync.c: worker thread has repeatable tasks, going to sleep for a while.");
				/* This could be further optimized, using pthread_cond_timedwait() */
				ms_mutex_unlock(&obj->mutex);
				bctbx_sleep_ms(10);
				ms_mutex_lock(&obj->mutex);
			}else{
				/* If there no tasks left at all , go to sleep until new tasks are queued.*/
				/* and wait for other tasks to come */
				ms_debug("msasync.c: worker thread has no tasks.");
				obj->inwait = TRUE;
				ms_cond_wait(&obj->cond, &obj->mutex);
				obj->inwait = FALSE;
			}
		}else{
			ms_message("msasync.c: worker thread is exiting.");
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

static void ms_worker_thread_queue_task(MSWorkerThread *obj, MSTask *task){
	ms_mutex_lock(&obj->mutex);
	task->state = MSTaskQueued;
	obj->tasks = bctbx_list_append(obj->tasks, task);
	if (obj->inwait) ms_cond_signal(&obj->cond);
	ms_mutex_unlock(&obj->mutex);
}

void ms_worker_thread_add_task(MSWorkerThread *obj, MSTaskFunc func, void *data){
	MSTask *task = ms_task_new(obj, func, data, 0, TRUE);
	ms_worker_thread_queue_task(obj, task);
}

MSTask * ms_worker_thread_add_waitable_task(MSWorkerThread *obj, MSTaskFunc func, void *data){
	MSTask *task = ms_task_new(obj, func, data, 0, FALSE);
	ms_worker_thread_queue_task(obj, task);
	return task;
}

MSTask * ms_worker_thread_add_repeated_task(MSWorkerThread *obj, MSTaskFunc func, void *data, int repeat_interval){
	MSTask *task = ms_task_new(obj, func, data, repeat_interval, FALSE);
	ms_worker_thread_queue_task(obj, task);
	return task;
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
		ms_error("ms_async.c: Leaving %i tasks in worker thread.", (int)bctbx_list_size(obj->tasks));
	}
	ms_mutex_destroy(&obj->mutex);
	ms_cond_destroy(&obj->cond);
	ms_free(obj);
}


