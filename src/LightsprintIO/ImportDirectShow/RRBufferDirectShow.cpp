// --------------------------------------------------------------------------
// Video load & play using DirectShow.
// Copyright (C) 2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_DIRECTSHOW


#include "RRBufferDirectShow.h"
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include <windows.h>
#include <dshow.h> // not found? install Windows SDK or delete #define SUPPORT_DIRECTSHOW in supported_formats.h
#include <Qedit.h> // ISampleGrabber


#pragma comment(lib,"strmiids.lib") // not found? install Windows SDK or delete #define SUPPORT_DIRECTSHOW in supported_formats.h

struct __declspec(  uuid("{71771540-2017-11cf-ae26-0020afd79767}")  ) CLSID_Sampler;

using namespace rr;

static unsigned char g_classHeader[16];
static size_t g_classHeaderSize = 0;

//! Buffer for streamed video data, decoded by DirectShow.
//! Buffer is refcounted via "createReference()" and "delete".
class RRBufferDirectShow : public RRBuffer, public ISampleGrabberCB
{
public:

	//! First in buffer's lifetime.
	static RRBuffer* load(const char *filename)
	{
		RRBufferDirectShow* video = new RRBufferDirectShow(filename);
		if (!video->getWidth() || !video->getHeight())
			RR_SAFE_DELETE(video);
		return video;
	}

	//! Second in buffer's lifetime.
	void* operator new(std::size_t n)
	{
		return malloc(n);
	};

	//! Third in buffer's lifetime. If constructor fails, 0*0 buffer is created.
	RRBufferDirectShow(const char* _filename)
	{
		refCount = 1;
		filename = _filename;
		version = rand();
		front = NULL;
		back = NULL;
		backReady = false;
		width = 0;
		height = 0;
		duration = -1; // unlimited
		dsGraph = NULL;
		dsControl = NULL;
		dsEvent = NULL;
		dsSeeking = NULL;
		dsAudio = NULL;
		dsGrabberFilter = NULL;
		dsGrabber = NULL;

		HRESULT hr = CoInitialize(NULL);

		if (!_filename)
			return;

		hr = CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC, IID_IGraphBuilder, (void **)&dsGraph);
		if (dsGraph)
		{
			// query interfaces
			hr = dsGraph->QueryInterface(IID_IMediaControl, (void **)&dsControl);
			hr = dsGraph->QueryInterface(IID_IMediaEvent, (void **)&dsEvent);
			hr = dsGraph->QueryInterface(IID_IMediaSeeking, (void **)&dsSeeking);
			hr = dsGraph->QueryInterface(IID_IBasicAudio, (void **)&dsAudio);

			// setup graph
			hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&dsGrabberFilter);
			if (dsGrabberFilter)
			{
				hr = dsGraph->AddFilter(dsGrabberFilter, L"GrabberFilter");
				hr = dsGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&dsGrabber);
				if (dsGrabber)
				{
					AM_MEDIA_TYPE mt;
					ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
					mt.majortype = MEDIATYPE_Video;
					mt.subtype = MEDIASUBTYPE_RGB24;
					mt.formattype = FORMAT_VideoInfo;
					hr = dsGrabber->SetMediaType(&mt);
				}
			}

			// play file
			{
				unsigned len = (unsigned)(strlen(_filename)+1);
				wchar_t* filenamew = new wchar_t[len];
				for (unsigned i=0;i<len;i++) filenamew[i] = _filename[i];
				hr = dsGraph->RenderFile(filenamew, NULL);
				delete[] filenamew;
			}

			// disable auto show
			{
				IVideoWindow* dsWindow = NULL;
				hr = dsGraph->QueryInterface(IID_IVideoWindow, (void**)&dsWindow);
				if (dsWindow)
				{
					hr = dsWindow->put_AutoShow(OAFALSE);
					dsWindow->Release();
				}
			}

			// get video resolution
			{
				AM_MEDIA_TYPE mt;
				hr = dsGrabber->GetConnectedMediaType(&mt);
				if (SUCCEEDED(hr))
				{
					VIDEOINFOHEADER* vih = (VIDEOINFOHEADER*)mt.pbFormat;
					width = vih->bmiHeader.biWidth;
					height = vih->bmiHeader.biHeight;
					if (mt.cbFormat != 0)
					{
						CoTaskMemFree((PVOID)mt.pbFormat);
						mt.cbFormat = 0;
						mt.pbFormat = NULL;
					}
					if (mt.pUnk != NULL)
					{
						mt.pUnk->Release();
						mt.pUnk = NULL;
					}
				}
			}

			// setup grabbing
			hr = dsGrabber->SetOneShot(FALSE);
			hr = dsGrabber->SetBufferSamples(FALSE);
			hr = dsGrabber->SetCallback(this,1);

			// alloc buffers
			if (width && height)
			{
				front = new unsigned char[width*height*3];
				back = new unsigned char[width*height*3];
				// init to gray
				memset(front,128,width*height*3);
			}
		}
		
		/*/ video from camera
		IPin*           rnd  = 0; hr = sampler->FindPin(L"In", &rnd);
		ICreateDevEnum* devs = 0; hr = CoCreateInstance (CLSID_SystemDeviceEnum, 0, CLSCTX_INPROC, IID_ICreateDevEnum, (void **) &devs);
		IEnumMoniker*   cams = 0; hr = devs?devs->CreateClassEnumerator (CLSID_VideoInputDeviceCategory, &cams, 0):0;
		IMoniker*       mon  = 0; hr = cams?cams->Next (1, &mon, 0):0;
		IBaseFilter*    cam  = 0; hr = mon?mon->BindToObject(0,0,IID_IBaseFilter, (void**)&cam):0;
		IEnumPins*      pins = 0; hr = cam?cam->EnumPins(&pins):0;
		IPin*           cap  = 0; hr = pins?pins->Next(1,&cap, 0):0;
								  hr = graph->AddFilter(cam, L"Capture Source");
		hr = graph->Connect(cap,rnd);
		hr = graph->Render(cap);
		hr = ctrl->Run();
		*/
	}

	//! When deleting buffer, this is called first.
	virtual ~RRBufferDirectShow()
	{
		if (--refCount)
		{
			// skip destructor
			filename._skipDestructor();
			// store instance header before destruction
			g_classHeaderSize = RR_MIN(16,(char*)&filename-(char*)this); // filename must be first member variable in RRBuffer
			memcpy(g_classHeader,this,g_classHeaderSize);
		}
		else
		{
			// destruct
			RR_SAFE_RELEASE(dsGrabber);
			RR_SAFE_RELEASE(dsGrabberFilter);
			RR_SAFE_RELEASE(dsAudio);
			RR_SAFE_RELEASE(dsSeeking);
			RR_SAFE_RELEASE(dsEvent);
			RR_SAFE_RELEASE(dsControl);
			RR_SAFE_RELEASE(dsGraph);
			CoUninitialize();
			delete[] back;
			delete[] front;
		}
	}

	//! When deleting buffer, this is called last.
	void operator delete(void* p, std::size_t n)
	{
		if (p)
		{
			RRBufferDirectShow* b = (RRBufferDirectShow*)p;
			if (b->refCount)
			{
				// fix instance after destructor, restore first 4 or 8 bytes from backup
				// (it's not safe to restore from local static RRBufferDirectShow)
				memcpy(b,g_classHeader,g_classHeaderSize);
			}
			else
			{
				// delete instance after destructor
				free(p);
			}
		}
	};

	// ================== implements RRBuffer interface ====================

	// --------- refcounting ---------

	virtual RRBuffer* createReference()
	{
		if (this)
			refCount++;
		return this;
	}
	virtual unsigned getReferenceCount()
	{
		return refCount;
	}

	// --------- type/size/format are fixed ---------

	virtual RRBufferType getType() const
	{
		return BT_2D_TEXTURE;
	}
	virtual unsigned getWidth() const
	{
		return width;
	}
	virtual unsigned getHeight() const
	{
		return height;
	}
	virtual unsigned getDepth() const
	{
		return 1;
	}
	virtual RRBufferFormat getFormat() const
	{
		return BF_BGR;
	}
	virtual bool getScaled() const
	{
		return true;
	}
	virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data)
	{
		RRReporter::report(WARN,"reset() has no effect on video buffers.\n");
		return false;
	}

	// --------- element access ---------

	void setElement(unsigned index, const RRVec4& element)
	{
		if (index>=width*height)
		{
			RRReporter::report(WARN,"setElement(%d) out of range, buffer size %d*%d=%d.\n",index,width,height,width*height);
			return;
		}
		front[3*index+0] = RR_FLOAT2BYTE(element[2]);
		front[3*index+1] = RR_FLOAT2BYTE(element[1]);
		front[3*index+2] = RR_FLOAT2BYTE(element[0]);
		version++;
	}
	RRVec4 getElement(unsigned index) const
	{
		if (index>=width*height)
		{
			RRReporter::report(WARN,"getElement(%d) out of range, buffer size %d*%d=%d.\n",index,width,height,width*height);
			return RRVec4(0);
		}
		return RRVec4(
			RR_BYTE2FLOAT(front[index*3+2]),
			RR_BYTE2FLOAT(front[index*3+1]),
			RR_BYTE2FLOAT(front[index*3+0]),
			1);
	}
	RRVec4 getElement(const RRVec3& direction) const
	{
		return getElement(((unsigned)(direction[0]*width)%width) + ((unsigned)(direction[1]*height)%height) * width);
	}

	// --------- whole buffer access ---------

	virtual unsigned char* lock(RRBufferLock lock)
	{
		return front;
	}
	virtual void unlock()
	{
	}

	// --------- video access ---------

	virtual bool update()
	{
		// swap front and back buffers
		if (backReady)
		{
			backReady = false;
			unsigned char* tmp = front;
			front = back;
			back = tmp;
			version++;
			return true;
		}
		// loop video (must be called at least when we run out of data)
		if (dsEvent)
		{
			LONG ev;
			LONG_PTR p1,p2;
			if (dsEvent->GetEvent(&ev,&p1,&p2,0)==S_OK)
			{
				if (ev==EC_COMPLETE)
				{
					LONGLONG nula = 0;
					dsSeeking->SetPositions(&nula,AM_SEEKING_AbsolutePositioning,NULL,AM_SEEKING_NoPositioning);
				}
				dsEvent->FreeEventParams(ev,p1,p2);
			}
		}
		return false;
	}
	virtual void play()
	{
		if (dsControl)
			dsControl->Run();
	}
	virtual void stop()
	{
		if (dsControl)
			dsControl->Stop();
	}
	virtual void pause()
	{
		if (dsControl)
			dsControl->Pause();
	}
	virtual void setVolume(int volume)
	{
		if (dsAudio)
			dsAudio->put_Volume(volume); //-10000(ticho)..0(=100%)
	}
	virtual void setBalance(int balance)
	{
		if (dsAudio)
			dsAudio->put_Balance(balance); //-10000..10000
	}
	virtual float getDuration() const
	{
		return -1;
	}

	// ================== implement ISampleGrabberCB interface ====================

	STDMETHODIMP SampleCB(double _sampleTime, IMediaSample *_sample)
	{
        return S_OK;
	}
	STDMETHODIMP BufferCB(double _sampleTime, BYTE *_data, long _dataSize)
	{
		memcpy(back,_data,RR_MIN(width*height*3,(unsigned)_dataSize));
		backReady = true;
		return S_OK;
	}
	STDMETHOD(QueryInterface)(REFIID InterfaceIdentifier, VOID** ppvObject) throw()
	{
		return E_NOINTERFACE;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef(void)
	{
	return 2;
	}
	virtual ULONG STDMETHODCALLTYPE Release(void)
	{
		return 1;
	}

private:
	//! refCount is aligned and modified only by ++ and -- in createReference() and delete.
	//! This and volatile makes it mosly thread safe, at least on x86
	//! (still we clearly say it's not thread safe)
	volatile unsigned refCount;

	unsigned char* front; ///< frontbuffer, visible via RRBuffer interface
	unsigned char* back; ///< backbuffer, callback from background thread fills it
	bool backReady; ///< set when backbuffer contains new data
	unsigned width;
	unsigned height;
	float duration;
	IGraphBuilder* dsGraph;
	IMediaControl* dsControl;
	IMediaEvent* dsEvent;
	IMediaSeeking* dsSeeking;
	IBasicAudio* dsAudio;
	IBaseFilter* dsGrabberFilter;
	ISampleGrabber* dsGrabber;
};


/////////////////////////////////////////////////////////////////////////////
//
// main

void registerLoaderDirectShow()
{
	RRBuffer::setLoader(RRBufferDirectShow::load);
}

#endif // SUPPORT_DIRECTSHOW
