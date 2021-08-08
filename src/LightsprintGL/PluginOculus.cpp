// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Adds support for Oculus SDK 1.8 devices.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginOculus.h"

#ifdef SUPPORT_OCULUS // you can enable/disable Oculus support here

#include "Lightsprint/GL/PreserveState.h"
#include "OVR_CAPI_GL.h"
#include "Extras/OVR_Math.h"
#if defined(_WIN32)
	#include <dxgi.h> // for GetDefaultAdapterLuid
	#pragma comment(lib, "dxgi.lib")
#endif

#ifndef UNREFERENCED_PARAMETER
	#define UNREFERENCED_PARAMETER(a)
#endif

#ifndef assert
	#define assert RR_ASSERT
#endif

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// misc helpers from Oculus sample

static ovrGraphicsLuid GetDefaultAdapterLuid()
{
	ovrGraphicsLuid luid = ovrGraphicsLuid();

#if defined(_WIN32)
	IDXGIFactory* factory = nullptr;

	if (SUCCEEDED(CreateDXGIFactory(IID_PPV_ARGS(&factory))))
	{
		IDXGIAdapter* adapter = nullptr;

		if (SUCCEEDED(factory->EnumAdapters(0, &adapter)))
		{
			DXGI_ADAPTER_DESC desc;

			adapter->GetDesc(&desc);
			memcpy(&luid, &desc.AdapterLuid, sizeof(luid));
			adapter->Release();
		}

		factory->Release();
	}
#endif

	return luid;
}

static int Compare(const ovrGraphicsLuid& lhs, const ovrGraphicsLuid& rhs)
{
	return memcmp(&lhs, &rhs, sizeof(ovrGraphicsLuid));
}


/////////////////////////////////////////////////////////////////////////////
//
// our helpers

template<class T>
static rr::RRVec3 convertVec3(ovrVector3f a)
{
	return rr::RRVec3(a[0],a[1],a[2]);
}

static rr::RRVec3 convertVec3(OVR::Vector3f a)
{
	return rr::RRVec3(a[0],a[1],a[2]);
}

static rr::RRVec4 convertVec4(float a[4])
{
	return rr::RRVec4(a[0],a[1],a[2],a[3]);
}


/////////////////////////////////////////////////////////////////////////////
//
// reimplementation of DepthBuffer+TextureBuffer from Oculus sample

// #include "../../Samples/OculusRoomTiny_Advanced/Common/Win32_GLAppUtil.h"

#define DepthBuffer Texture

class TextureBuffer : public Texture
{
public:
	ovrSession          Session;
	ovrTextureSwapChain TextureChain;
	OVR::Sizei          texSize;
	unsigned            idBackup;

	TextureBuffer(ovrSession session, OVR::Sizei size)
		: Session(session), TextureChain(nullptr), texSize(size),
		Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,size.w,size.h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false)
	{
		// backup our texture id, to prevent resource leak
		// TODO: don't allocate our id at all, or at least allocate 1x1
		idBackup = id;

		ovrTextureSwapChainDesc desc = {};
		desc.Type = ovrTexture_2D;
		desc.ArraySize = 1;
		desc.Width = size.w;
		desc.Height = size.h;
		desc.MipLevels = 1;
		desc.Format = OVR_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.SampleCount = 1;
		desc.StaticImage = ovrFalse;
		ovrResult result = ovr_CreateTextureSwapChainGL(session, &desc, &TextureChain);
		int length = 0;
		ovr_GetTextureSwapChainLength(session, TextureChain, &length);
		if(OVR_SUCCESS(result))
		{
			for (int i = 0; i < length; ++i)
			{
				ovr_GetTextureSwapChainBufferGL(Session, TextureChain, i, &id);
				glBindTexture(GL_TEXTURE_2D, id);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			}
		}
	}

	void SetAndClearRenderSurface(Texture* depthMap, const FBO& oldState)
	{
		if (TextureChain)
		{
			int curIndex;
			ovr_GetTextureSwapChainCurrentIndex(Session, TextureChain, &curIndex);
			ovr_GetTextureSwapChainBufferGL(Session, TextureChain, curIndex, &id);
		}
		FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depthMap,oldState);
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,this,oldState);
		glEnable(GL_FRAMEBUFFER_SRGB);
	}

	void Commit()
	{
		if (TextureChain)
		{
			ovr_CommitTextureSwapChain(Session, TextureChain);
		}
	}

	~TextureBuffer()
	{
		if (TextureChain)
		{
			ovr_DestroyTextureSwapChain(Session, TextureChain);
			TextureChain = nullptr;
		}
		id = idBackup;
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// OculusDevice

void OVR_CDECL ovrLogCallback(uintptr_t userData, int level, const char* message)
{
	rr::RRReporter::report((level==ovrLogLevel_Error)?rr::ERRO:(
		(level==ovrLogLevel_Info)?rr::INF2:(
		(level==ovrLogLevel_Debug)?rr::WARN:
		rr::WARN))
		,"%s %d\n",message,(int)userData);
}

class OculusDevice : public VRDevice
{
	// scope = permanent
	bool                 oculusInited;
	ovrSession           oculusSession;
	ovrMirrorTexture     oculusMirrorTexture;
	unsigned             oculusMirrorFBO;
	unsigned             oculusMirrorW;
	unsigned             oculusMirrorH;
	unsigned             oculusFrameIndex;

	// scope = frame
	ovrPosef             oculusEyeRenderPose[2]; // updated by updateCamera(), read by submitFrame()
	double               oculusSensorSampleTime; // updated by updateCamera(), read by submitFrame()

public:
	// scope = permanent, public only for PluginRuntimeOculus
	float                eyeSeparation; // updated by updateCamera(), read by render()
	ovrHmdDesc           oculusHmdDesc;
	TextureBuffer*       oculusEyeRenderTexture[2];
	DepthBuffer*         oculusEyeDepthBuffer[2];

	OculusDevice()
	{
		rr::RRReportInterval report(rr::INF2,"Checking Oculus Rift...\n");

		oculusInited = false;
		oculusSession = nullptr;
		for (unsigned eye=0;eye<2;eye++)
		{
			oculusEyeRenderTexture[eye] = nullptr;
			oculusEyeDepthBuffer[eye] = nullptr;
		}
		oculusMirrorTexture = nullptr;
		oculusMirrorFBO = 0;
		oculusMirrorW = 0;
		oculusMirrorH = 0;
		oculusFrameIndex = 0;

		ovrInitParams oculusInitParams;
		oculusInitParams.Flags = 0;
		oculusInitParams.RequestedMinorVersion = 0;
		oculusInitParams.LogCallback = &ovrLogCallback;
		oculusInitParams.ConnectionTimeoutMS = 0;
		ovrResult result = ovr_Initialize(&oculusInitParams);
		if (!OVR_SUCCESS(result))
			rr::RRReporter::report(rr::INF2,"Oculus Rift not available.\n");
		else
		{
			oculusInited = true;

			ovrGraphicsLuid luid;
			// [#54] moved from OculusDevice::initialize() because of https://forums.oculus.com/viewtopic.php?f=20&t=24518
			ovrResult result = ovr_Create(&oculusSession, &luid);
			if (!OVR_SUCCESS(result))
				rr::RRReporter::report(rr::INF2,"Oculus Rift unavailable.\n");
			else
			if (Compare(luid, GetDefaultAdapterLuid()))
				rr::RRReporter::report(rr::WARN,"GPU Oculus Rift is connected to is not the default one.\n");
			else
				oculusHmdDesc = ovr_GetHmdDesc(oculusSession);
		}
	};

	virtual bool isOk()
	{
		return oculusSession;
	};

	virtual void getPose(rr::RRVec3& _outPos, rr::RRVec3& _outRot)
	{
		ovrSessionStatus sessionStatus;
		ovr_GetSessionStatus(oculusSession, &sessionStatus);
		//if (sessionStatus.ShouldQuit)
		//	exitRequested = true;
		if (sessionStatus.ShouldRecenter)
			ovr_RecenterTrackingOrigin(oculusSession);
		if (!sessionStatus.IsVisible)
			;//

		ovrEyeRenderDesc eyeRenderDesc[2];
		eyeRenderDesc[0] = ovr_GetRenderDesc(oculusSession, ovrEye_Left, oculusHmdDesc.DefaultEyeFov[0]);
		eyeRenderDesc[1] = ovr_GetRenderDesc(oculusSession, ovrEye_Right, oculusHmdDesc.DefaultEyeFov[1]);

		// Get eye poses, feeding in correct IPD offset
		ovrVector3f HmdToEyeOffset[2] = { eyeRenderDesc[0].HmdToEyeOffset, eyeRenderDesc[1].HmdToEyeOffset };

		ovr_GetEyePoses(oculusSession, oculusFrameIndex, ovrTrue, HmdToEyeOffset, oculusEyeRenderPose, &oculusSensorSampleTime);

		ovrTrackingState oculusTrackingState = ovr_GetTrackingState(oculusSession, 0, false);//oculusFrameTiming.DisplayMidpointSeconds);
		ovrPosef oculusHeadPose = oculusTrackingState.HeadPose.ThePose;
		OVR::Quat<float> oculusHeadPose_Orientation = oculusHeadPose.Orientation;
		oculusHeadPose_Orientation.GetYawPitchRoll(&_outRot.x,&_outRot.y,&_outRot.z);
		_outPos = convertVec3(oculusHeadPose.Position);

		eyeSeparation = fabs(eyeRenderDesc[0].HmdToEyeOffset.x-eyeRenderDesc[1].HmdToEyeOffset.x);
	};

	virtual void startFrame(unsigned mirrorW, unsigned mirrorH)
	{
		// adjust texture size when resMultiplier changes
		for (unsigned eye=0;eye<2;eye++)
		{
			ovrSizei idealTextureSize = ovr_GetFovTextureSize(oculusSession, (ovrEyeType)eye, oculusHmdDesc.DefaultEyeFov[eye], 1);
			ovrSizei effectiveTextureSize;
			effectiveTextureSize.w = ((unsigned)(idealTextureSize.w*resMultiplier)+3)&0xfffc;
			effectiveTextureSize.h = ((unsigned)(idealTextureSize.h*resMultiplier)+3)&0xfffc;
			if (!oculusEyeRenderTexture[eye] || oculusEyeRenderTexture[eye]->texSize!=effectiveTextureSize)
			{
				delete oculusEyeRenderTexture[eye];
				oculusEyeRenderTexture[eye] = new TextureBuffer(oculusSession, effectiveTextureSize);
				delete oculusEyeDepthBuffer[eye];
				oculusEyeDepthBuffer[eye]   = Texture::createShadowmap(effectiveTextureSize.w,effectiveTextureSize.h);
				if (!oculusEyeRenderTexture[eye]->TextureChain)
				{
					rr::RRReporter::report(rr::ERRO,"Oculus: Texture chain fail.\n");
				}
			}
		}
		if (1) // mirroring HMD to SV window
		{
			if (oculusMirrorW!=mirrorW || oculusMirrorH!=mirrorH)
			{
				if (oculusMirrorTexture)
					ovr_DestroyMirrorTexture(oculusSession, oculusMirrorTexture);

				ovrMirrorTextureDesc desc;
				memset(&desc, 0, sizeof(desc));
				desc.Width = mirrorW;
				desc.Height = mirrorH;
				desc.Format = OVR_FORMAT_R8G8B8A8_UNORM;
				ovrResult oculusResult = ovr_CreateMirrorTextureGL(oculusSession,&desc,&oculusMirrorTexture);
				RR_ASSERT(OVR_SUCCESS(oculusResult));

				oculusMirrorW = mirrorW;
				oculusMirrorH = mirrorH;

				// [#51] code from oculus sample
				GLuint texId;
				ovr_GetMirrorTextureBufferGL(oculusSession, oculusMirrorTexture, &texId);
				glGenFramebuffers(1, &oculusMirrorFBO);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, oculusMirrorFBO);
				glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texId, 0);
				glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

				ovr_SetTrackingOriginType(oculusSession, ovrTrackingOrigin_FloorLevel);
			}
		}
	}

	virtual void submitFrame()
	{
		//oculusOldFBOState.restore();
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); // at least this one is necessary for RL

		// distort image to HMD
		ovrLayerEyeFov oculusLayerEyeFov;
		oculusLayerEyeFov.Header.Type = ovrLayerType_EyeFov;
		oculusLayerEyeFov.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
		for (int eye = 0; eye < 2; eye++)
		{
			oculusLayerEyeFov.ColorTexture[eye] = oculusEyeRenderTexture[eye]->TextureChain;
			oculusLayerEyeFov.Viewport[eye]     = OVR::Recti(0,0,oculusEyeRenderTexture[eye]->texSize.w,oculusEyeRenderTexture[eye]->texSize.h);
			oculusLayerEyeFov.Fov[eye]          = oculusHmdDesc.DefaultEyeFov[eye];
			oculusLayerEyeFov.RenderPose[eye]   = oculusEyeRenderPose[eye];
			oculusLayerEyeFov.SensorSampleTime  = oculusSensorSampleTime;
		}
		ovrLayerHeader* oculusLayerHeader = &oculusLayerEyeFov.Header;
		ovrResult oculusResult = ovr_SubmitFrame(oculusSession, oculusFrameIndex, nullptr, &oculusLayerHeader, 1);
		RR_ASSERT(OVR_SUCCESS(oculusResult));

		// mirror distorted image from HMD to window
		if (oculusMirrorFBO)
		{
			// ([#51] code from oculus sample. simply using textureRenderer with oculusMirrorTexture renders black, dunno why)
			glBindFramebuffer(GL_READ_FRAMEBUFFER, oculusMirrorFBO);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(0, oculusMirrorH, oculusMirrorW, 0, 0, 0, oculusMirrorW, oculusMirrorH, GL_COLOR_BUFFER_BIT, GL_NEAREST); // nearly any parameter change produces GL_INVALID_OPERATION. reading texture in shader also fails.
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		}

		oculusFrameIndex++;
	};

	virtual ~OculusDevice()
	{
		if (oculusMirrorFBO)
		{
			glDeleteFramebuffers(1, &oculusMirrorFBO);
			oculusMirrorFBO = 0;
		}
		if (oculusMirrorTexture)
		{
			ovr_DestroyMirrorTexture(oculusSession, oculusMirrorTexture);
			oculusMirrorTexture = nullptr;
		}
		for (unsigned eye=0;eye<2;eye++)
		{
			RR_SAFE_DELETE(oculusEyeRenderTexture[eye]);
			RR_SAFE_DELETE(oculusEyeDepthBuffer[eye]);
		}
		// [#54]
		if (oculusSession)
		{
			ovr_Destroy(oculusSession);
			oculusSession = nullptr;
		}
		if (oculusInited)
		{
			ovr_Shutdown();
			oculusInited = false;
		}
	};
};


/////////////////////////////////////////////////////////////////////////////
//
// OculusDevice

VRDevice* createOculusDevice()
{
	OculusDevice* vr = new OculusDevice();
	if (!vr->isOk())
		rr::RR_SAFE_DELETE(vr);
	return vr;
}


/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeOculus

class PluginRuntimeOculus : public PluginRuntime
{

public:

	PluginRuntimeOculus(const PluginCreateRuntimeParams& params)
	{
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		const PluginParamsOculus& pp = *dynamic_cast<const PluginParamsOculus*>(&_pp);
		OculusDevice* vr = dynamic_cast<OculusDevice*>(pp.vrDevice);

		bool mono = _sp.camera->stereoMode!=rr::RRCamera::SM_OCULUS_RIFT;
		if (!mono && !vr)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Oculus Rift not available, rendering mono.\n"));
			mono = true;
		}
		if (mono)
		{
			_renderer.render(_pp.next,_sp);
			return;
		}

		vr->startFrame(_sp.viewport[2],_sp.viewport[3]);
		rr::RRCamera spcamera = *_sp.camera;
		spcamera.eyeSeparation = vr->eyeSeparation; // override eyeSeparation
		rr::RRCamera eye[2]; // 0=left, 1=right
		spcamera.getStereoCameras(eye[0],eye[1]);

		// GL_SCISSOR_TEST and glScissor() ensure that mirror renderer clears alpha only in viewport, not in whole render target (2x more fragments)
		// it could be faster, althout I did not see any speedup
		PreserveFlag p0(GL_SCISSOR_TEST,false);
		PreserveScissor p1;

		FBO oldFBOState = FBO::getState();

		// render left and right eye
		for (unsigned e=0;e<2;e++)
		{
			unsigned w = vr->oculusEyeRenderTexture[e]->texSize.w;
			unsigned h = vr->oculusEyeRenderTexture[e]->texSize.h;
			const ::ovrFovPort& tanHalfFov = vr->oculusHmdDesc.DefaultEyeFov[e];
			float aspect = w/(float)h;
			float fieldOfViewVerticalDeg = RR_RAD2DEG( 2*atan( (tanHalfFov.LeftTan + tanHalfFov.RightTan)/(2*aspect) ) );
			rr::RRVec2 screenCenter = rr::RRVec2( -( tanHalfFov.LeftTan - tanHalfFov.RightTan ) / ( tanHalfFov.LeftTan + tanHalfFov.RightTan ), ( tanHalfFov.UpTan - tanHalfFov.DownTan ) / ( tanHalfFov.UpTan + tanHalfFov.DownTan ) );
			eye[e].setProjection(false,aspect,fieldOfViewVerticalDeg,eye[e].getNear(),eye[e].getFar(),100,screenCenter); // override FOV, screenCenter

			PluginParamsShared oneEye = _sp;
			oneEye.camera = &eye[e];
			//oneEye.srgbCorrect = false;

			vr->oculusEyeRenderTexture[e]->SetAndClearRenderSurface(vr->oculusEyeDepthBuffer[e],oldFBOState); // does not clear
			// also, it sets GL_FRAMEBUFFER_SRGB. not restoring it at the end of this function feels like error,
			// but for some reason, restoring it makes sRGB wrong

			// render to textures
			oneEye.viewport[0] = 0;
			oneEye.viewport[1] = 0;
			oneEye.viewport[2] = w;
			oneEye.viewport[3] = h;
			glViewport(oneEye.viewport[0],oneEye.viewport[1],oneEye.viewport[2],oneEye.viewport[3]);
			glScissor(oneEye.viewport[0],oneEye.viewport[1],oneEye.viewport[2],oneEye.viewport[3]);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			_renderer.render(_pp.next,oneEye);

			oldFBOState.restore(); // OculusRoomTiny sample claims this is necessary workaround. but no effect visible
			vr->oculusEyeRenderTexture[e]->Commit();
		}
		oldFBOState.restore();

		// restore viewport after rendering stereo (it could be non-default, e.g. when enhanced sshot is enabled)
		glViewport(_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]);

		vr->submitFrame();
	}

	virtual ~PluginRuntimeOculus()
	{
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsStereo

PluginRuntime* PluginParamsOculus::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeOculus(params);
}

}; // namespace


#else // SUPPORT_OCULUS


/////////////////////////////////////////////////////////////////////////////
//
// No Oculus

namespace rr_gl
{

VRDevice* createOculusDevice()
{
	return nullptr;
}

PluginRuntime* PluginParamsOculus::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return nullptr;
}

}; // namespace

#endif // SUPPORT_OCULUS

