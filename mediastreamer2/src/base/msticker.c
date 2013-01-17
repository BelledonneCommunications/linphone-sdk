/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "mediastreamer2/msticker.h"

#ifndef WIN32
#include <sys/time.h>
#include <sys/resource.h>
#endif

static const double smooth_coef=0.9;

#ifndef TICKER_MEASUREMENTS

#define TICKER_MEASUREMENTS 1

#if defined(__ARM_ARCH__) 
#	if __ARM_ARCH__ < 7
/* as MSTicker load computation requires floating point, we prefer to disable it on ARM processors without FPU*/
#		undef TICKER_MEASUREMENTS
#		define TICKER_MEASUREMENTS 0 
#	endif
#endif

#endif

#define TICKER_INTERVAL 10

static void * ms_ticker_run(void *s);
static uint64_t get_cur_time_ms(void *);
static int wait_next_tick(void *, uint64_t virt_ticker_time);
static void remove_tasks_for_filter(MSTicker *ticker, MSFilter *f);

static void ms_ticker_start(MSTicker *s){
	s->run=TRUE;
	ms_thread_create(&s->thread,NULL,ms_ticker_run,s);
}

static void ms_ticker_init(MSTicker *ticker, const MSTickerParams *params)
{
	ms_mutex_init(&ticker->lock,NULL);
	ticker->execution_list=NULL;
	ticker->task_list=NULL;
	ticker->ticks=1;
	ticker->time=0;
	ticker->interval=TICKER_INTERVAL;
	ticker->run=FALSE;
	ticker->exec_id=0;
	ticker->get_cur_time_ptr=&get_cur_time_ms;
	ticker->get_cur_time_data=NULL;
	ticker->name=ms_strdup(params->name);
	ticker->av_load=0;
	ticker->prio=params->prio;
	ticker->wait_next_tick=wait_next_tick;
	ticker->wait_next_tick_data=ticker;
	ms_ticker_start(ticker);
}

MSTicker *ms_ticker_new(){
	MSTickerParams params;
	params.name="MSTicker";
	params.prio=MS_TICKER_PRIO_NORMAL;
	return ms_ticker_new_with_params(&params);
}

MSTicker *ms_ticker_new_with_params(const MSTickerParams *params){
	MSTicker *obj=(MSTicker *)ms_new(MSTicker,1);
	ms_ticker_init(obj,params);
	return obj;
}

static void ms_ticker_stop(MSTicker *s){
	ms_mutex_lock(&s->lock);
	s->run=FALSE;
	ms_mutex_unlock(&s->lock);
	if(s->thread)
		ms_thread_join(s->thread,NULL);
}

void ms_ticker_set_name(MSTicker *s, const char *name){
	if (s->name) ms_free(s->name);
	s->name=ms_strdup(name);
}

void ms_ticker_set_priority(MSTicker *ticker, MSTickerPrio prio){
	ticker->prio=prio;
}

static void ms_ticker_uninit(MSTicker *ticker)
{
	ms_ticker_stop(ticker);
	ms_free(ticker->name);
	ms_mutex_destroy(&ticker->lock);
}

void ms_ticker_destroy(MSTicker *ticker){
	ms_ticker_uninit(ticker);
	ms_free(ticker);
}


static MSList *get_sources(MSList *filters){
	MSList *sources=NULL;
	MSFilter *f;
	for(;filters!=NULL;filters=filters->next){
		f=(MSFilter*)filters->data;
		if (f->desc->ninputs==0){
			sources=ms_list_append(sources,f);
		}
	}
	return sources;
}

int ms_ticker_attach(MSTicker *ticker, MSFilter *f){
	return ms_ticker_attach_multiple(ticker,f,NULL);
}

int ms_ticker_attach_multiple(MSTicker *ticker,MSFilter *f,...)
{
	MSList *sources=NULL;
	MSList *filters=NULL;
	MSList *it;
	MSList *total_sources=NULL;
	va_list l;

	va_start(l,f);

	do{
		if (f->ticker==NULL) {
			filters=ms_filter_find_neighbours(f);
			sources=get_sources(filters);
			if (sources==NULL){
				ms_fatal("No sources found around filter %s",f->desc->name);
				ms_list_free(filters);
				break;
			}
			/*run preprocess on each filter: */
			for(it=filters;it!=NULL;it=it->next)
				ms_filter_preprocess((MSFilter*)it->data,ticker);
			ms_list_free(filters);
			total_sources=ms_list_concat(total_sources,sources);			
		}else ms_message("Filter %s is already being scheduled; nothing to do.",f->desc->name);
	}while ((f=va_arg(l,MSFilter*))!=NULL);
	va_end(l);
	if (total_sources){
		ms_mutex_lock(&ticker->lock);
		ticker->execution_list=ms_list_concat(ticker->execution_list,total_sources);
		ms_mutex_unlock(&ticker->lock);
	}
	return 0;
}

static void call_postprocess(MSFilter *f){
	if (f->postponed_task) remove_tasks_for_filter(f->ticker,f);
	ms_filter_postprocess(f);
}

int ms_ticker_detach(MSTicker *ticker,MSFilter *f){
	MSList *sources=NULL;
	MSList *filters=NULL;
	MSList *it;

	if (f->ticker==NULL) {
		ms_message("Filter %s is not scheduled; nothing to do.",f->desc->name);
		return 0;
	}

	ms_mutex_lock(&ticker->lock);

	filters=ms_filter_find_neighbours(f);
	sources=get_sources(filters);
	if (sources==NULL){
		ms_fatal("No sources found around filter %s",f->desc->name);
		ms_list_free(filters);
		ms_mutex_unlock(&ticker->lock);
		return -1;
	}

	for(it=sources;it!=NULL;it=ms_list_next(it)){
		ticker->execution_list=ms_list_remove(ticker->execution_list,it->data);
	}
	ms_mutex_unlock(&ticker->lock);
	ms_list_for_each(filters,(void (*)(void*))call_postprocess);
	ms_list_free(filters);
	ms_list_free(sources);
	return 0;
}


static bool_t filter_can_process(MSFilter *f, int tick){
	/* look if filters before this one have run */
	int i;
	MSQueue *l;
	for(i=0;i<f->desc->ninputs;i++){
		l=f->inputs[i];
		if (l!=NULL){
			if (l->prev.filter->last_tick!=tick) return FALSE;
		}
	}
	return TRUE;
}

static void call_process(MSFilter *f){
	bool_t process_done=FALSE;
	if (f->desc->ninputs==0 || f->desc->flags & MS_FILTER_IS_PUMP){
		ms_filter_process(f);
	}else{
		while (ms_filter_inputs_have_data(f)) {
			if (process_done){
				ms_warning("Re-scheduling filter %s: all data should be consumed in one process call, so fix it.",f->desc->name);
			}
			ms_filter_process(f);
			if (f->postponed_task) break;
			process_done=TRUE;
		}
	}
}

static void run_graph(MSFilter *f, MSTicker *s, MSList **unschedulable, bool_t force_schedule){
	int i;
	MSQueue *l;
	if (f->last_tick!=s->ticks ){
		if (filter_can_process(f,s->ticks) || force_schedule) {
			/* this is a candidate */
			f->last_tick=s->ticks;
			call_process(f);	
			/* now recurse to next filters */		
			for(i=0;i<f->desc->noutputs;i++){
				l=f->outputs[i];
				if (l!=NULL){
					run_graph(l->next.filter,s,unschedulable, force_schedule);
				}
			}
		}else{
			/* this filter has not all inputs that have been filled by filters before it. */
			*unschedulable=ms_list_prepend(*unschedulable,f);
		}
	}
}

static void run_graphs(MSTicker *s, MSList *execution_list, bool_t force_schedule){
	MSList *it;
	MSList *unschedulable=NULL;
	for(it=execution_list;it!=NULL;it=it->next){
		run_graph((MSFilter*)it->data,s,&unschedulable,force_schedule);
	}
	/* filters that are part of a loop haven't been called in process() because one of their input refers to a filter that could not be scheduled (because they could not be scheduled themselves)... Do you understand ?*/
	/* we resolve this by simply assuming that they must be called anyway 
	for the loop to run correctly*/
	/* we just recall run_graphs on them, as if they were source filters */
	if (unschedulable!=NULL) {
		run_graphs(s,unschedulable,TRUE);
		ms_list_free(unschedulable);
	}
}

static void run_tasks(MSTicker *ticker){
	MSList *elem,*prevelem=NULL;
	for (elem=ticker->task_list;elem!=NULL;){
		MSFilterTask *t=(MSFilterTask*)elem->data;
		ms_filter_task_process(t);
		ms_free(t);
		prevelem=elem;
		elem=elem->next;
		ms_free(prevelem);
	}
	ticker->task_list=NULL;
}

static void remove_tasks_for_filter(MSTicker *ticker, MSFilter *f){
	MSList *elem,*nextelem;
	for (elem=ticker->task_list;elem!=NULL;elem=nextelem){
		MSFilterTask *t=(MSFilterTask*)elem->data;
		nextelem=elem->next;
		if (t->f==f){
			ticker->task_list=ms_list_remove_link(ticker->task_list,elem);
			ms_free(t);
		}
	}
}

static uint64_t get_cur_time_ms(void *unused){
	MSTimeSpec ts;
	ms_get_cur_time(&ts);
	return (ts.tv_sec*1000LL) + ((ts.tv_nsec+500000LL)/1000000LL);
}

static void sleepMs(int ms){
#ifdef WIN32
	Sleep(ms);
#else
	struct timespec ts;
	ts.tv_sec=0;
	ts.tv_nsec=ms*1000000LL;
	nanosleep(&ts,NULL);
#endif
}

static int set_high_prio(MSTicker *obj){
	int precision=2;
	int prio=obj->prio;
	
	if (prio>MS_TICKER_PRIO_NORMAL){
#ifdef WIN32
		MMRESULT mm;
		TIMECAPS ptc;
		mm=timeGetDevCaps(&ptc,sizeof(ptc));
		if (mm==0){
			if (ptc.wPeriodMin<(UINT)precision)
				ptc.wPeriodMin=precision;
			else
				precision = ptc.wPeriodMin;
			mm=timeBeginPeriod(ptc.wPeriodMin);
			if (mm!=TIMERR_NOERROR){
				ms_warning("timeBeginPeriod failed.");
			}
			ms_message("win32 timer resolution set to %i ms",ptc.wPeriodMin);
		}else{
			ms_warning("timeGetDevCaps failed.");
		}

		if(!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST)){
			ms_warning("SetThreadPriority() failed (%d)\n", (int)GetLastError());
		}
#else
		struct sched_param param;
		int policy=SCHED_RR;
		memset(&param,0,sizeof(param));
		int result=0;
		char* env_prio_c=NULL;
		int min_prio, max_prio, env_prio;

		if (prio==MS_TICKER_PRIO_REALTIME)
			policy=SCHED_FIFO;
		
		min_prio = sched_get_priority_min(policy);
		max_prio = sched_get_priority_max(policy);
		env_prio_c = getenv("MS_TICKER_SCHEDPRIO");

		env_prio = (env_prio_c == NULL)?max_prio:atoi(env_prio_c);

		env_prio = MAX(MIN(env_prio, max_prio), min_prio);
		ms_message("Priority used: %d", env_prio);

		param.sched_priority=env_prio;
		if((result=pthread_setschedparam(pthread_self(),policy, &param))) {
			if (result==EPERM){
				/*
					The linux kernel has 
					sched_get_priority_max(SCHED_OTHER)=sched_get_priority_max(SCHED_OTHER)=0.
					As long as we can't use SCHED_RR or SCHED_FIFO, the only way to increase priority of a calling thread
					is to use setpriority().
				*/
				if (setpriority(PRIO_PROCESS,0,-20)==-1){
					ms_message("%s setpriority() failed: %s, nevermind.",obj->name,strerror(errno));
				}else{
					ms_message("%s priority increased to maximum.",obj->name);
				}
			}else ms_warning("%s: Set pthread_setschedparam failed: %s",obj->name,strerror(result));
		} else {
			ms_message("%s priority set to %s and value (%i)",obj->name,
			           policy==SCHED_FIFO ? "SCHED_FIFO" : "SCHED_RR", param.sched_priority);
		}
#endif
	}else ms_message("%s priority left to normal.",obj->name);
	return precision;
}

static void unset_high_prio(int precision){
#ifdef WIN32
	if(!SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL)){
		ms_warning("SetThreadPriority() failed (%d)\n", (int)GetLastError());
	}
	timeEndPeriod(precision);
#endif
}

static int wait_next_tick(void *data, uint64_t virt_ticker_time){
	MSTicker *s=(MSTicker*)data;
	uint64_t realtime;
	int64_t diff;
	int late;
	
	while(1){
		realtime=s->get_cur_time_ptr(s->get_cur_time_data)-s->orig;
		diff=s->time-realtime;
		if (diff>0){
			/* sleep until next tick */
			sleepMs((int)diff);
		}else{
			late=(int)-diff;
			break; /*exit the while loop */
		}
	}
	return late;
}

/*the ticker thread function that executes the filters */
void * ms_ticker_run(void *arg)
{
	MSTicker *s=(MSTicker*)arg;
	int lastlate=0;
	int precision=2;
	int late;
	
	precision = set_high_prio(s);

	s->ticks=1;
	s->orig=s->get_cur_time_ptr(s->get_cur_time_data);

	ms_mutex_lock(&s->lock);
	
	while(s->run){
		s->ticks++;
		/*Step 1: run the graphs*/
		{
#if TICKER_MEASUREMENTS
			MSTimeSpec begin,end;/*used to measure time spent in processing one tick*/
			double iload;

			ms_get_cur_time(&begin);
#endif
			run_tasks(s);
			run_graphs(s,s->execution_list,FALSE);
#if TICKER_MEASUREMENTS
			ms_get_cur_time(&end);
			iload=100*((end.tv_sec-begin.tv_sec)*1000.0 + (end.tv_nsec-begin.tv_nsec)/1000000.0)/(double)s->interval;
			s->av_load=(smooth_coef*s->av_load)+((1.0-smooth_coef)*iload);
#endif
		}
		ms_mutex_unlock(&s->lock);
		/*Step 2: wait for next tick*/
		s->time+=s->interval;
		late=s->wait_next_tick(s->wait_next_tick_data,s->time);
		if (late>s->interval*5 && late>lastlate){
			ms_warning("%s: We are late of %d miliseconds.",s->name,late);
		}
		lastlate=late;
		ms_mutex_lock(&s->lock);
	}
	ms_mutex_unlock(&s->lock);
	unset_high_prio(precision);
	ms_message("%s thread exiting",s->name);

	ms_thread_exit(NULL);
	return NULL;
}

void ms_ticker_set_time_func(MSTicker *ticker, MSTickerTimeFunc func, void *user_data){
	if (func==NULL) func=get_cur_time_ms;
	
	ticker->get_cur_time_ptr=func;
	ticker->get_cur_time_data=user_data;
	/*re-set the origin to take in account that previous function ptr and the
	new one may return different times*/
	ticker->orig=func(user_data)-ticker->time;
	
	ms_message("ms_ticker_set_time_func: ticker's time method updated.");
}

void ms_ticker_set_tick_func(MSTicker *ticker, MSTickerTickFunc func, void *user_data){
	if (func==NULL) {
		func=wait_next_tick;
		user_data=ticker;
	}
	ticker->wait_next_tick=func;
	ticker->wait_next_tick_data=user_data;
	/*re-set the origin to take in account that previous function ptr and the
	new one may return different times*/
	ticker->orig=ticker->get_cur_time_ptr(user_data)-ticker->time;
	ms_message("ms_ticker_set_tick_func: ticker's tick method updated.");
}

static void print_graph(MSFilter *f, MSTicker *s, MSList **unschedulable, bool_t force_schedule){
	int i;
	MSQueue *l;
	if (f->last_tick!=s->ticks ){
		if (filter_can_process(f,s->ticks) || force_schedule) {
			/* this is a candidate */
			f->last_tick=s->ticks;
			ms_message("print_graphs: %s", f->desc->name);
			/* now recurse to next filters */		
			for(i=0;i<f->desc->noutputs;i++){
				l=f->outputs[i];
				if (l!=NULL){
					print_graph(l->next.filter,s,unschedulable, force_schedule);
				}
			}
		}else{
			/* this filter has not all inputs that have been filled by filters before it. */
			*unschedulable=ms_list_prepend(*unschedulable,f);
		}
	}
}

static void print_graphs(MSTicker *s, MSList *execution_list, bool_t force_schedule){
	MSList *it;
	MSList *unschedulable=NULL;
	for(it=execution_list;it!=NULL;it=it->next){
		print_graph((MSFilter*)it->data,s,&unschedulable,force_schedule);
	}
	/* filters that are part of a loop haven't been called in process() because one of their input refers to a filter that could not be scheduled (because they could not be scheduled themselves)... Do you understand ?*/
	/* we resolve this by simply assuming that they must be called anyway 
	for the loop to run correctly*/
	/* we just recall run_graphs on them, as if they were source filters */
	if (unschedulable!=NULL) {
		print_graphs(s,unschedulable,TRUE);
		ms_list_free(unschedulable);
	}
}

void ms_ticker_print_graphs(MSTicker *ticker){
	print_graphs(ticker,ticker->execution_list,FALSE);
}

float ms_ticker_get_average_load(MSTicker *ticker){
#if	!TICKER_MEASUREMENTS
	static bool_t once=FALSE;
	if (once==FALSE){
		ms_warning("ms_ticker_get_average_load(): ticker load measurements disabled for performance reasons.");
		once=TRUE;
	}
#endif
	return ticker->av_load;
}


static uint64_t get_ms(const MSTimeSpec *ts){
	return (ts->tv_sec*1000LL) + ((ts->tv_nsec+500000LL)/1000000LL);
}

static uint64_t get_wallclock_ms(void){
	MSTimeSpec ts;
	ms_get_cur_time(&ts);
	return get_ms(&ts);
}

static const double clock_coef = .01;

MSTickerSynchronizer* ms_ticker_synchronizer_new(void) {
	MSTickerSynchronizer *obj=(MSTickerSynchronizer *)ms_new(MSTickerSynchronizer,1);
	obj->av_skew = 0;
	obj->offset = 0;
	return obj;
}

double ms_ticker_synchronizer_set_external_time(MSTickerSynchronizer* ts, const MSTimeSpec *time) {
	int64_t sound_time;
	int64_t diff;
	uint64_t wc = get_wallclock_ms();
	uint64_t ms = get_ms(time);
	if (ts->offset == 0) {
		ts->offset = wc - ms;
	}
	sound_time = ts->offset + ms;
	diff = wc - sound_time;
	ts->av_skew = (ts->av_skew * (1.0 - clock_coef)) + ((double) diff * clock_coef);
	return ts->av_skew;
}



uint64_t ms_ticker_synchronizer_get_corrected_time(MSTickerSynchronizer* ts) {
	/* round skew to timer resolution in order to avoid adapt the ticker just with statistical "noise" */
	int64_t rounded_skew=( ((int64_t)ts->av_skew)/(int64_t)TICKER_INTERVAL) * (int64_t)TICKER_INTERVAL;
	return get_wallclock_ms() - rounded_skew;
}

void ms_ticker_synchronizer_destroy(MSTickerSynchronizer* ts) {
	ms_free(ts);
}
