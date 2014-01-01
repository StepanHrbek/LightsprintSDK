// --------------------------------------------------------------------------
// Depth of field postprocess.
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "Lightsprint/GL/PluginDOF.h"
#include "Lightsprint/GL/PreserveState.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// PluginRuntimeDOF

class PluginRuntimeDOF : public PluginRuntime
{
	Texture* smallColor1;
	Texture* smallColor2;
	Texture* smallColor3;
	Texture* bigColor;
	Texture* bigDepth;
	Program* dofProgram1;
	Program* dofProgram2;
	Program* dofProgram3;

public:

PluginRuntimeDOF(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps)
{
	smallColor1 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	smallColor2 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	smallColor3 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	bigColor = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	bigDepth = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
	// #version 120 is necessary for 'poisson' array
	rr::RRString filename1(0,L"%lsdof.vs",pathToShaders.w_str());
	rr::RRString filename2(0,L"%lsdof.fs",pathToShaders.w_str());
	dofProgram1 = Program::create("#version 120\n#define PASS 1\n",filename1,filename2);
	dofProgram2 = Program::create("#version 120\n#define PASS 2\n",filename1,filename2);
	dofProgram3 = Program::create("#version 120\n#define PASS 3\n",filename1,filename2);
}

virtual ~PluginRuntimeDOF()
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

virtual void render(Renderer& _renderer, const PluginParams& _pp, const PluginParamsShared& _sp)
{
	_renderer.render(_pp.next,_sp);

	if (!smallColor1 || !smallColor2 || !smallColor3 || !bigColor || !bigDepth || !dofProgram1 || !dofProgram2 || !dofProgram3)
	{
		RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"DOF shader failed to initialize.\n"));
		return;
	}

	FBO oldFBOState = FBO::getState();

	// adjust map sizes to match render target size
	unsigned w = _sp.viewport[2];
	unsigned h = _sp.viewport[3];
	if (w!=bigColor->getBuffer()->getWidth() || h!=bigColor->getBuffer()->getHeight())
	{
		smallColor1->getBuffer()->reset(rr::BT_2D_TEXTURE,(w+1)/2,(h+1)/2,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		smallColor2->getBuffer()->reset(rr::BT_2D_TEXTURE,(w+1)/2,(h+1)/2,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		smallColor3->getBuffer()->reset(rr::BT_2D_TEXTURE,(w+1)/2,(h+1)/2,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		bigColor->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_RGBA,true,RR_GHOST_BUFFER);
		bigDepth->getBuffer()->reset(rr::BT_2D_TEXTURE,w,h,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER);
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
	PreserveFlag p3(GL_SCISSOR_TEST,false);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);

	// acquire source maps
	{
		// copy backbuffer to bigColor+bigDepth
		bigColor->bindTexture();
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,0,0,w,h,0);
		bigDepth->bindTexture();
		glCopyTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,0,0,w,h,0);
	}

	// fill smallColor1 with CoC
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,smallColor1);
	const rr::RRCamera& eye = *_sp.camera;
	dofProgram1->useIt();
	dofProgram1->sendTexture("depthMap",bigDepth);
	//dofProgram1->sendTexture("colorMap",bigColor);
	dofProgram1->sendUniform("pixelSize",1.0f/bigColor->getBuffer()->getWidth(),1.0f/bigColor->getBuffer()->getHeight());
	dofProgram1->sendUniform("depthRange",rr::RRVec3(eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())*eye.dofFar),eye.getFar()/(eye.getFar()-eye.getNear()),eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())*eye.dofNear)));
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
	dofProgram3->sendTexture("depthMap",bigDepth);
	dofProgram3->sendTexture("colorMap",bigColor);
	dofProgram3->sendTexture("smallMap",smallColor3);
	//dofProgram3->sendTexture("midMap",smallColor1);
	dofProgram3->sendUniform("pixelSize",1.0f/bigColor->getBuffer()->getWidth(),1.0f/bigColor->getBuffer()->getHeight());
	dofProgram3->sendUniform("depthRange",rr::RRVec3(eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())*eye.dofFar),eye.getFar()/(eye.getFar()-eye.getNear()),eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())*eye.dofNear)));
	glViewport(_sp.viewport[0],_sp.viewport[1],w,h);
	TextureRenderer::renderQuad();

}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsDOF

PluginRuntime* PluginParamsDOF::createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const
{
	return new PluginRuntimeDOF(pathToShaders, pathToMaps);
}

}; // namespace
