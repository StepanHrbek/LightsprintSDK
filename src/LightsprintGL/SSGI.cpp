// --------------------------------------------------------------------------
// Screen space global illumination postprocess.
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/GL/SSGI.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "PreserveState.h"

namespace rr_gl
{

SSGI::SSGI(const rr::RRString& pathToShaders)
{
	bigColor = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	bigColor2 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	bigColor3 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	bigDepth = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	ssgiUberProgram = UberProgram::create(rr::RRString(0,L"%lsssgi.vs",pathToShaders.w_str()),rr::RRString(0,L"%lsssgi.fs",pathToShaders.w_str()));
	ssgiProgram1 = ssgiUberProgram ? ssgiUberProgram->getProgram("#define PASS 1\n") : NULL;
	ssgiProgram2 = ssgiUberProgram ? ssgiUberProgram->getProgram("#define PASS 2\n") : NULL;
}

SSGI::~SSGI()
{
	delete ssgiUberProgram;
	delete bigDepth;
	delete bigColor3;
	delete bigColor2;
	delete bigColor;
}

void SSGI::applySSGI(unsigned _w, unsigned _h, const rr::RRCamera& _eye, float _intensity, float _radius, float _angleBias)
{
	if (!bigColor || !bigColor2 || !bigDepth || !ssgiProgram1 || !ssgiProgram2)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"SSGI shader failed to initialize.\n"));
		return;
	}

	// adjust map sizes to match render target size
	if (_w!=bigColor->getBuffer()->getWidth() || _h!=bigColor->getBuffer()->getHeight())
	{
		bigColor->getBuffer()->reset(rr::BT_2D_TEXTURE,_w,_h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		bigColor2->getBuffer()->reset(rr::BT_2D_TEXTURE,_w,_h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		bigColor3->getBuffer()->reset(rr::BT_2D_TEXTURE,_w,_h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		bigDepth->getBuffer()->reset(rr::BT_2D_TEXTURE,_w,_h,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
		bigColor->reset(false,false,false);
		bigColor2->reset(false,false,false);
		bigColor3->reset(false,false,false);
		bigDepth->reset(false,false,false);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	}
	
	// disable depth
	PreserveDepthTest p1;
	PreserveDepthMask p2;
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);

	// acquire source maps
	{
		// copy backbuffer to bigColor+bigDepth
		bigColor->bindTexture();
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,0,0,_w,_h,0);
		bigDepth->bindTexture();
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,0,0,_w,_h,0);
	}

	// calculate noisy SSGI texture
	Program* ssgiProgram = ssgiProgram1;
	ssgiProgram->useIt();
	ssgiProgram->sendTexture("depthMap",bigDepth);
	ssgiProgram->sendTexture("colorMap",bigColor);
	ssgiProgram->sendUniform("aoRange",_radius);
	ssgiProgram->sendUniform("occlusionIntensity",_intensity);
	ssgiProgram->sendUniform("angleBias",_angleBias);
	ssgiProgram->sendUniform("pixelSize",1.0f/_w,1.0f/_h);
	ssgiProgram->sendUniform("aoRangeInTextureSpaceIn1mDistance",(float)(_radius/(tan(_eye.getFieldOfViewHorizontalRad()/2)*2)),(float)(_radius/(tan(_eye.getFieldOfViewVerticalRad()/2)*2))); //!!! nefunguje v ortho
	ssgiProgram->sendUniform("depthRange",_eye.getNear()*_eye.getFar()/((_eye.getFar()-_eye.getNear())),_eye.getFar()/(_eye.getFar()-_eye.getNear()));
	if (1) // 0=noise, 1=blur
	{
		FBO oldFBOState = FBO::getState();
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,bigColor2);
		//glViewport(0,0,_w*2,_h*2);
		TextureRenderer::renderQuad();

		if (0) // 0=2d blur, 1=1d blur
		{
		// blur bigColor2 to render target
		oldFBOState.restore();
		ssgiProgram = ssgiProgram2;
		ssgiProgram->useIt();
		ssgiProgram->sendTexture("depthMap",bigDepth);
		ssgiProgram->sendTexture("colorMap",bigColor);
		ssgiProgram->sendTexture("aoMap",bigColor);
		ssgiProgram->sendUniform("pixelSize",1.0f/_w,0.f);
		ssgiProgram->sendUniform("depthRange",_eye.getNear()*_eye.getFar()/((_eye.getFar()-_eye.getNear())),_eye.getFar()/(_eye.getFar()-_eye.getNear()));
		PreserveFlag p0(GL_BLEND,GL_TRUE);
		PreserveBlendFunc p1;
		glBlendFunc(GL_ZERO,GL_SRC_COLOR);
		glViewport(0,0,_w,_h);
		TextureRenderer::renderQuad();
		}
		else
		{
		// blur bigColor2 to bigColor
		ssgiProgram = ssgiProgram2;
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,bigColor3);
		ssgiProgram->useIt();
		ssgiProgram->sendTexture("depthMap",bigDepth);
		ssgiProgram->sendTexture("colorMap",bigColor);
		ssgiProgram->sendTexture("aoMap",bigColor2);
		ssgiProgram->sendUniform("pixelSize",1.0f/_w,0.f);
		//ssgiProgram->sendUniform("depthRange",_eye.getNear()*_eye.getFar()/((_eye.getFar()-_eye.getNear())),_eye.getFar()/(_eye.getFar()-_eye.getNear()));
		TextureRenderer::renderQuad();

		// blur smallColor2 to render target
		oldFBOState.restore();
		ssgiProgram->sendTexture("depthMap",bigDepth);
		ssgiProgram->sendTexture("colorMap",bigColor);
		ssgiProgram->sendTexture("aoMap",bigColor3);
		ssgiProgram->sendUniform("pixelSize",0.f,1.0f/_h);
		//ssgiProgram->sendUniform("depthRange",_eye.getNear()*_eye.getFar()/((_eye.getFar()-_eye.getNear())),_eye.getFar()/(_eye.getFar()-_eye.getNear()));
		PreserveFlag p0(GL_BLEND,GL_TRUE);
		PreserveBlendFunc p1;
		glBlendFunc(GL_ZERO,GL_SRC_COLOR);
		TextureRenderer::renderQuad();
		}
	}
	else
	{
		PreserveFlag p0(GL_BLEND,GL_TRUE);
		PreserveBlendFunc p1;
		glBlendFunc(GL_ZERO,GL_SRC_COLOR);
		TextureRenderer::renderQuad();
	}
}

}; // namespace
