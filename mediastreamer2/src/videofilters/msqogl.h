/*
 * Copyright (c) 2021 Belledonne Communications SARL.
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

#ifndef _MS_QOGL_H
#define _MS_QOGL_H

#include <QQuickFramebufferObject>

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msogl_functions.h"

#include "opengles_display.h"


class BufferRenderer;
class QQuickWindow;

struct _FilterData {
	BufferRenderer * renderer;// Store the current Renderer. Do not manage memory as it is done by Qt
	OpenGlFunctions functions;
	
	struct opengles_display *display;
	
	MSVideoSize video_size; // Not used at this moment.
	
	bool_t show_video;
	bool_t mirroring;
	bool_t update_mirroring;
	bool_t update_context;
	
	mblk_t * prev_inm;
	MSFilter *parent;// Used to call render with the Filter in order to use lock mecanisms
};
typedef struct _FilterData FilterData;

class BufferRenderer : public QQuickFramebufferObject::Renderer {
public:
	BufferRenderer ();
	virtual ~BufferRenderer ();
	
	int mWidth, mHeight;// Frame sizes, requested by Qt
	FilterData * mParent;// Allow the renderer to call filter functions
	
protected:
	QOpenGLFramebufferObject *createFramebufferObject (const QSize &size) override;
	void render () override;
	void synchronize (QQuickFramebufferObject *item) override;

private:
	QQuickWindow *mWindow = nullptr;
};


#endif //_MS_QOGL_H
