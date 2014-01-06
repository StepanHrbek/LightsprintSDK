// --------------------------------------------------------------------------
// Stereo render.
// Copyright (C) 2010-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginStereo.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeStereo

class PluginRuntimeStereo : public PluginRuntime
{
	Texture* stereoTexture;
	UberProgram* stereoUberProgram;

public:

	PluginRuntimeStereo(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps)
	{
		stereoTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGB,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST);
		stereoUberProgram = UberProgram::create(rr::RRString(0,L"%lsstereo.vs",pathToShaders.w_str()),rr::RRString(0,L"%lsstereo.fs",pathToShaders.w_str()));
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		const PluginParamsStereo& pp = *dynamic_cast<const PluginParamsStereo*>(&_pp);
		
		if (pp.stereoMode==SM_MONO || !stereoUberProgram || !stereoTexture)
			return;

		const unsigned* viewport = _sp.viewport;

		// why rendering to multisampled screen rather than 1-sampled texture?
		//  we prefer quality over minor speedup
		// why not rendering to multisampled texture?
		//  it needs extension, possible minor speedup is not worth adding extra renderpath
		// why not quad buffered rendering?
		//  because it works only with quadro/firegl, this is GPU independent
		// why not stencil buffer masked rendering to odd/even lines?
		//  because lines blur with multisampled screen (even if multisampling is disabled)
		rr::RRCamera leftEye, rightEye;
		_sp.camera->getStereoCameras(leftEye,rightEye);
		bool swapEyes = pp.stereoMode==SM_INTERLACED_SWAP || pp.stereoMode==SM_TOP_DOWN || pp.stereoMode==SM_SIDE_BY_SIDE_SWAP || pp.stereoMode==SM_OCULUS_RIFT_SWAP;
		bool oculus = pp.stereoMode==SM_OCULUS_RIFT || pp.stereoMode==SM_OCULUS_RIFT_SWAP;
		if (oculus)
		{
			leftEye.setAspect(_sp.camera->getAspect()*0.5f);
			rightEye.setAspect(_sp.camera->getAspect()*0.5f);
			leftEye.setScreenCenter(rr::RRVec2(rr::RRReal(-pp.oculusLensShift*leftEye.getProjectionMatrix()[0]),0));
			rightEye.setScreenCenter(rr::RRVec2(rr::RRReal(pp.oculusLensShift*rightEye.getProjectionMatrix()[0]),0));
		}

		{
			// GL_SCISSOR_TEST and glScissor() ensure that mirror renderer clears alpha only in viewport, not in whole render target (2x more fragments)
			// it could be faster, althout I did not see any speedup
			PreserveFlag p0(GL_SCISSOR_TEST,true);
			PreserveScissor p1;

			// render left
			PluginParamsShared left = _sp;
			left.camera = swapEyes?&rightEye:&leftEye;
			left.viewport[0] = viewport[0];
			left.viewport[1] = viewport[1];
			left.viewport[2] = viewport[2];
			left.viewport[3] = viewport[3];
			if (pp.stereoMode==SM_SIDE_BY_SIDE || pp.stereoMode==SM_SIDE_BY_SIDE_SWAP || oculus)
				left.viewport[2] /= 2;
			else
				left.viewport[3] /= 2;
			glViewport(left.viewport[0],left.viewport[1],left.viewport[2],left.viewport[3]);
			glScissor(left.viewport[0],left.viewport[1],left.viewport[2],left.viewport[3]);
			_renderer.render(_pp.next,left);

			// render right
			// (it does not update layers as they were already updated when rendering left eye. this could change in future, if different eyes see different objects)
			PluginParamsShared right = left;
			right.camera = swapEyes?&leftEye:&rightEye;
			if (pp.stereoMode==SM_SIDE_BY_SIDE || pp.stereoMode==SM_SIDE_BY_SIDE_SWAP || oculus)
				right.viewport[0] += right.viewport[2];
			else
				right.viewport[1] += right.viewport[3];
			glViewport(right.viewport[0],right.viewport[1],right.viewport[2],right.viewport[3]);
			glScissor(right.viewport[0],right.viewport[1],right.viewport[2],right.viewport[3]);
			_renderer.render(_pp.next,right);
		}

		// composite
		if (pp.stereoMode==SM_INTERLACED || pp.stereoMode==SM_INTERLACED_SWAP || oculus)
		{
			// turns top-down images to interlaced or oculus
			if (oculus)
				glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
			else
				glViewport(viewport[0],viewport[1]+(viewport[3]%2),viewport[2],viewport[3]/2*2);
			stereoTexture->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,viewport[0],viewport[1],viewport[2],viewport[3]/2*2,0);
			Program* stereoProgram = stereoUberProgram->getProgram(oculus?"#define OCULUS_RIFT\n":"#define INTERLACED\n");
			if (stereoProgram)
			{
				glDisable(GL_CULL_FACE);
				stereoProgram->useIt();
				stereoProgram->sendTexture("map",stereoTexture);
				if (!oculus)
				{
					stereoProgram->sendUniform("mapHalfHeight",float(viewport[3]/2));
					TextureRenderer::renderQuad();
				}
				else
				{
					unsigned WindowWidth = viewport[2];
					unsigned WindowHeight = viewport[3];
					int DistortionXCenterOffset = 0;
					float DistortionScale = 1.5f;
					float w = float(viewport[2]/2) / float(WindowWidth);
					float h = float(viewport[3]) / float(WindowHeight);
					float y = float(viewport[1]) / float(WindowHeight);
					float as = float(viewport[2]/2) / float(viewport[3]);
					float scaleFactor = 1.0f / DistortionScale;

					stereoProgram->sendUniform("HmdWarpParam",rr::RRVec4(pp.oculusDistortionK));
					stereoProgram->sendUniform("ChromAbParam",rr::RRVec4(pp.oculusChromaAbCorrection));
					stereoProgram->sendUniform("Scale",(w/2) * scaleFactor, (h/2) * scaleFactor * as);
					stereoProgram->sendUniform("ScaleIn",(2/w), (2/h) / as);

					// distort left eye
					{
						float x = float(viewport[0]) / float(WindowWidth);
						stereoProgram->sendUniform("LensCenter",x + (w + DistortionXCenterOffset * 0.5f)*0.5f, y + h*0.5f);
						stereoProgram->sendUniform("ScreenCenter",x + w*0.5f, y + h*0.5f);
						float leftPosition[8] = {-1,-1, -1,1, 0,1, 0,-1};
						TextureRenderer::renderQuad(leftPosition);
					}

					// distort right eye
					{
						float x = float(viewport[0]+viewport[2]/2) / float(WindowWidth);
						stereoProgram->sendUniform("LensCenter",x + (w + DistortionXCenterOffset * 0.5f)*0.5f, y + h*0.5f);
						stereoProgram->sendUniform("ScreenCenter",x + w*0.5f, y + h*0.5f);
						float rightPosition[8] = {0,-1, 0,1, 1,1, 1,-1};
						TextureRenderer::renderQuad(rightPosition);
					}
				}
			}
		}

		// restore viewport after rendering stereo (it could be non-default, e.g. when enhanced sshot is enabled)
		glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
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

PluginRuntime* PluginParamsStereo::createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const
{
	return new PluginRuntimeStereo(pathToShaders, pathToMaps);
}

}; // namespace
