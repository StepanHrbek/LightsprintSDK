// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Adds support for Oculus SDK 0.6 devices.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginOculus.h"

#ifdef SUPPORT_OCULUS // you can enable/disable Oculus support here

#include "Lightsprint/GL/PreserveState.h"
#include "OVR_CAPI_GL.h"
#include "Extras/OVR_Math.h"

namespace rr_gl
{

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
// OculusDeviceImpl

class OculusDeviceImpl : public OculusDevice
{
	// scope = permanent
	bool                 oculusTexturesCreated;
	bool                 oculusTracking;
	unsigned             oculusMirrorFBO;
	unsigned             oculusMirrorW;
	unsigned             oculusMirrorH;
	unsigned             oculusFrameIndex;

	// scope = frame
	ovrEyeRenderDesc     oculusEyeRenderDesc[2];
	ovrPosef             oculusEyeRenderPose[2]; // updated by updateCamera(), read by submitFrame()
	double               oculusSensorSampleTime; // updated by updateCamera(), read by submitFrame()
	ovrVector3f          oculusHmdToEyeViewOffset[2];

public:
	// scope = permanent, public only for PluginRuntimeOculus
	ovrHmd               oculusSession;
	Texture*             oculusEyeRenderTexture[2];
	Texture*             oculusEyeDepthBuffer[2];
	ovrSwapTextureSet*   oculusSwapTextureSet[2];
	ovrTexture*          oculusMirrorTexture;

	OculusDeviceImpl()
	{
		rr::RRReportInterval report(rr::INF2,"Checking Oculus Rift...\n");

		oculusSession = nullptr;
		for (unsigned eye=0;eye<2;eye++)
		{
			oculusEyeRenderTexture[eye] = nullptr;
			oculusEyeDepthBuffer[eye] = nullptr;
			oculusSwapTextureSet[eye] = nullptr;
		}
		oculusTracking = false;
		oculusTexturesCreated = false;
		oculusMirrorTexture = nullptr;
		oculusMirrorFBO = 0;
		oculusMirrorW = 0;
		oculusMirrorH = 0;
		oculusFrameIndex = 0;

		// [#54] moved from OculusDevice::initialize() because of https://forums.oculus.com/viewtopic.php?f=20&t=24518
		ovrResult err = ovrHmd_Create(0,&oculusSession);
		if (!OVR_SUCCESS(err))
		{
			ovrResult err = ovrHmd_CreateDebug(ovrHmd_DK2,&oculusSession);
			if (OVR_SUCCESS(err))
				rr::RRReporter::report(rr::INF2,"Real Oculus Rift not available, faking one.\n");
			else
				rr::RRReporter::report(rr::INF2,"Oculus Rift not available.\n");
		}
	};

	virtual bool isOk()
	{
		return oculusSession;
	};

	virtual void updateCamera(rr::RRCamera& camera)
	{
		// read data from oculus
		ovrFrameTiming oculusFrameTiming = ovrHmd_GetFrameTiming(oculusSession, oculusFrameIndex);
		ovrTrackingState oculusTrackingState = ovrHmd_GetTrackingState(oculusSession, oculusFrameTiming.DisplayMidpointSeconds);
		oculusHmdToEyeViewOffset[0] = oculusEyeRenderDesc[0].HmdToEyeViewOffset;
		oculusHmdToEyeViewOffset[1] = oculusEyeRenderDesc[1].HmdToEyeViewOffset;
		ovr_CalcEyePoses(oculusTrackingState.HeadPose.ThePose, oculusHmdToEyeViewOffset, oculusEyeRenderPose);
		OVR::Posef oculusHeadPose = oculusTrackingState.HeadPose.ThePose;
		// apply oculus rotation to our camera
		static rr::RRVec3 oldOculusRot(0);
		rr::RRVec3 oculusRot(0);
		oculusHeadPose.Rotation.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&oculusRot.x,&oculusRot.y,&oculusRot.z);
		OVR::Matrix4f yawWithoutOculus = OVR::Matrix4f::RotationY(camera.getYawPitchRollRad().x-oldOculusRot.x);
		camera.setYawPitchRollRad(rr::RRVec3(camera.getYawPitchRollRad().x + oculusRot.x-oldOculusRot.x, oculusRot.y, oculusRot.z));
		oldOculusRot = oculusRot;
		// apply oculus translation to our camera
		static rr::RRVec3 oldOculusTrans(0);
		rr::RRVec3 oculusTrans = convertVec3(yawWithoutOculus.Transform(oculusHeadPose.Translation));
		camera.setPosition(camera.getPosition()+oculusTrans-oldOculusTrans);
		oldOculusTrans = oculusTrans;

		// enforce realistic eyeSeparation
		camera.eyeSeparation = ovrHmd_GetFloat(oculusSession, OVR_KEY_IPD, camera.eyeSeparation);
	};

	virtual void startFrame(unsigned mirrorW, unsigned mirrorH)
	{
		// configure oculus renderer once in SVCanvas life
		if (!oculusTexturesCreated)
		{
			oculusTexturesCreated = true;
			for (unsigned eye=0;eye<2;eye++)
			{
				ovrSizei idealTextureSize = ovrHmd_GetFovTextureSize(oculusSession, (ovrEyeType)eye, oculusSession->DefaultEyeFov[eye], 1);
				oculusEyeRenderTexture[eye] = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,idealTextureSize.w,idealTextureSize.h,1,rr::BF_RGBA,false,RR_GHOST_BUFFER),false,false);
				oculusEyeDepthBuffer[eye]   = Texture::createShadowmap(idealTextureSize.w,idealTextureSize.h);
			}
		}

		// configure oculus renderer once in SVCanvas life
		if (!oculusTracking)
		{
			ovrHmd_SetEnabledCaps(oculusSession, ovrHmdCap_LowPersistence | ovrHmdCap_DynamicPrediction);
			ovrResult oculusResult = ovrHmd_ConfigureTracking(oculusSession, ovrTrackingCap_Orientation|ovrTrackingCap_MagYawCorrection|ovrTrackingCap_Position, 0);
			oculusTracking = true;
			RR_ASSERT(OVR_SUCCESS(oculusResult));
			for (unsigned eye=0;eye<2;eye++)
			{
				ovrSizei idealTextureSize = ovrHmd_GetFovTextureSize(oculusSession, (ovrEyeType)eye, oculusSession->DefaultEyeFov[eye], 1);
				oculusResult = ovrHmd_CreateSwapTextureSetGL(oculusSession,GL_RGBA,idealTextureSize.w,idealTextureSize.h,&oculusSwapTextureSet[eye]); // it seems that oculus runtime ignores GL_RGBA request and creates GL_RGB. this leads to "mirror reflections disabled" warning
				RR_ASSERT(OVR_SUCCESS(oculusResult));
			}
			if (1) // mirroring HMD to SV window
			{
				oculusResult = ovrHmd_CreateMirrorTextureGL(oculusSession,GL_RGBA,oculusSession->Resolution.w,oculusSession->Resolution.h,(ovrTexture**)&oculusMirrorTexture);
				RR_ASSERT(OVR_SUCCESS(oculusResult));
				// [#51] code from oculus sample
				glGenFramebuffers(1, &oculusMirrorFBO);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, oculusMirrorFBO);
				glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ((ovrGLTexture*)oculusMirrorTexture)->OGL.TexId, 0);
				glFramebufferRenderbuffer(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
				glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			}
		}

		for (unsigned eye=0;eye<2;eye++)
			if (oculusSwapTextureSet[eye])
				oculusSwapTextureSet[eye]->CurrentIndex = (oculusSwapTextureSet[eye]->CurrentIndex + 1) % oculusSwapTextureSet[eye]->TextureCount;
	}

	virtual void submitFrame()
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		glBindBuffer(GL_ARRAY_BUFFER, 0); // at least this one is necessary for RL

		// distort image to HMD
		ovrViewScaleDesc oculusViewScaleDesc;
		oculusViewScaleDesc.HmdSpaceToWorldScaleInMeters = 1.0f;
		ovrLayerEyeFov oculusLayerEyeFov;
		oculusLayerEyeFov.Header.Type = ovrLayerType_EyeFov;
		oculusLayerEyeFov.Header.Flags = ovrLayerFlag_TextureOriginAtBottomLeft;
		for (int eye = 0; eye < 2; eye++)
		{
			oculusViewScaleDesc.HmdToEyeViewOffset[eye] = oculusHmdToEyeViewOffset[eye];
			oculusLayerEyeFov.ColorTexture[eye] = oculusSwapTextureSet[eye];
			oculusLayerEyeFov.Viewport[eye].Pos.x = 0;
			oculusLayerEyeFov.Viewport[eye].Pos.y = 0;
			oculusLayerEyeFov.Viewport[eye].Size = oculusSwapTextureSet[eye]->Textures[oculusSwapTextureSet[eye]->CurrentIndex].Header.TextureSize;
			oculusLayerEyeFov.Fov[eye] = oculusSession->DefaultEyeFov[eye];
			oculusLayerEyeFov.RenderPose[eye] = oculusEyeRenderPose[eye];
		}
		ovrLayerHeader* oculusLayerHeader = &oculusLayerEyeFov.Header;
		ovrResult oculusResult = ovrHmd_SubmitFrame(oculusSession, oculusFrameIndex, &oculusViewScaleDesc, &oculusLayerHeader, 1);
		RR_ASSERT(OVR_SUCCESS(oculusResult));

		// mirror distorted image from HMD to window
		if (oculusMirrorFBO)
		{
			// ([#51] code from oculus sample. simply using textureRenderer with oculusMirrorTexture renders black, dunno why)
			glBindFramebuffer(GL_READ_FRAMEBUFFER, oculusMirrorFBO);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			GLint w = ((ovrGLTexture*)oculusMirrorTexture)->OGL.Header.TextureSize.w;
			GLint h = ((ovrGLTexture*)oculusMirrorTexture)->OGL.Header.TextureSize.h;
			glBlitFramebuffer(0, h, w, 0, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST); // nearly any parameter change produces GL_INVALID_OPERATION. reading texture in shader also fails.
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		}

		oculusFrameIndex++;
	};

	virtual ~OculusDeviceImpl()
	{
		if (oculusMirrorFBO)
		{
			glDeleteFramebuffers(1, &oculusMirrorFBO);
			oculusMirrorFBO = 0;
		}
		if (oculusMirrorTexture)
		{
			ovrHmd_DestroyMirrorTexture(oculusSession, oculusMirrorTexture);
			oculusMirrorTexture = nullptr;
		}
		for (unsigned eye=0;eye<2;eye++)
		{
			if (oculusSwapTextureSet[eye])
			{
				ovrHmd_DestroySwapTextureSet(oculusSession,oculusSwapTextureSet[eye]);
				oculusSwapTextureSet[eye] = nullptr;
			}
			RR_SAFE_DELETE(oculusEyeRenderTexture[eye]);
			RR_SAFE_DELETE(oculusEyeDepthBuffer[eye]);
		}
		// [#54]
		if (oculusSession)
		{
			ovrHmd_Destroy(oculusSession);
			oculusSession = nullptr;
		}
	};
};


/////////////////////////////////////////////////////////////////////////////
//
// OculusDevice

void OVR_CDECL ovrLogCallback(int level, const char* message)
{
	rr::RRReporter::report((level==ovrLogLevel_Error)?rr::ERRO:(
		(level==ovrLogLevel_Info)?rr::INF2:(
		(level==ovrLogLevel_Debug)?rr::WARN:
		rr::WARN))
		,"%s\n",message);
}

void OculusDevice::initialize()
{
	ovrInitParams oculusInitParams;
	oculusInitParams.Flags = 0;
	oculusInitParams.RequestedMinorVersion = 0;
	oculusInitParams.LogCallback = &ovrLogCallback;
	oculusInitParams.ConnectionTimeoutMS = 0;
	ovrResult err = ovr_Initialize(&oculusInitParams);
}

OculusDevice* OculusDevice::create()
{
	OculusDevice* od = new OculusDeviceImpl();
	if (!od->isOk())
		rr::RR_SAFE_DELETE(od);
	return od;
}

void OculusDevice::shutdown()
{
	ovr_Shutdown();
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
		const OculusDeviceImpl* or = dynamic_cast<const OculusDeviceImpl*>(pp.oculusDevice);

		bool mono = _sp.camera->stereoMode!=rr::RRCamera::SM_OCULUS_RIFT;
		if (!mono && (!or || !or->oculusEyeRenderTexture[0] || !or->oculusEyeRenderTexture[1]))
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Oculus Rift not available, rendering mono.\n"));
			mono = true;
		}
		if (mono)
		{
			_renderer.render(_pp.next,_sp);
			return;
		}

		rr::RRCamera eye[2]; // 0=left, 1=right
		_sp.camera->getStereoCameras(eye[0],eye[1]);

		// GL_SCISSOR_TEST and glScissor() ensure that mirror renderer clears alpha only in viewport, not in whole render target (2x more fragments)
		// it could be faster, althout I did not see any speedup
		PreserveFlag p0(GL_SCISSOR_TEST,false);
		PreserveScissor p1;

		FBO oldFBOState = FBO::getState();

		// render left and right eye
		for (unsigned e=0;e<2;e++)
		{
			PluginParamsShared oneEye = _sp;
			oneEye.camera = &eye[e];
			//oneEye.srgbCorrect = false;
			unsigned w = or->oculusEyeRenderTexture[e]->getBuffer()->getWidth();
			unsigned h = or->oculusEyeRenderTexture[e]->getBuffer()->getHeight();
			const ::ovrFovPort& tanHalfFov = or->oculusSession->DefaultEyeFov[e];
			float aspect = w/(float)h;
			eye[e].setAspect(aspect);
			eye[e].setScreenCenter(rr::RRVec2( -( tanHalfFov.LeftTan - tanHalfFov.RightTan ) / ( tanHalfFov.LeftTan + tanHalfFov.RightTan ), ( tanHalfFov.UpTan - tanHalfFov.DownTan ) / ( tanHalfFov.UpTan + tanHalfFov.DownTan ) ));
			eye[e].setFieldOfViewVerticalDeg(RR_RAD2DEG( 2*atan( (tanHalfFov.LeftTan + tanHalfFov.RightTan)/(2*aspect) ) ));

			// render to textures
			oneEye.viewport[0] = 0;
			oneEye.viewport[1] = 0;
			oneEye.viewport[2] = w;
			oneEye.viewport[3] = h;

			rr_gl::FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,or->oculusEyeDepthBuffer[e],oldFBOState);
			rr_gl::FBO::setRenderTargetGL(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D, ((ovrGLTexture*)&(or->oculusSwapTextureSet[e]->Textures[or->oculusSwapTextureSet[e]->CurrentIndex]))->OGL.TexId,oldFBOState);
			glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
			//glEnable(GL_FRAMEBUFFER_SRGB);

			glViewport(oneEye.viewport[0],oneEye.viewport[1],oneEye.viewport[2],oneEye.viewport[3]);
			glScissor(oneEye.viewport[0],oneEye.viewport[1],oneEye.viewport[2],oneEye.viewport[3]);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			_renderer.render(_pp.next,oneEye);

			oldFBOState.restore(); // OculusRoomTiny sample claims this is necessary workaround. but no effect visible
		}
		oldFBOState.restore();

		// restore viewport after rendering stereo (it could be non-default, e.g. when enhanced sshot is enabled)
		glViewport(_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]);
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

void OculusDevice::initialize()
{
}

OculusDevice* OculusDevice::create()
{
	return new OculusDevice();
}

void OculusDevice::shutdown()
{
}

PluginRuntime* PluginParamsOculus::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return nullptr;
}

}; // namespace

#endif // SUPPORT_OCULUS

