/*
 * Copyright (c) 2010-2021 Belledonne Communications SARL.
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

// Windows Redefinitions error
#ifdef _WIN32
#include <winsock2.h>
#endif
#include "msqogl.h"
#include <QOpenGLFramebufferObjectFormat>
#include <QThread>
#include <QQuickWindow>

#include "mediastreamer2/msvideo.h"

// Based on generic_opengl_display.c
// =============================================================================

BufferRenderer::BufferRenderer () {
}

BufferRenderer::~BufferRenderer () {
	qInfo() << QStringLiteral("Delete Renderer");
	if(mParent){
		ms_filter_lock(mParent->parent);
		if( mParent->renderer == this)// Check if it is the same object. This deletion could be delayed for any reasons (Managed by Qt). We don't want to remove a new created object.
			mParent->renderer = NULL;
		ms_filter_unlock(mParent->parent);
	}
}

QOpenGLFramebufferObject *BufferRenderer::createFramebufferObject (const QSize &size) {
	QOpenGLFramebufferObjectFormat format;
	format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
	format.setInternalTextureFormat(GL_RGBA8);
	format.setSamples(4);
	
	mWidth = size.width();
	mHeight = size.height();
	mParent->update_context = TRUE;
	
	return new QOpenGLFramebufferObject(size, format);
}

static int qogl_call_render (MSFilter *f, void *arg);
void BufferRenderer::render () {
	// Draw with ms filter.
	if(mParent->parent){
		qogl_call_render(mParent->parent, NULL);
		// Synchronize opengl calls with QML.
		if (mWindow)
			mWindow->resetOpenGLState();
	}
}

void BufferRenderer::synchronize (QQuickFramebufferObject *item) {
	// No mutex needed here. It's a synchronized area.
	mWindow = item->window();
}

// =============================================================================
// Process.
// =============================================================================

void * getProcAddress(const char * name){
	return (void*)QOpenGLContext::currentContext()->getProcAddress(name);
}
static void qogl_init (MSFilter *f) {
	FilterData *data = ms_new0(FilterData, 1);
	data->display = ogl_display_new();
	data->show_video = TRUE;
	data->mirroring = TRUE;
	data->update_mirroring = FALSE;
	data->prev_inm = NULL;
	data->renderer = NULL;
	data->parent = f;
	memset(&data->functions, 0, sizeof(data->functions));
	data->functions.getProcAddress = getProcAddress;
	
	f->data = data;
}

static void qogl_uninit (MSFilter *f) {
	FilterData *data = (FilterData *)f->data;
	ogl_display_free(data->display);
	if( data->renderer)
		data->renderer->mParent = NULL;
	ms_free(data);
}

static int qogl_call_render (MSFilter *f, void *arg);
static void qogl_process (MSFilter *f) {
	FilterData *data;
	MSPicture src;
	mblk_t *inm;
	
	ms_filter_lock(f);
	
	data = (FilterData *)f->data;
	// No context given or video disabled.
	if ( !data->show_video || !data->renderer)
		goto end;
	if ( f->inputs[0] != NULL &&
			((inm = ms_queue_peek_last(f->inputs[0])) != NULL) &&
			ms_yuv_buf_init_from_mblk(&src, inm) == 0
			) {
		data->video_size.width = src.w;
		data->video_size.height = src.h;
		
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
	
	if (f->inputs[0] != NULL)
		ms_queue_flush(f->inputs[0]);
	
	if (f->inputs[1] != NULL)
		ms_queue_flush(f->inputs[1]);
}

// =============================================================================
// Methods.
// =============================================================================

static int qogl_set_video_size (MSFilter *f, void *arg) {
	ms_filter_lock(f);
	((FilterData *)f->data)->video_size = *(MSVideoSize *)arg;
	ms_filter_unlock(f);
	
	return 0;
}

// if arg is NULL, stop rendering by removing renderer
static int qogl_set_native_window_id (MSFilter *f, void *arg) {
	(void)f;
	FilterData *data;
	
	ms_filter_lock(f);
	
	data = (FilterData *)f->data;
	if( !arg || arg && !(*(QQuickFramebufferObject::Renderer**)arg) ){
		data->renderer = NULL;
	}
	ms_filter_unlock(f);
	return 0;
}

// When we get an Id, we create a new BufferRenderer if it doesn't exist
static int qogl_get_native_window_id (MSFilter *f, void *arg) {
	FilterData *data=(FilterData*)f->data;
	if( !data->renderer){
		data->renderer = new BufferRenderer();
		data->renderer->mParent = data;
		data->update_context = TRUE;
	}	
	*(QQuickFramebufferObject::Renderer**)arg=dynamic_cast<QQuickFramebufferObject::Renderer*>(data->renderer);
	return 0;
}

static int qogl_show_video (MSFilter *f, void *arg) {
	ms_filter_lock(f);
	((FilterData *)f->data)->show_video = *(bool_t *)arg;
	ms_filter_unlock(f);
	return 0;
}

static int qogl_zoom (MSFilter *f, void *arg) {
	ms_filter_lock(f);
	ogl_display_zoom(((FilterData *)f->data)->display, (float*)arg);
	ms_filter_unlock(f);
	return 0;
}

static int qogl_enable_mirroring (MSFilter *f, void *arg) {
	FilterData *data = (FilterData *)f->data;
	ms_filter_lock(f);
	data->mirroring = *(bool_t *)arg;
	// This is a request to update the mirroring flag and it will be honored as soon as a new frame comes in
	data->update_mirroring = TRUE;
	ms_filter_unlock(f);
	return 0;
}

static int qogl_call_render (MSFilter *f, void *arg) {
	
	FilterData *data;
	(void)arg;
	ms_filter_lock(f);
	
	data = (FilterData *)f->data;
	if (data->show_video && data->renderer){
		if (data->update_context) {
			ogl_display_init(data->display, &data->functions, data->renderer->mWidth , data->renderer->mHeight);
			data->update_context = FALSE;
		}
		ogl_display_render(data->display, 0);
	}
	
	ms_filter_unlock(f);
	
	return 0;
}

// =============================================================================
// Register filter.
// =============================================================================

static MSFilterMethod methods[] = {
	{ MS_FILTER_SET_VIDEO_SIZE, qogl_set_video_size },
	{ MS_VIDEO_DISPLAY_SET_NATIVE_WINDOW_ID, qogl_set_native_window_id },
	{ MS_VIDEO_DISPLAY_GET_NATIVE_WINDOW_ID, qogl_get_native_window_id },
	{ MS_VIDEO_DISPLAY_SHOW_VIDEO, qogl_show_video },
	{ MS_VIDEO_DISPLAY_ZOOM, qogl_zoom },
	{ MS_VIDEO_DISPLAY_ENABLE_MIRRORING, qogl_enable_mirroring },
	//{ MS_OGL_RENDER, qogl_call_render }, // qogl_call_render is autocalled by Qt, there is no need to put it in interface
	{ 0, NULL }
};
#ifdef _WIN32
MSFilterDesc ms_qogl_desc = {
	MS_FILTER_PLUGIN_ID,
	"MSQOGL",
	"A Qt opengl video display",
	MS_FILTER_OTHER,
	NULL,
	2,
	0,
	qogl_init,
	NULL,
	qogl_process,
	NULL,
	qogl_uninit,
	methods
};

#else
MSFilterDesc ms_qogl_desc = {
	.id = MS_FILTER_PLUGIN_ID,
	.name = "MSQOGL",
	.text = "A Qt opengl video display",
	.category = MS_FILTER_OTHER,
	.ninputs = 2,
	.noutputs = 0,
	.init = qogl_init,
	.process = qogl_process,
	.uninit = qogl_uninit,
	.methods = methods
};
#endif
#ifndef VERSION
#define VERSION "debug"
#endif

extern "C" Q_DECL_EXPORT void libmsqogl_init(MSFactory* factory) {
	ms_factory_register_filter(factory, &ms_qogl_desc);
	ms_message("libmsqogl " VERSION " plugin loaded");
}
