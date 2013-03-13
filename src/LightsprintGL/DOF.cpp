// --------------------------------------------------------------------------
// Depth of field postprocess.
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/GL/DOF.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "PreserveState.h"
#include "tmpstr.h"

namespace rr_gl
{

DOF::DOF(const char* pathToShaders)
{
	smallColor1 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	smallColor2 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	smallColor3 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	bigColor = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	bigDepth = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	dofProgram1 = Program::create("#define PASS 1\n",tmpstr("%sdof.vs",pathToShaders),tmpstr("%sdof.fs",pathToShaders));
	dofProgram2 = Program::create("#define PASS 2\n",tmpstr("%sdof.vs",pathToShaders),tmpstr("%sdof.fs",pathToShaders));
	dofProgram3 = Program::create("#define PASS 3\n",tmpstr("%sdof.vs",pathToShaders),tmpstr("%sdof.fs",pathToShaders));
}

DOF::~DOF()
{
	delete dofProgram3;
	delete dofProgram2;
	delete dofProgram1;
	delete bigDepth;
	delete bigColor;
	delete smallColor3;
	delete smallColor2;
	delete smallColor1;
}

void DOF::applyDOF(unsigned _w, unsigned _h, const rr::RRCamera& _eye)
{
	if (!smallColor1 || !smallColor2 || !smallColor3 || !bigColor || !bigDepth || !dofProgram1 || !dofProgram2 || !dofProgram3)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"DOF shader failed to initialize.\n"));
		return;
	}

	FBO oldFBOState = FBO::getState();

	// adjust map sizes to match render target size
	if (_w!=bigColor->getBuffer()->getWidth() || _h!=bigColor->getBuffer()->getHeight())
	{
		smallColor1->getBuffer()->reset(rr::BT_2D_TEXTURE,(_w+1)/2,(_h+1)/2,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		smallColor2->getBuffer()->reset(rr::BT_2D_TEXTURE,(_w+1)/2,(_h+1)/2,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		smallColor3->getBuffer()->reset(rr::BT_2D_TEXTURE,(_w+1)/2,(_h+1)/2,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		bigColor->getBuffer()->reset(rr::BT_2D_TEXTURE,_w,_h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		bigDepth->getBuffer()->reset(rr::BT_2D_TEXTURE,_w,_h,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
		smallColor1->reset(false,false,false);
		smallColor2->reset(false,false,false);
		smallColor3->reset(false,false,false);
		bigColor->reset(false,false,false);
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

	// fill smallColor1 with CoC
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,smallColor1);
	dofProgram1->useIt();
	dofProgram1->sendTexture("depthMap",bigDepth);
	//dofProgram1->sendTexture("colorMap",bigColor);
	dofProgram1->sendUniform("pixelSize",1.0f/bigColor->getBuffer()->getWidth(),1.0f/bigColor->getBuffer()->getHeight());
	dofProgram1->sendUniform("depthRange",rr::RRVec3(_eye.getNear()*_eye.getFar()/((_eye.getFar()-_eye.getNear())*_eye.dofFar),_eye.getFar()/(_eye.getFar()-_eye.getNear()),_eye.getNear()*_eye.getFar()/((_eye.getFar()-_eye.getNear())*_eye.dofNear)));
	glViewport(0,0,smallColor1->getBuffer()->getWidth(),smallColor1->getBuffer()->getHeight());
	TextureRenderer::renderQuad();

	// blur smallColor1 to smallColor2
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,smallColor2);
	dofProgram2->useIt();
	dofProgram2->sendTexture("smallMap",smallColor1);
	dofProgram2->sendUniform("pixelSize",1.0f/smallColor1->getBuffer()->getWidth(),0.f);
	TextureRenderer::renderQuad();

	// blur smallColor2 to smallColor3
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,smallColor3);
	dofProgram2->sendTexture("smallMap",smallColor2);
	dofProgram2->sendUniform("pixelSize",0.f,1.0f/smallColor1->getBuffer()->getHeight());
	TextureRenderer::renderQuad();

	// blur bigColor+bigDepth to render target using CoC from smallColor
	oldFBOState.restore();
	dofProgram3->useIt();
	//dofProgram3->sendTexture("depthMap",bigDepth);
	dofProgram3->sendTexture("colorMap",bigColor);
	dofProgram3->sendTexture("smallMap",smallColor3);
	dofProgram3->sendTexture("midMap",smallColor1);
	dofProgram3->sendUniform("pixelSize",1.0f/bigColor->getBuffer()->getWidth(),1.0f/bigColor->getBuffer()->getHeight());
	dofProgram3->sendUniform("depthRange",rr::RRVec3(_eye.getNear()*_eye.getFar()/((_eye.getFar()-_eye.getNear())*_eye.dofFar),_eye.getFar()/(_eye.getFar()-_eye.getNear()),_eye.getNear()*_eye.getFar()/((_eye.getFar()-_eye.getNear())*_eye.dofNear)));
	glViewport(0,0,bigColor->getBuffer()->getWidth(),bigColor->getBuffer()->getHeight());
	TextureRenderer::renderQuad();
}

}; // namespace
