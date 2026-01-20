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

#include "mediastreamer2/msasync.h"
#include "mediastreamer2/mscommon.h"
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

typedef struct _FilterData {
	MSOglContextInfo context_info;
	float fps;
#ifdef MS2_WINDOWS_UWP
	Platform::Agile<CoreApplicationView>
	    window_id; // This switch avoid using marshalling and /clr. This is the cause of not using void**
	ms_mutex_t current_task_lock;
	concurrency::task<void>
	    current_task;       // Processing queue. Each task will be run inside the current_task thread (that is not ui).
	bool_t process_stopped; // Goal : stop outside callbacks.
	ThreadPoolTimer ^ process_timer; // Goal : cancelling new process.
	TimeSpan process_period;         // After processing a frame, delay the next process by period.
	MSFilter *parent_filter; // Goal: Get the filter from FilterData to know if the process is stop without having to
	                         // use the Filter (that can be freed by mediastreamer).
#else
	void *window_id; // Used for window management. On UWP, it is a CoreWindow that can be closed.
	bool_t running;
	MSWorkerThread *process_thread;
	MSTask *refresh_task;
#endif
	void *requested_window_id;
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
} FilterData;

static ms_mutex_t gLock; // Protect OpenGL call from threads in order to serialize them
static bool_t gMutexInitialized = FALSE;

#ifndef MS2_WINDOWS_UWP
/***********************************************************************************\
                                THREAD MANAGEMENT
                        msogl_get_worker / msogl_release_worker

Call msogl_get_worker to get a worker.
When finishing rendering, call msogl_release_worker to release the current worker.
From the result, call ms_worker_thread_destroy if needed.

\***********************************************************************************/
typedef struct _MSOGLSharedContext {
	MSWorkerThread *process_thread;
	int use_count;
} MSOGLSharedContext;
static MSOGLSharedContext shared_context = {NULL, 0};
static ms_mutex_t shared_context_lock;
static bool_t shared_context_initialized = FALSE;
static void destroy_shared_context_mutex(void) {
	if (shared_context_initialized) {
		ms_mutex_destroy(&shared_context_lock);
		shared_context_initialized = FALSE;
	}
}
static MSWorkerThread *msogl_get_worker(void) {
	if (!shared_context_initialized) {
		shared_context_initialized = TRUE;
		ms_mutex_init(&shared_context_lock, NULL);
		atexit(destroy_shared_context_mutex);
	}
	ms_mutex_lock(&shared_context_lock);
	if (shared_context.use_count == 0) {
		shared_context.process_thread = ms_worker_thread_new("MSOGL");
		ms_message("[MSOGL] Rendering thread created: %p", shared_context.process_thread);
	} else
		ms_message("[MSOGL] Requesting rendering thread: %p:%d", shared_context.process_thread,
		           shared_context.use_count);
	++shared_context.use_count;
	ms_mutex_unlock(&shared_context_lock);
	return shared_context.process_thread;
}

static bool_t msogl_release_worker(MSWorkerThread *worker) {
	ms_mutex_lock(&shared_context_lock);
	if (worker != shared_context.process_thread) {
		ms_error("[MSOGL] worker thread mismatch.");
		ms_mutex_unlock(&shared_context_lock);
		return FALSE;
	}
	--shared_context.use_count;
	if (shared_context.use_count == 0) {
		ms_message("[MSOGL] Marking for destroying rendering thread: %p", shared_context.process_thread);
		// ms_worker_thread_destroy(shared_context.process_thread, FALSE);
		ms_mutex_unlock(&shared_context_lock);
		return TRUE;
	} else {
		ms_message("[MSOGL] Marking for not destroying rendering thread: %p:%d", shared_context.process_thread,
		           shared_context.use_count);
		ms_mutex_unlock(&shared_context_lock);
		return FALSE;
	}
}

/***********************************************************************************/

static int ogl_call_render(MSFilter *f, void *arg);

static bool_t msogl_destroy_opengl(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	ms_debug("[MSOGL] msogl_destroy_opengl");
	ogl_display_uninit(data->display, TRUE);
	ogl_display_free_and_nullify(&data->display);
	return TRUE;
}

static bool_t msogl_full_destroy_opengl(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	ms_debug("[MSOGL] msogl_full_destroy_opengl");
	ogl_display_terminate(data->display);
	ogl_display_free_and_nullify(&data->display);
	return TRUE;
}

static bool_t msogl_rendering(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	if (data->video_mode != (unsigned long)MS_FILTER_VIDEO_NONE) ogl_call_render(f, data);
	return TRUE;
}
#endif

// =============================================================================
// Process.
// =============================================================================

static void ogl_init(MSFilter *f) {
	FilterData *data = ms_new0(FilterData, 1);
	memset(&data->functions, 0, sizeof(data->functions));
	data->functions.glInitialized = FALSE;
	data->functions.eglInitialized = FALSE;
	data->functions.loadQtLibs = FALSE;
	data->display = ogl_display_new();
	ogl_display_set_default_functions(data->display, &data->functions);
	data->show_video = TRUE;
	data->mirroring = TRUE;
	data->update_mirroring = FALSE;
	data->prev_inm = NULL;

	data->video_mode = (unsigned long)MS_FILTER_VIDEO_NONE;
	data->context_info.width = MS_VIDEO_SIZE_CIF_W;
	data->context_info.height = MS_VIDEO_SIZE_CIF_H;
	data->context_info.window = NULL;
	data->mode = MSVideoDisplayBlackBars;
	data->fps = 30.0;
#ifdef MS2_WINDOWS_UWP
	data->process_stopped = TRUE;
	data->process_timer = nullptr;
	data->current_task = concurrency::task_from_result();
	data->process_period.Duration = (__int64)(10000000 / data->fps); // 30 FPS
	data->parent_filter = f;
	ms_mutex_init(&data->current_task_lock, NULL);
#else
	data->process_thread = msogl_get_worker();
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
	if (data->video_mode == (unsigned long)MS_FILTER_VIDEO_AUTO && data->context_info.window) {
		ogl_destroy_window((EGLNativeWindowType *)&data->context_info.window, &data->window_id);
	}

	ms_mutex_unlock(&gLock);
#ifdef MS2_WINDOWS_UWP
	data->process_stopped = TRUE;
	ms_mutex_destroy(&data->current_task_lock);
#endif
	ms_free(data);
}

//---------------------------------------------------------------------------------------------------
#ifndef MS2_WINDOWS_UWP
static void ogl_uninit(MSFilter *f) {
	ms_filter_lock(f);
	FilterData *data = (FilterData *)f->data;
	MSTask *task;

	bool_t toDestroy = msogl_release_worker(data->process_thread);
	if (toDestroy) {
		ms_message("[MSOGL]:%p msogl_uninit: prepare for full destroy in thread %lx", f, ms_thread_self());
		task =
		    ms_worker_thread_add_waitable_task(data->process_thread, (MSTaskFunc)msogl_full_destroy_opengl, (void *)f);
	} else {
		ms_message("[MSOGL]:%p msogl_uninit: prepare for simple destroy in thread %lx. %d left", f, ms_thread_self(),
		           shared_context.use_count);
		task = ms_worker_thread_add_waitable_task(data->process_thread, (MSTaskFunc)msogl_destroy_opengl, (void *)f);
	}
	ms_task_wait_completion(task);
	ms_task_destroy(task);
	ms_message("[MSOGL]:%p msogl_uninit: %s renderer in thread %lx. %d left", f,
	           (toDestroy ? "destroying" : "releasing"), ms_thread_self(), shared_context.use_count);
	if (toDestroy) ms_worker_thread_destroy(shared_context.process_thread, FALSE);
	ogl_uninit_data(data);
	ms_filter_unlock(f);
}
#endif

static void ogl_preprocess(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	ms_filter_lock(f);
	if (data->show_video) {
		if (data->video_mode == (unsigned long)MS_FILTER_VIDEO_AUTO && !data->context_info.window) {
			ms_mutex_lock(&gLock);
			ogl_create_window((EGLNativeWindowType *)&data->context_info.window, &data->window_id);
			ms_mutex_unlock(&gLock);
			data->update_context = UPDATE_CONTEXT_UPDATE;
			data->context_info.width = MS_VIDEO_SIZE_CIF_W;
			data->context_info.height = MS_VIDEO_SIZE_CIF_H;
		}
	}
#ifndef MS2_WINDOWS_UWP
	data->refresh_task = ms_worker_thread_add_repeated_task(data->process_thread, (MSTaskFunc)msogl_rendering, f,
	                                                        MIN((int)(1000.0 / data->fps), 333));
#endif
	ms_filter_unlock(f);
}

#ifndef MS2_WINDOWS_UWP
static void ogl_postprocess(MSFilter *f) {
	ms_filter_lock(f);
	FilterData *data = (FilterData *)f->data;
	if (data->refresh_task) ms_task_cancel_and_destroy(data->refresh_task);
	data->refresh_task = NULL;
	ms_filter_unlock(f);
}
#endif
//---------------------------------------------------------------------------------------------------
static void ogl_process(MSFilter *f) {

	ms_filter_lock(f);
	FilterData *data = (FilterData *)f->data;
	if (!data->display) goto end;
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

static bool_t msogl_set_native_window_id(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	MSOglContextInfo *context_info;
	unsigned long video_mode = *((unsigned long *)data->requested_window_id);
	ms_filter_lock(f);

	context_info = *((MSOglContextInfo **)data->requested_window_id);
	if (video_mode != (unsigned long)MS_FILTER_VIDEO_NONE) {
		ms_message("[MSOGL]:%p Set native window id: %p NativeID=[%lu => %lu] on %p", f, (void *)context_info,
		           data->video_mode, video_mode, data->display);
		if (video_mode == (unsigned long)MS_FILTER_VIDEO_AUTO) { // Create a new Window
			if (
#ifdef MS2_WINDOWS_UWP
			    !data->process_stopped &&
#endif
			    !data->context_info.window) {
				ms_mutex_lock(&gLock);
				ogl_create_window((EGLNativeWindowType *)&data->context_info.window, &data->window_id);
				ms_mutex_unlock(&gLock);
			}
			if (data->video_mode != (unsigned long)MS_FILTER_VIDEO_AUTO &&
			    data->video_mode != (unsigned long)MS_FILTER_VIDEO_NONE)
				data->update_context = UPDATE_CONTEXT_DISPLAY_UNINIT;
			else data->update_context = UPDATE_CONTEXT_UPDATE;
			data->context_info.width = MS_VIDEO_SIZE_CIF_W;
			data->context_info.height = MS_VIDEO_SIZE_CIF_H;
			data->video_mode = video_mode;
		} else if (data->context_info.getProcAddress != context_info->getProcAddress ||
		           (context_info->window && data->context_info.window != context_info->window) ||
		           (!context_info->window && (data->context_info.width != context_info->width ||
		                                      data->context_info.height != context_info->height))) {
			ms_message("[MSOGL]:%p Use native window : %p, win=%p size=%dx%d on %p", f, (void *)context_info,
			           (void *)context_info->window, context_info->width, context_info->height, data->display);
			data->functions.getProcAddress = context_info->getProcAddress;
			data->context_info = *context_info;
			data->update_context = UPDATE_CONTEXT_DISPLAY_UNINIT;
			data->video_mode = video_mode;
		}
	} else if (data->video_mode != (unsigned long)MS_FILTER_VIDEO_NONE) {
		ms_message("[MSOGL]:%p Unset native window id: NativeID=[%lu => %lu] on %p", f, data->video_mode, video_mode,
		           data->display);
		data->update_context = UPDATE_CONTEXT_DISPLAY_UNINIT;
		if (data->video_mode == (unsigned long)MS_FILTER_VIDEO_AUTO && data->context_info.window)
			ogl_destroy_window((EGLNativeWindowType *)&data->context_info.window, &data->window_id);
		memset(&data->context_info, 0, sizeof data->context_info);
		data->video_mode = video_mode;
	}
	ms_filter_unlock(f);
	return TRUE;
}

static int ogl_set_native_window_id(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	data->requested_window_id = arg;
#ifdef MS2_WINDOWS_UWP
	return msogl_set_native_window_id(f);
#else
	// Use waitable task to avoid implement special structure to manage arg and ensure persistent memory coming from
	// outside.
	MSTask *task = ms_worker_thread_add_waitable_task(data->process_thread, (MSTaskFunc)msogl_set_native_window_id, f);
	ms_task_wait_completion(task);
	ms_task_destroy(task);
	return 0;
#endif
}

static int ogl_get_native_window_id(MSFilter *f, void *arg) {
	FilterData *s = (FilterData *)f->data;
	MSOglContextInfo **id = (MSOglContextInfo **)arg;
	*id = &s->context_info;

	return 0;
}

static int ogl_create_native_window_id(BCTBX_UNUSED(MSFilter *f), void *arg) {
	MSOglContextInfo *id = (MSOglContextInfo **)arg ? *(MSOglContextInfo **)arg : ms_new0(MSOglContextInfo, 1);
#ifdef MS2_WINDOWS_UWP
	Platform::Agile<CoreApplicationView> window_id;
#else
	void *window_id = NULL;
#endif
	ogl_create_window((EGLNativeWindowType *)&id->window, &window_id);
	id->width = MS_VIDEO_SIZE_CIF_W;
	id->height = MS_VIDEO_SIZE_CIF_H;

	*(MSOglContextInfo **)arg = id;

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
		if ((data->update_context & UPDATE_CONTEXT_DISPLAY_UNINIT) == UPDATE_CONTEXT_DISPLAY_UNINIT ||
		    (context_info && context_info->window)) {
			ogl_display_uninit(data->display, TRUE);
		}
		if (context_info) {
			if (context_info->window) {
				// Window is set : do EGL initialization from it
				ms_message("[MSOGL]:%p Auto init on %p", f, data->display);
				ogl_display_auto_init(data->display, &data->functions, (EGLNativeWindowType)context_info->window,
				                      context_info->width, context_info->height);
			} else {
				// Just use input size as it is needed for viewport
				ms_message("[MSOGL]:%p Init on %p", f, data->display);
				ogl_display_init(data->display, &data->functions, context_info->width, context_info->height);
			}
		}
		data->update_context = UPDATE_CONTEXT_NOTHING;
	}
	if (data->show_video && context_info && data->display &&
	    (context_info->window || (!context_info->window && context_info->width && context_info->height))) {
		int status = ogl_display_make_current(data->display, TRUE);
		if (status == -1) {
			ms_warning("[MSOGL] Failed to make EGLSurface current");
		} else if (status >= 0) {
			ogl_display_render(data->display, 0, data->mode);
		}
	}
	if (data->display) ogl_display_notify_errors(data->display, f);
	ms_mutex_unlock(&gLock);
	if (f != NULL) ms_filter_unlock(f);
	return 0;
}

//---------------------------------------------------------------------------------------------------

static int ms_ogl_display_set_egl_target_contexts(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	ogl_display_set_target_context(data->display, (const MSEGLContextDescriptor *)arg);
	return 0;
}

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
	ms_mutex_lock(&data->current_task_lock);
	data->current_task = data->current_task.then(functor);
	ms_mutex_unlock(&data->current_task_lock);
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
	// Wait on concurrency cannot be call from Ui thread or we get an invalid_operation exception.
	// If waiting is needed to clean correctly data without being disrupted from other instances
	// , we may need to hack a wait procedure.
	// data->current_task.wait(); // The uninit is a final state. Wait for completion.
}

static task<void> delay_process_uwp(FilterData *data) {
	// Called from current_task : it is safe to check stopping_process
	if (data->process_stopped) return task_from_result();
	else
		return create_task([data]() {
			       if (!data->process_stopped)
				       // Keep data to check directly if we are stopped instead of checking F that can be freed by
				       // mediastreamer
				       run_ui_task_uwp([data]() {
					       if (!data->process_stopped) ogl_call_render(data->parent_filter, data);
				       });
			       return task_from_result();
		       })
		    .then([data]() {
			    if (!data->process_stopped) {
				    data->process_timer = ThreadPoolTimer::CreateTimer(
				        ref new TimerElapsedHandler([data](ThreadPoolTimer ^ source) {
					        add_task_uwp(data, [data]() { return delay_process_uwp(data); });
				        }),
				        data->process_period);
			    }
			    return task_from_result();
		    });
}

static void ogl_preprocess_uwp(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	add_task_uwp(data, [data]() {
		ogl_preprocess(data->parent_filter);
		run_ui_task_uwp([data]() {
			data->process_timer =
			    ThreadPoolTimer::CreateTimer(ref new TimerElapsedHandler([data](ThreadPoolTimer ^ source) {
				                                 add_task_uwp(data, [data]() {
					                                 data->process_stopped = FALSE;
					                                 return delay_process_uwp(data);
				                                 });
			                                 }),
			                                 data->process_period);
		});
		return task_from_result();
	});
}

static int ogl_call_render_uwp(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	auto param = (FilterData *)arg;

	// Do not use 'arg' because of scope variable (should not be really an issue here because arg is not used when this
	// function is called)
	add_task_uwp(data, [data, param]() {
		run_ui_task_uwp([data, param]() {
			if (!data->process_stopped) ogl_call_render(data->parent_filter, (void *)&param);
		});
		return task_from_result();
	});
	return 0;
}

static void ogl_postprocess_uwp(MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	data->process_stopped = TRUE;
	if (data->process_timer) data->process_timer->Cancel();
}

//------------------------------------------------------------------------------

static int ogl_set_native_window_id_uwp(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	MSOglContextInfo *context_info = *((MSOglContextInfo **)arg);
	MSOglContextInfo context_info_data; // Store copy to keep scope from threads
	unsigned long video_mode = *(unsigned long *)arg == MS_FILTER_VIDEO_NONE   ? MS_FILTER_VIDEO_NONE
	                           : *(unsigned long *)arg == MS_FILTER_VIDEO_AUTO ? MS_FILTER_VIDEO_AUTO
	                                                                           : 1;

	if (context_info && video_mode == 1) {
		context_info_data = *context_info;
	}
	ms_message("[MSOGL] Set uwp window id : %p", (void *)context_info);
	// Do not use 'arg' because of scope variable
	add_task_uwp(data, [f, data, context_info, context_info_data, video_mode]() {
		if (context_info && video_mode == 1) {
			auto context_info_ref = &context_info_data;
			ogl_set_native_window_id(data->parent_filter, (void *)&context_info_ref);
		} else {
			ogl_set_native_window_id(data->parent_filter, (void *)&context_info);
		}
		return task_from_result();
	});
	return 0;
}

static int ms_ogl_display_set_egl_target_contexts_uwp(MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	auto param = *(const MSEGLContextDescriptor **const)arg;

	// Do not use 'arg' because of scope variable
	add_task_uwp(data, [data, param]() {
		run_ui_task_uwp([data, param]() {
			if (!data->process_stopped) ms_ogl_display_set_egl_target_contexts(data->parent_filter, (void *)&param);
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
                                   {MS_VIDEO_DISPLAY_CREATE_NATIVE_WINDOW_ID, ogl_create_native_window_id},
                                   {MS_VIDEO_DISPLAY_SHOW_VIDEO, ogl_show_video},
                                   {MS_VIDEO_DISPLAY_ZOOM, ogl_zoom},
                                   {MS_VIDEO_DISPLAY_ENABLE_MIRRORING, ogl_enable_mirroring},
                                   {MS_VIDEO_DISPLAY_SET_MODE, ogl_set_mode},
                                   {MS_OGL_RENDER, ogl_call_render_uwp},
                                   {MS_OGL_DISPLAY_SET_EGL_TARGET_CONTEXT, ms_ogl_display_set_egl_target_contexts_uwp},
                                   {0, NULL}};
MSFilterDesc ms_ogl_desc = {MS_OGL_ID,                        // id
                            "MSOGL",                          // name
                            N_("OpenGL/ES display via EGL."), // text
                            MS_FILTER_OTHER,                  // category
                            NULL,                             // enc_fmt
                            2,                                // ninputs
                            0,                                // noutputs
                            ogl_init,                         // init
                            ogl_preprocess_uwp,               // preprocess
                            ogl_process,                      // process
                            ogl_postprocess_uwp,              // postprocess
                            ogl_uninit_uwp,                   // uninit
                            methods};
#else
static MSFilterMethod methods[] = {{MS_FILTER_SET_VIDEO_SIZE, ogl_set_video_size},
                                   {MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, ogl_set_native_window_id},
                                   {MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, ogl_get_native_window_id},
                                   {MS_VIDEO_DISPLAY_CREATE_NATIVE_WINDOW_ID, ogl_create_native_window_id},
                                   {MS_VIDEO_DISPLAY_SHOW_VIDEO, ogl_show_video},
                                   {MS_VIDEO_DISPLAY_ZOOM, ogl_zoom},
                                   {MS_VIDEO_DISPLAY_ENABLE_MIRRORING, ogl_enable_mirroring},
                                   {MS_VIDEO_DISPLAY_SET_MODE, ogl_set_mode},
                                   {MS_OGL_RENDER, ogl_call_render},
                                   {MS_OGL_DISPLAY_SET_EGL_TARGET_CONTEXT, ms_ogl_display_set_egl_target_contexts},
                                   {0, NULL}};
MSFilterDesc ms_ogl_desc = {
    MS_OGL_ID,                        // id
    "MSOGL",                          // name
    N_("OpenGL/ES display via EGL."), // text
    MS_FILTER_OTHER,                  // category
    NULL,                             // enc_fmt
    2,                                // ninputs
    0,                                // noutputs
    ogl_init,                         // init
    ogl_preprocess,                   // preprocess
    ogl_process,                      // process
    ogl_postprocess,                  // postprocess
    ogl_uninit,                       // uninit
    methods                           // methods
};
#endif
#ifdef __cplusplus
} // extern "C"
#endif
MS_FILTER_DESC_EXPORT(ms_ogl_desc)
