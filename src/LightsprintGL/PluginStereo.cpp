// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Stereo render.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginStereo.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

// copy of ovrFovPort from Oculus SDK
struct OvrFovPort
{
	float UpTan;
	float DownTan;
	float LeftTan;
	float RightTan;
};

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeStereo

class PluginRuntimeStereo : public PluginRuntime
{
	Texture* stereoTexture;
	UberProgram* stereoUberProgram;

public:

	PluginRuntimeStereo(const PluginCreateRuntimeParams& params)
	{
		stereoTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGB,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST); // GL_NEAREST is for interlaced
		stereoUberProgram = UberProgram::create(rr::RRString(0,L"%lsstereo.vs",params.pathToShaders.w_str()),rr::RRString(0,L"%lsstereo.fs",params.pathToShaders.w_str()));
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		const PluginParamsStereo& pp = *dynamic_cast<const PluginParamsStereo*>(&_pp);
		
		if (_sp.camera->stereoMode==rr::RRCamera::SM_MONO || !stereoUberProgram || !stereoTexture)
			return;

		unsigned viewport[4] = {_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]}; // our temporary viewport, could differ from _sp.viewport

		// why rendering (all modes but oculus) to multisampled screen rather than 1-sampled texture?
		//  we prefer quality over minor speedup
		// why not rendering to multisampled texture?
		//  it needs extension, possible minor speedup is not worth adding extra renderpath
		// why not quad buffered rendering?
		//  because it works only with quadro/firegl, this is GPU independent
		// why not stencil buffer masked rendering to odd/even lines?
		//  because lines blur with multisampled screen (even if multisampling is disabled)
		rr::RRCamera eye[2]; // 0=left, 1=right
		_sp.camera->getStereoCameras(eye[0],eye[1]);
		bool swapEyes = _sp.camera->stereoMode==rr::RRCamera::SM_TOP_DOWN;
		if (_sp.camera->stereoMode==rr::RRCamera::SM_OCULUS_RIFT && pp.oculusTanHalfFov)
		{
			float aspect = _sp.camera->getAspect()*0.5f;
			for (unsigned e=0;e<2;e++)
			{
				const OvrFovPort& tanHalfFov = ((const OvrFovPort*)(pp.oculusTanHalfFov))[e];
				eye[e].setAspect(aspect);
				eye[e].setScreenCenter(rr::RRVec2( -( tanHalfFov.LeftTan - tanHalfFov.RightTan ) / ( tanHalfFov.LeftTan + tanHalfFov.RightTan ), ( tanHalfFov.UpTan - tanHalfFov.DownTan ) / ( tanHalfFov.UpTan + tanHalfFov.DownTan ) ));
				eye[e].setFieldOfViewVerticalDeg(RR_RAD2DEG( 2*atan( (tanHalfFov.LeftTan + tanHalfFov.RightTan)/(2*aspect) ) ));
			}
		}

		{
			// GL_SCISSOR_TEST and glScissor() ensure that mirror renderer clears alpha only in viewport, not in whole render target (2x more fragments)
			// it could be faster, althout I did not see any speedup
			PreserveFlag p0(GL_SCISSOR_TEST,true);
			PreserveScissor p1;

			FBO oldFBOStateQB;
			if (_sp.camera->stereoMode==rr::RRCamera::SM_QUAD_BUFFERED)
				oldFBOStateQB = FBO::getState();

			// render left and right eye
			for (unsigned e=0;e<2;e++)
			{
				PluginParamsShared oneEye = _sp;
				oneEye.camera = &eye[swapEyes?1-e:e];
				oneEye.viewport[0] = viewport[0];
				oneEye.viewport[1] = viewport[1];
				oneEye.viewport[2] = viewport[2];
				oneEye.viewport[3] = viewport[3];
				if (_sp.camera->stereoMode==rr::RRCamera::SM_QUAD_BUFFERED)
				{
					if (e==0)
					{
						FBO::setRenderBuffers(GL_BACK_LEFT);
					}
					else
					{
						FBO::setRenderBuffers(GL_BACK_RIGHT);
						glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
					}
				}
				else
				{
					int ofs = (_sp.camera->stereoMode==rr::RRCamera::SM_SIDE_BY_SIDE || _sp.camera->stereoMode==rr::RRCamera::SM_OCULUS_RIFT)?0:1;
					oneEye.viewport[2+ofs] /= 2;
					if (e==1)
						oneEye.viewport[ofs] += oneEye.viewport[2+ofs];
				}
				glViewport(oneEye.viewport[0],oneEye.viewport[1],oneEye.viewport[2],oneEye.viewport[3]);
				glScissor(oneEye.viewport[0],oneEye.viewport[1],oneEye.viewport[2],oneEye.viewport[3]);
				_renderer.render(_pp.next,oneEye);
			}

			if (_sp.camera->stereoMode==rr::RRCamera::SM_QUAD_BUFFERED)
				oldFBOStateQB.restore();
		}

		// composite
		if (_sp.camera->stereoMode==rr::RRCamera::SM_INTERLACED)
		{
			// disable depth
			PreserveDepthTest p1;
			PreserveDepthMask p2;
			glDisable(GL_DEPTH_TEST);
			glDepthMask(0);

			// turns top-down images to interlaced
			glViewport(_sp.viewport[0],_sp.viewport[1]+(_sp.viewport[3]%2),_sp.viewport[2],_sp.viewport[3]/2*2);
			stereoTexture->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]/2*2,0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			Program* stereoProgram = stereoUberProgram->getProgram("#define INTERLACED\n");
			if (stereoProgram)
			{
				glDisable(GL_CULL_FACE);
				stereoProgram->useIt();
				stereoProgram->sendTexture("map",stereoTexture);
				stereoProgram->sendUniform("mapHalfHeight",float(_sp.viewport[3]/2));
				TextureRenderer::renderQuad();
			}
		}

		// restore viewport after rendering stereo (it could be non-default, e.g. when enhanced sshot is enabled)
		glViewport(_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]);
	}

	virtual ~PluginRuntimeStereo()
	{
		RR_SAFE_DELETE(stereoUberProgram);
		if (stereoTexture)
			delete stereoTexture->getBuffer();
		RR_SAFE_DELETE(stereoTexture);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsStereo

PluginRuntime* PluginParamsStereo::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeStereo(params);
}

}; // namespace
