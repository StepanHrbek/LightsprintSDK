// --------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Contours postprocess.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginContours.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeContours

class PluginRuntimeContours : public PluginRuntime
{
	Texture* bigDepth;
	Program* contoursProgram;

public:

	PluginRuntimeContours(const PluginCreateRuntimeParams& params)
	{
		bigDepth = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		contoursProgram = Program::create(nullptr,rr::RRString(0,L"%lscontours.vs",params.pathToShaders.w_str()),rr::RRString(0,L"%lscontours.fs",params.pathToShaders.w_str()));
	}

	virtual ~PluginRuntimeContours()
	{
		delete contoursProgram;
		delete bigDepth;
	}

	virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
	{
		_renderer.render(_pp.next,_sp);

		const PluginParamsContours& pp = *dynamic_cast<const PluginParamsContours*>(&_pp);
		
		if (!bigDepth || !contoursProgram)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"Contours shader failed to initialize.\n"));
			return;
		}

		// adjust map sizes to match render target size
		unsigned w = _sp.viewport[2];
		unsigned h = _sp.viewport[3];
		if (w!=bigDepth->getBuffer()->getWidth() || h!=bigDepth->getBuffer()->getHeight())
		{
			bigDepth->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
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
			// copy depthbuffer to bigDepth
			bigDepth->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,_sp.viewport[0],_sp.viewport[1],w,h,0);
		}

		// trace contours
		PreserveFlag p4(GL_BLEND,GL_TRUE);
		PreserveBlendFunc p5;
		glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
		contoursProgram->useIt();
		contoursProgram->sendTexture("depthMap",bigDepth);
		contoursProgram->sendUniform("tPixelSize",1.0f/w,1.0f/h);
		contoursProgram->sendUniform("silhouetteColor",pp.silhouetteColor);
		contoursProgram->sendUniform("creaseColor",pp.creaseColor);
		TextureRenderer::renderQuad();
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsSSGI

PluginRuntime* PluginParamsContours::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeContours(params);
}

}; // namespace
