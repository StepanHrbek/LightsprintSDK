// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Video load & play using DirectShow.
// - it is for Windows only (RRBufferFFmpeg is multiplatform)
// - under some conditions, it stops playing after switch to fullscreen (RRBufferFFmpeg has no problem with fullscreen)
// - in clean Windows, it supports only few codecs and fileformats (RRBufferFFmpeg supports much wider range)
// + it has better license (RRBufferFFmpeg can be built as LGPL or GPL, which is problem for some users)
// + it can play output from webcam (open filename "c@pture)
// --------------------------------------------------------------------------

#include "../supported_formats.h"
#ifdef SUPPORT_DIRECTSHOW

#include "RRBufferDirectShow.h"
#include "Lightsprint/RRBuffer.h"
#include "Lightsprint/RRDebug.h"
#include <atomic>
#include <windows.h>
#include <dshow.h> // not found? install Windows SDK or delete #define SUPPORT_DIRECTSHOW in supported_formats.h
#include "Qedit.h" // ISampleGrabber. This file used to be part of Windows SDK, but it's hard to find now, so we have our own replacement.


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

	//! First in buffer's lifetime (load from one location).
	static RRBuffer* load(const RRString& _filename)
	{
		RRBufferDirectShow* video = new RRBufferDirectShow(_filename);
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
	RRBufferDirectShow(const RRString& _filename)
	{
		refCount = 1;
		filename = _filename; // [#36] exact filename we are trying to open (we don't have a locator -> no attempts to open similar names)
		version = rand();
		front = nullptr;
		back = nullptr;
		backReady = false;
		width = 0;
		height = 0;
		duration = -1; // unlimited
		dsGraph = nullptr;
		dsControl = nullptr;
		dsEvent = nullptr;
		dsSeeking = nullptr;
		dsAudio = nullptr;
		dsGrabberFilter = nullptr;
		dsGrabber = nullptr;

		HRESULT hr = CoInitialize(nullptr);

		if (_filename.empty())
			return;

		hr = CoCreateInstance(CLSID_FilterGraph, nullptr, CLSCTX_INPROC, IID_IGraphBuilder, (void **)&dsGraph);
		if (dsGraph)
		{
			// query interfaces
			hr = dsGraph->QueryInterface(IID_IMediaControl, (void **)&dsControl);
			hr = dsGraph->QueryInterface(IID_IMediaEvent, (void **)&dsEvent);
			hr = dsGraph->QueryInterface(IID_IMediaSeeking, (void **)&dsSeeking);
			hr = dsGraph->QueryInterface(IID_IBasicAudio, (void **)&dsAudio);

			// setup graph
			hr = CoCreateInstance(CLSID_SampleGrabber, nullptr, CLSCTX_INPROC_SERVER, IID_IBaseFilter, (void**)&dsGrabberFilter);
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
	
			if (_filename=="c@pture")
			{
				// capture from video input
				IPin*           capRnd  = nullptr; hr = dsGrabberFilter->FindPin(L"In",&capRnd);
				ICreateDevEnum* capDevs = nullptr; hr = CoCreateInstance(CLSID_SystemDeviceEnum,0,CLSCTX_INPROC,IID_ICreateDevEnum,(void**)&capDevs);
				IEnumMoniker*   capCams = nullptr; hr = capDevs ? capDevs->CreateClassEnumerator(CLSID_VideoInputDeviceCategory,&capCams,0) : 0;
				IMoniker*       capMon  = nullptr; hr = capCams ? capCams->Next(1,&capMon,0) : 0;
				IBaseFilter*    capCam  = nullptr; hr = capMon ? capMon->BindToObject(0,0,IID_IBaseFilter,(void**)&capCam) : 0;
				IEnumPins*      capPins = nullptr; hr = capCam ? capCam->EnumPins(&capPins) : 0;
				IPin*           capCap  = nullptr; hr = capPins ? capPins->Next(1,&capCap,0) : 0;
				hr = dsGraph->AddFilter(capCam,L"Capture Source");
				hr = dsGraph->Connect(capCap,capRnd);
				hr = dsGraph->Render(capCap);
				RR_SAFE_RELEASE(capMon);
				RR_SAFE_RELEASE(capCams);
				RR_SAFE_RELEASE(capDevs);
				RR_SAFE_RELEASE(capCam);
				RR_SAFE_RELEASE(capPins);
				RR_SAFE_RELEASE(capCap);
				RR_SAFE_RELEASE(capRnd);
			}
			else
			{
				// play file
				hr = dsGraph->RenderFile(_filename.w_str(), nullptr);
			}

			// disable auto show
			{
				IVideoWindow* dsWindow = nullptr;
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
						mt.pbFormat = nullptr;
					}
					if (mt.pUnk != nullptr)
					{
						mt.pUnk->Release();
						mt.pUnk = nullptr;
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
				// however, if last reference remains, try to delete it from cache
				if (b->refCount==1)
					b->deleteFromCache();
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

	virtual RRBuffer* createReference() override
	{
		refCount++;
		return this;
	}
	virtual unsigned getReferenceCount() override
	{
		return refCount;
	}

	// --------- type/size/format are fixed ---------

	virtual RRBufferType getType() const override
	{
		return BT_2D_TEXTURE;
	}
	virtual unsigned getWidth() const override
	{
		return width;
	}
	virtual unsigned getHeight() const override
	{
		return height;
	}
	virtual unsigned getDepth() const override
	{
		return 1;
	}
	virtual RRBufferFormat getFormat() const override
	{
		return BF_BGR;
	}
	virtual bool getScaled() const override
	{
		return true;
	}
	virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data) override
	{
		RRReporter::report(WARN,"reset() has no effect on video buffers.\n");
		return false;
	}

	// --------- element access ---------

	virtual void setElement(unsigned index, const RRVec4& _element, const RRColorSpace* colorSpace) override
	{
		if (index>=width*height)
		{
			RRReporter::report(WARN,"setElement(%d) out of range, buffer size %d*%d=%d.\n",index,width,height,width*height);
			return;
		}
		RRVec4 element = _element;
		if (colorSpace)
			colorSpace->fromLinear(element);
		front[3*index+0] = RR_FLOAT2BYTE(element[2]);
		front[3*index+1] = RR_FLOAT2BYTE(element[1]);
		front[3*index+2] = RR_FLOAT2BYTE(element[0]);
		version++;
	}
	virtual RRVec4 getElement(unsigned index, const RRColorSpace* colorSpace) const override
	{
		if (index>=width*height)
		{
			RRReporter::report(WARN,"getElement(%d) out of range, buffer size %d*%d=%d.\n",index,width,height,width*height);
			return RRVec4(0);
		}
		RRVec4 result(
			RR_BYTE2FLOAT(front[index*3+2]),
			RR_BYTE2FLOAT(front[index*3+1]),
			RR_BYTE2FLOAT(front[index*3+0]),
			1);
		if (colorSpace)
			colorSpace->toLinear(result);
		return result;
	}
	virtual RRVec4 getElementAtPosition(const RRVec3& position, const RRColorSpace* colorSpace, bool interpolated) const override
	{
		return getElement(((unsigned)(position[0]*width)%width) + ((unsigned)(position[1]*height)%height) * width, colorSpace);
	}
	virtual RRVec4 getElementAtDirection(const RRVec3& direction, const RRColorSpace* colorSpace) const override
	{
		// 360*180 degree panorama (equirectangular projection)
		unsigned index = ((unsigned)( (asin(direction.y/direction.length())*(1.0f/RR_PI)+0.5f) * height) % height) * width;
		float d = direction.x*direction.x+direction.z*direction.z;
		if (d)
		{
			float sin_angle = direction.x/sqrt(d);
			float angle = asin(sin_angle);
			if (direction.z<0) angle = (rr::RRReal)(RR_PI-angle);
			index += (unsigned)( (angle*(-0.5f/RR_PI)+0.75f) * width) % width;
		}
		return getElement(index, colorSpace);
	}

	// --------- whole buffer access ---------

	virtual unsigned char* lock(RRBufferLock lock) override
	{
		return front;
	}
	virtual void unlock() override
	{
	}

	// --------- video access ---------

	virtual bool update() override
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
					dsSeeking->SetPositions(&nula,AM_SEEKING_AbsolutePositioning,nullptr,AM_SEEKING_NoPositioning);
				}
				dsEvent->FreeEventParams(ev,p1,p2);
			}
		}
		return false;
	}
	virtual void play() override
	{
		if (dsControl)
			dsControl->Run();
	}
	virtual void stop() override
	{
		if (dsControl)
			dsControl->Stop();
	}
	virtual void pause() override
	{
		if (dsControl)
			dsControl->Pause();
	}
	virtual void seek(float secondsFromStart) override
	{
		if (dsSeeking)
		{
			LONGLONG pos = (LONGLONG)(secondsFromStart*1e7f); // default unit = 100 nanoseconds, can be changed with SetTimeFormat
			dsSeeking->SetPositions(&pos,AM_SEEKING_AbsolutePositioning,nullptr,AM_SEEKING_NoPositioning);
		}
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
	virtual float getDuration() const override
	{
		if (dsSeeking)
		{
			LONGLONG pos = 0; // default unit = 100 nanoseconds, can be changed with SetTimeFormat
			if (dsSeeking->GetStopPosition(&pos)==S_OK)
				return pos*1e-7f;
		}
		return -1;
	}

	// ================== implement ISampleGrabberCB interface ====================

	STDMETHODIMP SampleCB(double _sampleTime, IMediaSample *_sample) override
	{
		return S_OK;
	}
	STDMETHODIMP BufferCB(double _sampleTime, BYTE *_data, long _dataSize) override
	{
		memcpy(back,_data,RR_MIN(width*height*3,(unsigned)_dataSize));
		backReady = true;
		return S_OK;
	}
	STDMETHOD(QueryInterface)(REFIID InterfaceIdentifier, VOID** ppvObject) throw() override
	{
		return E_NOINTERFACE;
	}
	virtual ULONG STDMETHODCALLTYPE AddRef(void) override
	{
	return 2;
	}
	virtual ULONG STDMETHODCALLTYPE Release(void) override
	{
		return 1;
	}

private:
	std::atomic<unsigned> refCount;

	unsigned char*  front; ///< frontbuffer, visible via RRBuffer interface
	unsigned char*  back; ///< backbuffer, callback from background thread fills it
	bool            backReady; ///< set when backbuffer contains new data
	unsigned        width;
	unsigned        height;
	float           duration;

	IGraphBuilder*  dsGraph;
	IMediaControl*  dsControl;
	IMediaEvent*    dsEvent;
	IMediaSeeking*  dsSeeking;
	IBasicAudio*    dsAudio;
	IBaseFilter*    dsGrabberFilter;
	ISampleGrabber* dsGrabber;
};


/////////////////////////////////////////////////////////////////////////////
//
// main

void registerLoaderDirectShow()
{
	// We list only several important fileformats here.
	// If you need directshow to work with different fileformats, modify our list.
	// You can even change it to "c@pture;*.*".
	// Note that directshow might be able to open also still image fileformats like *.jpg" if you include their extensions here.
	// Right now, only c@pture is unique to directshow loader,
	// videos are better served by FFmpeg loader, still images are better served by FreeImage loader.
	RRBuffer::registerLoader("c@pture;*.avi;*.mkv;*.mov;*.wmv;*.mpg;*.mpeg;*.mp4",RRBufferDirectShow::load);
}

#endif // SUPPORT_DIRECTSHOW
