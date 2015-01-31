// --------------------------------------------------------------------------
// Stereo render.
// Copyright (C) 2010-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginStereo.h"
#include "Lightsprint/GL/PreserveState.h"

//#define SCALE 1.3 // not defined=render to multisampled screen, defined=render to this many times larger texture

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
#ifdef SCALE
	Texture* colorTexture; // oculus
	Texture* depthTexture; // oculus
#endif
	UberProgram* stereoUberProgram;

public:

	PluginRuntimeStereo(const PluginCreateRuntimeParams& params)
	{
		stereoTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGB,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST); // GL_NEAREST is for interlaced
#ifdef SCALE
		colorTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false);
		depthTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false);
#endif
		stereoUberProgram = UberProgram::create(rr::RRString(0,L"%lsstereo.vs",params.pathToShaders.w_str()),rr::RRString(0,L"%lsstereo.fs",params.pathToShaders.w_str()));
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		const PluginParamsStereo& pp = *dynamic_cast<const PluginParamsStereo*>(&_pp);
		
#ifdef SCALE
		if (_sp.camera->stereoMode==rr::RRCamera::SM_MONO || !stereoUberProgram || !stereoTexture || !colorTexture || !depthTexture)
#else
		if (_sp.camera->stereoMode==rr::RRCamera::SM_MONO || !stereoUberProgram || !stereoTexture)
#endif
			return;

		unsigned viewport[4] = {_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]}; // our temporary viewport, could differ from _sp.viewport

#ifdef SCALE
		FBO oldFBOState = FBO::getState();
		if (_sp.camera->stereoMode==rr::RRCamera::SM_OCULUS_RIFT)
		{
			// render to texture bigger than _sp.viewport
			viewport[0] = 0;
			viewport[1] = 0;
			viewport[2] = _sp.viewport[2]*SCALE;
			viewport[3] = _sp.viewport[3]*SCALE;
			if (colorTexture->getBuffer()->getWidth()!=viewport[2] || colorTexture->getBuffer()->getHeight()!=viewport[3])
			{
				colorTexture->getBuffer()->reset(rr::BT_2D_TEXTURE,viewport[2],viewport[3],1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
				colorTexture->reset(false,false,_sp.srgbCorrect);
			}
			if (depthTexture->getBuffer()->getWidth()!=viewport[2] || depthTexture->getBuffer()->getHeight()!=viewport[3])
			{
				depthTexture->getBuffer()->reset(rr::BT_2D_TEXTURE,viewport[2],viewport[3],1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
				depthTexture->reset(false,false,false);
			}
			FBO::setRenderTarget(GL_DEPTH_ATTACHMENT,GL_TEXTURE_2D,depthTexture,oldFBOState);
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,colorTexture,oldFBOState);
			glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
		}
#endif

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
		bool swapEyes = _sp.camera->stereoSwap != (_sp.camera->stereoMode==rr::RRCamera::SM_TOP_DOWN);
		if (_sp.camera->stereoMode==rr::RRCamera::SM_OCULUS_RIFT && pp.oculusTanHalfFov)
		{
			float aspect = _sp.camera->getAspect()*0.5f;
			for (unsigned e=0;e<2;e++)
			{
				const OvrFovPort& tanHalfFov = ((const OvrFovPort*)(pp.oculusTanHalfFov))[e];
				eye[e].setAspect(aspect);
				//eye[e].setScreenCenter(_sp.camera->getScreenCenter()+rr::RRVec2(rr::RRReal((e?1:-1)*pp.oculusLensShift*1.15*eye[0].getProjectionMatrix()[0]),0));
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

			// render left
			PluginParamsShared left = _sp;
			left.camera = &eye[swapEyes?1:0];
			left.viewport[0] = viewport[0];
			left.viewport[1] = viewport[1];
			left.viewport[2] = viewport[2];
			left.viewport[3] = viewport[3];
			if (_sp.camera->stereoMode==rr::RRCamera::SM_QUAD_BUFFERED)
			{
				FBO::setRenderBuffers(GL_BACK_LEFT);
			}
			else
			{
				if (_sp.camera->stereoMode==rr::RRCamera::SM_SIDE_BY_SIDE || _sp.camera->stereoMode==rr::RRCamera::SM_OCULUS_RIFT)
					left.viewport[2] /= 2;
				else
					left.viewport[3] /= 2;
			}
			glViewport(left.viewport[0],left.viewport[1],left.viewport[2],left.viewport[3]);
			glScissor(left.viewport[0],left.viewport[1],left.viewport[2],left.viewport[3]);
			_renderer.render(_pp.next,left);

			// render right
			// (it does not update layers as they were already updated when rendering left eye. this could change in future, if different eyes see different objects)
			PluginParamsShared right = left;
			right.camera = &eye[swapEyes?0:1];
			if (_sp.camera->stereoMode==rr::RRCamera::SM_QUAD_BUFFERED)
			{
				FBO::setRenderBuffers(GL_BACK_RIGHT);
				glClear(GL_DEPTH_BUFFER_BIT|GL_COLOR_BUFFER_BIT);
			}
			else
			{
				if (_sp.camera->stereoMode==rr::RRCamera::SM_SIDE_BY_SIDE || _sp.camera->stereoMode==rr::RRCamera::SM_OCULUS_RIFT)
					right.viewport[0] += right.viewport[2];
				else
					right.viewport[1] += right.viewport[3];
			}
			glViewport(right.viewport[0],right.viewport[1],right.viewport[2],right.viewport[3]);
			glScissor(right.viewport[0],right.viewport[1],right.viewport[2],right.viewport[3]);
			_renderer.render(_pp.next,right);

			if (_sp.camera->stereoMode==rr::RRCamera::SM_QUAD_BUFFERED)
				oldFBOStateQB.restore();
		}

		// composite
		if (_sp.camera->stereoMode==rr::RRCamera::SM_INTERLACED)// || _sp.camera->stereoMode==SM_OCULUS_RIFT)
		{
			// disable depth
			PreserveDepthTest p1;
			PreserveDepthMask p2;
			glDisable(GL_DEPTH_TEST);
			glDepthMask(0);

			// turns top-down images to interlaced or oculus
			if (_sp.camera->stereoMode==rr::RRCamera::SM_OCULUS_RIFT)
			{
#ifdef SCALE
				oldFBOState.restore();
				colorTexture->bindTexture();
#endif
				glViewport(_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]);
			}
			else
			{
				glViewport(_sp.viewport[0],_sp.viewport[1]+(_sp.viewport[3]%2),_sp.viewport[2],_sp.viewport[3]/2*2);
#ifdef SCALE
				colorTexture->bindTexture();
				glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]/2*2,0);
			}
#else
			}
			stereoTexture->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]/2*2,0);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (_sp.camera->stereoMode==rr::RRCamera::SM_OCULUS_RIFT)?GL_LINEAR:GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (_sp.camera->stereoMode==rr::RRCamera::SM_OCULUS_RIFT)?GL_LINEAR:GL_NEAREST);
#endif
			Program* stereoProgram = stereoUberProgram->getProgram((_sp.camera->stereoMode==rr::RRCamera::SM_OCULUS_RIFT)?"#define OCULUS_RIFT\n":"#define INTERLACED\n");
			if (stereoProgram)
			{
				glDisable(GL_CULL_FACE);
				stereoProgram->useIt();
				if (_sp.camera->stereoMode==rr::RRCamera::SM_INTERLACED)
				{
					stereoProgram->sendTexture("map",stereoTexture);
					stereoProgram->sendUniform("mapHalfHeight",float(_sp.viewport[3]/2));
					TextureRenderer::renderQuad();
				}
#if 0 // our distortion code from Oculus SDK 0.3. these SDK 0.4 days we just prepare side-by-side image and let caller ask Oculus SDK to distort it
				else
				{
#ifdef SCALE
					PreserveFlag p0(GL_FRAMEBUFFER_SRGB,_sp.srgbCorrect);
					stereoProgram->sendTexture("map",colorTexture);
#else
					stereoProgram->sendTexture("map",stereoTexture);
#endif
					unsigned WindowWidth = _sp.viewport[2];
					unsigned WindowHeight = _sp.viewport[3];
					float DistortionXCenterOffset = pp.oculusLensShift;
					float DistortionScale = 1.3f;
					float w = float(_sp.viewport[2]/2) / float(WindowWidth);
					float h = float(_sp.viewport[3]) / float(WindowHeight);
					float y = float(_sp.viewport[1]) / float(WindowHeight);
					float as = float(_sp.viewport[2]/2) / float(_sp.viewport[3]);
					float scaleFactor = 1.0f / DistortionScale;

					stereoProgram->sendUniform("HmdWarpParam",rr::RRVec4(pp.oculusDistortionK));
					stereoProgram->sendUniform("ChromAbParam",rr::RRVec4(pp.oculusChromaAbCorrection));
					stereoProgram->sendUniform("Scale",(w/2) * scaleFactor, (h/2) * scaleFactor * as);
					stereoProgram->sendUniform("ScaleIn",(2/w), (2/h) / as);

					// distort left eye
					{
						float x = float(_sp.viewport[0]) / float(WindowWidth);
						stereoProgram->sendUniform("LensCenter",x + (w + DistortionXCenterOffset * 0.5f)*0.5f, y + h*0.5f);
						stereoProgram->sendUniform("ScreenCenter",x + w*0.5f, y + h*0.5f);
						float leftPosition[8] = {-1,-1, -1,1, 0,1, 0,-1};
						TextureRenderer::renderQuad(leftPosition);
					}

					// distort right eye
					{
						float x = float(_sp.viewport[0]+_sp.viewport[2]/2) / float(WindowWidth);
						stereoProgram->sendUniform("LensCenter",x + (w - DistortionXCenterOffset * 0.5f)*0.5f, y + h*0.5f);
						stereoProgram->sendUniform("ScreenCenter",x + w*0.5f, y + h*0.5f);
						float rightPosition[8] = {0,-1, 0,1, 1,1, 1,-1};
						TextureRenderer::renderQuad(rightPosition);
					}
				}
#endif // 0
			}
		}

		// restore viewport after rendering stereo (it could be non-default, e.g. when enhanced sshot is enabled)
		//if (_sp.camera->stereoMode!=SM_OCULUS_RIFT)
			glViewport(_sp.viewport[0],_sp.viewport[1],_sp.viewport[2],_sp.viewport[3]);
	}

	virtual ~PluginRuntimeStereo()
	{
		RR_SAFE_DELETE(stereoUberProgram);
#ifdef SCALE
		if (depthTexture)
			delete depthTexture->getBuffer();
		RR_SAFE_DELETE(depthTexture);
		if (colorTexture)
			delete colorTexture->getBuffer();
		RR_SAFE_DELETE(colorTexture);
#endif
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
