// --------------------------------------------------------------------------
// Screen space global illumination postprocess.
// Copyright (C) 2012-2014 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginSSGI.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeSSGI


class PluginRuntimeSSGI : public PluginRuntime
{
	Texture* bigColor;
	Texture* bigColor2;
	Texture* bigColor3;
	Texture* bigDepth;
	UberProgram* ssgiUberProgram;
	Program* ssgiProgram1;
	Program* ssgiProgram2;

public:

	PluginRuntimeSSGI(const PluginCreateRuntimeParams& params)
	{
		bigColor = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigColor2 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigColor3 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigDepth = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		ssgiUberProgram = UberProgram::create(rr::RRString(0,L"%lsssgi.vs",params.pathToShaders.w_str()),rr::RRString(0,L"%lsssgi.fs",params.pathToShaders.w_str()));
		ssgiProgram1 = ssgiUberProgram ? ssgiUberProgram->getProgram("#define PASS 1\n") : NULL;
		ssgiProgram2 = ssgiUberProgram ? ssgiUberProgram->getProgram("#define PASS 2\n") : NULL;
	}

	virtual ~PluginRuntimeSSGI()
	{
		delete ssgiUberProgram;
		delete bigDepth;
		delete bigColor3;
		delete bigColor2;
		delete bigColor;
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		_renderer.render(_pp.next,_sp);

		const PluginParamsSSGI& pp = *dynamic_cast<const PluginParamsSSGI*>(&_pp);
		
		if (!bigColor || !bigColor2 || !bigDepth || !ssgiProgram1 || !ssgiProgram2)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"SSGI shader failed to initialize.\n"));
			return;
		}

		// adjust map sizes to match render target size
		unsigned w = _sp.viewport[2];
		unsigned h = _sp.viewport[3];
		if (w!=bigColor->getBuffer()->getWidth() || h!=bigColor->getBuffer()->getHeight())
		{
			bigColor->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
			bigColor2->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
			bigColor3->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
			bigDepth->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
			bigColor->reset(false,false,false);
			bigColor2->reset(false,false,false);
			bigColor3->reset(false,false,false);
			bigDepth->reset(false,false,false);
#ifndef RR_GL_ES2
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#endif
		}
	
		// disable depth
		PreserveDepthTest p1;
		PreserveDepthMask p2;
		PreserveFlag p3(GL_SCISSOR_TEST,false);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(0);

		// acquire source maps
		{
			// copy backbuffer to bigColor+bigDepth
			bigColor->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,_sp.viewport[0],_sp.viewport[1],w,h,0);
			bigDepth->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,_sp.viewport[0],_sp.viewport[1],w,h,0);
		}

		// calculate noisy SSGI texture
		const rr::RRCamera& eye = *_sp.camera;
		Program* ssgiProgram = ssgiProgram1;
		ssgiProgram->useIt();
		ssgiProgram->sendTexture("depthMap",bigDepth);
		ssgiProgram->sendTexture("colorMap",bigColor);
		ssgiProgram->sendUniform("mAORange",pp.radius);
		ssgiProgram->sendUniform("occlusionIntensity",pp.intensity);
		ssgiProgram->sendUniform("angleBias",pp.angleBias);
		ssgiProgram->sendUniform("tPixelSize",1.0f/w,1.0f/h); // in texture space
		ssgiProgram->sendUniform("mScreenSizeIn1mDistance",(float)(2*tan(eye.getFieldOfViewHorizontalRad()/2)),(float)(2*tan(eye.getFieldOfViewVerticalRad()/2))); //!!! nefunguje v ortho
		ssgiProgram->sendUniform("mDepthRange",eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())),eye.getFar()/(eye.getFar()-eye.getNear()));

		if (1) // 0=noise, 1=blur
		{
			// bigColor2 = noisy SSGI
			FBO oldFBOState = FBO::getState();
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,bigColor2,oldFBOState);
			glViewport(0,0,w,h);
			TextureRenderer::renderQuad();

			// bigColor3 = blurred(bigColor2)
			ssgiProgram = ssgiProgram2;
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,bigColor3,oldFBOState);
			ssgiProgram->useIt();
			ssgiProgram->sendTexture("depthMap",bigDepth);
			ssgiProgram->sendTexture("colorMap",bigColor);
			ssgiProgram->sendTexture("aoMap",bigColor2);
			ssgiProgram->sendUniform("tPixelSize",1.0f/w,0.f);
			TextureRenderer::renderQuad();

			// backbuffer = blurred(bigColor3)
			oldFBOState.restore();
			ssgiProgram->sendTexture("depthMap",bigDepth);
			ssgiProgram->sendTexture("colorMap",bigColor);
			ssgiProgram->sendTexture("aoMap",bigColor3);
			ssgiProgram->sendUniform("tPixelSize",0.f,1.0f/h);
			glViewport(_sp.viewport[0],_sp.viewport[1],w,h);
		}
		PreserveFlag p4(GL_BLEND,GL_TRUE);
		PreserveBlendFunc p5;
		glBlendFunc(GL_ZERO,GL_SRC_COLOR);
		TextureRenderer::renderQuad();
		//if (_textureRenderer)
		//	_textureRenderer->render2D(bigDepth,NULL,0,0.35f,0.3f,0.3f,-1);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsSSGI

PluginRuntime* PluginParamsSSGI::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeSSGI(params);
}

}; // namespace
