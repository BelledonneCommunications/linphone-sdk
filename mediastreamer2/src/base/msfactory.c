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

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/mseventqueue.h"
#include "basedescs.h"

#if !defined(_WIN32_WCE)
#include <sys/types.h>
#endif
#ifndef WIN32
#include <dirent.h>
#else
#ifndef PACKAGE_PLUGINS_DIR
#if defined(WIN32) || defined(_WIN32_WCE)
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define PACKAGE_PLUGINS_DIR "lib\\mediastreamer\\plugins\\"
#else
#define PACKAGE_PLUGINS_DIR "."
#endif
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

#ifdef __QNX__
#include <sys/syspage.h>
#endif


static MSFactory *fallback_factory=NULL;


static void ms_fmt_descriptor_destroy(MSFmtDescriptor *obj);

#define DEFAULT_MAX_PAYLOAD_SIZE 1440

int ms_factory_get_payload_max_size(MSFactory *factory){
	return factory->max_payload_size;
}

void ms_factory_set_payload_max_size(MSFactory *obj, int size){
	if (size<=0) size=DEFAULT_MAX_PAYLOAD_SIZE;
	obj->max_payload_size=size;
}

#define MS_MTU_DEFAULT 1500

void ms_factory_set_mtu(MSFactory *obj, int mtu){
	/*60= IPv6+UDP+RTP overhead */
	if (mtu>60){
		obj->mtu=mtu;
		ms_factory_set_payload_max_size(obj,mtu-60);
	}else {
		if (mtu>0){
			ms_warning("MTU is too short: %i bytes, using default value instead.",mtu);
		}
		ms_factory_set_mtu(obj,MS_MTU_DEFAULT);
	}
}

int ms_factory_get_mtu(MSFactory *obj){
	return obj->mtu;
}

unsigned int ms_factory_get_cpu_count(MSFactory *obj) {
	return obj->cpu_count;
}

void ms_factory_set_cpu_count(MSFactory *obj, unsigned int c) {
	ms_message("CPU count set to %d", c);
	obj->cpu_count = c;
}

static int compare_stats_with_name(const MSFilterStats *stat, const char *name){
	return strcmp(stat->name,name);
}

static MSFilterStats *find_or_create_stats(MSFactory *factory, MSFilterDesc *desc){
	MSList *elem=ms_list_find_custom(factory->stats_list,(MSCompareFunc)compare_stats_with_name,desc->name);
	MSFilterStats *ret=NULL;
	if (elem==NULL){
		ret=ms_new0(MSFilterStats,1);
		ret->name=desc->name;
		factory->stats_list=ms_list_append(factory->stats_list,ret);
	}else ret=(MSFilterStats*)elem->data;
	return ret;
}

/**
 * Used by the legacy functions before MSFactory was added.
 * Do not use in an application.
**/
MSFactory *ms_factory_get_fallback(void){
	return fallback_factory;
}

void ms_factory_init(MSFactory *obj){
	int i;
	long num_cpu=1;
	char *debug_log_enabled;
#ifdef WIN32
	SYSTEM_INFO sysinfo;
#endif

#if defined(ENABLE_NLS)
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
#endif
	debug_log_enabled=getenv("MEDIASTREAMER_DEBUG");
	if (debug_log_enabled!=NULL && (strcmp("1",debug_log_enabled)==0) ){
		ortp_set_log_level_mask(ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	}

	ms_message("Mediastreamer2 factory " MEDIASTREAMER_VERSION " (git: " GIT_VERSION ") initialized.");
	/* register builtin MSFilter's */
	for (i=0;ms_base_filter_descs[i]!=NULL;i++){
		ms_factory_register_filter(obj,ms_base_filter_descs[i]);
	}

#ifdef WIN32 /*fixme to be tested*/
	GetNativeSystemInfo( &sysinfo );

	num_cpu = sysinfo.dwNumberOfProcessors;
#elif __APPLE__ || __linux
	num_cpu = sysconf( _SC_NPROCESSORS_CONF); /*check the number of processors configured, not just the one that are currently active.*/
#elif __QNX__
	num_cpu = _syspage_ptr->num_cpu;
#else
#warning "There is no code that detects the number of CPU for this platform."
#endif
	ms_factory_set_cpu_count(obj,num_cpu);
	ms_factory_set_mtu(obj,MS_MTU_DEFAULT);
	ms_message("ms_factory_init() done");
}

MSFactory *ms_factory_create_fallback(void){
	if (fallback_factory==NULL){
		fallback_factory=ms_factory_new();
	}
	return fallback_factory;
}

MSFactory *ms_factory_new(void){
	MSFactory *obj=ms_new0(MSFactory,1);
	ms_factory_init(obj);
	return obj;
}

/**
 * Destroy the factory.
 * This should be done after destroying all objects created by the factory.
**/
void ms_factory_destroy(MSFactory *factory){
	ms_factory_uninit_plugins(factory);
	if (factory->evq) ms_event_queue_destroy(factory->evq);
	factory->formats=ms_list_free_with_data(factory->formats,(void(*)(void*))ms_fmt_descriptor_destroy);
	factory->desc_list=ms_list_free(factory->desc_list);
	ms_list_for_each(factory->stats_list,ms_free);
	factory->stats_list=ms_list_free(factory->stats_list);
	if (factory->plugins_dir) ms_free(factory->plugins_dir);
	ms_free(factory);
	if (factory==fallback_factory) fallback_factory=NULL;
}



void ms_factory_register_filter(MSFactory* factory, MSFilterDesc* desc ) {
	if (desc->id==MS_FILTER_NOT_SET_ID){
		ms_fatal("MSFilterId for %s not set !",desc->name);
	}
	/*lastly registered encoder/decoders may replace older ones*/
	factory->desc_list=ms_list_prepend(factory->desc_list,desc);
}

bool_t ms_factory_codec_supported(MSFactory* factory, const char *mime){
	MSFilterDesc *enc = ms_factory_get_encoding_capturer(factory, mime);
	MSFilterDesc *dec = ms_factory_get_decoding_renderer(factory, mime);

	if (enc == NULL) enc = ms_factory_get_encoder(factory, mime);
	if (dec == NULL) dec = ms_factory_get_decoder(factory, mime);

	if(enc!=NULL && dec!=NULL) return TRUE;

	if(enc==NULL) ms_message("Could not find encoder for %s", mime);
	if(dec==NULL) ms_message("Could not find decoder for %s", mime);
	return FALSE;
}

MSFilterDesc * ms_factory_get_encoding_capturer(MSFactory* factory, const char *mime) {
	MSList *elem;

	for (elem = factory->desc_list; elem != NULL; elem = ms_list_next(elem)) {
		MSFilterDesc *desc = (MSFilterDesc *)elem->data;
		if (desc->category == MS_FILTER_ENCODING_CAPTURER) {
			char *saveptr=NULL;
			char *enc_fmt = ms_strdup(desc->enc_fmt);
			char *token = strtok_r(enc_fmt, " ", &saveptr);
			while (token != NULL) {
				if (strcasecmp(token, mime) == 0) {
					break;
				}
				token = strtok_r(NULL, " ", &saveptr);
			}
			ms_free(enc_fmt);
			if (token != NULL) return desc;
		}
	}
	return NULL;
}

MSFilterDesc * ms_factory_get_decoding_renderer(MSFactory* factory, const char *mime) {
	MSList *elem;

	for (elem = factory->desc_list; elem != NULL; elem = ms_list_next(elem)) {
		MSFilterDesc *desc = (MSFilterDesc *)elem->data;
		if (desc->category == MS_FILTER_DECODER_RENDERER) {
			char *saveptr=NULL;
			char *enc_fmt = ms_strdup(desc->enc_fmt);
			char *token = strtok_r(enc_fmt, " ", &saveptr);
			while (token != NULL) {
				if (strcasecmp(token, mime) == 0) {
					break;
				}
				token = strtok_r(NULL, " ", &saveptr);
			}
			ms_free(enc_fmt);
			if (token != NULL) return desc;
		}
	}
	return NULL;
}

MSFilterDesc * ms_factory_get_encoder(MSFactory* factory, const char *mime){
	MSList *elem;
	for (elem=factory->desc_list;elem!=NULL;elem=ms_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if ((desc->category==MS_FILTER_ENCODER || desc->category==MS_FILTER_ENCODING_CAPTURER) &&
			strcasecmp(desc->enc_fmt,mime)==0){
			return desc;
		}
	}
	return NULL;
}

MSFilterDesc * ms_factory_get_decoder(MSFactory* factory, const char *mime){
	MSList *elem;
	for (elem=factory->desc_list;elem!=NULL;elem=ms_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if ((desc->category==MS_FILTER_DECODER || desc->category==MS_FILTER_DECODER_RENDERER) &&
			strcasecmp(desc->enc_fmt,mime)==0){
			return desc;
		}
	}
	return NULL;
}

MSFilter * ms_factory_create_encoder(MSFactory* factory, const char *mime){
	MSFilterDesc *desc=ms_factory_get_encoder(factory,mime);
	if (desc!=NULL) return ms_factory_create_filter_from_desc(factory,desc);
	return NULL;
}

MSFilter * ms_factory_create_decoder(MSFactory* factory, const char *mime){
	MSFilterDesc *desc=ms_filter_get_decoder(mime);
	if (desc!=NULL) return ms_factory_create_filter_from_desc(factory,desc);
	return NULL;
}

MSFilter *ms_factory_create_filter_from_desc(MSFactory* factory, MSFilterDesc *desc){
	MSFilter *obj;
	obj=(MSFilter *)ms_new0(MSFilter,1);
	ms_mutex_init(&obj->lock,NULL);
	obj->desc=desc;
	if (desc->ninputs>0)	obj->inputs=(MSQueue**)ms_new0(MSQueue*,desc->ninputs);
	if (desc->noutputs>0)	obj->outputs=(MSQueue**)ms_new0(MSQueue*,desc->noutputs);

	if (factory->statistics_enabled){
		obj->stats=find_or_create_stats(factory,desc);
	}
	obj->factory=factory;
	if (obj->desc->init!=NULL)
		obj->desc->init(obj);
	return obj;
}

MSFilter *ms_factory_create_filter(MSFactory* factory, MSFilterId id){
	MSFilterDesc *desc;
	if (id==MS_FILTER_PLUGIN_ID){
		ms_warning("cannot create plugin filters with ms_filter_new_from_id()");
		return NULL;
	}
	desc=ms_factory_lookup_filter_by_id(factory,id);
	if (desc) return ms_factory_create_filter_from_desc(factory,desc);
	ms_error("No such filter with id %i",id);
	return NULL;
}

MSFilterDesc *ms_factory_lookup_filter_by_name(MSFactory* factory, const char *filter_name){
	MSList *elem;
	for (elem=factory->desc_list;elem!=NULL;elem=ms_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if (strcmp(desc->name,filter_name)==0){
			return desc;
		}
	}
	return NULL;
}

MSFilterDesc* ms_factory_lookup_filter_by_id( MSFactory* factory, MSFilterId id){
	MSList *elem;

	for (elem=factory->desc_list;elem!=NULL;elem=ms_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if (desc->id==id){
			return desc;
		}
	}
	return NULL;
}

MSList *ms_factory_lookup_filter_by_interface(MSFactory* factory, MSFilterInterfaceId id){
	MSList *ret=NULL;
	MSList *elem;
	for(elem=factory->desc_list;elem!=NULL;elem=elem->next){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if (ms_filter_desc_implements_interface(desc,id))
			ret=ms_list_append(ret,desc);
	}
	return ret;
}

MSFilter *ms_factory_create_filter_from_name(MSFactory* factory, const char *filter_name){
	MSFilterDesc *desc=ms_factory_lookup_filter_by_name(factory, filter_name);
	if (desc==NULL) return NULL;
	return ms_factory_create_filter_from_desc(factory,desc);
}

void ms_factory_enable_statistics(MSFactory* obj, bool_t enabled){
	obj->statistics_enabled=enabled;
}

const MSList * ms_factory_get_statistics(MSFactory* obj){
	return obj->stats_list;
}

void ms_factory_reset_statistics(MSFactory *obj){
	MSList *elem;

	for(elem=obj->stats_list;elem!=NULL;elem=elem->next){
		MSFilterStats *stats=(MSFilterStats *)elem->data;
		stats->elapsed=0;
		stats->count=0;
	}
}

static int usage_compare(const MSFilterStats *s1, const MSFilterStats *s2){
	if (s1->elapsed==s2->elapsed) return 0;
	if (s1->elapsed<s2->elapsed) return 1;
	return -1;
}


void ms_factory_log_statistics(MSFactory *obj){
	MSList *sorted=NULL;
	MSList *elem;
	uint64_t total=1;
	for(elem=obj->stats_list;elem!=NULL;elem=elem->next){
		MSFilterStats *stats=(MSFilterStats *)elem->data;
		sorted=ms_list_insert_sorted(sorted,stats,(MSCompareFunc)usage_compare);
		total+=stats->elapsed;
	}
	ms_message("===========================================================");
	ms_message("                  FILTER USAGE STATISTICS                  ");
	ms_message("Name                Count     Time/tick (ms)      CPU Usage");
	ms_message("-----------------------------------------------------------");
	for(elem=sorted;elem!=NULL;elem=elem->next){
		MSFilterStats *stats=(MSFilterStats *)elem->data;
		double percentage=100.0*((double)stats->elapsed)/(double)total;
		double tpt=((double)stats->elapsed*1e-6)/((double)stats->count+1.0);
		ms_message("%-19s %-9i %-19g %-10g",stats->name,stats->count,tpt,percentage);
	}
	ms_message("===========================================================");
	ms_list_free(sorted);
}

#ifndef PLUGINS_EXT
	#define PLUGINS_EXT ".so"
#endif
typedef void (*init_func_t)(MSFactory *);

int ms_factory_load_plugins(MSFactory *factory, const char *dir){
	int num=0;
#if defined(WIN32) && !defined(_WIN32_WCE)
	WIN32_FIND_DATA FileData;
	HANDLE hSearch;
	char szDirPath[1024];
#ifdef UNICODE
	wchar_t wszDirPath[1024];
#endif
	char szPluginFile[1024];
	BOOL fFinished = FALSE;
	const char *tmp=getenv("DEBUG");
	BOOL debug=(tmp!=NULL && atoi(tmp)==1);

	snprintf(szDirPath, sizeof(szDirPath), "%s", dir);

	// Start searching for .dll files in the current directory.
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
	snprintf(szDirPath, sizeof(szDirPath), "%s\\*.dll", dir);
#else
	snprintf(szDirPath, sizeof(szDirPath), "%s\\libms*.dll", dir);
#endif
#ifdef UNICODE
	mbstowcs(wszDirPath, szDirPath, sizeof(wszDirPath));
	hSearch = FindFirstFileExW(wszDirPath, FindExInfoStandard, &FileData, FindExSearchNameMatch, NULL, 0);
#else
	hSearch = FindFirstFileExA(szDirPath, FindExInfoStandard, &FileData, FindExSearchNameMatch, NULL, 0);
#endif
	if (hSearch == INVALID_HANDLE_VALUE)
	{
		ms_message("no plugin (*.dll) found in [%s] [%d].", szDirPath, (int)GetLastError());
		return 0;
	}
	snprintf(szDirPath, sizeof(szDirPath), "%s", dir);

	while (!fFinished)
	{
		/* load library */
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
		UINT em=0;
#endif
		HINSTANCE os_handle;
#ifdef UNICODE
		wchar_t wszPluginFile[2048];
		char filename[512];
		wcstombs(filename, FileData.cFileName, sizeof(filename));
		snprintf(szPluginFile, sizeof(szPluginFile), "%s\\%s", szDirPath, filename);
		mbstowcs(wszPluginFile, szPluginFile, sizeof(wszPluginFile));
#else
		snprintf(szPluginFile, sizeof(szPluginFile), "%s\\%s", szDirPath, FileData.cFileName);
#endif
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
		if (!debug) em = SetErrorMode (SEM_FAILCRITICALERRORS);

#ifdef UNICODE
		os_handle = LoadLibraryExW(wszPluginFile, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#else
		os_handle = LoadLibraryExA(szPluginFile, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
#endif
		if (os_handle==NULL)
		{
			ms_message("Fail to load plugin %s with altered search path: error %i",szPluginFile,(int)GetLastError());
#ifdef UNICODE
			os_handle = LoadLibraryExW(wszPluginFile, NULL, 0);
#else
			os_handle = LoadLibraryExA(szPluginFile, NULL, 0);
#endif
		}
		if (!debug) SetErrorMode (em);
#else
		os_handle = LoadPackagedLibrary(wszPluginFile, 0);
#endif
		if (os_handle==NULL)
			ms_error("Fail to load plugin %s: error %i", szPluginFile, (int)GetLastError());
		else{
			init_func_t initroutine;
			char szPluginName[256];
			char szMethodName[256];
			char *minus;
#ifdef UNICODE
			snprintf(szPluginName, sizeof(szPluginName), "%s", filename);
#else
			snprintf(szPluginName, sizeof(szPluginName), "%s", FileData.cFileName);
#endif
			/*on mingw, dll names might be libsomething-3.dll. We must skip the -X.dll stuff*/
			minus=strchr(szPluginName,'-');
			if (minus) *minus='\0';
			else szPluginName[strlen(szPluginName)-4]='\0'; /*remove .dll*/
			snprintf(szMethodName, sizeof(szMethodName), "%s_init", szPluginName);
			initroutine = (init_func_t) GetProcAddress (os_handle, szMethodName);
				if (initroutine!=NULL){
					initroutine(factory);
					ms_message("Plugin loaded (%s)", szPluginFile);
					// Add this new loaded plugin to the list (useful for FreeLibrary at the end)
					factory->ms_plugins_loaded_list=ms_list_append(factory->ms_plugins_loaded_list,os_handle);
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

#elif defined(HAVE_DLOPEN)
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
		if (
#ifndef __QNX__
			(de->d_type==DT_REG || de->d_type==DT_UNKNOWN || de->d_type==DT_LNK) &&
#endif
			(ext=strstr(de->d_name,PLUGINS_EXT))!=NULL) {
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
					func(factory);
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

void ms_factory_uninit_plugins(MSFactory *factory){
#if defined(WIN32)
	MSList *elem;
#endif

#if defined(WIN32)
	for(elem=factory->ms_plugins_loaded_list;elem!=NULL;elem=elem->next)
	{
		HINSTANCE handle=(HINSTANCE )elem->data;
		FreeLibrary(handle) ;
	}

	factory->ms_plugins_loaded_list = ms_list_free(factory->ms_plugins_loaded_list);
#endif
}

void ms_factory_init_plugins(MSFactory *obj) {
	if (obj->plugins_dir == NULL) {
#ifdef PACKAGE_PLUGINS_DIR
		obj->plugins_dir = ms_strdup(PACKAGE_PLUGINS_DIR);
#else
		obj->plugins_dir = ms_strdup("");
#endif
	}
	if (strlen(obj->plugins_dir) > 0) {
		ms_message("Loading ms plugins from [%s]",obj->plugins_dir);
		ms_factory_load_plugins(obj,obj->plugins_dir);
	}
}

void ms_factory_set_plugins_dir(MSFactory *obj, const char *path) {
	if (obj->plugins_dir != NULL) {
		ms_free(obj->plugins_dir);
		obj->plugins_dir=NULL;
	}
	if (path)
		obj->plugins_dir = ms_strdup(path);
}

struct _MSEventQueue *ms_factory_get_event_queue(MSFactory *obj){
	if (obj->evq==NULL){
		obj->evq=ms_event_queue_new();
	}
	return obj->evq;
}

/*this function is for compatibility, when event queues were created by the application*/
void ms_factory_set_event_queue(MSFactory *obj, MSEventQueue *evq){
	obj->evq=evq;
}

static int compare_fmt(const MSFmtDescriptor *a, const MSFmtDescriptor *b){
	if (a->type!=b->type) return -1;
	if (strcasecmp(a->encoding,b->encoding)!=0) return -1;
	if (a->rate!=b->rate) return -1;
	if (a->nchannels!=b->nchannels) return -1;
	if (a->fmtp==NULL && b->fmtp!=NULL) return -1;
	if (a->fmtp!=NULL && b->fmtp==NULL) return -1;
	if (a->fmtp && b->fmtp && strcmp(a->fmtp,b->fmtp)!=0) return -1;
	if (a->type==MSVideo){
		if (a->vsize.width!=b->vsize.width || a->vsize.height!=b->vsize.height) return -1;
		if (a->fps!=b->fps) return -1;
	}
	return 0;
}

static MSFmtDescriptor * ms_fmt_descriptor_new_copy(const MSFmtDescriptor *orig){
	MSFmtDescriptor *obj=ms_new0(MSFmtDescriptor,1);
	obj->type=orig->type;
	obj->rate=orig->rate;
	obj->nchannels=orig->nchannels;
	if (orig->fmtp) obj->fmtp=ms_strdup(orig->fmtp);
	if (orig->encoding) obj->encoding=ms_strdup(orig->encoding);
	obj->vsize=orig->vsize;
	obj->fps=orig->fps;
	return obj;
}

const char *ms_fmt_descriptor_to_string(const MSFmtDescriptor *obj){
	MSFmtDescriptor *mutable_fmt=(MSFmtDescriptor*)obj;
	if (!obj) return "null";
	if (obj->text==NULL){
		if (obj->type==MSAudio){
			mutable_fmt->text=ms_strdup_printf("type=audio;encoding=%s;rate=%i;channels=%i;fmtp='%s'",
							 obj->encoding,obj->rate,obj->nchannels,obj->fmtp ? obj->fmtp : "");
		}else{
			mutable_fmt->text=ms_strdup_printf("type=video;encoding=%s;vsize=%ix%i;fps=%f;fmtp='%s'",
							 obj->encoding,obj->vsize.width,obj->vsize.height,obj->fps,obj->fmtp ? obj->fmtp : "");
		}
	}
	return obj->text;
}

static void ms_fmt_descriptor_destroy(MSFmtDescriptor *obj){
	if (obj->encoding) ms_free(obj->encoding);
	if (obj->fmtp) ms_free(obj->fmtp);
	if (obj->text) ms_free(obj->text);
	ms_free(obj);
}

const MSFmtDescriptor *ms_factory_get_format(MSFactory *obj, const MSFmtDescriptor *ref){
	MSFmtDescriptor *ret;
	MSList *found;
	if ((found=ms_list_find_custom(obj->formats,(int (*)(const void*, const void*))compare_fmt, ref))==NULL){
		obj->formats=ms_list_append(obj->formats,ret=ms_fmt_descriptor_new_copy(ref));
	}else{
		ret=(MSFmtDescriptor *)found->data;
	}
	return ret;
}

const MSFmtDescriptor * ms_factory_get_audio_format(MSFactory *obj, const char *mime, int rate, int channels, const char *fmtp){
	MSFmtDescriptor tmp={0};
	tmp.type=MSAudio;
	tmp.encoding=(char*)mime;
	tmp.rate=rate;
	tmp.nchannels=channels;
	tmp.fmtp=(char*)fmtp;
	return ms_factory_get_format(obj,&tmp);
}

const MSFmtDescriptor * ms_factory_get_video_format(MSFactory *obj, const char *mime, MSVideoSize size, float fps, const char *fmtp){
	MSFmtDescriptor tmp={0};
	tmp.type=MSVideo;
	tmp.encoding=(char*)mime;
	tmp.rate=90000;
	tmp.vsize=size;
	tmp.fmtp=(char*)fmtp;
	tmp.fps=fps;
	return ms_factory_get_format(obj,&tmp);
}

