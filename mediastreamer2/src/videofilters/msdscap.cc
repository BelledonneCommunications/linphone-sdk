/* msdscap - mediastreamer2 plugin for video capture using directshow 
   This source code requires visual studio to build.

   Copyright (C) 2009 Simon Morlat

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>. 
*/

#include <mediastreamer2/mswebcam.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/msvideo.h>

#include <dshow.h>
#include <basetyps.h>


#define MAX_FILTER_NAME 128
#define MAX_PIN_NAME 128



/*
 * The following definitions are normally in Qedit.h that is now deprecated by Microsoft.
 * Therefore we need to duplicate them here.
 */
DEFINE_GUID(CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3,
	0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DEFINE_GUID(IID_ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce,
	0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f);
DEFINE_GUID(CLSID_NullRenderer, 0xc1f400a4, 0x3f08, 0x11d3,
	0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37);
DECLARE_INTERFACE_(ISampleGrabberCB, IUnknown)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID, PVOID*) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS)PURE;
	STDMETHOD_(ULONG, Release)(THIS)PURE;
	STDMETHOD(SampleCB)(THIS_ double, IMediaSample*) PURE;
	STDMETHOD(BufferCB)(THIS_ double, BYTE*, long) PURE;
};
DECLARE_INTERFACE_(ISampleGrabber, IUnknown){
	STDMETHOD(QueryInterface)(THIS_ REFIID, PVOID*) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS)PURE;
	STDMETHOD_(ULONG, Release)(THIS)PURE;
	STDMETHOD(SetOneShot)(THIS_ BOOL) PURE;
	STDMETHOD(SetMediaType)(THIS_ const AM_MEDIA_TYPE*) PURE;
	STDMETHOD(GetConnectedMediaType)(THIS_ AM_MEDIA_TYPE*) PURE;
	STDMETHOD(SetBufferSamples)(THIS_ BOOL) PURE;
	STDMETHOD(GetCurrentBuffer)(THIS_ long*, long*) PURE;
	STDMETHOD(GetCurrentSample)(THIS_ IMediaSample**) PURE;
	STDMETHOD(SetCallBack)(THIS_ ISampleGrabberCB *, long) PURE;
};




/*
Convenient shared ptr class for COM objects. Like std::shared_ptr.
*/
template <typename _ComType>
class SharedComPtr{
public:
	SharedComPtr() : _ptr(0){
	}
	~SharedComPtr(){
		assign(NULL);
	}
	SharedComPtr(const SharedComPtr &ptr) : _ptr(NULL){
		assign(ptr._ptr);
	}
	SharedComPtr<_ComType> &operator=(const SharedComPtr<_ComType> &o){
		assign(o._ptr);
		return *this;
	}
	void reset(){
		assign(NULL);
	}
	bool operator==(const SharedComPtr<_ComType> &o)const{
		return _ptr==o._ptr;
	}
	bool operator==(const void *optr)const{
		return _ptr==optr;
	}
	bool operator!=(const SharedComPtr<_ComType> &o)const{
		return _ptr!=o._ptr;
	}
	_ComType *operator->(){
		return _ptr;
	}
	const _ComType *operator->()const{
		return _ptr;
	}
	_ComType *get(){
		return _ptr;
	}
	const _ComType *get()const{
		return _ptr;
	}
	_ComType **operator&(){
		return &_ptr;
	}
	//do not use directly, use makeShared<>() instead:
	SharedComPtr(_ComType *ptr) : _ptr(ptr){
	}
private:
	void assign(_ComType *newptr){
		if (_ptr){
			_ptr->Release();
		}
		if (newptr) newptr->AddRef();
		_ptr=newptr;
	}
	_ComType *_ptr;
};

template <typename _ComType>
SharedComPtr<_ComType> makeShared(REFCLSID rclsid,LPUNKNOWN pUnkOuter,DWORD dwClsContext,REFIID riid){
	_ComType *ptr=NULL;
	if (CoCreateInstance(rclsid,pUnkOuter,dwClsContext,riid,(void**)&ptr)!=S_OK) return SharedComPtr<_ComType>();
	return SharedComPtr<_ComType>(ptr);
}

template <typename _ComType>
SharedComPtr<_ComType> makeShared(REFCLSID rclsid,REFIID riid){
	return makeShared<_ComType>(rclsid,NULL,CLSCTX_INPROC,riid);
}




class DSCapture : public ISampleGrabberCB{
public:
	DSCapture(){
		qinit(&_rq);
		ms_mutex_init(&_mutex,NULL);
		_vsize.width=MS_VIDEO_SIZE_CIF_W;
		_vsize.height=MS_VIDEO_SIZE_CIF_H;
		_fps=15;
		_start_time=0;
		_frame_count=0;
		_pixfmt=MS_YUV420P;
		_ready=false;
		m_refCount=1;
	}
	virtual ~DSCapture(){
		if (_ready) stopAndClean();
		flushq(&_rq,0);
		ms_mutex_destroy(&_mutex);
	}
	STDMETHODIMP QueryInterface( REFIID riid, void **ppv );
	STDMETHODIMP_(ULONG) AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);
	STDMETHODIMP SampleCB(double,IMediaSample*);
	STDMETHODIMP BufferCB(double,BYTE*,long);
	int startDshowGraph();
	void stopAndClean();
	mblk_t *readFrame(){
		mblk_t *ret=NULL;
		ms_mutex_lock(&_mutex);
		ret=getq(&_rq);
		ms_mutex_unlock(&_mutex);
		return ret;
	}
	bool isTimeToSend(uint64_t ticker_time);
	MSVideoSize getVSize(){
		if (!_ready) createDshowGraph(); /* so that _vsize is updated according to hardware capabilities*/
		return _vsize;
	}
	void setVSize(MSVideoSize vsize){
		_vsize=vsize;
	}
	void setFps(float fps){
		_fps=fps;
	}
	float getFps(){
		return _fps;
	}
	MSPixFmt getPixFmt(){
		if (!_ready) createDshowGraph(); /* so that _pixfmt is updated*/
		return _pixfmt;
	}
	void setDeviceIndex(int index){
		_devid=index;
	}
	MSAverageFPS avgfps;
protected:
  	long m_refCount;
private:
	static SharedComPtr< IPin > findPin( SharedComPtr<IBaseFilter> &filter, PIN_DIRECTION direction, int index );
	int createDshowGraph();
	int selectBestFormat(SharedComPtr<IAMStreamConfig> streamConfig, int count);
	int _devid;
	MSVideoSize _vsize;
	queue_t _rq;
	ms_mutex_t _mutex;
	float _fps;
	float _start_time;
	int _frame_count;
	MSPixFmt _pixfmt;
	SharedComPtr< IGraphBuilder > _graphBuilder;
	SharedComPtr< IBaseFilter > _source;
	SharedComPtr< IBaseFilter > _nullRenderer;
	SharedComPtr< IBaseFilter > _grabberBase;
	SharedComPtr< IMediaControl > _mediaControl;
	SharedComPtr< IMediaEvent > _mediaEvent;
	bool _ready;
};


STDMETHODIMP DSCapture::QueryInterface(REFIID riid, void **ppv)
{
	return E_NOINTERFACE;
};

STDMETHODIMP_(ULONG) DSCapture::AddRef(){
	m_refCount++;
	return m_refCount;
}

STDMETHODIMP_(ULONG) DSCapture::Release()
{
	ms_message("DSCapture::Release");
	if ( !InterlockedDecrement( &m_refCount ) ) {
		int refcnt=m_refCount;
		delete this;
		return refcnt;
	}
	return m_refCount;
}

STDMETHODIMP DSCapture::SampleCB( double par1 , IMediaSample * sample)
{
	uint8_t *p;
	unsigned int size;
	
	if (sample->GetPointer(&p)!=S_OK){
		ms_error("error in GetPointer()");
		return S_OK;
	}
	size=sample->GetSize();
	//ms_message( "DSCapture::SampleCB pointer=%p, size=%i",p,size);
	mblk_t *m=allocb(size+128,0);
	/* make a copy into new buffer with extra bytes, because swscale
	is doing invalid reads past the end of the original buffers due to mmx optimisations */
	memcpy(m->b_wptr,p,size);
	m->b_wptr+=size;
	
	ms_mutex_lock(&_mutex);
	putq(&_rq,ms_yuv_buf_alloc_from_buffer(_vsize.width,_vsize.height,m));
	ms_mutex_unlock(&_mutex);
  	return S_OK;
}



STDMETHODIMP DSCapture::BufferCB( double, BYTE *b, long len)
{
	ms_message("DSCapture::BufferCB");
	return S_OK;
}

static void dscap_init(MSFilter *f){
	DSCapture *s=new DSCapture();
	f->data=s;
}



static void dscap_uninit(MSFilter *f){
	DSCapture *s=(DSCapture*)f->data;
	s->Release();
}

static char * fourcc_to_char(char *str, uint32_t fcc){
	memcpy(str,&fcc,4);
	str[4]='\0';
	return str;
}

static int find_best_format(SharedComPtr<IAMStreamConfig> streamConfig, int count,MSVideoSize *requested_size, MSPixFmt requested_fmt ){
	int i;
	MSVideoSize best_found={0,0};
	int best_index=-1;
	char fccstr[5];
	char selected_fcc[5];
	for (i=0; i<count; i++ ) {
		VIDEO_STREAM_CONFIG_CAPS videoConfig;
		AM_MEDIA_TYPE *mediaType;
		if (streamConfig->GetStreamCaps( i, &mediaType,
                                                 (BYTE *)&videoConfig )!=S_OK){
			ms_error( "Error getting stream capabilities");
			return -1;
		}
		if ( mediaType->majortype == MEDIATYPE_Video &&
			mediaType->formattype == FORMAT_VideoInfo &&
			mediaType->cbFormat >= sizeof(VIDEOINFOHEADER) ) {
			VIDEOINFOHEADER *infoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
			ms_message("Seeing format %ix%i %s",(int)infoHeader->bmiHeader.biWidth,(int)infoHeader->bmiHeader.biHeight,
					fourcc_to_char(fccstr,infoHeader->bmiHeader.biCompression));
			if (ms_fourcc_to_pix_fmt(infoHeader->bmiHeader.biCompression)==requested_fmt){
				MSVideoSize cur;
				cur.width=infoHeader->bmiHeader.biWidth;
				cur.height=infoHeader->bmiHeader.biHeight;
				if (ms_video_size_greater_than(*requested_size,cur)){
					if (ms_video_size_greater_than(cur,best_found)){
						best_found=cur;
						best_index=i;
						fourcc_to_char(selected_fcc,infoHeader->bmiHeader.biCompression);
					}
				}
			}
		}
		if ( mediaType->cbFormat != 0 )
			CoTaskMemFree( (PVOID)mediaType->pbFormat );
		if ( mediaType->pUnk != NULL ) mediaType->pUnk->Release();
			CoTaskMemFree( (PVOID)mediaType );
	}
	if (best_index!=-1) {
		*requested_size=best_found;
		ms_message("Best camera format is %s %ix%i",selected_fcc,best_found.width,best_found.height);
	}
	return best_index;
}

int DSCapture::selectBestFormat(SharedComPtr<IAMStreamConfig> streamConfig, int count){
	int index;

	_pixfmt=MS_YUV420P;
	index=find_best_format(streamConfig, count, &_vsize, _pixfmt);
	if (index!=-1) goto success;
	_pixfmt=MS_YUY2;
	index=find_best_format(streamConfig, count, &_vsize,_pixfmt);
	if (index!=-1) goto success;
	_pixfmt=MS_YUYV;
	index=find_best_format(streamConfig, count, &_vsize, _pixfmt);
	if (index!=-1) goto success;
	_pixfmt=MS_MJPEG;
	index=find_best_format(streamConfig, count, &_vsize, _pixfmt);
	if (index!=-1) goto success;
	_pixfmt=MS_RGB24;
	index=find_best_format(streamConfig, count, &_vsize, _pixfmt);
	if (index!=-1) {
		_pixfmt=MS_RGB24_REV;
		goto success;
	}
	ms_error("This camera does not support any of our pixel formats.");
	return -1;
	
	success:
	VIDEO_STREAM_CONFIG_CAPS videoConfig;
	AM_MEDIA_TYPE *mediaType;
	if (streamConfig->GetStreamCaps( index, &mediaType,
                    (BYTE *)&videoConfig )!=S_OK){
		ms_error( "Error getting stream capabilities" );
	}
 	streamConfig->SetFormat( mediaType );
	return 0;
}

int DSCapture::createDshowGraph(){
	SharedComPtr< ICreateDevEnum > createDevEnum;
	
	CoInitialize(NULL);
	if ((createDevEnum=makeShared<ICreateDevEnum>( CLSID_SystemDeviceEnum,
                                    IID_ICreateDevEnum ))==NULL){
		ms_error("Could not create device enumerator");
		return -1;
	}
	SharedComPtr< IEnumMoniker > enumMoniker;
	if (createDevEnum->CreateClassEnumerator( CLSID_VideoInputDeviceCategory, &enumMoniker, 0 )!=S_OK){
		ms_error("Fail to create class enumerator.");
		return -1;
	}
	
	ULONG fetched = 0;
	if ((_graphBuilder=makeShared<IGraphBuilder>( CLSID_FilterGraph, IID_IGraphBuilder ))==NULL){
		ms_error("Could not create graph builder.");
		return -1;
	}
    	SharedComPtr< IMoniker > moniker;
    	enumMoniker->Reset();
 	for ( int i=0;enumMoniker->Next( 1, &moniker, &fetched )==S_OK;++i ) {
		if (i==_devid){
			if (moniker->BindToObject( 0, 0, IID_IBaseFilter, (void **)&_source )!=S_OK){
				ms_error("Error binding moniker to base filter" );
				return -1;
			}
		}
	}
	if (_source==NULL){
		ms_error("Could not interface with webcam devid=%i",_devid);
		return -1;
	}

 	if (_graphBuilder->AddFilter( _source.get(), L"Source" )!=S_OK){
    		ms_error("Error adding camera source to filter graph" );
    		return -1;
	}
	SharedComPtr< IPin > sourceOut = findPin( _source, PINDIR_OUTPUT, 0 );
	if (sourceOut==NULL){
		ms_error("Error getting output pin of camera source" );
		return -1;
	}
	SharedComPtr< IAMStreamConfig > streamConfig;
	if (sourceOut->QueryInterface( IID_IAMStreamConfig,
                                  (void **)&streamConfig )!=S_OK){
		ms_error("Error requesting stream configuration API" );
		return -1;
	}
	int count, size;
	if (streamConfig->GetNumberOfCapabilities( &count, &size )!=S_OK){
		ms_error("Error getting number of capabilities" );
		return -1;
	}
	if (selectBestFormat(streamConfig,count)!=0){
		return -1;
	}

	if ((_grabberBase=makeShared<IBaseFilter>( CLSID_SampleGrabber,IID_IBaseFilter))==NULL){
    		ms_error("Error creating sample grabber" );
    		return -1;
	}
	if (_graphBuilder->AddFilter( _grabberBase.get(), L"Grabber" )!=S_OK){
		ms_error("Error adding sample grabber to filter graph");
		return -1;
	}
	SharedComPtr< ISampleGrabber > sampleGrabber;
	if (_grabberBase->QueryInterface( IID_ISampleGrabber,
                                               (void **)&sampleGrabber )!=S_OK){
		ms_error("Error requesting sample grabber interface");
		return -1;
	}
	if (sampleGrabber->SetOneShot( FALSE )!=S_OK){
 		ms_error("Error disabling one-shot mode" );
		return -1;
	}
	if (sampleGrabber->SetBufferSamples( TRUE )!=S_OK){
		ms_error("Error enabling buffer sampling" );
		return -1;
	}
	if (sampleGrabber->SetCallBack(this, 0 )!=S_OK){
		ms_error("Error setting callback interface for grabbing" );
		return -1;
	}
	SharedComPtr< IPin > grabberIn = findPin( _grabberBase, PINDIR_INPUT, 0 );
	if (grabberIn == NULL){
		ms_error("Error getting input of sample grabber");
		return -1;
	}
	SharedComPtr< IPin > grabberOut = findPin( _grabberBase, PINDIR_OUTPUT, 0 );
	if (grabberOut==NULL){
		ms_error("Error getting output of sample grabber" );
		return -1;
	}
	if ((_nullRenderer=makeShared<IBaseFilter>(CLSID_NullRenderer, IID_IBaseFilter))==NULL){
		ms_error("Error creating Null Renderer" );
		return -1;
	}
	if (_graphBuilder->AddFilter( _nullRenderer.get(), L"Sink" )!=S_OK){
		ms_error("Error adding null renderer to filter graph" );
		return -1;
	}
	SharedComPtr< IPin > nullIn = findPin( _nullRenderer, PINDIR_INPUT, 0 );
	if (_graphBuilder->Connect( sourceOut.get(), grabberIn.get() )!=S_OK){
		ms_error("Error connecting source to sample grabber" );
		return -1;
	}
	if (_graphBuilder->Connect( grabberOut.get(), nullIn.get() )!=S_OK){
		ms_error("Error connecting sample grabber to sink" );
		return -1;
	}
	ms_message("Directshow graph is now ready to run.");

	if (_graphBuilder->QueryInterface( IID_IMediaControl,
						(void **)&_mediaControl )!=S_OK){
		ms_error("Error requesting media control interface" );
		return -1;
	}
	if (_graphBuilder->QueryInterface( IID_IMediaEvent,
			(void **)&_mediaEvent )!=S_OK){
		ms_error("Error requesting event interface" );
		return -1;
	}
	_ready=true;
	return 0;
}

int DSCapture::startDshowGraph(){
	if (!_ready) {
		if (createDshowGraph()!=0) return -1;
	}
	HRESULT r=_mediaControl->Run();
	if (r!=S_OK && r!=S_FALSE){
		ms_error("Error starting graph (%i)",(int)r);
		return -1;
	}
	ms_message("Graph started");
	return 0;
}

void DSCapture::stopAndClean(){
	if (_mediaControl.get()!=NULL){
		HRESULT r;
		r=_mediaControl->Stop();
		if (r!=S_OK){
			ms_error("msdscap: Could not stop graph !");
			fflush(NULL);
		}
    	_graphBuilder->RemoveFilter(_source.get());
    	_graphBuilder->RemoveFilter(_grabberBase.get());
		_graphBuilder->RemoveFilter(_nullRenderer.get());
	}
	_source.reset();
	_grabberBase.reset();
	_nullRenderer.reset();
	_mediaControl.reset();
	_mediaEvent.reset();
	_graphBuilder.reset();
	CoUninitialize();
	ms_mutex_lock(&_mutex);
	flushq(&_rq,0);
	ms_mutex_unlock(&_mutex);
	_ready=false;
}

bool DSCapture::isTimeToSend(uint64_t ticker_time){
	if (_frame_count==-1){
		_start_time=(float)ticker_time;
		_frame_count=0;
	}
	int cur_frame=(int)(((float)ticker_time-_start_time)*_fps/1000.0);
	if (cur_frame>_frame_count){
		_frame_count++;
		return true;
	}
	return false;
}


SharedComPtr< IPin > DSCapture::findPin( SharedComPtr<IBaseFilter> &filter, PIN_DIRECTION direction, int index ){
	int i=0;
	SharedComPtr< IPin > pin_it;
	SharedComPtr< IEnumPins > enumerator;
	
	if (filter->EnumPins( &enumerator )!=S_OK){
		ms_error("Error getting pin enumerator" );
		return SharedComPtr< IPin >();
	}
	while(enumerator->Next(1,&pin_it,NULL)==S_OK){
		PIN_DIRECTION it_dir;
		if (pin_it->QueryDirection(&it_dir)==S_OK){
			if (it_dir==direction){
				if (index==i) return pin_it;
				i++;
			}
		}else ms_warning("Failed to query direction for pin.");
	}
	ms_error("There is no %s pin %i on this dshow filter",direction==PINDIR_INPUT ? "input" : "output", index);
	return SharedComPtr< IPin >();
}

static void dscap_preprocess(MSFilter * obj){
	DSCapture *s=(DSCapture*)obj->data;
	ms_average_fps_init(&s->avgfps,"msdscap: fps=%f");
	s->startDshowGraph();
}

static void dscap_postprocess(MSFilter * obj){
	DSCapture *s=(DSCapture*)obj->data;
	s->stopAndClean();
}

static void dscap_process(MSFilter * obj){
	DSCapture *s=(DSCapture*)obj->data;
	mblk_t *m;
	uint32_t timestamp;

	if (s->isTimeToSend(obj->ticker->time)){
		mblk_t *om=NULL;
		/*keep the most recent frame if several frames have been captured */
		while((m=s->readFrame())!=NULL){
			if (om!=NULL) freemsg(om);
			om=m;
		}
		if (om!=NULL){
			timestamp=(uint32_t)(obj->ticker->time*90);/* rtp uses a 90000 Hz clockrate for video*/
			mblk_set_timestamp_info(om,timestamp);
			ms_queue_put(obj->outputs[0],om);
			ms_average_fps_update(&s->avgfps,obj->ticker->time);
		}
	}
}

static int dscap_set_fps(MSFilter *f, void *arg){
	DSCapture *s=(DSCapture*)f->data;
	s->setFps(*(float*)arg);
	return 0;
}

static int dscap_get_fps(MSFilter *f, void *arg){
	DSCapture *s=(DSCapture*)f->data;
	if (f->ticker){
		*((float*)arg)=ms_average_fps_get(&s->avgfps);
	} else {
		*((float*)arg)=s->getFps();
	}
	return 0;
}

static int dscap_get_pix_fmt(MSFilter *f,void *arg){
	DSCapture *s=(DSCapture*)f->data;
	*((MSPixFmt*)arg)=s->getPixFmt();
	return 0;
}

static int dscap_set_vsize(MSFilter *f, void *arg){
	DSCapture *s=(DSCapture*)f->data;
	s->setVSize(*((MSVideoSize*)arg));
	return 0;
}

static int dscap_get_vsize(MSFilter *f, void *arg){
	DSCapture *s=(DSCapture*)f->data;
	MSVideoSize *vs=(MSVideoSize*)arg;
	*vs=s->getVSize();
	return 0;
}

static MSFilterMethod methods[]={
	{	MS_FILTER_SET_FPS	,	dscap_set_fps	},
	{	MS_FILTER_GET_FPS	,	dscap_get_fps	},
	{	MS_FILTER_GET_PIX_FMT	,	dscap_get_pix_fmt	},
	{	MS_FILTER_SET_VIDEO_SIZE, dscap_set_vsize	},
	{	MS_FILTER_GET_VIDEO_SIZE, dscap_get_vsize	},
	{	0								,	NULL			}
};

MSFilterDesc ms_dscap_desc={
	MS_DSCAP_ID,
	"MSDsCap",
	N_("A webcam grabber based on directshow."),
	MS_FILTER_OTHER,
	NULL,
	0,
	1,
	dscap_init,
	dscap_preprocess,
	dscap_process,
	dscap_postprocess,
	dscap_uninit,
	methods
};


static void ms_dshow_detect(MSWebCamManager *obj);
static MSFilter * ms_dshow_create_reader(MSWebCam *obj){
	MSFilter *f=ms_factory_create_filter_from_desc(ms_web_cam_get_factory(obj), &ms_dscap_desc);
	DSCapture *s=(DSCapture*)f->data;
	s->setDeviceIndex((int)obj->data);
	return f;
}

#ifdef _MSC_VER
extern "C" {
#endif
MSWebCamDesc ms_dshow_cam_desc={
	"Directshow capture",
	&ms_dshow_detect,
	NULL,
	&ms_dshow_create_reader,
	NULL
};
#ifdef _MSC_VER
}
#endif

static void ms_dshow_detect(MSWebCamManager *obj){
	SharedComPtr<IPropertyBag> pBag;
	
	CoInitialize(NULL);
 
	SharedComPtr< ICreateDevEnum > createDevEnum;
	if ((createDevEnum=makeShared<ICreateDevEnum>( CLSID_SystemDeviceEnum,
                                    IID_ICreateDevEnum))==NULL){
		ms_error( "Could not create device enumerator" );
		return ;
	}
 	SharedComPtr< IEnumMoniker > enumMoniker;
	if (createDevEnum->CreateClassEnumerator( CLSID_VideoInputDeviceCategory, &enumMoniker, 0 )!=S_OK){
		ms_error("Fail to create class enumerator.");
		return;
	}

	ULONG fetched = 0;
	SharedComPtr< IMoniker > moniker;
	for ( int i=0;enumMoniker->Next( 1, &moniker, &fetched )==S_OK;++i ) {
		VARIANT var;
		if (moniker->BindToStorage( 0, 0, IID_IPropertyBag, (void**) &pBag )!=S_OK)
			continue;
		VariantInit(&var);
		if (pBag->Read( L"FriendlyName", &var, NULL )!=S_OK)
			continue;
		char szName[256];
		WideCharToMultiByte(CP_ACP,0,var.bstrVal,-1,szName,256,0,0);
		MSWebCam *cam=ms_web_cam_new(&ms_dshow_cam_desc);
		cam->name=ms_strdup(szName);
		cam->data=(void*)i;
		ms_web_cam_manager_prepend_cam(obj,cam);
		VariantClear(&var);
	}
}



