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

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#include "gitversion.h"
#else
#   ifndef MEDIASTREAMER_VERSION
#   define MEDIASTREAMER_VERSION "unknown"
#   endif
#	ifndef GIT_VERSION
#	define GIT_VERSION "unknown"
#	endif
#endif

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/mscodecutils.h"
#include "mediastreamer2/msfilter.h"

#include "basedescs.h"

#if !defined(_WIN32_WCE)
#include <sys/types.h>
#endif
#ifndef WIN32
#include <dirent.h>
#else
#ifndef PACKAGE_PLUGINS_DIR
#if defined(WIN32) || defined(_WIN32_WCE)
#define PACKAGE_PLUGINS_DIR "lib\\mediastreamer\\plugins\\"
#else
#define PACKAGE_PLUGINS_DIR "."
#endif
#endif
#endif
#ifdef HAVE_DLOPEN
#include <dlfcn.h>
#endif

#ifdef __APPLE__
   #include "TargetConditionals.h"
#endif

#ifdef ANDROID
#include <android/log.h>
#endif

#if defined(WIN32) && !defined(_WIN32_WCE)
static MSList *ms_plugins_loaded_list;
#endif

static unsigned int cpu_count = 1;

unsigned int ms_get_cpu_count() {
	return cpu_count;
}

void ms_set_cpu_count(unsigned int c) {
	ms_message("CPU count set to %d", c);
	cpu_count = c;
}

MSList *ms_list_new(void *data){
	MSList *new_elem=(MSList *)ms_new(MSList,1);
	new_elem->prev=new_elem->next=NULL;
	new_elem->data=data;
	return new_elem;
}

MSList *ms_list_append_link(MSList *elem, MSList *new_elem){
	MSList *it=elem;
	if (elem==NULL) return new_elem;
	while (it->next!=NULL) it=ms_list_next(it);
	it->next=new_elem;
	new_elem->prev=it;
	return elem;
}

MSList * ms_list_append(MSList *elem, void * data){
	MSList *new_elem=ms_list_new(data);
	return ms_list_append_link(elem,new_elem);
}

MSList * ms_list_prepend(MSList *elem, void *data){
	MSList *new_elem=ms_list_new(data);
	if (elem!=NULL) {
		new_elem->next=elem;
		elem->prev=new_elem;
	}
	return new_elem;
}


MSList * ms_list_concat(MSList *first, MSList *second){
	MSList *it=first;
	if (it==NULL) return second;
	while(it->next!=NULL) it=ms_list_next(it);
	it->next=second;
	second->prev=it;
	return first;
}

MSList * ms_list_free(MSList *list){
	MSList *elem = list;
	MSList *tmp;
	if (list==NULL) return NULL;
	while(elem->next!=NULL) {
		tmp = elem;
		elem = elem->next;
		ms_free(tmp);
	}
	ms_free(elem);
	return NULL;
}

MSList * ms_list_remove(MSList *first, void *data){
	MSList *it;
	it=ms_list_find(first,data);
	if (it) return ms_list_remove_link(first,it);
	else {
		ms_warning("ms_list_remove: no element with %p data was in the list", data);
		return first;
	}
}

int ms_list_size(const MSList *first){
	int n=0;
	while(first!=NULL){
		++n;
		first=first->next;
	}
	return n;
}

void ms_list_for_each(const MSList *list, void (*func)(void *)){
	for(;list!=NULL;list=list->next){
		func(list->data);
	}
}

void ms_list_for_each2(const MSList *list, void (*func)(void *, void *), void *user_data){
	for(;list!=NULL;list=list->next){
		func(list->data,user_data);
	}
}

MSList *ms_list_remove_link(MSList *list, MSList *elem){
	MSList *ret;
	if (elem==list){
		ret=elem->next;
		elem->prev=NULL;
		elem->next=NULL;
		if (ret!=NULL) ret->prev=NULL;
		ms_free(elem);
		return ret;
	}
	elem->prev->next=elem->next;
	if (elem->next!=NULL) elem->next->prev=elem->prev;
	elem->next=NULL;
	elem->prev=NULL;
	ms_free(elem);
	return list;
}

MSList *ms_list_find(MSList *list, void *data){
	for(;list!=NULL;list=list->next){
		if (list->data==data) return list;
	}
	return NULL;
}

MSList *ms_list_find_custom(MSList *list, int (*compare_func)(const void *, const void*), const void *user_data){
	for(;list!=NULL;list=list->next){
		if (compare_func(list->data,user_data)==0) return list;
	}
	return NULL;
}

void * ms_list_nth_data(const MSList *list, int index){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (i==index) return list->data;
	}
	ms_error("ms_list_nth_data: no such index in list.");
	return NULL;
}

int ms_list_position(const MSList *list, MSList *elem){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (elem==list) return i;
	}
	ms_error("ms_list_position: no such element in list.");
	return -1;
}

int ms_list_index(const MSList *list, void *data){
	int i;
	for(i=0;list!=NULL;list=list->next,++i){
		if (data==list->data) return i;
	}
	ms_error("ms_list_index: no such element in list.");
	return -1;
}

MSList *ms_list_insert_sorted(MSList *list, void *data, int (*compare_func)(const void *, const void*)){
	MSList *it,*previt=NULL;
	MSList *nelem;
	MSList *ret=list;
	if (list==NULL) return ms_list_append(list,data);
	else{
		nelem=ms_list_new(data);
		for(it=list;it!=NULL;it=it->next){
			previt=it;
			if (compare_func(data,it->data)<=0){
				nelem->prev=it->prev;
				nelem->next=it;
				if (it->prev!=NULL)
					it->prev->next=nelem;
				else{
					ret=nelem;
				}
				it->prev=nelem;
				return ret;
			}
		}
		previt->next=nelem;
		nelem->prev=previt;
	}
	return ret;
}

MSList *ms_list_insert(MSList *list, MSList *before, void *data){
	MSList *elem;
	if (list==NULL || before==NULL) return ms_list_append(list,data);
	for(elem=list;elem!=NULL;elem=ms_list_next(elem)){
		if (elem==before){
			if (elem->prev==NULL)
				return ms_list_prepend(list,data);
			else{
				MSList *nelem=ms_list_new(data);
				nelem->prev=elem->prev;
				nelem->next=elem;
				elem->prev->next=nelem;
				elem->prev=nelem;
			}
		}
	}
	return list;
}

MSList *ms_list_copy(const MSList *list){
	MSList *copy=NULL;
	const MSList *iter;
	for(iter=list;iter!=NULL;iter=ms_list_next(iter)){
		copy=ms_list_append(copy,iter->data);
	}
	return copy;
}


#ifndef PLUGINS_EXT
	#define PLUGINS_EXT ".so"
#endif
typedef void (*init_func_t)(void);

int ms_load_plugins(const char *dir){
	int num=0;
#if defined(WIN32) && !defined(_WIN32_WCE)
	WIN32_FIND_DATA FileData;
	HANDLE hSearch;
	char szDirPath[1024];
	char szPluginFile[1024];
	BOOL fFinished = FALSE;
	const char *tmp=getenv("DEBUG");
	BOOL debug=(tmp!=NULL && atoi(tmp)==1);
	snprintf(szDirPath, sizeof(szDirPath), "%s", dir);

	// Start searching for .dll files in the current directory.

	snprintf(szDirPath, sizeof(szDirPath), "%s\\*.dll", dir);
	hSearch = FindFirstFile(szDirPath, &FileData);
	if (hSearch == INVALID_HANDLE_VALUE)
	{
		ms_message("no plugin (*.dll) found in %s.", szDirPath);
		return 0;
	}
	snprintf(szDirPath, sizeof(szDirPath), "%s", dir);

	while (!fFinished)
	{
		/* load library */
		HINSTANCE os_handle;
		UINT em=0;
		if (!debug) em = SetErrorMode (SEM_FAILCRITICALERRORS);

		snprintf(szPluginFile, sizeof(szPluginFile), "%s\\%s", szDirPath, FileData.cFileName);
		os_handle = LoadLibraryEx (szPluginFile, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
		if (os_handle==NULL)
		{
			ms_message("Fail to load plugin %s with altered search path: error %i",szPluginFile,(int)GetLastError());
			os_handle = LoadLibraryEx (szPluginFile, NULL, 0);
		}
		if (!debug) SetErrorMode (em);
		if (os_handle==NULL)
			ms_error("Fail to load plugin %s", szPluginFile);
		else{
			init_func_t initroutine;
			char szPluginName[256];
			char szMethodName[256];
			char *minus;
			snprintf(szPluginName, 256, "%s", FileData.cFileName);
			/*on mingw, dll names might be libsomething-3.dll. We must skip the -X.dll stuff*/
			minus=strchr(szPluginName,'-');
			if (minus) *minus='\0';
			else szPluginName[strlen(szPluginName)-4]='\0'; /*remove .dll*/
			snprintf(szMethodName, 256, "%s_init", szPluginName);
			initroutine = (init_func_t) GetProcAddress (os_handle, szMethodName);
				if (initroutine!=NULL){
					initroutine();
					ms_message("Plugin loaded (%s)", szPluginFile);
					// Add this new loaded plugin to the list (useful for FreeLibrary at the end)
					ms_plugins_loaded_list=ms_list_append(ms_plugins_loaded_list,os_handle);
					num++;
				}else{
					ms_warning("Could not locate init routine of plugin %s. Should be %s",
					szPluginFile, szMethodName);
				}
		}
		if (!FindNextFile(hSearch, &FileData)) {
			if (GetLastError() == ERROR_NO_MORE_FILES){
				fFinished = TRUE;
			}
			else
			{
				ms_error("couldn't find next plugin dll.");
				fFinished = TRUE;
			}
		}
	}
	/* Close the search handle. */
	FindClose(hSearch);

#elif HAVE_DLOPEN
	char plugin_name[64];
	DIR *ds;
	MSList *loaded_plugins = NULL;
	struct dirent *de;
	char *ext;
	char *fullpath;
	ds=opendir(dir);
	if (ds==NULL){
		ms_message("Cannot open directory %s: %s",dir,strerror(errno));
		return -1;
	}
	while( (de=readdir(ds))!=NULL){
		if ((de->d_type==DT_REG || de->d_type==DT_UNKNOWN || de->d_type==DT_LNK) && (ext=strstr(de->d_name,PLUGINS_EXT))!=NULL) {
			void *handle;
			snprintf(plugin_name, MIN(sizeof(plugin_name), ext - de->d_name + 1), "%s", de->d_name);
			if (ms_list_find_custom(loaded_plugins, (MSCompareFunc)strcmp, plugin_name) != NULL) continue;
			loaded_plugins = ms_list_append(loaded_plugins, ms_strdup(plugin_name));
			fullpath=ms_strdup_printf("%s/%s",dir,de->d_name);
			ms_message("Loading plugin %s...",fullpath);

			if ( (handle=dlopen(fullpath,RTLD_NOW))==NULL){
				ms_warning("Fail to load plugin %s : %s",fullpath,dlerror());
			}else {
				char *initroutine_name=ms_malloc0(strlen(de->d_name)+10);
				char *p;
				void *initroutine=NULL;
				strcpy(initroutine_name,de->d_name);
				p=strstr(initroutine_name,PLUGINS_EXT);
				if (p!=NULL){
					strcpy(p,"_init");
					initroutine=dlsym(handle,initroutine_name);
				}

#ifdef __APPLE__
				if (initroutine==NULL){
					/* on macosx: library name are libxxxx.1.2.3.dylib */
					/* -> MUST remove the .1.2.3 */
					p=strstr(initroutine_name,".");
					if (p!=NULL)
					{
						strcpy(p,"_init");
						initroutine=dlsym(handle,initroutine_name);
					}
				}
#endif

				if (initroutine!=NULL){
					init_func_t func=(init_func_t)initroutine;
					func();
					ms_message("Plugin loaded (%s)", fullpath);
					num++;
				}else{
					ms_warning("Could not locate init routine of plugin %s",de->d_name);
				}
				ms_free(initroutine_name);
			}
			ms_free(fullpath);
		}
	}
	ms_list_for_each(loaded_plugins, ms_free);
	ms_list_free(loaded_plugins);
	closedir(ds);
#else
	ms_warning("no loadable plugin support: plugins cannot be loaded.");
	num=-1;
#endif
	return num;
}

void ms_unload_plugins(){
#if defined(WIN32) && !defined(_WIN32_WCE)
	 MSList *elem;

	for(elem=ms_plugins_loaded_list;elem!=NULL;elem=elem->next)
	{
		HINSTANCE handle=(HINSTANCE )elem->data;
        FreeLibrary(handle) ;
	}

	ms_plugins_loaded_list = ms_list_free(ms_plugins_loaded_list);
#endif
}

#ifdef ANDROID
#define LOG_DOMAIN "mediastreamer"
static void ms_android_log_handler(OrtpLogLevel lev, const char *fmt, va_list args){
	int prio;
	switch(lev){
	case ORTP_DEBUG:	prio = ANDROID_LOG_DEBUG;	break;
	case ORTP_MESSAGE:	prio = ANDROID_LOG_INFO;	break;
	case ORTP_WARNING:	prio = ANDROID_LOG_WARN;	break;
	case ORTP_ERROR:	prio = ANDROID_LOG_ERROR;	break;
	case ORTP_FATAL:	prio = ANDROID_LOG_FATAL;	break;
	default:		prio = ANDROID_LOG_DEFAULT;	break;
	}
	__android_log_vprint(prio, LOG_DOMAIN, fmt, args);
}
#endif

void ms_base_init(){
	int i;

#if defined(ENABLE_NLS)
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
#endif

#if !defined(_WIN32_WCE)
	if (getenv("MEDIASTREAMER_DEBUG")!=NULL){
		ortp_set_log_level_mask(ORTP_DEBUG|ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	}
#endif
//#ifdef ANDROID
//	ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
//	ortp_set_log_handler(ms_android_log_handler);
//#endif
	ms_message("Mediastreamer2 " MEDIASTREAMER_VERSION " (git: " GIT_VERSION ") starting.");
	/* register builtin MSFilter's */
	for (i=0;ms_base_filter_descs[i]!=NULL;i++){
		ms_filter_register(ms_base_filter_descs[i]);
	}

	ms_message("ms_base_init() done");
}

void ms_base_exit(){
	ms_filter_unregister_all();
	ms_unload_plugins();
}

void ms_plugins_init(void) {
#ifdef PACKAGE_PLUGINS_DIR
	ms_message("Loading ms plugins from [%s]",PACKAGE_PLUGINS_DIR);
	ms_load_plugins(PACKAGE_PLUGINS_DIR);
#endif
}

void ms_sleep(int seconds){
#ifdef WIN32
	Sleep(seconds*1000);
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
#ifdef WIN32
	Sleep((DWORD)(usec/1000));
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

#define DEFAULT_MAX_PAYLOAD_SIZE 1440

static int max_payload_size=DEFAULT_MAX_PAYLOAD_SIZE;

int ms_get_payload_max_size(){
	return max_payload_size;
}

void ms_set_payload_max_size(int size){
	if (size<=0) size=DEFAULT_MAX_PAYLOAD_SIZE;
	max_payload_size=size;
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
	unsigned int max_plc_time;
};

/*** plc context begin***/
unsigned long ms_concealer_context_get_total_number_of_plc(MSConcealerContext* obj) {
	return obj->total_number_for_plc;
}

MSConcealerContext* ms_concealer_context_new(unsigned int max_plc_time){
	MSConcealerContext *obj=(MSConcealerContext *) ms_new(MSConcealerContext,1);
	obj->sample_time=-1;
	obj->plc_start_time=-1;
	obj->total_number_for_plc=0;
	obj->max_plc_time=max_plc_time;
	return obj;
}

void ms_concealer_context_destroy(MSConcealerContext* context) {
	ms_free(context);
}

int ms_concealer_inc_sample_time(MSConcealerContext* obj, uint64_t current_time, int time_increment, int got_packet){
	int plc_duration=0;
	if (obj->sample_time==-1){
		obj->sample_time=(int64_t)current_time;
	}
	obj->sample_time+=time_increment;
	if (obj->plc_start_time!=-1 && got_packet){
		plc_duration=current_time-obj->plc_start_time;
		obj->plc_start_time=-1;
		if (plc_duration>obj->max_plc_time) plc_duration=obj->max_plc_time;
	}
	return plc_duration;
}

unsigned int ms_concealer_context_is_concealement_required(MSConcealerContext* obj,uint64_t current_time) {
	
	if(obj->sample_time == -1) return 0; /*no valid value*/
	
	if (obj->sample_time < current_time){
		int plc_duration;
		if (obj->plc_start_time==-1)
			obj->plc_start_time=obj->sample_time;
		plc_duration=current_time-obj->plc_start_time;
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
	MSConcealerTsContext *obj=(MSConcealerTsContext *) ms_new(MSConcealerTsContext,1);
	obj->sample_ts=-1;
	obj->plc_start_ts=-1;
	obj->total_number_for_plc=0;
	obj->max_plc_ts=max_plc_ts;
	return obj;
}

void ms_concealer_ts_context_destroy(MSConcealerTsContext* context) {
	ms_free(context);
}

int ms_concealer_ts_context_inc_sample_ts(MSConcealerTsContext* obj, uint64_t current_ts, int ts_increment, int got_packet){
	int plc_duration=0;
	if (obj->sample_ts==-1){
		obj->sample_ts=(int64_t)current_ts;
	}
	obj->sample_ts+=ts_increment;
	if (obj->plc_start_ts!=-1 && got_packet){
		plc_duration=current_ts-obj->plc_start_ts;
		obj->plc_start_ts=-1;
		if (plc_duration>obj->max_plc_ts) plc_duration=obj->max_plc_ts;
	}
	return plc_duration;
}

unsigned int ms_concealer_ts_context_is_concealement_required(MSConcealerTsContext* obj, uint64_t current_ts) {
	if(obj->sample_ts == -1) return 0; /*no valid value*/
	
	if (obj->sample_ts < current_ts){
		int plc_duration;
		if (obj->plc_start_ts==-1)
			obj->plc_start_ts=obj->sample_ts;
		plc_duration=current_ts-obj->plc_start_ts;
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

/*** plc context end***/
