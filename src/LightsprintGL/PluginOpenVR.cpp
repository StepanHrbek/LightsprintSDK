// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Adds support for OpenVR devices.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginOpenVR.h"

#ifdef SUPPORT_OPENVR // you can enable/disable OpenVR support here

#include "Lightsprint/GL/PreserveState.h"
#include <openvr.h>
#if defined(_WIN32)
	#pragma comment(lib, "openvr_api.lib")
#endif

namespace rr_gl
{


/////////////////////////////////////////////////////////////////////////////
//
// our helpers

static rr::RRMatrix3x4 convertMatrix(vr::HmdMatrix34_t &a)
{
	return rr::RRMatrix3x4(a.m[0],false);
}

/////////////////////////////////////////////////////////////////////////////
//
// OpenVRDevice

class OpenVRDevice : public VRDevice
{
	vr::IVRSystem*       m_pHMD;
	vr::TrackedDevicePose_t m_rTrackedDevicePose[ vr::k_unMaxTrackedDeviceCount ];
	rr::RRMatrix3x4      m_rmat4DevicePose[ vr::k_unMaxTrackedDeviceCount ];
	rr::RRMatrix3x4      m_mat4HMDPose;
	std::string          m_strPoseClasses; // what classes we saw poses for this frame
	char                 m_rDevClassChar[ vr::k_unMaxTrackedDeviceCount ]; // for each device, a character representing its class

public:
	unsigned             optimalW;
	unsigned             optimalH;
	unsigned             effectiveW;
	unsigned             effectiveH;
	float                eyeSeparation;
	float                aspect;
	float                fieldOfViewVerticalDeg[2];
	rr::RRVec2           screenCenter[2];
	Texture*             eyeRenderTexture[2];
	Texture*             eyeDepthBuffer[2];

public:
	OpenVRDevice()
	{
		rr::RRReportInterval report(rr::INF2,"Checking openvr...\n");

		m_pHMD = nullptr;
		m_strPoseClasses = "";
		memset(m_rDevClassChar, 0, sizeof(m_rDevClassChar));

		optimalW = 0;
		optimalH = 0;
		effectiveW = 0;
		effectiveH = 0;
		aspect = 1;
		for (unsigned eye=0;eye<2;eye++)
		{
			fieldOfViewVerticalDeg[eye] = 100;
			screenCenter[eye] = rr::RRVec2(0);
			eyeRenderTexture[eye] = nullptr;
			eyeDepthBuffer[eye] = nullptr;
		}
		openvrInit();
	};

	void openvrInit()
	{
		vr::EVRInitError eError = vr::VRInitError_None;
		m_pHMD = vr::VR_Init( &eError, vr::VRApplication_Scene );
		if ( eError != vr::VRInitError_None )
		{
			m_pHMD = NULL;
			rr::RRReporter::report(rr::INF2,"Unable to init openvr runtime: %hs\n", vr::VR_GetVRInitErrorAsEnglishDescription( eError ));
			return;
		}
		if ( !vr::VRCompositor() )
		{
			vr::VR_Shutdown();
			m_pHMD = NULL;
			rr::RRReporter::report(rr::INF2,"Unable to init openvr runtime: no compositor.\n");
			return;
		}
		m_pHMD->GetRecommendedRenderTargetSize( &optimalW, &optimalH );
		vr::HmdMatrix34_t left = m_pHMD->GetEyeToHeadTransform(vr::Eye_Left);
		vr::HmdMatrix34_t right = m_pHMD->GetEyeToHeadTransform(vr::Eye_Right);
		eyeSeparation = right.m[0][3]-left.m[0][3];
		aspect = optimalW/(float)optimalH; //*1.5f; for pimax 4k with piplay 1.1.92 and 2k checked
		for (unsigned eye=0;eye<2;eye++)
		{
			float left,right,top,bottom;
			m_pHMD->GetProjectionRaw((vr::EVREye)eye,&left,&right,&top,&bottom);
			left = -left; // make it like ovrFovPort
			top = -top;
			fieldOfViewVerticalDeg[eye] = RR_RAD2DEG(2*atan((left+right)/(2*aspect)));
			screenCenter[eye] = rr::RRVec2(-(left-right)/(left+right),(top-bottom)/(top+bottom));
		}
	}

	bool isOk()
	{
		return m_pHMD;
	};

	virtual void getPose(rr::RRVec3& _outPos, rr::RRVec3& _outRot)
	{
		vr::VRCompositor()->WaitGetPoses(m_rTrackedDevicePose, vr::k_unMaxTrackedDeviceCount, NULL, 0 );
		/*
		m_strPoseClasses = "";
		for ( int nDevice = 0; nDevice < vr::k_unMaxTrackedDeviceCount; ++nDevice )
		{
			if ( m_rTrackedDevicePose[nDevice].bPoseIsValid )
			{
				m_rmat4DevicePose[nDevice] = convertMatrix(m_rTrackedDevicePose[nDevice].mDeviceToAbsoluteTracking);
				if (m_rDevClassChar[nDevice]==0)
				{
					switch (m_pHMD->GetTrackedDeviceClass(nDevice))
					{
						case vr::TrackedDeviceClass_Controller:        m_rDevClassChar[nDevice] = 'C'; break;
						case vr::TrackedDeviceClass_HMD:               m_rDevClassChar[nDevice] = 'H'; break;
						case vr::TrackedDeviceClass_Invalid:           m_rDevClassChar[nDevice] = 'I'; break;
						case vr::TrackedDeviceClass_Other:             m_rDevClassChar[nDevice] = 'O'; break;
						case vr::TrackedDeviceClass_TrackingReference: m_rDevClassChar[nDevice] = 'T'; break;
						default:                                       m_rDevClassChar[nDevice] = '?'; break;
					}
				}
				m_strPoseClasses += m_rDevClassChar[nDevice];
			}
		}

		if ( m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid )
		{
			m_mat4HMDPose = m_rmat4DevicePose[vr::k_unTrackedDeviceIndex_Hmd];
		}
		*/
		if ( m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid )
		{
			m_mat4HMDPose = convertMatrix(m_rTrackedDevicePose[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
		}
		_outPos = m_mat4HMDPose.getTranslation();
		_outRot = m_mat4HMDPose.getYawPitchRoll();
	};

	void startFrame(unsigned mirrorW, unsigned mirrorH)
	{
		// resize textures after resMultiplier change
		effectiveW = ((unsigned)(optimalW*resMultiplier)+3)&0xfffffffc;
		effectiveH = ((unsigned)(optimalH*resMultiplier)+3)&0xfffffffc;
		bool wrongRes[2];
		for (unsigned eye=0;eye<2;eye++)
			wrongRes[eye] = eyeRenderTexture[eye] && (eyeRenderTexture[eye]->getBuffer()->getWidth()!=effectiveW || eyeRenderTexture[eye]->getBuffer()->getHeight()!=effectiveH);
		if (wrongRes[0] || wrongRes[1])
		{
			// at the moment, resize is not possible according to https://github.com/ValveSoftware/openvr/issues/147
			// so instead we restart whole OpenVR
			rr::RRReporter::report(rr::INF2,"Restarting openvr in order to change resolution.\n");
			openvrDone(); // alternatively we can call inplace dtor+ctor, but it would reset resMultiplier
			openvrInit();
			// recalculate effectiveW/H, zeroed in openvrInit
			effectiveW = ((unsigned)(optimalW*resMultiplier)+3)&0xfffffffc;
			effectiveH = ((unsigned)(optimalH*resMultiplier)+3)&0xfffffffc;
		}
		for (unsigned eye=0;eye<2;eye++)
		{
			if (!eyeRenderTexture[eye] || wrongRes[eye])
			{
				if (eyeRenderTexture[eye]) delete eyeRenderTexture[eye]->getBuffer();
				delete eyeRenderTexture[eye];
				eyeRenderTexture[eye] = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,effectiveW,effectiveH,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false);
				eyeRenderTexture[eye]->reset(false,false,true);
				delete eyeDepthBuffer[eye];
				eyeDepthBuffer[eye] = Texture::createShadowmap(effectiveW,effectiveH);
			}
		}

		// Process SteamVR events
		vr::VREvent_t event;
		while( m_pHMD->PollNextEvent( &event, sizeof( event ) ) )
		{
			switch( event.eventType )
			{
			case vr::VREvent_TrackedDeviceActivated:
				{
					//SetupRenderModelForTrackedDevice( event.trackedDeviceIndex );
					//dprintf( "Device %u attached. Setting up render model.\n", event.trackedDeviceIndex );
				}
				break;
			case vr::VREvent_TrackedDeviceDeactivated:
				{
					//dprintf( "Device %u detached.\n", event.trackedDeviceIndex );
				}
				break;
			case vr::VREvent_TrackedDeviceUpdated:
				{
					//dprintf( "Device %u updated.\n", event.trackedDeviceIndex );
				}
				break;
			}
		}

		// Process SteamVR controller state
		for( vr::TrackedDeviceIndex_t unDevice = 0; unDevice < vr::k_unMaxTrackedDeviceCount; unDevice++ )
		{
			vr::VRControllerState_t state;
			//if( m_pHMD->GetControllerState( unDevice, &state ) )
			{
				//m_rbShowTrackedDevice[ unDevice ] = state.ulButtonPressed == 0;
			}
		}
	}

	void submitFrame()
	{
		if ( m_pHMD )
		{
			vr::Texture_t leftEyeTexture = {(void*)(intptr_t)eyeRenderTexture[0]->getId(), vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
			vr::VRCompositor()->Submit(vr::Eye_Left, &leftEyeTexture );
			vr::Texture_t rightEyeTexture = {(void*)(intptr_t)eyeRenderTexture[1]->getId(), vr::TextureType_OpenGL, vr::ColorSpace_Gamma };
			vr::VRCompositor()->Submit(vr::Eye_Right, &rightEyeTexture );
		}
	};

	void openvrDone()
	{
		if ( m_pHMD )
		{
			// debug version sometimes crashes inside vrserver.exe
			// surprisingly it does not crash if we add some wait here before vr::VR_Shutdown()
			// but it still does not make system reliable, vr::VR_Init() freezes after +-10 init/shutdown cycles
			// so better crash immediately
			m_pHMD = NULL;
			vr::VR_Shutdown();
		}
	}

	virtual ~OpenVRDevice()
	{
		openvrDone();
		for (unsigned eye=0;eye<2;eye++)
		{
			if (eyeRenderTexture[eye])
			{
				delete eyeRenderTexture[eye]->getBuffer();
				RR_SAFE_DELETE(eyeRenderTexture[eye]);
			}
			RR_SAFE_DELETE(eyeDepthBuffer[eye]);
		}
	};
};


/////////////////////////////////////////////////////////////////////////////
//
// OpenVRDevice

VRDevice* createOpenVRDevice()
{
	OpenVRDevice* vr = new OpenVRDevice();
	if (!vr->isOk())
		rr::RR_SAFE_DELETE(vr);
	return vr;
}


/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeOpenVR

class PluginRuntimeOpenVR : public PluginRuntime
{

public:

	PluginRuntimeOpenVR(const PluginCreateRuntimeParams& params)
	{
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		const PluginParamsOpenVR& pp = *dynamic_cast<const PluginParamsOpenVR*>(&_pp);
		OpenVRDevice* vr = dynamic_cast<OpenVRDevice*>(pp.vrDevice);

		bool mono = _sp.camera->stereoMode!=rr::RRCamera::SM_OPENVR;
		if (!mono && !vr)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"OpenVR not available, rendering mono.\n"));
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
			eye[e].setProjection(false,vr->aspect,vr->fieldOfViewVerticalDeg[e],eye[e].getNear(),eye[e].getFar(),100,vr->screenCenter[e]);

			PluginParamsShared oneEye = _sp;
			oneEye.camera = &eye[e];
			//oneEye.srgbCorrect = false;

			FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,vr->eyeDepthBuffer[e],oldFBOState);
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,vr->eyeRenderTexture[e],oldFBOState);
			// set GL_FRAMEBUFFER_SRGB. not restoring it at the end of this function feels like error,
			// but for some reason, restoring it makes sRGB wrong
			glEnable(GL_FRAMEBUFFER_SRGB);

			// render to textures
			oneEye.viewport[0] = 0;
			oneEye.viewport[1] = 0;
			oneEye.viewport[2] = vr->effectiveW;
			oneEye.viewport[3] = vr->effectiveH;
			glViewport(oneEye.viewport[0],oneEye.viewport[1],oneEye.viewport[2],oneEye.viewport[3]);
			glScissor(oneEye.viewport[0],oneEye.viewport[1],oneEye.viewport[2],oneEye.viewport[3]);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			_renderer.render(_pp.next,oneEye);
		}
		oldFBOState.restore();

		// restore viewport after rendering stereo (it could be non-default, e.g. when enhanced sshot is enabled)
		glViewport(_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]);

		// mirroring
		_renderer.getTextureRenderer()->render2D(vr->eyeRenderTexture[0],nullptr,0,0,0.5f,1,-1);
		_renderer.getTextureRenderer()->render2D(vr->eyeRenderTexture[1],nullptr,0.5f,0,0.5f,1,-1);

		vr->submitFrame();
	}

	virtual ~PluginRuntimeOpenVR()
	{
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsStereo

PluginRuntime* PluginParamsOpenVR::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeOpenVR(params);
}

}; // namespace


#else // SUPPORT_OPENVR


/////////////////////////////////////////////////////////////////////////////
//
// No OpenVR

namespace rr_gl
{

VRDevice* createOpenVRDevice()
{
	return nullptr;
}

PluginRuntime* PluginParamsOpenVR::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return nullptr;
}

}; // namespace

#endif // SUPPORT_OPENVR

