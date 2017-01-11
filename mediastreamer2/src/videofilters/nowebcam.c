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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/mscommon.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mswebcam.h"
#include "../voip/nowebcam.h"


#if __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif

#define FF_INPUT_BUFFER_PADDING_SIZE 32

#if LIBAVCODEC_VERSION_MAJOR >= 57

#ifdef _MSC_VER
#pragma warning(disable : 4996)
#else
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#endif

#if TARGET_OS_IPHONE
#include <CoreGraphics/CGDataProvider.h>
#include <CoreGraphics/CGImage.h>
#include <CoreGraphics/CGContext.h>
#include <CoreGraphics/CGBitmapContext.h>
#endif

#include <sys/stat.h>

static mblk_t *_ms_load_jpeg_as_yuv(const char *jpgpath, MSVideoSize *reqsize) {
#if defined(_WIN32)
	mblk_t *m = NULL;
	DWORD st_sizel;
	DWORD st_sizeh;
	uint8_t *jpgbuf;
	DWORD err;
	HANDLE fd;
#ifdef UNICODE
	WCHAR wUnicode[1024];
	MultiByteToWideChar(CP_UTF8, 0, jpgpath, -1, wUnicode, 1024);
#else
	const char *wUnicode = jpgpath;
#endif
#ifndef MS2_WINDOWS_DESKTOP
	fd = CreateFile2(wUnicode, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, NULL);
#else
	fd = CreateFile(wUnicode, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
#endif
	if (fd == INVALID_HANDLE_VALUE) {
		ms_error("Failed to open %s",jpgpath);
		return NULL;
	}
	st_sizel = 0;
	st_sizeh = 0;
#ifndef MS2_WINDOWS_DESKTOP
	{
		WIN32_FILE_ATTRIBUTE_DATA attr_data;
		GetFileAttributesEx(wUnicode, GetFileExInfoStandard, &attr_data);
		st_sizel = attr_data.nFileSizeLow;
		st_sizeh = attr_data.nFileSizeHigh;
	}
#else
	st_sizel = GetFileSize(fd, &st_sizeh);
#endif
	if (st_sizeh > 0 || st_sizel <= 0) {
		CloseHandle(fd);
		ms_error("Can't load file %s",jpgpath);
		return NULL;
	}
	jpgbuf=(uint8_t*)ms_malloc0(st_sizel);
	if (jpgbuf == NULL) {
		CloseHandle(fd);
		ms_error("Cannot allocate buffer for %s", jpgpath);
		return NULL;
	}
	err = 0;
	ReadFile(fd, jpgbuf, st_sizel, &err, NULL) ;

	if (err != st_sizel) {
		  ms_error("Could not read as much as wanted !");
	}
	m = jpeg2yuv(jpgbuf, st_sizel, reqsize);
	ms_free(jpgbuf);
	if (m == NULL) {
		CloseHandle(fd);
		ms_error("Cannot load image from buffer for %s", jpgpath);
		return NULL;
	}
	CloseHandle(fd);
	return m;
#else
	mblk_t *m = NULL;
	struct stat statbuf;
	uint8_t *jpgbuf;
	int err;
	int fd = open(jpgpath, O_RDONLY);

	if (fd != -1) {
		fstat(fd, &statbuf);
		if (statbuf.st_size <= 0) {
			close(fd);
			ms_error("Cannot load %s", jpgpath);
			return NULL;
		}
		jpgbuf=(uint8_t*)ms_malloc0(statbuf.st_size + FF_INPUT_BUFFER_PADDING_SIZE);
		if (jpgbuf == NULL) {
			close(fd);
			ms_error("Cannot allocate buffer for %s", jpgpath);
			return NULL;
		}
		err = read(fd,jpgbuf,statbuf.st_size);
		if (err != statbuf.st_size) {
			ms_error("Could not read as much as wanted: %i<>%li !", err, (long)statbuf.st_size);
		}
		m = jpeg2yuv(jpgbuf,statbuf.st_size,reqsize);
		ms_free(jpgbuf);
		if (m == NULL) {
			close(fd);
			ms_error("Cannot load image from buffer for %s", jpgpath);
			return NULL;
		}
	} else {
		ms_error("Cannot load %s", jpgpath);
		return NULL;
	}
	close(fd);
	return m;
#endif
}

static mblk_t * generate_black_yuv_frame(MSVideoSize *reqsize) {
	MSPicture dest;
	mblk_t *m = ms_yuv_buf_alloc(&dest, reqsize->width, reqsize->height);
	int ysize = dest.w * dest.h;
	int usize = ysize / 4;
	memset(dest.planes[0], 16, ysize);
	memset(dest.planes[1], 128, usize);
	memset(dest.planes[2], 128, usize);
	return m;
}

mblk_t *ms_load_jpeg_as_yuv(const char *jpgpath, MSVideoSize *reqsize) {
	mblk_t *m = NULL;
	if (jpgpath != NULL) {
		m = _ms_load_jpeg_as_yuv(jpgpath, reqsize);
	}
	if (m == NULL) m = generate_black_yuv_frame(reqsize);
	return m;
}


#ifndef PACKAGE_DATA_DIR
#define PACKAGE_DATA_DIR "share"
#endif

#ifndef NOWEBCAM_JPG
#define NOWEBCAM_JPG "nowebcamCIF"
#endif

static char *def_image = NULL;

static const char *def_image_path = PACKAGE_DATA_DIR "/images/" NOWEBCAM_JPG ".jpg";


mblk_t *ms_load_nowebcam(MSVideoSize *reqsize, int idx) {
	char tmp[256];
	if (idx < 0)
		snprintf(tmp, sizeof(tmp), "%s/images/%s.jpg", PACKAGE_DATA_DIR, NOWEBCAM_JPG);
	else
		snprintf(tmp, sizeof(tmp), "%s/images/%s%i.jpg", PACKAGE_DATA_DIR, NOWEBCAM_JPG, idx);
	return ms_load_jpeg_as_yuv(tmp, reqsize);
}

typedef struct _SIData {
	MSVideoSize vsize;
	char *nowebcamimage;
	uint64_t lasttime;
	float fps;
	mblk_t *pic;
}SIData;

void static_image_init(MSFilter *f) {
	SIData *d = (SIData*)ms_new0(SIData,1);
	d->vsize.width = MS_VIDEO_SIZE_CIF_W;
	d->vsize.height = MS_VIDEO_SIZE_CIF_H;

	if (def_image)
		d->nowebcamimage=ms_strdup(def_image);
	d->lasttime = 0;
	d->pic = NULL;
	d->fps = 1;
	f->data = d;
}

void static_image_uninit(MSFilter *f) {
	SIData *d = (SIData*)f->data;
	if (d->nowebcamimage) ms_free(d->nowebcamimage);
	ms_free(d);
}

void static_image_preprocess(MSFilter *f) {
	SIData *d = (SIData*)f->data;
	if (d->pic == NULL) {
		d->pic = ms_load_jpeg_as_yuv(d->nowebcamimage, &d->vsize);
	}
}

void static_image_process(MSFilter *f) {
	SIData *d = (SIData*)f->data;
	uint64_t frame_interval = (uint64_t)(1000/d->fps);
	/*output a frame whenever needed, i.e. respect the FPS parameter */
	if ((f->ticker->time - d->lasttime>frame_interval) || d->lasttime == 0) {
		ms_mutex_lock(&f->lock);
		if (d->pic) {
			mblk_t *o = dupmsg(d->pic);
			/*prevent mirroring at the output*/
			mblk_set_precious_flag(o,1);
			ms_queue_put(f->outputs[0],o);
		}
		ms_filter_unlock(f);
		d->lasttime = f->ticker->time;
	}
}

void static_image_postprocess(MSFilter *f) {
	SIData *d = (SIData*)f->data;
	if (d->pic) {
		freemsg(d->pic);
		d->pic = NULL;
	}
}

static int static_image_set_fps(MSFilter *f, void *arg) {
	SIData *d = (SIData*)f->data;
	d->fps = *((float*)arg);
	d->lasttime = 0;
	return 0;
}

static int static_image_get_fps(MSFilter *f, void *arg) {
	SIData *d = (SIData*)f->data;
	*((float*)arg) = d->fps;
	return 0;
}

int static_image_set_vsize(MSFilter *f, void* data) {
#ifndef NO_FFMPEG
	SIData *d = (SIData*)f->data;
	d->vsize = *(MSVideoSize*)data;
#else
    // no rescaling without ffmpeg
#endif
	return 0;
}

int static_image_get_vsize(MSFilter *f, void* data) {
	SIData *d = (SIData*)f->data;
	static_image_preprocess(f);
	*(MSVideoSize*)data = d->vsize;
	return 0;
}

int static_image_get_pix_fmt(MSFilter *f, void *data) {
	*(MSPixFmt*)data = MS_YUV420P;
	return 0;
}

static int static_image_set_image(MSFilter *f, void *arg) {
	SIData *d = (SIData*)f->data;
	const char *image = (const char *)arg;
	ms_filter_lock(f);

	if (d->nowebcamimage) {
		ms_free(d->nowebcamimage);
		d->nowebcamimage = NULL;
	}

	if (image != NULL && image[0] != '\0')
		d->nowebcamimage = ms_strdup(image);

	if (d->pic != NULL) {
		/* Get rid of the old image and force a new preprocess so that the
			 new image is properly read. */
		freemsg(d->pic);
		d->pic = NULL;
	}
	d->lasttime = 0;
	static_image_preprocess(f);

	ms_filter_unlock(f);
	return 0;
}

MSFilterMethod static_image_methods[] = {
	{	MS_FILTER_SET_FPS,	static_image_set_fps	},
	{	MS_FILTER_GET_FPS,	static_image_get_fps	},
	{	MS_FILTER_SET_VIDEO_SIZE, static_image_set_vsize },
	{	MS_FILTER_GET_VIDEO_SIZE, static_image_get_vsize },
	{	MS_FILTER_GET_PIX_FMT, static_image_get_pix_fmt },
	{	MS_STATIC_IMAGE_SET_IMAGE, static_image_set_image },
	{	0,0 }
};

MSFilterDesc ms_static_image_desc = {
	MS_STATIC_IMAGE_ID,
	"MSStaticImage",
	"A filter that outputs a static image.",
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	static_image_init,
	static_image_preprocess,
	static_image_process,
	static_image_postprocess,
	static_image_uninit,
	static_image_methods,
	0
};

MS_FILTER_DESC_EXPORT(ms_static_image_desc)

static void static_image_detect(MSWebCamManager *obj);

static void static_image_cam_init(MSWebCam *cam) {
	cam->name=ms_strdup("Static picture");

	if (def_image == NULL)
		def_image = ms_strdup(def_image_path);
}



static MSFilter *static_image_create_reader(MSWebCam *obj){
		return ms_factory_create_filter_from_desc(ms_web_cam_get_factory(obj), &ms_static_image_desc);
}
MSWebCamDesc static_image_desc={
	"StaticImage",
	&static_image_detect,
	&static_image_cam_init,
	&static_image_create_reader,
	NULL,
	NULL
};

static void static_image_detect(MSWebCamManager *obj){
	MSWebCam *cam=ms_web_cam_new(&static_image_desc);
	ms_web_cam_manager_add_cam(obj,cam);
}

void ms_static_image_set_default_image(const char *path){
	if (def_image!=NULL)
		ms_free(def_image);
	def_image=NULL;
	if (path)
		def_image=ms_strdup(path);
}

const char *ms_static_image_get_default_image(){
	return def_image;
}

#if __clang__
#pragma clang diagnostic pop
#endif
