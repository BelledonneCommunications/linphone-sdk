/* msdscap-mingw - mediastreamer2 plugin for video capture using directshow 
   Unlike winvideods.c filter, the following source code compiles with mingw.
   winvideods.c requires visual studio to build.

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
/* 
This plugin has been written by Simon Morlat based on the work made by
Jan Wedekind, posted on mingw tracker here:
http://sourceforge.net/tracker/index.php?func=detail&aid=1819367&group_id=2435&atid=302435
He wrote all the declarations missing to get directshow capture working
with mingw, and provided a demo code that worked great with minimal code.
*/

#include <mediastreamer2/mswebcam.h>
#include <mediastreamer2/msfilter.h>
#include <mediastreamer2/msticker.h>
#include <mediastreamer2/msvideo.h>

#include <basetyps.h>
#include <initguid.h>
#include <ocidl.h>

#undef CINTERFACE

#define MAX_FILTER_NAME 128
#define MAX_PIN_NAME 128

DEFINE_GUID( CLSID_VideoInputDeviceCategory, 0x860BB310, 0x5D01,
             0x11d0, 0xBD, 0x3B, 0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86);
DEFINE_GUID( CLSID_SystemDeviceEnum, 0x62BE5D10, 0x60EB, 0x11d0,
             0xBD, 0x3B, 0x00, 0xA0, 0xC9, 0x11, 0xCE, 0x86 );
DEFINE_GUID( CLSID_FilterGraph, 0xe436ebb3, 0x524f, 0x11ce,
             0x9f, 0x53, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( CLSID_SampleGrabber, 0xc1f400a0, 0x3f08, 0x11d3,
             0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37 );
DEFINE_GUID( CLSID_NullRenderer,0xc1f400a4, 0x3f08, 0x11d3,
             0x9f, 0x0b, 0x00, 0x60, 0x08, 0x03, 0x9e, 0x37 );

DEFINE_GUID( IID_IGraphBuilder, 0x56a868a9, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70);
DEFINE_GUID( IID_IBaseFilter, 0x56a86895, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );
DEFINE_GUID( IID_ICreateDevEnum, 0x29840822, 0x5b84, 0x11d0,
             0xbd, 0x3b, 0x00, 0xa0, 0xc9, 0x11, 0xce, 0x86 );

DEFINE_GUID( IID_IEnumPins, 0x56a86892, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );

DEFINE_GUID( IID_IPin, 0x56a86891, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );
DEFINE_GUID( IID_ISampleGrabber, 0x6b652fff, 0x11fe, 0x4fce,
             0x92, 0xad, 0x02, 0x66, 0xb5, 0xd7, 0xc7, 0x8f );
DEFINE_GUID( IID_ISampleGrabberCB, 0x0579154a, 0x2b53, 0x4994,
             0xb0, 0xd0, 0xe7, 0x73, 0x14, 0x8e, 0xff, 0x85 );
DEFINE_GUID( IID_IMediaEvent, 0x56a868b6, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );
DEFINE_GUID( IID_IMediaControl, 0x56a868b1, 0x0ad4, 0x11ce,
             0xb0, 0x3a, 0x00, 0x20, 0xaf, 0x0b, 0xa7, 0x70 );
DEFINE_GUID( IID_IAMStreamConfig, 0xc6e13340, 0x30ac, 0x11d0,
             0xa1, 0x8c, 0x00, 0xa0, 0xc9, 0x11, 0x89, 0x56 );
DEFINE_GUID( MEDIATYPE_Video, 0x73646976, 0x0000, 0x0010,
             0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 );

typedef interface IBaseFilter IBaseFilter;
typedef interface IReferenceClock IReferenceClock;
typedef interface IFilterGraph IFilterGraph;

typedef LONGLONG REFERENCE_TIME;
typedef long OAFilterState;
typedef LONG_PTR OAEVENT;

/*DECLARE_ENUMERATOR_ is defined in mingw, but maybe not in VisualStudio.*/
#ifndef DECLARE_ENUMERATOR_
#define DECLARE_ENUMERATOR_(I,T) \
	DECLARE_INTERFACE_(I,IUnknown) \
	{ \
		STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE; \
		STDMETHOD_(ULONG,AddRef)(THIS) PURE; \
		STDMETHOD_(ULONG,Release)(THIS) PURE; \
		STDMETHOD(Next)(THIS_ ULONG,T*,ULONG*) PURE;\
		STDMETHOD(Skip)(THIS_ ULONG) PURE; \
		STDMETHOD(Reset)(THIS) PURE; \
		STDMETHOD(Clone)(THIS_ I**) PURE; \
	}
#endif


typedef struct tagVIDEOINFOHEADER {
	RECT rcSource;
	RECT rcTarget;
	DWORD dwBitRate;
	DWORD dwBitErrorRate;
	REFERENCE_TIME AvgTimePerFrame;
	BITMAPINFOHEADER bmiHeader;
} VIDEOINFOHEADER;

typedef struct _AMMediaType {
	GUID majortype;
	GUID subtype;
	BOOL bFixedSizeSamples;
	BOOL bTemporalCompression;
	ULONG lSampleSize;
	GUID formattype;
	IUnknown *pUnk;
	ULONG cbFormat;
	BYTE *pbFormat;
} AM_MEDIA_TYPE;

DECLARE_ENUMERATOR_(IEnumMediaTypes,AM_MEDIA_TYPE*);

typedef struct _VIDEO_STREAM_CONFIG_CAPS
{
	GUID guid;
	ULONG VideoStandard;
	SIZE InputSize;
	SIZE MinCroppingSize;
	SIZE MaxCroppingSize;
	int CropGranularityX;
	int CropGranularityY;
	int CropAlignX;
	int CropAlignY;
	SIZE MinOutputSize;
	SIZE MaxOutputSize;
	int OutputGranularityX;
	int OutputGranularityY;
	int StretchTapsX;
	int StretchTapsY;
	int ShrinkTapsX;
	int ShrinkTapsY;
	LONGLONG MinFrameInterval;
	LONGLONG MaxFrameInterval;
	LONG MinBitsPerSecond;
	LONG MaxBitsPerSecond;
} VIDEO_STREAM_CONFIG_CAPS;

typedef enum _FilterState {
	State_Stopped,
	State_Paused,
	State_Running
} FILTER_STATE;


//http://msdn.microsoft.com/en-us/library/windows/desktop/dd375787%28v=vs.85%29.aspx
typedef struct _FilterInfo {
	WCHAR achName[MAX_FILTER_NAME]; 
	IFilterGraph *pGraph;
} FILTER_INFO;

typedef enum _PinDirection {
	PINDIR_INPUT,
	PINDIR_OUTPUT
} PIN_DIRECTION;

typedef struct _PinInfo {
	IBaseFilter *pFilter;
	PIN_DIRECTION dir;
	WCHAR achName[MAX_PIN_NAME];
} PIN_INFO;

//http://msdn.microsoft.com/en-us/library/windows/desktop/dd390397%28v=vs.85%29.aspx
#undef INTERFACE
#define INTERFACE IPin
DECLARE_INTERFACE_(IPin,IUnknown)
{
	STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;
	STDMETHOD(Connect)(THIS_ IPin*,const AM_MEDIA_TYPE*) PURE;
	STDMETHOD(ReceiveConnection)(THIS_ IPin*,const AM_MEDIA_TYPE*) PURE;
	STDMETHOD(Disconnect)(THIS) PURE;
	STDMETHOD(ConnectedTo)(THIS_ IPin**) PURE;
	STDMETHOD(ConnectionMediaType)(THIS_ AM_MEDIA_TYPE*) PURE;
	STDMETHOD(QueryPinInfo)(THIS_ PIN_INFO*) PURE;
	STDMETHOD(QueryDirection)(THIS_ PIN_DIRECTION*) PURE;
};
#undef INTERFACE

DECLARE_ENUMERATOR_(IEnumPins,IPin*);

#define INTERFACE IMediaEvent
DECLARE_INTERFACE_(IMediaEvent,IDispatch)
{
  STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;
  STDMETHOD_(ULONG,Release)(THIS) PURE;
  STDMETHOD(GetEventHandle)(THIS_ OAEVENT*) PURE;
  STDMETHOD(GetEvent)(THIS_ long*,LONG_PTR,LONG_PTR,long) PURE;
  STDMETHOD(WaitForCompletion)(THIS_ long,long*) PURE;
  STDMETHOD(CancelDefaultHandling)(THIS_ long) PURE;
  STDMETHOD(RestoreDefaultHandling)(THIS_ long) PURE;
  STDMETHOD(FreeEventParams)(THIS_ long,LONG_PTR,LONG_PTR) PURE;
};
#undef INTERFACE



#define INTERFACE IMediaControl
DECLARE_INTERFACE_(IMediaControl,IDispatch)
{
  STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;
  STDMETHOD_(ULONG,Release)(THIS) PURE;
  STDMETHOD(Run)(THIS) PURE;
  STDMETHOD(Pause)(THIS) PURE;
  STDMETHOD(Stop)(THIS) PURE;
  STDMETHOD(GetState)(THIS_ LONG,OAFilterState*) PURE;
  STDMETHOD(RenderFile)(THIS_ BSTR) PURE;
  STDMETHOD(AddSourceFilter)(THIS_ BSTR,IDispatch**) PURE;
  STDMETHOD(get_FilterCollection)(THIS_ IDispatch**) PURE;
  STDMETHOD(get_RegFilterCollection)(THIS_ IDispatch**) PURE;
  STDMETHOD(StopWhenReady)(THIS) PURE;
};
#undef INTERFACE

#define INTERFACE IAMStreamConfig
DECLARE_INTERFACE_(IAMStreamConfig,IUnknown)
{
  STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;
  STDMETHOD_(ULONG,Release)(THIS) PURE;
  STDMETHOD(SetFormat)(THIS_ AM_MEDIA_TYPE*) PURE;
  STDMETHOD(GetFormat)(THIS_ AM_MEDIA_TYPE**) PURE;
  STDMETHOD(GetNumberOfCapabilities)(THIS_ int*,int*) PURE;
  STDMETHOD(GetStreamCaps)(THIS_ int,AM_MEDIA_TYPE**,BYTE*) PURE;
};
#undef INTERFACE

#define INTERFACE IMediaFilter
DECLARE_INTERFACE_(IMediaFilter,IPersist){
	STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;
	STDMETHOD(Stop)(THIS) PURE;
	STDMETHOD(Pause)(THIS) PURE;
	STDMETHOD(Run)(THIS_ REFERENCE_TIME) PURE;
	STDMETHOD(GetState)(THIS_ DWORD,FILTER_STATE*) PURE;
	STDMETHOD(SetSyncSource)(THIS_ IReferenceClock*) PURE;
	STDMETHOD(GetSyncSource)(THIS_ IReferenceClock**) PURE;
};
#undef INTERFACE

#define INTERFACE IBaseFilter
DECLARE_INTERFACE_(IBaseFilter,IMediaFilter)
{
  STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;
  STDMETHOD_(ULONG,Release)(THIS) PURE;
  STDMETHOD(EnumPins)(THIS_ IEnumPins**) PURE;
  STDMETHOD(FindPin)(THIS_ LPCWSTR,IPin**) PURE;
  STDMETHOD(QueryFilterInfo)(THIS_ FILTER_INFO*) PURE;
  STDMETHOD(JoinFilterGraph)(THIS_ IFilterGraph*,LPCWSTR) PURE;
  STDMETHOD(QueryVendorInfo)(THIS_ LPWSTR*) PURE;
};
#undef INTERFACE

DECLARE_ENUMERATOR_(IEnumFilters,IBaseFilter*);

#define INTERFACE IFilterGraph
DECLARE_INTERFACE_(IFilterGraph,IUnknown)
{
  STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;
  STDMETHOD_(ULONG,Release)(THIS) PURE;
  STDMETHOD(AddFilter)(THIS_ IBaseFilter*,LPCWSTR) PURE;
  STDMETHOD(RemoveFilter)(THIS_ IBaseFilter*) PURE;
  STDMETHOD(EnumFilters)(THIS_ IEnumFilters**) PURE;
  STDMETHOD(FindFilterByName)(THIS_ LPCWSTR,IBaseFilter**) PURE;
  STDMETHOD(ConnectDirect)(THIS_ IPin*,IPin*,const AM_MEDIA_TYPE*) PURE;
  STDMETHOD(Reconnect)(THIS_ IPin*) PURE;
  STDMETHOD(Disconnect)(THIS_ IPin*) PURE;
  STDMETHOD(SetDefaultSyncSource)(THIS) PURE;
};
#undef INTERFACE

#define INTERFACE IGraphBuilder
DECLARE_INTERFACE_(IGraphBuilder,IFilterGraph)
{
  STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;
  STDMETHOD_(ULONG,Release)(THIS) PURE;
  STDMETHOD(Connect)(THIS_ IPin*,IPin*) PURE;
  STDMETHOD(Render)(THIS_ IPin*) PURE;
  STDMETHOD(RenderFile)(THIS_ LPCWSTR,LPCWSTR) PURE;
  STDMETHOD(AddSourceFilter)(THIS_ LPCWSTR,LPCWSTR,IBaseFilter**) PURE;
  STDMETHOD(SetLogFile)(THIS_ DWORD_PTR) PURE;
  STDMETHOD(Abort)(THIS) PURE;
  STDMETHOD(ShouldOperationContinue)(THIS) PURE;
};
#undef INTERFACE

#define INTERFACE ICreateDevEnum
DECLARE_INTERFACE_(ICreateDevEnum,IUnknown)
{
  STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;
  STDMETHOD_(ULONG,Release)(THIS) PURE;
  STDMETHOD(CreateClassEnumerator)(THIS_ REFIID,IEnumMoniker**,DWORD) PURE;
};
#undef INTERFACE

#define INTERFACE IMediaSample
DECLARE_INTERFACE_(IMediaSample,IUnknown)
{
  STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;
  STDMETHOD_(ULONG,Release)(THIS) PURE;
  STDMETHOD(GetPointer)(THIS_ BYTE **) PURE;
  STDMETHOD_(long, GetSize)(THIS) PURE;
};

#undef INTERFACE

#define INTERFACE ISampleGrabberCB
DECLARE_INTERFACE_(ISampleGrabberCB,IUnknown)
{
  STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
  STDMETHOD_(ULONG,AddRef)(THIS) PURE;
  STDMETHOD_(ULONG,Release)(THIS) PURE;
  STDMETHOD(SampleCB)(THIS_ double,IMediaSample*) PURE;
  STDMETHOD(BufferCB)(THIS_ double,BYTE*,long) PURE;
};
#undef INTERFACE

#define INTERFACE ISampleGrabber
DECLARE_INTERFACE_(ISampleGrabber,IUnknown){
	STDMETHOD(QueryInterface)(THIS_ REFIID,PVOID*) PURE;
	STDMETHOD_(ULONG,AddRef)(THIS) PURE;
	STDMETHOD_(ULONG,Release)(THIS) PURE;
	STDMETHOD(SetOneShot)(THIS_ BOOL) PURE;
	STDMETHOD(SetMediaType)(THIS_ const AM_MEDIA_TYPE*) PURE;
	STDMETHOD(GetConnectedMediaType)(THIS_ AM_MEDIA_TYPE*) PURE;
	STDMETHOD(SetBufferSamples)(THIS_ BOOL) PURE;
	STDMETHOD(GetCurrentBuffer)(THIS_ long*,long*) PURE;
	STDMETHOD(GetCurrentSample)(THIS_ IMediaSample**) PURE;
	STDMETHOD(SetCallBack)(THIS_ ISampleGrabberCB *,long) PURE;
};
#undef INTERFACE



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
	MSPixFmt getPixFmt(){
		if (!_ready) createDshowGraph(); /* so that _pixfmt is updated*/
		return _pixfmt;
	}
	void setDeviceIndex(int index){
		_devid=index;
	}
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

static void dummy(void*p){
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
	mblk_t *m;
	if (_pixfmt!=MS_RGB24_REV){
		m=esballoc(p,size,0,dummy);
		m->b_wptr+=size;
	}else{
		/* make a copy for BGR24 buffers into a new end-padded buffer, because swscale
		is doing invalid reads past the end of the original buffers */
		m=allocb(size+128,0);
		memcpy(m->b_wptr,p,size);
		m->b_wptr+=size;
	}
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
			mediaType->cbFormat != 0 ) {
			VIDEOINFOHEADER *infoHeader = (VIDEOINFOHEADER*)mediaType->pbFormat;
			ms_message("Seeing format %ix%i %s",infoHeader->bmiHeader.biWidth,infoHeader->bmiHeader.biHeight,
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
		ms_error("Error starting graph (%i)",r);
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
		}
	}
}

static int dscap_set_fps(MSFilter *f, void *arg){
	DSCapture *s=(DSCapture*)f->data;
	s->setFps(*(float*)arg);
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
	MSFilter *f=ms_filter_new_from_desc(&ms_dscap_desc);
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



