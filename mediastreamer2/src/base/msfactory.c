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
#ifndef _WIN32
#include <dirent.h>
#else
#ifndef PACKAGE_PLUGINS_DIR
#if defined(_WIN32) || defined(_WIN32_WCE)
#ifdef MS2_WINDOWS_DESKTOP
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


MS2_DEPRECATED static MSFactory *fallback_factory=NULL;

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

void ms_factory_add_platform_tag(MSFactory *obj, const char *tag) {
	if ((tag == NULL) || (tag[0] == '\0')) return;
	if (bctbx_list_find_custom(obj->platform_tags, (bctbx_compare_func)strcasecmp, tag) == NULL) {
		obj->platform_tags = bctbx_list_append(obj->platform_tags, ms_strdup(tag));
	}
}

bctbx_list_t * ms_factory_get_platform_tags(MSFactory *obj) {
	return obj->platform_tags;
}

char * ms_factory_get_platform_tags_as_string(MSFactory *obj) {
	return ms_tags_list_as_string(obj->platform_tags);
}

struct _MSVideoPresetsManager * ms_factory_get_video_presets_manager(MSFactory *factory) {
	return factory->video_presets_manager;
}

static int compare_stats_with_name(const MSFilterStats *stat, const char *name){
	return strcmp(stat->name,name);
}

static MSFilterStats *find_or_create_stats(MSFactory *factory, MSFilterDesc *desc){
	bctbx_list_t *elem=bctbx_list_find_custom(factory->stats_list,(bctbx_compare_func)compare_stats_with_name,desc->name);
	MSFilterStats *ret=NULL;
	if (elem==NULL){
		ret=ms_new0(MSFilterStats,1);
		ret->name=desc->name;
		factory->stats_list=bctbx_list_append(factory->stats_list,ret);
	}else ret=(MSFilterStats*)elem->data;
	return ret;
}

void ms_factory_init(MSFactory *obj){
	int i;
	long num_cpu=1;
	char *debug_log_enabled = NULL;
	char *tags;
#ifdef _WIN32
	SYSTEM_INFO sysinfo;
#endif

#if defined(ENABLE_NLS)
	bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
#endif
#ifndef MS2_WINDOWS_UNIVERSAL
	debug_log_enabled=getenv("MEDIASTREAMER_DEBUG");
#endif
	if (debug_log_enabled!=NULL && (strcmp("1",debug_log_enabled)==0) ){
		ortp_set_log_level_mask(ORTP_LOG_DOMAIN, ORTP_MESSAGE|ORTP_WARNING|ORTP_ERROR|ORTP_FATAL);
	}

	ms_message("Mediastreamer2 factory " MEDIASTREAMER_VERSION " (git: " GIT_VERSION ") initialized.");
	/* register builtin MSFilter's */
	for (i=0;ms_base_filter_descs[i]!=NULL;i++){
		ms_factory_register_filter(obj,ms_base_filter_descs[i]);
	}

#ifdef _WIN32 /*fixme to be tested*/
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
#ifdef _WIN32
	ms_factory_add_platform_tag(obj, "win32");
#ifdef MS2_WINDOWS_PHONE
	ms_factory_add_platform_tag(obj, "windowsphone");
#endif
#ifdef MS2_WINDOWS_UNIVERSAL
	ms_factory_add_platform_tag(obj, "windowsuniversal");
#endif
#endif
#ifdef __APPLE__
	ms_factory_add_platform_tag(obj, "apple");
#endif
#ifdef __linux
	ms_factory_add_platform_tag(obj, "linux");
#endif
#ifdef __QNX__
	ms_factory_add_platform_tag(obj, "qnx");
#endif
#ifdef ANDROID
	ms_factory_add_platform_tag(obj, "android");
#endif
#ifdef TARGET_OS_IPHONE
	ms_factory_add_platform_tag(obj, "ios");
#endif
#if defined(__arm__) || defined(_M_ARM)
	ms_factory_add_platform_tag(obj, "arm");
#else
	ms_factory_add_platform_tag(obj, "x86");
#endif
#if defined(ANDROID) || (TARGET_OS_IPHONE == 1) || defined(__arm__) || defined(_M_ARM)
	ms_factory_add_platform_tag(obj, "embedded");
#else
	ms_factory_add_platform_tag(obj, "desktop");
#endif
	tags = ms_factory_get_platform_tags_as_string(obj);
	ms_message("ms_factory_init() done: platform_tags=%s", tags);
	ms_free(tags);
}



MSFactory *ms_factory_new(void){
	MSFactory *obj=ms_new0(MSFactory,1);
	ms_factory_init(obj);
	return obj;
}


void ms_factory_register_filter(MSFactory* factory, MSFilterDesc* desc ) {
	if (desc->id==MS_FILTER_NOT_SET_ID){
		ms_fatal("MSFilterId for %s not set !",desc->name);
	}
	desc->flags|=MS_FILTER_IS_ENABLED; /*by default a registered filter is enabled*/

	/*lastly registered encoder/decoders may replace older ones*/
	factory->desc_list=bctbx_list_prepend(factory->desc_list,desc);
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
	bctbx_list_t *elem;

	for (elem = factory->desc_list; elem != NULL; elem = bctbx_list_next(elem)) {
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
	bctbx_list_t *elem;

	for (elem = factory->desc_list; elem != NULL; elem = bctbx_list_next(elem)) {
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
	bctbx_list_t *elem;
	for (elem=factory->desc_list;elem!=NULL;elem=bctbx_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if ((desc->flags & MS_FILTER_IS_ENABLED)
			&& (desc->category==MS_FILTER_ENCODER || desc->category==MS_FILTER_ENCODING_CAPTURER)
			&& strcasecmp(desc->enc_fmt,mime)==0){
			return desc;
		}
	}
	return NULL;
}

MSFilterDesc * ms_factory_get_decoder(MSFactory* factory, const char *mime){
	bctbx_list_t *elem;
	for (elem=factory->desc_list;elem!=NULL;elem=bctbx_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if ((desc->flags & MS_FILTER_IS_ENABLED)
			&& (desc->category==MS_FILTER_DECODER || desc->category==MS_FILTER_DECODER_RENDERER)
			&& strcasecmp(desc->enc_fmt,mime)==0){
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
	//MSFilterDesc *desc=ms_filter_get_decoder(mime);
	MSFilterDesc *desc = ms_factory_get_decoder(factory, mime);
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

struct _MSSndCardManager* ms_factory_get_snd_card_manager(MSFactory *factory){
	return factory->sndcardmanager;
}

struct _MSWebCamManager* ms_factory_get_web_cam_manager(MSFactory* f){
	return f->wbcmanager;
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

MSFilterDesc *ms_factory_lookup_filter_by_name(const MSFactory* factory, const char *filter_name){
	bctbx_list_t *elem;
	for (elem=factory->desc_list;elem!=NULL;elem=bctbx_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if (strcmp(desc->name,filter_name)==0){
			return desc;
		}
	}
	return NULL;
}

MSFilterDesc* ms_factory_lookup_filter_by_id( MSFactory* factory, MSFilterId id){
	bctbx_list_t *elem;

	for (elem=factory->desc_list;elem!=NULL;elem=bctbx_list_next(elem)){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if (desc->id==id){
			return desc;
		}
	}
	return NULL;
}

bctbx_list_t *ms_factory_lookup_filter_by_interface(MSFactory* factory, MSFilterInterfaceId id){
	bctbx_list_t *ret=NULL;
	bctbx_list_t *elem;
	for(elem=factory->desc_list;elem!=NULL;elem=elem->next){
		MSFilterDesc *desc=(MSFilterDesc*)elem->data;
		if (ms_filter_desc_implements_interface(desc,id))
			ret=bctbx_list_append(ret,desc);
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

const bctbx_list_t * ms_factory_get_statistics(MSFactory* obj){
	return obj->stats_list;
}

void ms_factory_reset_statistics(MSFactory *obj){
	bctbx_list_t *elem;

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
	bctbx_list_t *sorted=NULL;
	bctbx_list_t *elem;
	uint64_t total=1;
	for(elem=obj->stats_list;elem!=NULL;elem=elem->next){
		MSFilterStats *stats=(MSFilterStats *)elem->data;
		sorted=bctbx_list_insert_sorted(sorted,stats,(bctbx_compare_func)usage_compare);
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
	bctbx_list_free(sorted);
}

#ifndef PLUGINS_EXT
	#define PLUGINS_EXT ".so"
#endif
typedef void (*init_func_t)(MSFactory *);

int ms_factory_load_plugins(MSFactory *factory, const char *dir){
	int num=0;
#if defined(_WIN32) && !defined(_WIN32_WCE)
	WIN32_FIND_DATA FileData;
	HANDLE hSearch;
	char szDirPath[1024];
#ifdef UNICODE
	wchar_t wszDirPath[1024];
#endif
	char szPluginFile[1024];
	BOOL fFinished = FALSE;
	const char *tmp = NULL;
	BOOL debug = FALSE;
#ifndef MS2_WINDOWS_UNIVERSAL
	tmp = getenv("DEBUG");
#endif
	debug = (tmp != NULL && atoi(tmp) == 1);

	snprintf(szDirPath, sizeof(szDirPath), "%s", dir);

	// Start searching for .dll files in the current directory.
	snprintf(szDirPath, sizeof(szDirPath), "%s\\libms*.dll", dir);
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
#ifdef MS2_WINDOWS_DESKTOP
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
#ifdef MS2_WINDOWS_DESKTOP
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
					factory->ms_plugins_loaded_list=bctbx_list_append(factory->ms_plugins_loaded_list,os_handle);
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
	bctbx_list_t *loaded_plugins = NULL;
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
			(strstr(de->d_name, "libms") == de->d_name) && ((ext=strstr(de->d_name,PLUGINS_EXT))!=NULL)) {
			void *handle;
			snprintf(plugin_name, MIN(sizeof(plugin_name), (size_t)(ext - de->d_name + 1)), "%s", de->d_name);
			if (bctbx_list_find_custom(loaded_plugins, (bctbx_compare_func)strcmp, plugin_name) != NULL) continue;
			loaded_plugins = bctbx_list_append(loaded_plugins, ms_strdup(plugin_name));
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
	bctbx_list_for_each(loaded_plugins, ms_free);
	bctbx_list_free(loaded_plugins);
	closedir(ds);
#else
	ms_warning("no loadable plugin support: plugins cannot be loaded.");
	num=-1;
#endif
	return num;
}

void ms_factory_uninit_plugins(MSFactory *factory){
#if defined(_WIN32)
	bctbx_list_t *elem;
#endif

#if defined(_WIN32)
	for(elem=factory->ms_plugins_loaded_list;elem!=NULL;elem=elem->next)
	{
		HINSTANCE handle=(HINSTANCE )elem->data;
		FreeLibrary(handle) ;
	}

	factory->ms_plugins_loaded_list = bctbx_list_free(factory->ms_plugins_loaded_list);
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

struct _MSEventQueue *ms_factory_create_event_queue(MSFactory *obj) {
	if (obj->evq==NULL){
		obj->evq=ms_event_queue_new();
	}
	return obj->evq;
}

void ms_factory_destroy_event_queue(MSFactory *obj) {
	
	ms_event_queue_destroy(obj->evq);
	ms_factory_set_event_queue(obj,NULL);
	
	
}


struct _MSEventQueue *ms_factory_get_event_queue(MSFactory *obj){
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

bool_t ms_fmt_descriptor_equals(const MSFmtDescriptor *fmt1, const MSFmtDescriptor *fmt2) {
	if (!fmt1 || !fmt2) return FALSE;
	return compare_fmt(fmt1, fmt2) == 0;
}

static void ms_fmt_descriptor_destroy(MSFmtDescriptor *obj){
	if (obj->encoding) ms_free(obj->encoding);
	if (obj->fmtp) ms_free(obj->fmtp);
	if (obj->text) ms_free(obj->text);
	ms_free(obj);
}

const MSFmtDescriptor *ms_factory_get_format(MSFactory *obj, const MSFmtDescriptor *ref){
	MSFmtDescriptor *ret;
	bctbx_list_t *found;
	if ((found=bctbx_list_find_custom(obj->formats,(int (*)(const void*, const void*))compare_fmt, ref))==NULL){
		obj->formats=bctbx_list_append(obj->formats,ret=ms_fmt_descriptor_new_copy(ref));
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

int ms_factory_enable_filter_from_name(MSFactory *factory, const char *name, bool_t enable) {
	MSFilterDesc *desc=ms_factory_lookup_filter_by_name(factory,name);
	if (!desc) {
		ms_error("Cannot enable/disable unknown filter [%s] on factory [%p]",name,factory);
		return -1;
	}
	if (enable) desc->flags |= MS_FILTER_IS_ENABLED;
	else desc->flags &= ~MS_FILTER_IS_ENABLED;
	ms_message("Filter [%s]  %s on factory [%p]",name,(enable ? "enabled" : "disabled"),factory);
	return 0;
}

bool_t ms_factory_filter_from_name_enabled(const MSFactory *factory, const char *name) {
	MSFilterDesc *desc=ms_factory_lookup_filter_by_name(factory,name);
	if (!desc) {
		ms_error("Cannot get enable/disable state for unknown filter [%s] on factory [%p]",name,factory);
		return FALSE;
	}
	return !!(desc->flags & MS_FILTER_IS_ENABLED);
}


void ms_factory_register_offer_answer_provider(MSFactory *f, MSOfferAnswerProvider *offer_answer_prov){
	if (bctbx_list_find(f->offer_answer_provider_list, offer_answer_prov)) return; /*avoid registering several time the same pointer*/
	f->offer_answer_provider_list = bctbx_list_prepend(f->offer_answer_provider_list, offer_answer_prov);
}

MSOfferAnswerProvider * ms_factory_get_offer_answer_provider(MSFactory *f, const char *mime_type){
	const bctbx_list_t *elem;
	for (elem = f->offer_answer_provider_list; elem != NULL; elem = elem->next){
		MSOfferAnswerProvider *prov = (MSOfferAnswerProvider*) elem->data;
		if (strcasecmp(mime_type, prov->mime_type) == 0)
			return prov;
	}
	return NULL;
}

MSOfferAnswerContext * ms_factory_create_offer_answer_context(MSFactory *f, const char *mime_type){
	MSOfferAnswerProvider *prov = ms_factory_get_offer_answer_provider(f, mime_type);
	if (prov) return prov->create_context();
	return NULL;
}

MSDevicesInfo* ms_factory_get_devices_info(MSFactory *f) {
	return f->devices_info;
}

#ifdef ANDROID
#include "sys/system_properties.h"
#include <jni.h>


#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"

JNIEXPORT jint JNICALL Java_org_linphone_mediastream_Factory_enableFilterFromName(JNIEnv* env,  jobject obj, jlong factoryPtr, jstring jname, jboolean enable) {
	MSFactory *factory = (MSFactory *) factoryPtr;
	const char *name = jname ? (*env)->GetStringUTFChars(env, jname, NULL) : NULL;
	int result = ms_factory_enable_filter_from_name(factory, name, enable);
	(*env)->ReleaseStringUTFChars(env, jname, name);
	return result;
}
JNIEXPORT jboolean JNICALL Java_org_linphone_mediastream_Factory_filterFromNameEnabled(JNIEnv* env, jobject obj, jlong factoryPtr, jstring jname) {
	const char *name = jname ? (*env)->GetStringUTFChars(env, jname, NULL) : NULL;
	MSFactory *factory = (MSFactory *) factoryPtr;
	jboolean result = ms_factory_filter_from_name_enabled(factory, name);
	(*env)->ReleaseStringUTFChars(env, jname, name);
	return result;
}

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

JNIEXPORT jint JNICALL Java_org_linphone_mediastream_MediastreamerAndroidContext_enableFilterFromNameImpl(JNIEnv* env,  jobject obj, jstring jname, jboolean enable) {
	const char *mime;
	int result;
	if (ms_factory_get_fallback() == NULL) {
		ms_error("Java_org_linphone_mediastream_MediastreamerAndroidContext_enableFilterFromNameImpl(): no fallback factory. Use Factory.enableFilterFromName()");
		return -1;
	}
	mime = jname ? (*env)->GetStringUTFChars(env, jname, NULL) : NULL;
	result = ms_factory_enable_filter_from_name(ms_factory_get_fallback(),mime,enable);
	(*env)->ReleaseStringUTFChars(env, jname, mime);
	return result;
}
JNIEXPORT jboolean JNICALL Java_org_linphone_mediastream_MediastreamerAndroidContext_filterFromNameEnabledImpl(JNIEnv* env, jobject obj, jstring jname) {
	const char *mime;
	jboolean result;
	if (ms_factory_get_fallback() == NULL) {
		ms_error("Java_org_linphone_mediastream_MediastreamerAndroidContext_filterFromNameEnabledImpl(): no fallback factory. Use Factory.filterFromNameEnabled()");
		return FALSE;
	}
	mime = jname ? (*env)->GetStringUTFChars(env, jname, NULL) : NULL;
	result = ms_factory_filter_from_name_enabled(ms_factory_get_fallback(),mime);
	(*env)->ReleaseStringUTFChars(env, jname, mime);
	return result;
}

#endif



#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

/**
* Destroy the factory.
* This should be done after destroying all objects created by the factory.
**/
void ms_factory_destroy(MSFactory *factory) {
	if (factory->voip_uninit_func) factory->voip_uninit_func(factory);
	ms_factory_uninit_plugins(factory);
	if (factory->evq) ms_factory_destroy_event_queue(factory);
	factory->formats = bctbx_list_free_with_data(factory->formats, (void(*)(void*))ms_fmt_descriptor_destroy);
	factory->desc_list = bctbx_list_free(factory->desc_list);
	bctbx_list_for_each(factory->stats_list, ms_free);
	factory->stats_list = bctbx_list_free(factory->stats_list);
	factory->offer_answer_provider_list = bctbx_list_free(factory->offer_answer_provider_list);
	bctbx_list_for_each(factory->platform_tags, ms_free);
	factory->platform_tags = bctbx_list_free(factory->platform_tags);
	if (factory->plugins_dir) ms_free(factory->plugins_dir);
	ms_free(factory);
	if (factory == fallback_factory) fallback_factory = NULL;
}


MSFactory *ms_factory_create_fallback(void){
	if (fallback_factory==NULL){
		fallback_factory=ms_factory_new();
	}
	return fallback_factory;
}

/**
 * Used by the legacy functions before MSFactory was added.
 * Do not use in an application.
**/
MSFactory *ms_factory_get_fallback(void){
	return fallback_factory;
}


