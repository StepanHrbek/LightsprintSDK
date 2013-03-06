// --------------------------------------------------------------------------
// Depth of field postprocess.
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/GL/DOF.h"
#include "PreserveState.h"
#include "tmpstr.h"

namespace rr_gl
{

DOF::DOF(const char* pathToShaders)
{
	bigColor = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	bigDepth = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	dofProgram = Program::create("#define PASS 1\n",tmpstr("%sdof.vs",pathToShaders),tmpstr("%sdof.fs",pathToShaders));
}

DOF::~DOF()
{
	delete dofProgram;
	delete bigDepth;
	delete bigColor;
}

void DOF::applyDOF(unsigned _w, unsigned _h, const rr::RRCamera& _eye)
{
	if (!bigColor || !bigDepth || !dofProgram)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"DOF shader failed to initialize.\n"));
		return;
	}

	FBO oldFBOState = FBO::getState();

	// adjust map sizes to match render target size
	if (_w!=bigColor->getBuffer()->getWidth() || _h!=bigColor->getBuffer()->getHeight())
	{
		bigColor->getBuffer()->reset(rr::BT_2D_TEXTURE,_w,_h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		bigDepth->getBuffer()->reset(rr::BT_2D_TEXTURE,_w,_h,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
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

	// blur bigColor+bigDepth to render target
	oldFBOState.restore();
	dofProgram->useIt();
	dofProgram->sendTexture("colorMap",bigColor,0);
	dofProgram->sendTexture("depthMap",bigDepth,1);
	dofProgram->sendUniform("zNear",_eye.getNear());
	dofProgram->sendUniform("zFar",_eye.getFar());
	dofProgram->sendUniform("focusNearFar",_eye.dofNear,_eye.dofFar);
	dofProgram->sendUniform("radiusAtTwiceFocalLength",5.0f/bigColor->getBuffer()->getWidth(),5.0f/bigColor->getBuffer()->getHeight());
	glViewport(0,0,bigColor->getBuffer()->getWidth(),bigColor->getBuffer()->getHeight());
	TextureRenderer::renderQuad();
}

}; // namespace
