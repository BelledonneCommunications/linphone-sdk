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

#ifdef HAVE_LINUX_VIDEODEV2_H

#define POSSIBLE_FORMATS_COUNT 4

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <string.h>
#include <sys/mman.h>
#include <poll.h>

#include <linux/videodev2.h>

#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/mswebcam.h"

#ifdef HAVE_LIBV4L2
#include <libv4l2.h>
#else

#define v4l2_open open
#define v4l2_close close
#define v4l2_mmap mmap
#define v4l2_munmap munmap
#define v4l2_ioctl ioctl

#endif


static void inc_ref(mblk_t*m){
	dblk_ref(m->b_datap);
	if (m->b_cont)
		inc_ref(m->b_cont);
}

static void dec_ref(mblk_t *m){
	if (m->b_cont)
		dec_ref(m->b_cont);
	dblk_unref(m->b_datap);
}

typedef struct V4l2State{
	int fd;
	ms_thread_t thread;
	bool_t thread_run;
	queue_t rq;
	ms_mutex_t mutex;
	char *dev;
	char *mmapdbuf;
	int msize;/*mmapped size*/
	MSVideoSize vsize;
	MSVideoSize got_vsize;
	int pix_fmt;
	int picture_size;
	mblk_t *frames[VIDEO_MAX_FRAME];
	int frame_ind;
	int frame_max;
	float fps;
	unsigned int start_time;
	MSAverageFPS avgfps;
	int th_frame_count;
	int queued;
	bool_t configured;
}V4l2State;

static int msv4l2_open(V4l2State *s){
	int fd=v4l2_open(s->dev,O_RDWR|O_NONBLOCK);
	if (fd==-1){
		ms_error("Could not open %s: %s",s->dev,strerror(errno));
		return -1;
	}
	s->fd=fd;
	return 0;
}

static int msv4l2_close(V4l2State *s){
	if (s->fd!=-1){
		v4l2_close(s->fd);
		s->fd=-1;
		s->configured=FALSE;
	}
	return 0;
}

static bool_t v4lv2_try_format( int fd, struct v4l2_format *fmt, int fmtid){
	fmt->type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	fmt->fmt.pix.pixelformat = fmtid;
	fmt->fmt.pix.field = V4L2_FIELD_ANY;

	if (v4l2_ioctl (fd, VIDIOC_TRY_FMT, fmt)<0){
		ms_message("VIDIOC_TRY_FMT: %s",strerror(errno));
		return FALSE;
	}
	if((int)fmt->fmt.pix.pixelformat != fmtid) {
		ms_message("VIDIOC_TRY_FMT: got different format");
		return FALSE;
	}
	if (v4l2_ioctl (fd, VIDIOC_S_FMT, fmt)<0){
		ms_message("VIDIOC_S_FMT: %s",strerror(errno));
		return FALSE;
	}
	return TRUE;
}

static int get_picture_buffer_size(MSPixFmt pix_fmt, int w, int h){
	switch(pix_fmt){
		case MS_YUV420P:
			return (w*h*3)/2;
		break;
		case MS_RGB24:
			return w*h*3;
		break;
		case MS_YUYV:
			return w*h*2;
		break;
		default:
			return 0;
	}
	return 0;
}

#ifdef VIDIOC_ENUM_FRAMEINTERVALS

static int query_max_fps_for_format_resolution(int fd, int pixelformat, MSVideoSize vsize) {
	int fps = -1;
	struct v4l2_frmivalenum frmival = { 0 };
	frmival.index = 0;
	frmival.pixel_format = pixelformat;
	frmival.width = vsize.width;
	frmival.height = vsize.height;

	while (v4l2_ioctl(fd, VIDIOC_ENUM_FRAMEINTERVALS, &frmival) >= 0) {
		if (frmival.type == V4L2_FRMIVAL_TYPE_DISCRETE) {
			fps = MAX(fps, (int) (frmival.discrete.denominator / frmival.discrete.numerator));
		} else if (frmival.type == V4L2_FRMIVAL_TYPE_STEPWISE) {
			return (int) (frmival.stepwise.max.denominator / frmival.stepwise.max.numerator);
		} else if (frmival.type == V4L2_FRMIVAL_TYPE_CONTINUOUS) {
			return (int) (frmival.stepwise.min.denominator / frmival.stepwise.min.numerator);
		}
		frmival.index++;
	}
	return fps;
}

#endif

typedef struct _V4L2FormatDescription {
	/* format */
	int pixel_format;
	/* max fps */
	int max_fps;
	/* native or emulated */
	bool_t native;
	/* compressed or not */
	bool_t compressed;
	/*format is supported*/
	bool_t supported;

} V4L2FormatDescription;

static MSPixFmt v4l2_format_to_ms(int v4l2format) {
	switch (v4l2format) {
		case V4L2_PIX_FMT_YUV420:
			return MS_YUV420P;
		case V4L2_PIX_FMT_YUYV:
			return MS_YUYV;
		case V4L2_PIX_FMT_MJPEG:
			return MS_MJPEG;
		case V4L2_PIX_FMT_RGB24:
			return MS_RGB24;
		default:
			ms_error("Unknown v4l2 format 0x%08x", v4l2format);
			return MS_PIX_FMT_UNKNOWN;
	}
}

static const V4L2FormatDescription* query_format_description_for_size(int fd, MSVideoSize vsize) {
	/* hardcode supported format in preferred order*/
	static V4L2FormatDescription formats[POSSIBLE_FORMATS_COUNT];
	int i=0;

	memset(formats,0,sizeof(formats));

	formats[i].pixel_format = V4L2_PIX_FMT_YUV420;
	formats[i].max_fps = -1;
	i++;
	/* we must avoid YUYV (and actually any YUV format different than YUV420P) because the pixel converter/scaler implementation
	 * of ffmpeg is not optimized for arm. So we need to prefer YUV420P if directly available or MJPEG*/
#ifndef __arm__
	formats[i].pixel_format = V4L2_PIX_FMT_YUYV;
	formats[i].max_fps = -1;
	i++;
#endif

	formats[i].pixel_format = V4L2_PIX_FMT_MJPEG;
	formats[i].max_fps = -1;
	i++;

#ifdef __arm__
	formats[i].pixel_format = V4L2_PIX_FMT_YUYV;
	formats[i].max_fps = -1;
	i++;
#endif

	formats[i].pixel_format = V4L2_PIX_FMT_RGB24;
	formats[i].max_fps = -1;
	i++;

	{
		struct v4l2_fmtdesc fmt;
		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		while (v4l2_ioctl(fd, VIDIOC_ENUM_FMT, &fmt) >= 0) {
			for (i=0; i<POSSIBLE_FORMATS_COUNT; i++) {
				if ((int)fmt.pixelformat == formats[i].pixel_format) {
#ifdef VIDIOC_ENUM_FRAMEINTERVALS
					formats[i].max_fps = query_max_fps_for_format_resolution(fd, fmt.pixelformat, vsize);
#endif
#ifdef V4L2_FMT_FLAG_EMULATED
					formats[i].native = !(fmt.flags & V4L2_FMT_FLAG_EMULATED);
#endif
					formats[i].compressed = fmt.flags & V4L2_FMT_FLAG_COMPRESSED;
					formats[i].supported = TRUE;
					ms_message("format %s : max_fps=%i, native=%i, compressed=%i",
						   ms_pix_fmt_to_string(v4l2_format_to_ms(fmt.pixelformat)),
						   formats[i].max_fps,
						   formats[i].native,
						   formats[i].compressed);
					break;
				}
			}
			fmt.index++;
		}
	}
	return formats;
}

MSPixFmt msv4l2_pick_best_format_x86(int fd, const V4L2FormatDescription* format_desc, MSVideoSize vsize, float target_fps) {
	/* rules for picking a format are:
	    - only max_fps >= target_fps images/sec are considered
	    - native > compressed > emulated
	*/
	enum { PREFER_NATIVE = 0, PREFER_COMPRESSED, NO_PREFERENCE} i;
	int j;
	for (i=PREFER_NATIVE; i<=NO_PREFERENCE; i++) {
		for (j=0; j<POSSIBLE_FORMATS_COUNT; j++) {
			int candidate = -1;
			if (!format_desc[j].supported) continue;
			switch (i) {
				case PREFER_NATIVE:
					if (format_desc[j].max_fps >= target_fps && format_desc[j].native)
						candidate = j;
					break;
				case PREFER_COMPRESSED:
					/*usually compressed format allow the biggest picture size*/
					if (format_desc[j].compressed)
						candidate = j;
					break;
				case NO_PREFERENCE:
				default:
					candidate = j;
					break;
			}

			if (candidate != -1) {
				struct v4l2_format fmt = { 0 };
				fmt.fmt.pix.width       = vsize.width;
				fmt.fmt.pix.height      = vsize.height;
				ms_message("Candidate: %i",candidate);

				if (v4lv2_try_format(fd, &fmt, format_desc[j].pixel_format)) {
					MSPixFmt selected=v4l2_format_to_ms(format_desc[j].pixel_format);
					ms_message("V4L2: selected format is %s", ms_pix_fmt_to_string(selected));
					return selected;
				}
			}
		}
	}

	ms_error("No compatible format found");
	return MS_PIX_FMT_UNKNOWN;
}

MSPixFmt msv4l2_pick_best_format_basic(int fd, const V4L2FormatDescription* format_desc, MSVideoSize vsize, float target_fps) {
	int j;
	/* rules for picking a format are:
	   available fps > requested fps
	   no matter whether it is compressed or non-native, since swscale pixel converter is supposed to be much less efficient than libv4l's one
	   in this case.
	*/
	for (j=0; j<POSSIBLE_FORMATS_COUNT; j++) {
		if (!format_desc[j].supported) continue;
		if (format_desc[j].max_fps >= target_fps || format_desc[j].max_fps==-1 /*max fps unknown*/){
			struct v4l2_format fmt;
			fmt.fmt.pix.width       = vsize.width;
			fmt.fmt.pix.height      = vsize.height;

			if (v4lv2_try_format(fd, &fmt, format_desc[j].pixel_format)) {
				MSPixFmt selected=v4l2_format_to_ms(format_desc[j].pixel_format);
				ms_message("V4L2: selected format is %s", ms_pix_fmt_to_string(selected));
				return selected;
			}
		}
	}

	ms_error("No compatible format found");
	return MS_PIX_FMT_UNKNOWN;
}

#if defined(__i386) || defined(__x86_64__)
#define pick_best_format msv4l2_pick_best_format_x86
#else
#define pick_best_format msv4l2_pick_best_format_basic
#endif

static int set_camera_feature(V4l2State *s, unsigned int ctl_id, int value, const char *feature_name){
	struct v4l2_ext_control ctl={0};
	struct v4l2_ext_controls ctls;
	struct v4l2_queryctrl queryctrl={0};

	memset(&ctls, 0, sizeof(ctls));

	queryctrl.id = ctl_id;
	if (ioctl (s->fd, VIDIOC_QUERYCTRL, &queryctrl)!=0) {
		ms_warning("%s not supported: %s",feature_name,strerror(errno));
		return -1;
	} else if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) {
		ms_warning("%s setting disabled.",feature_name);
		return -1;
	}else {
#ifdef V4L2_CTRL_CLASS_CAMERA
		ctl.id=ctl_id;
		ctl.value=value;
		ctl.size=sizeof(int);
		ctls.count=1;
		ctls.controls=&ctl;
		ctls.ctrl_class=V4L2_CTRL_CLASS_CAMERA;

		if (v4l2_ioctl(s->fd,VIDIOC_S_EXT_CTRLS,&ctls)!=0){
			ms_warning("Could not enable %s: %s", feature_name, strerror(errno));
			return -1;
		}
#endif
	}
	return 0;
}


static int msv4l2_configure(V4l2State *s){
	struct v4l2_capability cap;
	struct v4l2_format fmt;
	MSVideoSize vsize;
	const char *focus;

	if (v4l2_ioctl (s->fd, VIDIOC_QUERYCAP, &cap)<0) {
		ms_message("Not a v4lv2 driver.");
		return -1;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		ms_error("%s is not a video capture device\n",s->dev);
		return -1;
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		ms_error("%s does not support streaming i/o\n",s->dev);
		return -1;
	}


	ms_message("Driver is %s, version is %i", cap.driver, cap.version);

	memset(&fmt,0,sizeof(fmt));

	fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (v4l2_ioctl (s->fd, VIDIOC_G_FMT, &fmt)<0){
		ms_error("VIDIOC_G_FMT failed: %s",strerror(errno));
	}
	vsize=s->vsize;

	do{
		const V4L2FormatDescription* formats_desc = query_format_description_for_size(s->fd, s->vsize);
		s->pix_fmt = pick_best_format(s->fd, formats_desc, s->vsize, s->fps);

		if (s->pix_fmt == MS_PIX_FMT_UNKNOWN)
			s->vsize=ms_video_size_get_just_lower_than(s->vsize);
	} while(s->vsize.width!=0 && (s->pix_fmt == MS_PIX_FMT_UNKNOWN));

	if (s->vsize.width==0){
		ms_message("Could not find any combination of resolution/pixel-format that works !");
		s->vsize=vsize;
		ms_message("Fallback. Trying to force YUV420 format");
		memset(&fmt, 0, sizeof(fmt));
		fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUV420;
		fmt.fmt.pix.width = s->vsize.width;
		fmt.fmt.pix.height = s->vsize.height;
		fmt.fmt.pix.field = V4L2_FIELD_ANY;
		if(v4l2_ioctl(s->fd, VIDIOC_S_FMT, &fmt) != 0) {
			ms_error("VIDIOC_S_FMT failed: %s", strerror(errno));
			return -1;
		}
		s->pix_fmt = v4l2_format_to_ms(fmt.fmt.pix.pixelformat);
	}

	memset(&fmt,0,sizeof(fmt));
	fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

	if (v4l2_ioctl (s->fd, VIDIOC_G_FMT, &fmt)<0){
		ms_error("VIDIOC_G_FMT failed: %s",strerror(errno));
	}else{
		ms_message("Size of webcam delivered pictures is %ix%i. Format:0x%08x",fmt.fmt.pix.width,fmt.fmt.pix.height, s->pix_fmt);
		s->vsize.width=fmt.fmt.pix.width;
		s->vsize.height=fmt.fmt.pix.height;
	}
	s->picture_size=get_picture_buffer_size(s->pix_fmt,s->vsize.width,s->vsize.height);

	focus=getenv("MS2_CAM_FOCUS");
	if (focus){
		if (strcasecmp(focus,"auto")==0){
#ifdef V4L2_CID_AUTO_FOCUS_RANGE
			set_camera_feature(s,V4L2_CID_AUTO_FOCUS_RANGE,V4L2_AUTO_FOCUS_RANGE_AUTO ,"auto range");
#endif
#ifdef V4L2_CID_FOCUS_AUTO
			set_camera_feature(s,V4L2_CID_FOCUS_AUTO,1,"auto-focus");
#endif
		}else if (strcasecmp(focus,"infinity")==0){
#ifdef V4L2_CID_AUTO_FOCUS_RANGE
			set_camera_feature(s,V4L2_CID_AUTO_FOCUS_RANGE,V4L2_AUTO_FOCUS_RANGE_INFINITY ,"infinity range");
#endif
#ifdef V4L2_CID_FOCUS_AUTO
			set_camera_feature(s,V4L2_CID_FOCUS_AUTO,1,"auto-focus");
#endif
		}
	}

	s->configured=TRUE;
	return 0;
}

static int msv4l2_do_mmap(V4l2State *s){
	struct v4l2_requestbuffers req;
	int i;
	enum v4l2_buf_type type;

	memset(&req,0,sizeof(req));

	req.count               = 4;
	req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	req.memory              = V4L2_MEMORY_MMAP;

	if (v4l2_ioctl (s->fd, VIDIOC_REQBUFS, &req)<0) {
		ms_error("Error requesting info on mmap'd buffers: %s",strerror(errno));
		return -1;
	}

	for (i=0; i<(int)req.count; ++i) {
		struct v4l2_buffer buf;
		mblk_t *msg;
		void *start;
		memset(&buf,0,sizeof(buf));

		buf.type=V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory=V4L2_MEMORY_MMAP;
		buf.index=i;

		if (v4l2_ioctl (s->fd, VIDIOC_QUERYBUF, &buf)<0){
			ms_error("Could not VIDIOC_QUERYBUF : %s",strerror(errno));
			return -1;
		}

		start=v4l2_mmap (NULL /* start anywhere */,
			buf.length,
			PROT_READ | PROT_WRITE /* required */,
			MAP_SHARED /* recommended */,
			s->fd, buf.m.offset);

		if (start==NULL){
			ms_error("Could not v4l2_mmap: %s",strerror(errno));
		}
		msg=esballoc(start,buf.length,0,NULL);
		msg->b_wptr+=buf.length;
		s->frames[i]=ms_yuv_buf_alloc_from_buffer(s->vsize.width, s->vsize.height, msg);
	}
	s->frame_max=req.count;
	for (i = 0; i < s->frame_max; ++i) {
		struct v4l2_buffer buf;

		memset(&buf,0,sizeof(buf));
		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		buf.memory      = V4L2_MEMORY_MMAP;
		buf.index       = i;
		if (-1==v4l2_ioctl (s->fd, VIDIOC_QBUF, &buf)){
			ms_error("VIDIOC_QBUF failed: %s",strerror(errno));
		}else {
			inc_ref(s->frames[i]);
			s->queued++;
		}
	}
	/*start capture immediately*/
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 ==v4l2_ioctl (s->fd, VIDIOC_STREAMON, &type)){
		ms_error("VIDIOC_STREAMON failed: %s",strerror(errno));
		return -1;
	}
	return 0;
}

static mblk_t *v4l2_dequeue_ready_buffer(V4l2State *s, int poll_timeout_ms){
	struct v4l2_buffer buf;
	mblk_t *ret=NULL;
	struct pollfd fds;

	memset(&buf,0,sizeof(buf));
	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	memset(&fds,0,sizeof(fds));
	fds.events=POLLIN;
	fds.fd=s->fd;
	/*check with poll if there is something to read */
	if (poll(&fds,1,poll_timeout_ms)==1 && fds.revents==POLLIN){
		if (v4l2_ioctl(s->fd, VIDIOC_DQBUF, &buf)<0) {
			switch (errno) {
			case EAGAIN:
				ms_warning("VIDIOC_DQBUF failed with EAGAIN, this is a driver bug !");
				usleep(20000);
			case EIO:
				/* Could ignore EIO, see spec. */
				break;
			default:
				ms_warning("VIDIOC_DQBUF failed: %s",strerror(errno));
			}
		}else{
			s->queued--;
			ms_debug("v4l2: de-queue buf %i",buf.index);
			/*decrement ref count of dequeued buffer */
			ret=s->frames[buf.index];
			dec_ref(ret);
			if ((int)buf.index >= s->frame_max){
				ms_error("buf.index>=s->max_frames !");
				return NULL;
			}
			if (buf.bytesused<=30){
				ms_warning("Ignoring empty buffer...");
				return NULL;
			}
			/*normally buf.bytesused should contain the right buffer size; however we have found a buggy
			driver that puts a random value inside */
			if (s->picture_size!=0)
				ret->b_cont->b_wptr=ret->b_cont->b_rptr+s->picture_size;
			else ret->b_cont->b_wptr=ret->b_cont->b_rptr+buf.bytesused;
		}
	}
	return ret;
}

static mblk_t * v4lv2_grab_image(V4l2State *s, int poll_timeout_ms){
	struct v4l2_buffer buf;
	int k;
	bool_t no_slot_available = TRUE;
	mblk_t *ret=NULL;

	memset(&buf,0,sizeof(buf));

	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	buf.memory = V4L2_MEMORY_MMAP;

	/*queue buffers whose ref count is 1, because they are not
	 used anywhere in the filter chain */
	for(k=0;k<s->frame_max;++k){
		if (dblk_ref_value(s->frames[k]->b_datap)==1){
			no_slot_available = FALSE;
			buf.index=k;
			if (-1==v4l2_ioctl (s->fd, VIDIOC_QBUF, &buf))
				ms_warning("VIDIOC_QBUF %i failed: %s",k,  strerror(errno));
			else {
				/*increment ref count of queued buffer*/
				inc_ref(s->frames[k]);
				s->queued++;
			}
		}
	}

	if (s->queued){
		ret=v4l2_dequeue_ready_buffer(s,poll_timeout_ms);
	}else if (no_slot_available){
		ms_usleep(100000);
	}
	return ret;
}

static void msv4l2_do_munmap(V4l2State *s){
	int i;
	enum v4l2_buf_type type;
	/*stop capture immediately*/
	type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	if (-1 ==v4l2_ioctl (s->fd, VIDIOC_STREAMOFF, &type)){
		ms_error("VIDIOC_STREAMOFF failed: %s",strerror(errno));
	}

	for(i=0;i<s->frame_max;++i){
		mblk_t *msg=s->frames[i]->b_cont;
		int len=dblk_lim(msg->b_datap)-dblk_base(msg->b_datap);
		if (v4l2_munmap(dblk_base(msg->b_datap),len)<0){
			ms_warning("MSV4l2: Fail to unmap: %s",strerror(errno));
		}
		freemsg(s->frames[i]);
		s->frames[i]=NULL;
	}
}



static void msv4l2_init(MSFilter *f){
	V4l2State *s=ms_new0(V4l2State,1);
	s->dev=ms_strdup("/dev/video0");
	s->fd=-1;
	s->vsize=MS_VIDEO_SIZE_CIF;
	s->fps=15;
	s->configured=FALSE;
	f->data=s;
	qinit(&s->rq);
}

static void msv4l2_uninit(MSFilter *f){
	V4l2State *s=(V4l2State*)f->data;
	ms_free(s->dev);
	flushq(&s->rq,0);
	ms_mutex_destroy(&s->mutex);
	ms_free(s);
}

static void *msv4l2_thread(void *ptr){
	V4l2State *s=(V4l2State*)ptr;
	uint64_t start;

	ms_message("msv4l2_thread starting");
	if (s->fd==-1){
		if( msv4l2_open(s)!=0){
			ms_warning("msv4l2 could not be openned");
			goto close;
		}
	}

	if (!s->configured && msv4l2_configure(s)!=0){
		ms_warning("msv4l2 could not be configured");
		goto close;
	}

	if (msv4l2_do_mmap(s)!=0)
	{
		ms_warning("msv4l2 do mmap");
		goto close;
	}

	ms_message("V4L2 video capture started.");
	while(s->thread_run)
	{
		if (s->fd!=-1){
			mblk_t *m;
			m=v4lv2_grab_image(s,50);
			if (m){
				mblk_t *om=dupmsg(m);
				mblk_set_marker_info(om,(s->pix_fmt==MS_MJPEG));
				ms_mutex_lock(&s->mutex);
				putq(&s->rq,om);
				ms_mutex_unlock(&s->mutex);
			}
		}
	}
	/*dequeue pending buffers so that we can properly unref them (avoids memleak ), and even worse crashes (vmware)*/
	start=ortp_get_cur_time_ms();
	while(s->queued){
		v4l2_dequeue_ready_buffer(s,50);
		if (ortp_get_cur_time_ms()-start > 5000){
			ms_warning("msv4l2: still [%i] buffers not dequeued at exit !", s->queued);
			break;
		}
	}
	msv4l2_do_munmap(s);
close:
	msv4l2_close(s);
	ms_message("msv4l2_thread exited.");
	ms_thread_exit(NULL);
	return NULL;
}

static void msv4l2_preprocess(MSFilter *f){
	V4l2State *s=(V4l2State*)f->data;
	s->thread_run=TRUE;
	ms_thread_create(&s->thread,NULL,msv4l2_thread,s);
	s->th_frame_count=-1;
	ms_average_fps_init(&s->avgfps,"V4L2 capture: fps=%f");
}

static void msv4l2_process(MSFilter *f){
	V4l2State *s=(V4l2State*)f->data;
	uint32_t timestamp;
	int cur_frame;
	uint32_t curtime=f->ticker->time;
	float elapsed;

	if (s->th_frame_count==-1){
		s->start_time=curtime;
		s->th_frame_count=0;
	}
	elapsed=((float)(curtime-s->start_time))/1000.0;
	cur_frame=elapsed*s->fps;

	if (cur_frame>=s->th_frame_count){
		mblk_t *om=NULL;
		ms_mutex_lock(&s->mutex);
		/*keep the most recent frame if several frames have been captured */
		if (s->fd!=-1){
			mblk_t *tmp=NULL;
			while((tmp=getq(&s->rq))!=NULL){
				if (om!=NULL) freemsg(om);
				om=tmp;
			}
		}
		ms_mutex_unlock(&s->mutex);
		if (om!=NULL){
			timestamp=f->ticker->time*90;/* rtp uses a 90000 Hz clockrate for video*/
			mblk_set_timestamp_info(om,timestamp);
			mblk_set_marker_info(om,TRUE);
			ms_queue_put(f->outputs[0],om);
			ms_average_fps_update(&s->avgfps,f->ticker->time);
		}
		s->th_frame_count++;
	}
}

static void msv4l2_postprocess(MSFilter *f){
	V4l2State *s=(V4l2State*)f->data;

	s->thread_run = FALSE;
	if(s->thread) {
		ms_thread_join(s->thread,NULL);
		ms_message("msv4l2 thread has joined.");
	}
	else {
		ms_warning("msv4l2 thread was already stopped");
	}

	flushq(&s->rq,0);
}

static int msv4l2_set_fps(MSFilter *f, void *arg){
	V4l2State *s=(V4l2State*)f->data;
	s->fps=*(float*)arg;
	return 0;
}

static int msv4l2_set_vsize(MSFilter *f, void *arg){
	V4l2State *s=(V4l2State*)f->data;
	s->vsize=*(MSVideoSize*)arg;
	s->configured=FALSE;
	return 0;
}

static int msv4l2_check_configured(V4l2State *s){
	if (s->configured) return 0;
	if (s->fd!=-1){
		msv4l2_close(s);
	}
	if (msv4l2_open(s)==0){
		msv4l2_configure(s);
	}
	return 0;
}

static int msv4l2_get_vsize(MSFilter *f, void *arg){
	V4l2State *s=(V4l2State*)f->data;
	msv4l2_check_configured(s);
	*(MSVideoSize*)arg=s->vsize;
	return 0;
}

static int msv4l2_get_pixfmt(MSFilter *f, void *arg){
	V4l2State *s=(V4l2State*)f->data;
	msv4l2_check_configured(s);
	*(MSPixFmt*)arg=s->pix_fmt;
	return 0;
}

static int msv4l2_set_devfile(MSFilter *f, void *arg){
	V4l2State *s=(V4l2State*)f->data;
	if (s->dev) ms_free(s->dev);
	s->dev=ms_strdup((char*)arg);
	return 0;
}

static int msv4l2_get_fps(MSFilter *f, void *arg){
	V4l2State *s=(V4l2State*)f->data;
	if (f->ticker){
		*(float*)arg=ms_average_fps_get(&s->avgfps);
	}else *(float*)arg=s->fps;
	return 0;
}

static MSFilterMethod msv4l2_methods[]={
	{	MS_FILTER_SET_FPS	,	msv4l2_set_fps	},
	{	MS_FILTER_SET_VIDEO_SIZE,	msv4l2_set_vsize	},
	{	MS_FILTER_GET_VIDEO_SIZE,	msv4l2_get_vsize	},
	{	MS_FILTER_GET_PIX_FMT	,	msv4l2_get_pixfmt	},
	{	MS_FILTER_GET_FPS	,	msv4l2_get_fps	},
	{	0			,	NULL		}
};

MSFilterDesc ms_v4l2_desc={
	.id=MS_V4L2_CAPTURE_ID,
	.name="MSV4L2Capture",
	.text=N_("A filter to grab pictures from Video4Linux2-powered cameras"),
	.category=MS_FILTER_OTHER,
	.ninputs=0,
	.noutputs=1,
	.init=msv4l2_init,
	.preprocess=msv4l2_preprocess,
	.process=msv4l2_process,
	.postprocess=msv4l2_postprocess,
	.uninit=msv4l2_uninit,
	.methods=msv4l2_methods
};

MS_FILTER_DESC_EXPORT(ms_v4l2_desc)

static MSFilter *msv4l2_create_reader(MSWebCam *obj){
	MSFilter *f=ms_factory_create_filter(ms_web_cam_get_factory(obj),MS_V4L2_CAPTURE_ID);
	msv4l2_set_devfile(f,obj->name);
	return f;
}

static void msv4l2_detect(MSWebCamManager *obj);

static void msv4l2_cam_init(MSWebCam *cam){
}

MSWebCamDesc v4l2_card_desc={
	"V4L2",
	&msv4l2_detect,
	&msv4l2_cam_init,
	&msv4l2_create_reader,
	NULL,
	NULL
};

static void msv4l2_detect(MSWebCamManager *obj){
	struct v4l2_capability cap;
	char devname[32];
	int i;

	for(i=0;i<10;++i){
		int fd;

		snprintf(devname,sizeof(devname),"/dev/video%i",i);

		fd=open(devname,O_RDWR);
		if (fd!=-1){
			if (v4l2_ioctl (fd, VIDIOC_QUERYCAP, &cap)==0) {
				/* is a V4LV2 */
				uint32_t camera_caps = cap.capabilities;
#ifdef V4L2_CAP_DEVICE_CAPS
				if (cap.capabilities & V4L2_CAP_DEVICE_CAPS) {
					camera_caps = cap.device_caps;
				}
#else
				camera_caps = V4L2_CAP_VIDEO_CAPTURE;
#endif
				if (((camera_caps & V4L2_CAP_VIDEO_CAPTURE)
#ifdef V4L2_CAP_VIDEO_CAPTURE_MPLANE
					|| (camera_caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE)
#endif
					) && !((camera_caps & V4L2_CAP_VIDEO_OUTPUT)
#ifdef V4L2_CAP_VIDEO_OUTPUT_MPLANE
					|| (camera_caps & V4L2_CAP_VIDEO_OUTPUT_MPLANE)
#endif
					)) {
					MSWebCam *cam=ms_web_cam_new(&v4l2_card_desc);
					cam->name=ms_strdup(devname);
					ms_web_cam_manager_add_cam(obj,cam);
				}
			}
			close(fd);
		}else if (errno != ENOENT){
			ms_message("Could not open %s: %s", devname, strerror(errno));
		}
	}
}


#endif
