#ifdef __APPLE__

#include "mediastreamer-config.h"
#include "mediastreamer2/msvideo.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msv4l.h"
#include "mediastreamer2/mswebcam.h"
#include "nowebcam.h"

#import <QTKit/QTkit.h>

struct v4mState;

@interface NsMsWebCam :NSObject
{
	QTCaptureDeviceInput *input;
	QTCaptureDecompressedVideoOutput * output;
	QTCaptureSession *session;
	//QTCaptureDevice *device;
	
	ms_mutex_t mutex;
	queue_t rq;
};

-(id) init;
-(void) dealloc;
-(int) start;
-(int) stop;
-(void) setSize:(MSVideoSize) size;
-(MSVideoSize) getSize;
-(void) setName:(char*) name;
-(int) getPixFmt;

-(QTCaptureSession *) session;
-(queue_t*) rq;
-(ms_mutex_t *) mutex;

@end

@implementation NsMsWebCam 

-(void)captureOutput:(QTCaptureOutput *)captureOutput didOutputVideoFrame:(CVImageBufferRef)videoFrame withSampleBuffer:(QTSampleBuffer *)sampleBuffer fromConnection:
(QTCaptureConnection *)connection
{
	ms_mutex_lock(&mutex);	
	mblk_t *buf;
	uint8_t * data = (uint8_t *)[sampleBuffer bytesForAllSamples];
	int size = [sampleBuffer lengthForAllSamples];
	buf=allocb(size,0);
	memcpy(buf->b_wptr, data, size);
	buf->b_wptr+=size;			
	putq(&rq, buf);
	ms_mutex_unlock(&mutex);
}

-(id) init {
	qinit(&rq);
	ms_mutex_init(&mutex,NULL);
	
	session = [[QTCaptureSession alloc] init];
	output = [[QTCaptureDecompressedVideoOutput alloc] init];
	
	[output setDelegate: self];
	
	
	return self;
}

-(void) dealloc {
	[self stop];
	
	if(session)
	{		
		[session release];
		session = nil;
	}
		
	if(input)
	{
		[input release];
		input = nil;
	}
	
	if(output)
	{
		[output release];
		output = nil;
	}
	
	flushq(&rq,0);
	ms_mutex_destroy(&mutex);

	[super dealloc];
}

-(int) start {
	
	[session startRunning];
	
	return 0;
}

-(int) stop {
	
	[session stopRunning];
	
	return 0;
}

-(int) getPixFmt{
	
	QTCaptureDevice *device = [input device];
	if([device isOpen])
	{	
		NSArray * array = [device formatDescriptions];
	
		NSEnumerator *enumerator = [array objectEnumerator];
		QTFormatDescription *desc;
		while ((desc = [enumerator nextObject])) 
		{
			if ([desc mediaType] == QTMediaTypeVideo)
			{
				UInt32 fmt = [desc formatType];
				
				if( fmt == kCVPixelFormatType_420YpCbCr8Planar)
					return MS_YUV420P;
				
				//else if( fmt == MS_YUYV)
				//	return;
				
				else if( fmt == kCVPixelFormatType_24RGB)
					return MS_RGB24;
				
				//else if( fmt == MS_RGB24_REV)
				//	return;
				//else if( fmt == MS_MJPEG)
				//	return;
				else if( fmt == kUYVY422PixelFormat)
					return MS_UYVY;
				
				else if( fmt == kYUVSPixelFormat)
					return MS_YUY2;
				
				else if( fmt == k32RGBAPixelFormat)
					return MS_RGBA32;
				
            }
        }
    }
	
	return MS_YUV420P;
}

-(void) setName:(char*) name
{
	NSError *error = nil;
	unsigned int i = 0;
	 
	QTCaptureDevice * device = [QTCaptureDevice defaultInputDeviceWithMediaType:QTMediaTypeVideo];
	 
	if(name != nil)
	{
		NSArray * array = [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];
	 
		for(i = 0 ; i < [array count]; i++)
		{
			QTCaptureDevice * deviceTmp = [array objectAtIndex:i];
			if(!strcmp([[device localizedDisplayName] UTF8String], name))
			{
				device = deviceTmp;
				break;
			}
		}
	}
	
	if(device)
		if(![device open:&error])
			return;
	
	input = [[QTCaptureDeviceInput alloc] initWithDevice:device];
	
	[session addInput:input error:&error];
	[session addOutput:output error:&error];

}

-(void) setSize:(MSVideoSize) size
{	
	NSDictionary * dic = [NSDictionary dictionaryWithObjectsAndKeys:
	 [NSNumber numberWithInteger:size.width], (id)kCVPixelBufferWidthKey,
	 [NSNumber numberWithInteger:size.height],(id)kCVPixelBufferHeightKey,
	 //[NSNumber numberWithInteger:kCVPixelFormatType_420YpCbCr8Planar], (id)kCVPixelBufferPixelFormatTypeKey,
						  nil];
	
	[output setPixelBufferAttributes:dic];
}

-(MSVideoSize) getSize
{
	MSVideoSize size;
	
	size.width = MS_VIDEO_SIZE_QCIF_W;
	size.height = MS_VIDEO_SIZE_QCIF_H;
	
	if(output)
	{
		NSDictionary * dic = [output pixelBufferAttributes];
		
		size.width = [[dic objectForKey:(id)kCVPixelBufferWidthKey] integerValue];
		size.height = [[dic objectForKey:(id)kCVPixelBufferHeightKey] integerValue];
	}
	
	return size;
}

-(QTCaptureSession *) session
{
	return	session;
}

-(queue_t*) rq
{
	return &rq;
}

-(ms_mutex_t *) mutex
{
	return &mutex;
}

@end

typedef struct v4mState{	
	NsMsWebCam * webcam;
	NSAutoreleasePool* myPool;
	mblk_t *mire;
	int frame_ind;	
	float fps;
	float start_time;
	int frame_count;
	bool_t usemire;
}v4mState;

static void v4m_init(MSFilter *f){
	v4mState *s=ms_new0(v4mState,1);
	s->myPool = [[NSAutoreleasePool alloc] init];
	s->webcam= [[NsMsWebCam alloc] init];
	[s->webcam retain];
	s->mire=NULL;	
	s->start_time=0;
	s->frame_count=-1;
	s->fps=15;
	s->usemire=(getenv("DEBUG")!=NULL);
	f->data=s;
}

static int v4m_start(MSFilter *f, void *arg) {
	v4mState *s=(v4mState*)f->data;
	[s->webcam start];
	ms_message("v4m video device opened.");
	return 0;
}

static int v4m_stop(MSFilter *f, void *arg){
	v4mState *s=(v4mState*)f->data;
	[s->webcam stop];
	ms_message("v4m video device closed.");
	
	return 0;
}

static void v4m_uninit(MSFilter *f){
	v4mState *s=(v4mState*)f->data;
	v4m_stop(f,NULL);
	
	freemsg(s->mire);
	[s->webcam release];
	[s->myPool release];
	ms_free(s);
}

static mblk_t * v4m_make_mire(v4mState *s){
	unsigned char *data;
	int i,j,line,pos;
	MSVideoSize vsize = [s->webcam getSize];
	int patternw=vsize.width/6; 
	int patternh=vsize.height/6;
	int red,green=0,blue=0;
	if (s->mire==NULL){
		s->mire=allocb(vsize.width*vsize.height*3,0);
		s->mire->b_wptr=s->mire->b_datap->db_lim;
	}
	data=s->mire->b_rptr;
	for (i=0;i<vsize.height;++i){
		line=i*vsize.width*3;
		if ( ((i+s->frame_ind)/patternh) & 0x1) red=255;
		else red= 0;
		for (j=0;j<vsize.width;++j){
			pos=line+(j*3);
			
			if ( ((j+s->frame_ind)/patternw) & 0x1) blue=255;
			else blue= 0;
			
			data[pos]=red;
			data[pos+1]=green;
			data[pos+2]=blue;
		}
	}
	s->frame_ind++;
	return s->mire;
}

static mblk_t * v4m_make_nowebcam(v4mState *s){
	if (s->mire==NULL && s->frame_ind==0){
		//s->mire=ms_load_nowebcam(&s->vsize, -1);
	}
	s->frame_ind++;
	return s->mire;
}

static void v4m_process(MSFilter * obj){
	v4mState *s=(v4mState*)obj->data;
	uint32_t timestamp;
	int cur_frame;
	if (s->frame_count==-1){
		s->start_time=obj->ticker->time;
		s->frame_count=0;
	}

	ms_mutex_lock([s->webcam mutex]);

	cur_frame=((obj->ticker->time-s->start_time)*s->fps/1000.0);
	if (cur_frame>=s->frame_count)
	{
		mblk_t *om=NULL;
		/*keep the most recent frame if several frames have been captured */
		if ([[s->webcam session] isRunning])
		{
			om=getq([s->webcam rq]);
		}
		else
		{
			/*if (s->pix_fmt==MS_YUV420P && s->vsize.width==MS_VIDEO_SIZE_CIF_W && s->vsize.height==MS_VIDEO_SIZE_CIF_H)
		    {
				if (s->usemire)
				{
					om=dupmsg(v4m_make_mire(s));
				}
				else 
				{
					mblk_t *tmpm=v4m_make_nowebcam(s);
					if (tmpm) 
						om=dupmsg(tmpm);
				}
		    }*/
		}
		
		if (om!=NULL)
		{
			timestamp=obj->ticker->time*90;/* rtp uses a 90000 Hz clockrate for video*/
			mblk_set_timestamp_info(om,timestamp);
			mblk_set_marker_info(om,TRUE);	
			ms_queue_put(obj->outputs[0],om);
			s->frame_count++;
		}
	}
	else 
		flushq([s->webcam rq],0);

	ms_mutex_unlock([s->webcam mutex]);
}

static void v4m_preprocess(MSFilter *f){
	v4m_start(f,NULL);
        
}

static void v4m_postprocess(MSFilter *f){
	v4m_stop(f,NULL);
}

static int v4m_set_fps(MSFilter *f, void *arg){
	v4mState *s=(v4mState*)f->data;
	s->fps=*((float*)arg);
	s->frame_count=-1;
	return 0;
}

static int v4m_get_pix_fmt(MSFilter *f,void *arg){
	v4mState *s=(v4mState*)f->data;
	*((MSPixFmt*)arg) = [s->webcam getPixFmt];
	return 0;
}

static int v4m_set_vsize(MSFilter *f, void *arg){
	v4mState *s=(v4mState*)f->data;
	[s->webcam setSize:*((MSVideoSize*)arg)];
	return 0;
}

static int v4m_get_vsize(MSFilter *f, void *arg){
	v4mState *s=(v4mState*)f->data;
	*(MSVideoSize*)arg = [s->webcam getSize];
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_FPS		,	v4m_set_fps		},
	{	MS_FILTER_GET_PIX_FMT	,	v4m_get_pix_fmt	},
	{	MS_FILTER_SET_VIDEO_SIZE, 	v4m_set_vsize	},
	{	MS_V4L_START			,	v4m_start		},
	{	MS_V4L_STOP				,	v4m_stop		},
	{	MS_FILTER_GET_VIDEO_SIZE,	v4m_get_vsize	},
	{	0						,	NULL			}
};

MSFilterDesc ms_v4m_desc={
	.id=MS_V4L_ID,
	.name="MSV4m",
	.text="A video for macosx compatible source filter to stream pictures.",
	.ninputs=0,
	.noutputs=1,
	.category=MS_FILTER_OTHER,
	.init=v4m_init,
	.preprocess=v4m_preprocess,
	.process=v4m_process,
	.postprocess=v4m_postprocess,
	.uninit=v4m_uninit,
	.methods=methods
};

MS_FILTER_DESC_EXPORT(ms_v4m_desc)
        
static void ms_v4m_detect(MSWebCamManager *obj);

static void ms_v4m_cam_init(MSWebCam *cam)
{
}

static int v4m_set_device(MSFilter *f, void *arg)
{	
	v4mState *s=(v4mState*)f->data;
        
	/*s->id = (char*) malloc(sizeof(char)*strlen((char*)arg));
	strcpy(s->id,(char*)arg);*/

	return 0;
}

static int v4m_set_name(MSFilter *f, void *arg){
	
	v4mState *s=(v4mState*)f->data;
        
	//s->name = (char*) malloc(sizeof(char)*strlen((char*)arg));
	//strcpy(s->name,(char*)arg);
	
	[s->webcam setName:(char*)arg];

	return 0;
}

static MSFilter *ms_v4m_create_reader(MSWebCam *obj)
{	
	MSFilter *f= ms_filter_new_from_desc(&ms_v4m_desc); 
        
	v4m_set_device(f,obj->id);
	v4m_set_name(f,obj->data);
        
	return f;
}

MSWebCamDesc ms_v4m_cam_desc={
	"VideoForMac grabber",
	&ms_v4m_detect,
	&ms_v4m_cam_init,
	&ms_v4m_create_reader,
	NULL
};


static void ms_v4m_detect(MSWebCamManager *obj){
  
	unsigned int i = 0;
	NSAutoreleasePool* myPool = [[NSAutoreleasePool alloc] init];
	
	NSArray * array = [QTCaptureDevice inputDevicesWithMediaType:QTMediaTypeVideo];
	
	for(i = 0 ; i < [array count]; i++)
	{
		QTCaptureDevice * device = [array objectAtIndex:i];
		MSWebCam *cam=ms_web_cam_new(&ms_v4m_cam_desc);
		
		cam->name= ms_strdup([[device localizedDisplayName] UTF8String]);
		cam->id = ms_strdup([[device uniqueID] UTF8String]);
		cam->data = NULL;
		ms_web_cam_manager_add_cam(obj,cam);
	}
	[myPool release];
}
        
#endif        
