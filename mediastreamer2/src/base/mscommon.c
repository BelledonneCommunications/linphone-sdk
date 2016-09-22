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



#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msfilter.h"

#if !defined(_WIN32_WCE)
#include <sys/types.h>
#endif
#ifndef _WIN32
#include <dirent.h>
#endif
#ifdef HAVE_DLOPEN
#include <dlfcn.h>
#endif

#ifdef __APPLE__
   #include "TargetConditionals.h"
#endif

#ifdef __QNX__
#include <sys/syspage.h>
#endif


/* we need this pragma because this file implements much of compatibility functions*/
#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

unsigned int ms_get_cpu_count() {
	return ms_factory_get_cpu_count(ms_factory_get_fallback());
}

void ms_set_cpu_count(unsigned int c) {
	ms_factory_set_cpu_count(ms_factory_get_fallback(),c);
}

MSList *ms_list_new(void *data){
	return bctbx_list_new(data);
}

MSList *ms_list_append_link(MSList *elem, MSList *new_elem){
	return bctbx_list_append_link(elem, new_elem);
}

MSList * ms_list_append(MSList *elem, void * data){
	return bctbx_list_append(elem, data);
}

MSList * ms_list_prepend(MSList *elem, void *data){
	return bctbx_list_prepend(elem, data);
}

MSList * ms_list_concat(MSList *first, MSList *second){
	return bctbx_list_concat(first, second);
}

MSList * ms_list_free_with_data(MSList *list, void (*freefunc)(void*)){
	return bctbx_list_free_with_data(list, freefunc);
}

MSList * ms_list_free(MSList *elem){
	return bctbx_list_free(elem);
}

MSList * ms_list_remove(MSList *first, void *data){
	return bctbx_list_remove(first, data);
}

MSList * ms_list_remove_custom(MSList *first, MSCompareFunc compare_func, const void *user_data) {
	return bctbx_list_remove_custom(first, compare_func, user_data);
}

int ms_list_size(const MSList *first){
	return (int)bctbx_list_size(first);
}

void ms_list_for_each(const MSList *list, void (*func)(void *)){
	bctbx_list_for_each(list, func);
}

void ms_list_for_each2(const MSList *list, void (*func)(void *, void *y), void *user_data){
	bctbx_list_for_each2(list, func, user_data);
}

void ms_list_for_each3(const MSList *list, void (*func)(void *, void *, void*), void *user_data, void* factory){
	for(;list!=NULL;list=list->next){
		func(list->data,user_data, factory);
	}
}

MSList *ms_list_remove_link(MSList *list, MSList *elem){
	return bctbx_list_unlink(list, elem);
}

MSList *ms_list_find(MSList *list, void *data){
	return bctbx_list_find(list, data);
}

MSList *ms_list_find_custom(MSList *list, int (*compare_func)(const void *, const void*), const void *user_data){
	return bctbx_list_find_custom(list, compare_func, user_data);
}

void * ms_list_nth_data(const MSList *list, int index){
	return bctbx_list_nth_data(list, index);
}

int ms_list_position(const MSList *list, MSList *elem){
	return bctbx_list_position(list, elem);
}

int ms_list_index(const MSList *list, void *data){
	return bctbx_list_index(list, data);
}

MSList *ms_list_insert_sorted(MSList *list, void *data, int (*compare_func)(const void *, const void*)){
	return bctbx_list_insert_sorted(list, data, compare_func);
}

MSList *ms_list_insert(MSList *list, MSList *before, void *data){
	return bctbx_list_insert(list, before, data);
}

MSList *ms_list_copy(const MSList *list){
	return bctbx_list_copy(list);
}

MSList *ms_list_copy_with_data(const MSList *list, void *(*copyfunc)(void *)){
	return bctbx_list_copy_with_data(list, copyfunc);
}

MSList* ms_list_next(const MSList *list) {
	return bctbx_list_next(list);
}

char * ms_tags_list_as_string(const bctbx_list_t *list) {
	char *tags_str = NULL;
	const bctbx_list_t *elem = list;
	while (elem != NULL) {
		char *elem_str = (char *)elem->data;
		if (tags_str == NULL) {
			tags_str = ms_strdup(elem_str);
		} else {
			char *old = tags_str;
			tags_str = ms_strdup_printf("%s,%s", old, elem_str);
			ms_free(old);
		}
		elem = elem->next;
	}
	return tags_str;
}

bool_t ms_tags_list_contains_tag(const bctbx_list_t *list, const char *tag) {
	const bctbx_list_t *elem = list;
	while (elem != NULL) {
		char *tag_from_list = (char *)elem->data;
		if (strcasecmp(tag, tag_from_list) == 0)
			return TRUE;
		elem = elem->next;
	}
	return FALSE;
}

int ms_load_plugins(const char *dir){
	return ms_factory_load_plugins(ms_factory_get_fallback(),dir);
}

static int ms_base_ref=0;
static int ms_plugins_ref=0;

void ms_base_init(){
	ms_base_ref++;
	if ( ms_base_ref>1 ) {
		ms_message ("Skipping ms_base_init, because [%i] ref",ms_base_ref);
		return;
	}
	ms_factory_create_fallback();
	ms_factory_get_fallback();
}

void ms_base_exit(){
	--ms_base_ref;
	if ( ms_base_ref>0 ) {
		ms_message ("Skipping ms_base_exit, still [%i] ref",ms_base_ref);
		return;
	}
	ms_factory_destroy(ms_factory_get_fallback());
}

void ms_plugins_init(void) {
	ms_plugins_ref++;
	if ( ms_plugins_ref>1 ) {
		ms_message ("Skipping ms_plugins_init, because [%i] ref",ms_plugins_ref);
		return;
	}
	ms_factory_init_plugins(ms_factory_get_fallback());
}

void ms_plugins_exit(void) {
	--ms_plugins_ref;
	if ( ms_plugins_ref>0 ) {
		ms_message ("Skipping ms_plugins_exit, still [%i] ref",ms_plugins_ref);
		return;
	}
	ms_factory_uninit_plugins(ms_factory_get_fallback());
}

void ms_set_plugins_dir(const char *path) {
	ms_factory_set_plugins_dir(ms_factory_get_fallback(),path);
}

void ms_sleep(int seconds){
#ifdef _WIN32
#ifdef MS2_WINDOWS_DESKTOP
	Sleep(seconds*1000);
#else
	HANDLE sleepEvent = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
	if (!sleepEvent) return;
	WaitForSingleObjectEx(sleepEvent, seconds * 1000, FALSE);
#endif
#else
	struct timespec ts,rem;
	int err;
	ts.tv_sec=seconds;
	ts.tv_nsec=0;
	do {
		err=nanosleep(&ts,&rem);
		ts=rem;
	}while(err==-1 && errno==EINTR);
#endif
}

void ms_usleep(uint64_t usec){
#ifdef _WIN32
#ifdef MS2_WINDOWS_DESKTOP
	Sleep((DWORD)(usec/1000));
#else
	HANDLE sleepEvent = CreateEventEx(NULL, NULL, CREATE_EVENT_MANUAL_RESET, EVENT_ALL_ACCESS);
	if (!sleepEvent) return;
	WaitForSingleObjectEx(sleepEvent, (DWORD)(usec / 1000), FALSE);
#endif
#else
	struct timespec ts,rem;
	int err;
	ts.tv_sec=usec/1000000LL;
	ts.tv_nsec=(usec%1000000LL)*1000;
	do {
		err=nanosleep(&ts,&rem);
		ts=rem;
	}while(err==-1 && errno==EINTR);
#endif
}

int ms_get_payload_max_size(){
	return ms_factory_get_payload_max_size(ms_factory_get_fallback());
}

void ms_set_payload_max_size(int size){
	ms_factory_set_payload_max_size(ms_factory_get_fallback(),size);
}

extern void _android_key_cleanup(void*);

void ms_thread_exit(void* ref_val) {
#ifdef ANDROID
	// due to a bug in old Bionic version
	// cleanup of jni manually
	// works directly with Android 2.2
	_android_key_cleanup(NULL);
#endif
#if !defined(__linux) || defined(ANDROID)
	ortp_thread_exit(ref_val); // pthread_exit futex issue: http://lkml.indiana.edu/hypermail/linux/kernel/0902.0/00153.html
#endif
}


struct _MSConcealerContext {
	int64_t sample_time;
	int64_t plc_start_time;
	unsigned long total_number_for_plc;
	uint32_t max_plc_time;
};

/*** plc context begin***/
unsigned long ms_concealer_context_get_total_number_of_plc(MSConcealerContext* obj) {
	return obj->total_number_for_plc;
}

MSConcealerContext* ms_concealer_context_new(uint32_t max_plc_time){
	MSConcealerContext *obj=(MSConcealerContext *) ms_new0(MSConcealerContext,1);
	obj->sample_time=-1;
	obj->plc_start_time=-1;
	obj->total_number_for_plc=0;
	obj->max_plc_time=max_plc_time;
	return obj;
}

void ms_concealer_context_destroy(MSConcealerContext* context) {
	ms_free(context);
}

uint32_t ms_concealer_inc_sample_time(MSConcealerContext* obj, uint64_t current_time, uint32_t time_increment, bool_t got_packet){
	uint32_t plc_duration=0;
	if (obj->sample_time==-1){
		obj->sample_time=(int64_t)current_time;
	}
	obj->sample_time+=time_increment;
	if (obj->plc_start_time!=-1 && got_packet){
		plc_duration=(uint32_t)(current_time-obj->plc_start_time);
		obj->plc_start_time=-1;
		if (plc_duration>obj->max_plc_time) plc_duration=obj->max_plc_time;
	}
	return plc_duration;
}

unsigned int ms_concealer_context_is_concealement_required(MSConcealerContext* obj, uint64_t current_time) {
	if(obj->sample_time == -1) return 0; /*no valid value*/

	if ((uint64_t)obj->sample_time <= current_time){
		uint32_t plc_duration;
		if (obj->plc_start_time==-1)
			obj->plc_start_time=obj->sample_time;
		plc_duration=(uint32_t)(current_time-(uint64_t)obj->plc_start_time);
		if (plc_duration<obj->max_plc_time) {
			obj->total_number_for_plc++;
			return 1;
		}else{
			/*reset sample time, so that we don't do PLC anymore and can resync properly when the stream restarts*/
			obj->sample_time=-1;
			return 0;
		}
	}
	return 0;
}


struct _MSConcealerTsContext {
	int64_t sample_ts;
	int64_t plc_start_ts;
	unsigned long total_number_for_plc;
	unsigned int max_plc_ts;
};

/*** plc context begin***/
unsigned long ms_concealer_ts_context_get_total_number_of_plc(MSConcealerTsContext* obj) {
	return obj->total_number_for_plc;
}

MSConcealerTsContext* ms_concealer_ts_context_new(unsigned int max_plc_ts){
	MSConcealerTsContext *obj=(MSConcealerTsContext *) ms_new0(MSConcealerTsContext,1);
	obj->sample_ts=-1;
	obj->plc_start_ts=-1;
	obj->total_number_for_plc=0;
	obj->max_plc_ts=max_plc_ts;
	return obj;
}

void ms_concealer_ts_context_destroy(MSConcealerTsContext* context) {
	ms_free(context);
}

uint32_t ms_concealer_ts_context_inc_sample_ts(MSConcealerTsContext* obj, uint64_t current_ts, uint32_t ts_increment, bool_t got_packet){
	uint32_t plc_duration=0;
	if (obj->sample_ts==-1){
		obj->sample_ts=(int64_t)current_ts;
	}
	obj->sample_ts+=ts_increment;
	if (obj->plc_start_ts!=-1 && got_packet){
		plc_duration=(uint32_t)(current_ts-(uint64_t)obj->plc_start_ts);
		obj->plc_start_ts=-1;
		if (plc_duration>obj->max_plc_ts) plc_duration=obj->max_plc_ts;
	}
	return plc_duration;
}

unsigned int ms_concealer_ts_context_is_concealement_required(MSConcealerTsContext* obj, uint64_t current_ts) {
	if(obj->sample_ts == -1) return 0; /*no valid value*/

	if ((uint64_t)obj->sample_ts < current_ts){
		uint32_t plc_duration;
		if (obj->plc_start_ts==-1)
			obj->plc_start_ts=obj->sample_ts;
		plc_duration=(uint32_t)(current_ts-(uint64_t)obj->plc_start_ts);
		if (plc_duration<obj->plc_start_ts) {
			obj->total_number_for_plc++;
			return 1;
		}else{
			/*reset sample time, so that we don't do PLC anymore and can resync properly when the stream restarts*/
			obj->sample_ts=-1;
			return 0;
		}
	}
	return 0;
}

char *ms_load_file_content(FILE *f, size_t *nbytes){
	size_t bufsize=2048;
	size_t step=bufsize;
	size_t pos=0;
	size_t count;
	char *buffer=ms_malloc(bufsize+1);
	
	while((count=fread(buffer+pos, 1, step, f))>0){
		pos+=count;
		if (pos+step>=bufsize){
			bufsize*=2;
			buffer=ms_realloc(buffer, bufsize+1);
		}
	}
	if (nbytes) *nbytes=pos;
	buffer[pos]='\0';
	return buffer;
}

char *ms_load_path_content(const char *path, size_t *nbytes){
	FILE *f=fopen(path,"rb");
	char *buffer;
	if (!f) {
		ms_error("ms_load_file_content(): could not open [%s]",path);
		return NULL;
	}
	buffer=ms_load_file_content(f, nbytes);
	fclose(f);
	return buffer;
}

/*** plc context end***/
