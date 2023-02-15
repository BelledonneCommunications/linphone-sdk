/*
 * Copyright (c) 2010-2022 Belledonne Communications SARL.
 *
 * This file is part of mediastreamer2
 * (see https://gitlab.linphone.org/BC/public/mediastreamer2).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msogl.h"
#include "mediastreamer2/msogl_functions.h"
#include "mediastreamer2/msvideo.h"

#ifdef MS2_WINDOWS_UWP

#include <ppltasks.h>

using namespace Windows::UI::Core;
using namespace Windows::ApplicationModel::Core;
using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::System::Threading;
using namespace Concurrency;
#endif
#include "opengles_display.h"

// =============================================================================
enum { UPDATE_CONTEXT_NOTHING = 0, UPDATE_CONTEXT_UPDATE = 1, UPDATE_CONTEXT_DISPLAY_UNINIT = 2 };

struct _FilterData {
	MSOglContextInfo context_info;
#ifdef MS2_WINDOWS_UWP
	Platform::Agile<CoreApplicationView>
	    window_id; // This switch avoid using marshalling and /clr. This is the cause of not using void**
	concurrency::task<void>
	    mCurrentTask;       // Processing queue. Each task will be run inside the mCurrentTask thread (that is not ui).
	bool_t mProcessStopped; // Goal : stop outside callbacks.
	ThreadPoolTimer ^ mProcessTimer; // Goal : cancelling new process.
	TimeSpan mProcessPeriod;         // After processing a frame, delay the next process by period.
	MSFilter *mParentFilter; // Goal: Get the filter from FilterData to know if the process is stop without having to
	                         // use the Filter (that can be freed by mediastreamer).
#else
	void *window_id; // Used for window managment. On UWP, it is a CoreWindow that can be closed.
#endif
	OpenGlFunctions functions;
	unsigned long video_mode;

	struct opengles_display *display;

	MSVideoSize video_size; // Not used at this moment.

	bool_t show_video;
	bool_t mirroring;
	bool_t update_mirroring;
	bool_t update_context;

	mblk_t *prev_inm;
	MSVideoDisplayMode mode;
};

typedef struct _FilterData FilterData;
static ms_mutex_t gLock; // Protect OpenGL call from threads in order to serialize them
static bool_t gMutexInitialized = FALSE;

// =============================================================================
// Process.
// =============================================================================
static int ogl_call_render(MSFilter *f, void *arg);

static void ogl_init(MSFilter *f) {
	FilterData *data = ms_new0(FilterData, 1);
	data->display = ogl_display_new();
	data->show_video = TRUE;
	data->mirroring = TRUE;
	data->update_mirroring = FALSE;
	data->prev_inm = NULL;
	memset(&data->functions, 0, sizeof(data->functions));
	data->functions.glInitialized = FALSE;
	data->functions.eglInitialized = FALSE;
	data->video_mode = MS_FILTER_VIDEO_NONE;
	data->context_info.width = MS_VIDEO_SIZE_CIF_W;
	data->context_info.height = MS_VIDEO_SIZE_CIF_H;
	data->context_info.window = NULL;
	data->mode = MSVideoDisplayBlackBars;
#ifdef MS2_WINDOWS_UWP
	data->mProcessStopped = FALSE;
	data->mProcessTimer = nullptr;
	data->mCurrentTask = concurrency::task_from_result();
	data->mProcessPeriod.Duration = 10000000 / 30; // 30 FPS
	data->mParentFilter = f;
#endif
	f->data = data;
	if (!gMutexInitialized) {
		gMutexInitialized = TRUE;
		ms_mutex_init(&gLock, NULL);
	}
}

//---------------------------------------------------------------------------------------------------

static void ogl_uninit_data(FilterData *data) {
	ms_mutex_lock(&gLock);
	if (data->video_mode == MS_FILTER_VIDEO_AUTO && data->context_info.window) {
		ogl_destroy_window((EGLNativeWindowType *)&data->context_info.window, &data->window_id);
	}
	ogl_display_uninit(data->display, TRUE);
	ogl_display_free(data->display);
	data->display = NULL;
	ms_free(data);
	ms_mutex_unlock(&gLock);
}

//---------------------------------------------------------------------------------------------------

static void ogl_uninit(MSFilter *f) {
	ms_filter_lock(f);
	FilterData *data = (FilterData *)f->data;
	ogl_uninit_data(data);
	ms_filter_unlock(f);
}

static void ogl_preprocess(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	if (data->show_video) {
		if (data->video_mode == MS_FILTER_VIDEO_AUTO && !data->context_info.window) {
			ogl_create_window((EGLNativeWindowType *)&data->context_info.window, &data->window_id);
			data->update_context = UPDATE_CONTEXT_UPDATE;
			data->context_info.width = MS_VIDEO_SIZE_CIF_W;
			data->context_info.height = MS_VIDEO_SIZE_CIF_H;
		}
	}
}

//---------------------------------------------------------------------------------------------------
static void ogl_process(MSFilter *f) {
	ms_filter_lock(f);
	FilterData *data = (FilterData *)f->data;
	MSOglContextInfo *context_info;
	MSPicture src;
	mblk_t *inm;
	context_info = &data->context_info;

	// No context given or video disabled.
	if (!data->show_video || !context_info->window) goto end;

	if (f->inputs[0] != NULL && ((inm = ms_queue_peek_last(f->inputs[0])) != NULL) &&
	    ms_yuv_buf_init_from_mblk(&src, inm) == 0) {
		data->video_size.width = src.w;
		data->video_size.height = src.h;

		// if (data->mirroring && !mblk_get_precious_flag(inm))
		//	ms_yuv_buf_mirror(&src);

		ogl_display_set_yuv_to_display(data->display, inm);

		// Apply mirroring flag if the frame changed compared to last time process was executed or at the 1st iteration
		if (((data->prev_inm != inm) || (data->prev_inm == NULL)) && (data->update_mirroring)) {
			ogl_display_enable_mirroring_to_display(data->display, data->mirroring);
			data->update_mirroring = FALSE;
		}
		data->prev_inm = inm;
	}

end:
#ifndef MS2_WINDOWS_UWP
	ogl_call_render(NULL, data);
#endif
	ms_filter_unlock(f);
	if (f->inputs[0] != NULL) ms_queue_flush(f->inputs[0]);

	if (f->inputs[1] != NULL) ms_queue_flush(f->inputs[1]);
}

// =============================================================================
// Methods.
// =============================================================================

static int ogl_set_video_size(MSFilter *f, void *arg) {
	ms_filter_lock(f);
	((FilterData *)f->data)->video_size = *(MSVideoSize *)arg;
	ms_filter_unlock(f);

	return 0;
}

static int ogl_set_native_window_id(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	MSOglContextInfo *context_info;

	ms_filter_lock(f);

	context_info = *((MSOglContextInfo **)arg);
	if ((intptr_t)context_info != (intptr_t)MS_FILTER_VIDEO_NONE) {
		ms_message("[MSOGL] Set native window id : %p", (void *)context_info);
		if ((intptr_t)context_info == (intptr_t)MS_FILTER_VIDEO_AUTO) { // Create a new Window
			if (!data->context_info.window)
				ogl_create_window((EGLNativeWindowType *)&data->context_info.window, &data->window_id);
			data->update_context = UPDATE_CONTEXT_UPDATE;
			data->context_info.width = MS_VIDEO_SIZE_CIF_W;
			data->context_info.height = MS_VIDEO_SIZE_CIF_H;
			data->video_mode = MS_FILTER_VIDEO_AUTO;
		} else if (data->context_info.getProcAddress != context_info->getProcAddress ||
		           (context_info->window && data->context_info.window != context_info->window) ||
		           (!context_info->window && (data->context_info.width != context_info->width ||
		                                      data->context_info.height != context_info->height))) {
			ms_message("[MSOGL] Use native window : %p size=%dx%d", (void *)context_info->window, context_info->width,
			           context_info->height);
			data->functions.getProcAddress = context_info->getProcAddress;
			data->context_info = *context_info;
			data->update_context = UPDATE_CONTEXT_DISPLAY_UNINIT;
			data->video_mode = MS_FILTER_VIDEO_NONE;
		}
	} else {
		ms_message("[MSOGL] Reset native window id");
		data->update_context = UPDATE_CONTEXT_DISPLAY_UNINIT;
		if (data->video_mode == MS_FILTER_VIDEO_AUTO && data->context_info.window)
			ogl_destroy_window((EGLNativeWindowType *)&data->context_info.window, &data->window_id);
		memset(&data->context_info, 0, sizeof data->context_info);
		data->video_mode = MS_FILTER_VIDEO_NONE;
	}
	ms_filter_unlock(f);
	return 0;
}

static int ogl_get_native_window_id(MSFilter *f, void *arg) {
	FilterData *s = (FilterData *)f->data;
	MSOglContextInfo **id = (MSOglContextInfo **)arg;
	*id = &s->context_info;

	return 0;
}

static int ogl_show_video(MSFilter *f, void *arg) {
	ms_filter_lock(f);
	((FilterData *)f->data)->show_video = *(bool_t *)arg;
	ms_filter_unlock(f);

	return 0;
}

static int ogl_zoom(MSFilter *f, void *arg) {
	ms_filter_lock(f);
	ogl_display_zoom(((FilterData *)f->data)->display, (float *)arg);
	ms_filter_unlock(f);

	return 0;
}

static int ogl_set_mode(MSFilter *f, void *arg) {
	ms_filter_lock(f);
	((FilterData *)f->data)->mode = *((MSVideoDisplayMode *)arg);
	ms_filter_unlock(f);

	return 0;
}

static int ogl_enable_mirroring(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	ms_filter_lock(f);
	data->mirroring = *(bool_t *)arg;
	// This is a request to update the mirroring flag and it will be honored as soon as a new frame comes in
	data->update_mirroring = TRUE;
	ms_filter_unlock(f);

	return 0;
}

static int ogl_call_render(MSFilter *f, void *arg) {
	FilterData *data;
	if (f == NULL) data = (FilterData *)arg;
	else {
		ms_filter_lock(f);
		data = (FilterData *)f->data;
	}

	const MSOglContextInfo *context_info;

	ms_mutex_lock(&gLock);
	context_info = &data->context_info;
	if (data->update_context != UPDATE_CONTEXT_NOTHING) {
		if ((data->update_context & UPDATE_CONTEXT_DISPLAY_UNINIT) == UPDATE_CONTEXT_DISPLAY_UNINIT &&
		    !data->context_info.window)
			ogl_display_uninit(data->display, FALSE);
		if (context_info) {
			if (context_info->window) // Window is set : do EGL initialization from it
				ogl_display_auto_init(data->display, &data->functions, (EGLNativeWindowType)context_info->window,
				                      context_info->width, context_info->height);
			else // Just use input size as it is needed for viewport
				ogl_display_init(data->display, &data->functions, context_info->width, context_info->height);
		}
		data->update_context = UPDATE_CONTEXT_NOTHING;
	}
	if (data->show_video && context_info &&
	    (context_info->window || (!context_info->window && context_info->width && context_info->height))) {
		ogl_display_render(data->display, 0, data->mode);
	}
	ms_mutex_unlock(&gLock);
	if (f != NULL) ms_filter_unlock(f);
	return 0;
}

//---------------------------------------------------------------------------------------------------

static int ms_ogl_display_set_egl_target_context(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	ogl_display_set_target_context(data->display, (const MSEGLContextDescriptor *const)arg);
	return 0;
}

// Only used for testing reset process
/*
static void ogl_reset(MSFilter *f) {
    FilterData *data = (FilterData *)f->data;
    MSOglContextInfo d = data->context_info;
    MSOglContextInfo * dd = &d;
    ogl_uninit(f);
    ogl_init(f);
    ogl_set_native_window_id(f, &dd);
}
*/
// =============================================================================
//							UWP
// =============================================================================

#ifdef MS2_WINDOWS_UWP
//------------------------------------------------------------------------------
// Tools
//------------------------------------------------------------------------------

// Add a new task to do into the concurrency while locking the filter. This task in not in UI thread which means that we
// are allowed to wait an UI task without blocking the caller (which can be the UI itself) An additional advantage is
// that the currency have an event loop behaviour : new task is done after all others.
template <typename Functor>
static void add_task_uwp(FilterData *data, Functor functor) {
	bool_t lock = !data->mProcessStopped;
	if (lock) ms_filter_lock(data->mParentFilter);
	data->mCurrentTask = data->mCurrentTask.then(functor);
	if (lock) ms_filter_unlock(data->mParentFilter);
}

// Create and wait a task to do in the UI thread.
template <typename Functor>
static void run_ui_task_uwp(Functor functor) {
	Concurrency::create_task(CoreApplication::MainView->CoreWindow->Dispatcher->RunAsync(
	                             CoreDispatcherPriority::Low, ref new DispatchedHandler(functor)))
	    .wait();
}

//------------------------------------------------------------------------------

static void ogl_uninit_uwp(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	add_task_uwp(data, [data]() {
		run_ui_task_uwp([data]() { ogl_uninit_data(data); });
		return task_from_result();
	});
}

static task<void> delay_process_uwp(FilterData *data) {
	// Called from mCurrentTask : it is safe to check stopping_process
	if (data->mProcessStopped) return task_from_result();
	else
		return create_task([data]() {
			       if (!data->mProcessStopped)
				       // Keep data to check directly if we are stopped instead of checking F that can be freed by
				       // mediastreamer
				       run_ui_task_uwp([data]() {
					       if (!data->mProcessStopped) ogl_call_render(data->mParentFilter, data);
				       });
			       return task_from_result();
		       })
		    .then([data]() {
			    if (!data->mProcessStopped) {
				    data->mProcessTimer = ThreadPoolTimer::CreateTimer(
				        ref new TimerElapsedHandler([data](ThreadPoolTimer ^ source) {
					        add_task_uwp(data, [data]() { return delay_process_uwp(data); });
				        }),
				        data->mProcessPeriod);
			    }
			    return task_from_result();
		    });
}

static void ogl_preprocess_uwp(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	ogl_preprocess(f);
	data->mProcessTimer = ThreadPoolTimer::CreateTimer(ref new TimerElapsedHandler([data](ThreadPoolTimer ^ source) {
		                                                   add_task_uwp(data, [data]() {
			                                                   data->mProcessStopped = FALSE;
			                                                   return delay_process_uwp(data);
		                                                   });
	                                                   }),
	                                                   data->mProcessPeriod);
}

static int ogl_call_render_uwp(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	auto param = (FilterData *)arg;

	// Do not use 'arg' because of scope variable (should not be really an issue here because arg is not used when this
	// function is called)
	add_task_uwp(data, [data, param]() {
		run_ui_task_uwp([data, param]() {
			if (!data->mProcessStopped) ogl_call_render(data->mParentFilter, (void *)&param);
		});
		return task_from_result();
	});
	return 0;
}

static void ogl_postprocess_uwp(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	ms_filter_lock(f);
	data->mProcessStopped = TRUE;
	if (data->mProcessTimer) data->mProcessTimer->Cancel();
	ms_filter_unlock(f);
}

//------------------------------------------------------------------------------

static int ogl_set_native_window_id_uwp(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	MSOglContextInfo *context_info = *((MSOglContextInfo **)arg);
	MSOglContextInfo context_info_data; // Store copy to keep scope from threads

	if (context_info) {
		context_info_data = *context_info;
	}

	// Do not use 'arg' because of scope variable
	add_task_uwp(data, [f, data, context_info, context_info_data]() {
		run_ui_task_uwp([f, data, context_info, context_info_data]() {
			if (!data->mProcessStopped) {
				if (context_info) {
					auto context_info_ref = &context_info_data;
					ogl_set_native_window_id(data->mParentFilter, (void *)&context_info_ref);
				} else {
					ogl_set_native_window_id(data->mParentFilter, (void *)&context_info);
				}
			}
		});
		return task_from_result();
	});
	return 0;
}

static int ms_ogl_display_set_egl_target_context_uwp(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	auto param = *(const MSEGLContextDescriptor *const)arg;

	// Do not use 'arg' because of scope variable
	add_task_uwp(data, [data, param]() {
		run_ui_task_uwp([data, param]() {
			if (!data->mProcessStopped) ms_ogl_display_set_egl_target_context(data->mParentFilter, (void *)&param);
		});
		return task_from_result();
	});
	return 0;
}
#endif

// =============================================================================
// Register filter.
// =============================================================================

#ifdef __cplusplus
extern "C" {
#endif

#ifdef MS2_WINDOWS_UWP
static MSFilterMethod methods[] = {{MS_FILTER_SET_VIDEO_SIZE, ogl_set_video_size},
                                   {MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, ogl_set_native_window_id_uwp},
                                   {MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, ogl_get_native_window_id},
                                   {MS_VIDEO_DISPLAY_SHOW_VIDEO, ogl_show_video},
                                   {MS_VIDEO_DISPLAY_ZOOM, ogl_zoom},
                                   {MS_VIDEO_DISPLAY_ENABLE_MIRRORING, ogl_enable_mirroring},
                                   {MS_VIDEO_DISPLAY_SET_MODE, ogl_set_mode},
                                   {MS_OGL_RENDER, ogl_call_render_uwp},
                                   {MS_OGL_DISPLAY_SET_EGL_TARGET_CONTEXT, ms_ogl_display_set_egl_target_context_uwp},
                                   {0, NULL}};
MSFilterDesc ms_ogl_desc = {MS_OGL_ID,                          // id
                            "MSOGL",                            // name
                            N_("OpenGL ES 2 display via EGL."), // text
                            MS_FILTER_OTHER,                    // category
                            NULL,                               // enc_fmt
                            2,                                  // ninputs
                            0,                                  // noutputs
                            ogl_init,                           // init
                            ogl_preprocess_uwp,                 // preprocess
                            ogl_process,                        // process
                            ogl_postprocess_uwp,                // postprocess
                            ogl_uninit_uwp,                     // uninit
                            methods};
#else
static MSFilterMethod methods[] = {{MS_FILTER_SET_VIDEO_SIZE, ogl_set_video_size},
                                   {MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, ogl_set_native_window_id},
                                   {MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, ogl_get_native_window_id},
                                   {MS_VIDEO_DISPLAY_SHOW_VIDEO, ogl_show_video},
                                   {MS_VIDEO_DISPLAY_ZOOM, ogl_zoom},
                                   {MS_VIDEO_DISPLAY_ENABLE_MIRRORING, ogl_enable_mirroring},
                                   {MS_VIDEO_DISPLAY_SET_MODE, ogl_set_mode},
                                   {MS_OGL_RENDER, ogl_call_render},
                                   {MS_OGL_DISPLAY_SET_EGL_TARGET_CONTEXT, ms_ogl_display_set_egl_target_context},
                                   {0, NULL}};
MSFilterDesc ms_ogl_desc = {
    MS_OGL_ID,                          // id
    "MSOGL",                            // name
    N_("OpenGL ES 2 display via EGL."), // text
    MS_FILTER_OTHER,                    // category
    NULL,                               // enc_fmt
    2,                                  // ninputs
    0,                                  // noutputs
    ogl_init,                           // init
    ogl_preprocess,                     // preprocess
    ogl_process,                        // process
    NULL,                               // postprocess
    ogl_uninit,                         // uninit
    methods                             // methods
};
#endif
#ifdef __cplusplus
} // extern "C"
#endif
MS_FILTER_DESC_EXPORT(ms_ogl_desc)
