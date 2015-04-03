// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Depth of field postprocess.
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
	// accumulation based
	rr::RRString bokehFilename;
	rr::RRBuffer* bokehBuffer;

	// shader based
	Texture* smallColor1;
	Texture* smallColor2;
	Texture* smallColor3;
	Texture* bigColor;
	Texture* bigDepth;
	Program* dofProgram1;
	Program* dofProgram2;
	Program* dofProgram3;

public:

	PluginRuntimeDOF(const PluginCreateRuntimeParams& params)
	{
		// accumulation based
		bokehBuffer = nullptr;

		// shader based
		smallColor1 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		smallColor2 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		smallColor3 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigColor = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigDepth = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		rr::RRString filename1(0,L"%lsdof.vs",params.pathToShaders.w_str());
		rr::RRString filename2(0,L"%lsdof.fs",params.pathToShaders.w_str());
		dofProgram1 = Program::create("#define PASS 1\n",filename1,filename2);
		dofProgram2 = Program::create("#define PASS 2\n",filename1,filename2);
		dofProgram3 = Program::create("#define PASS 3\n",filename1,filename2);
	}

	virtual ~PluginRuntimeDOF()
	{
		// accumulation based
		delete bokehBuffer;

		// shader based
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
		const PluginParamsDOF& pp = *dynamic_cast<const PluginParamsDOF*>(&_pp);

		if (!_sp.camera)
		{
			RR_LIMITED_TIMES(1,rr::RRReporter::report(rr::WARN,"DOF: camera not set.\n"));
			return;
		}

		// accumulation based version
		if (pp.accumulated)
		{
			// select sample from bokeh texture
			if (bokehFilename!=pp.apertureShapeFilename)
			{
				bokehFilename = pp.apertureShapeFilename;
				delete bokehBuffer;
				bokehBuffer = rr::RRBuffer::load(pp.apertureShapeFilename);
			}
			unsigned numSamples = 0;
			float sampledSum = 0;
		more_samples_needed:
			rr::RRVec3 offsetInBuffer = rr::RRVec3(rand()/float(RAND_MAX),rand()/float(RAND_MAX),0); // 0..1
			if (bokehBuffer)
			{
				sampledSum += bokehBuffer->getElementAtPosition(offsetInBuffer,nullptr,false).RRVec3::avg();
				numSamples++;
				if (numSamples<100) // bad luck or black bokeh texture, stop sampling
				if (sampledSum<1) // sample is not inside bokeh shape, or is in darker region
					goto more_samples_needed;
			}
			else
			{
				rr::RRVec2 a(offsetInBuffer.x*2-1,offsetInBuffer.y*2-1);
				if (a.length2()>1) // sample is not inside default bokeh shape (circle)
					goto more_samples_needed;
			}

			// jitter camera [#48]
			PluginParamsShared sp(_sp);
			rr::RRCamera camera(*_sp.camera);
			rr::RRReal dofDistance = (camera.dofFar+camera.dofNear)/2;
			rr::RRVec2 offsetInMeters = rr::RRVec2(offsetInBuffer.x-0.5f,offsetInBuffer.y-0.5f)*camera.apertureDiameter; // how far do we move camera in right,up directions, -apertureDiameter/2..apertureDiameter/2 (m)
			camera.setPosition(camera.getPosition()+camera.getRight()*offsetInMeters.x+camera.getUp()*offsetInMeters.y);
			rr::RRVec2 visibleMetersAtFocusedDistance(tan(camera.getFieldOfViewHorizontalRad()/2)*dofDistance,tan(camera.getFieldOfViewVerticalRad()/2)*dofDistance); // from center to edge (m)
			rr::RRVec2 offsetOnScreen = offsetInMeters/visibleMetersAtFocusedDistance; // how far do we move camera in -1..1 screen space 
			camera.setScreenCenter(camera.getScreenCenter()-offsetOnScreen);
			sp.camera = &camera;

			// render frame
			_renderer.render(_pp.next,sp);
			return;
		}

		// shader based version
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
#ifndef RR_GL_ES2
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
#endif
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
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,_sp.viewport[0],_sp.viewport[1],w,h,0);
			bigDepth->bindTexture();
			glCopyTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,_sp.viewport[0],_sp.viewport[1],w,h,0);
		}

		// fill smallColor1 with CoC
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,smallColor1,oldFBOState);
		const rr::RRCamera& eye = *_sp.camera;
		dofProgram1->useIt();
		dofProgram1->sendTexture("depthMap",bigDepth);
		//dofProgram1->sendTexture("colorMap",bigColor);
		dofProgram1->sendUniform("tPixelSize",1.0f/bigColor->getBuffer()->getWidth(),1.0f/bigColor->getBuffer()->getHeight());
		dofProgram1->sendUniform("mScreenSizeIn1mDistance",(float)(2*tan(eye.getFieldOfViewHorizontalRad()/2)),(float)(2*tan(eye.getFieldOfViewVerticalRad()/2))); // from edge to edge //!!! nefunguje v ortho
		dofProgram1->sendUniform("mApertureDiameter",eye.apertureDiameter);
		dofProgram1->sendUniform("mFocusDistanceNear",eye.dofNear);
		dofProgram1->sendUniform("mFocusDistanceFar",eye.dofFar);
		//dofProgram1->sendUniform("depthRange",rr::RRVec3(eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())*eye.dofFar),eye.getFar()/(eye.getFar()-eye.getNear()),eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())*eye.dofNear)));
		dofProgram1->sendUniform("mDepthRange",eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())),eye.getFar()/(eye.getFar()-eye.getNear()));
		glViewport(0,0,smallColor1->getBuffer()->getWidth(),smallColor1->getBuffer()->getHeight());
		TextureRenderer::renderQuad();

		// blur smallColor1 to smallColor2
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,smallColor2,oldFBOState);
		dofProgram2->useIt();
		dofProgram2->sendTexture("smallMap",smallColor1);
		dofProgram2->sendUniform("tPixelSize",1.0f/smallColor1->getBuffer()->getWidth(),0.f);
		TextureRenderer::renderQuad();

		// blur smallColor2 to smallColor3
		FBO::setRenderTarget(GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,smallColor3,oldFBOState);
		dofProgram2->sendTexture("smallMap",smallColor2);
		dofProgram2->sendUniform("tPixelSize",0.f,1.0f/smallColor1->getBuffer()->getHeight());
		TextureRenderer::renderQuad();

		// blur bigColor+bigDepth to render target using CoC from smallColor
		oldFBOState.restore();
		dofProgram3->useIt();
		dofProgram3->sendTexture("depthMap",bigDepth);
		dofProgram3->sendTexture("colorMap",bigColor);
		dofProgram3->sendTexture("smallMap",smallColor3);
		//dofProgram3->sendTexture("midMap",smallColor1);
		dofProgram3->sendUniform("tPixelSize",1.0f/bigColor->getBuffer()->getWidth(),1.0f/bigColor->getBuffer()->getHeight());
		dofProgram3->sendUniform("mFocusDistanceFar",eye.dofFar);
		//dofProgram3->sendUniform("depthRange",rr::RRVec3(eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())*eye.dofFar),eye.getFar()/(eye.getFar()-eye.getNear()),eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())*eye.dofNear)));
		dofProgram3->sendUniform("mDepthRange",eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())),eye.getFar()/(eye.getFar()-eye.getNear()));
		enum {NUM_SAMPLES=60};
		rr::RRVec2 sample[2*NUM_SAMPLES] = {
			rr::RRVec2(-0.8769053f, -0.1801577f),
			rr::RRVec2(-0.560903f, 0.06491917f),
			rr::RRVec2(-0.8496868f, -0.4068463f),
			rr::RRVec2(-0.6684758f, -0.2965162f),
			rr::RRVec2(-0.7673495f, -0.005029257f),
			rr::RRVec2(-0.9880703f, 0.04116542f),
			rr::RRVec2(-0.7846559f, 0.3813636f),
			rr::RRVec2(-0.7290462f, 0.5805985f),
			rr::RRVec2(-0.3905317f, 0.3820366f),
			rr::RRVec2(-0.4862368f, 0.5637667f),
			rr::RRVec2(-0.9504438f, 0.2511463f),
			rr::RRVec2(-0.4776598f, -0.1798614f),
			rr::RRVec2(-0.604219f, 0.2934287f),
			rr::RRVec2(-0.2081008f, -0.04978198f),
			rr::RRVec2(-0.2508037f, 0.1751049f),
			rr::RRVec2(-0.6864566f, -0.5584068f),
			rr::RRVec2(-0.4704278f, -0.3887475f),
			rr::RRVec2(-0.4935126f, -0.6113455f),
			rr::RRVec2(-0.1219745f, -0.2906288f),
			rr::RRVec2(-0.2514639f, -0.4794517f),
			rr::RRVec2(0.001370483f, -0.5498651f),
			rr::RRVec2(-0.2616202f, -0.7039202f),
			rr::RRVec2(-0.08041584f, -0.8092448f),
			rr::RRVec2(0.03335056f, 0.2391213f),
			rr::RRVec2(0.005224985f, 0.01072675f),
			rr::RRVec2(0.1313823f, -0.1700258f),
			rr::RRVec2(-0.4821748f, 0.8128408f),
			rr::RRVec2(0.1671764f, -0.7225859f),
			rr::RRVec2(0.1472889f, -0.9805518f),
			rr::RRVec2(-0.319748f, -0.9090942f),
			rr::RRVec2(-0.2708471f, 0.8782267f),
			rr::RRVec2(-0.2557987f, 0.5722433f),
			rr::RRVec2(-0.02759428f, 0.67807520f),
			rr::RRVec2(-0.08137709f, 0.4126224f),
			rr::RRVec2(0.1456477f, 0.8903562f),
			rr::RRVec2(0.3371713f, 0.7640222f),
			rr::RRVec2(0.2000765f, 0.4972369f),
			rr::RRVec2(-0.06930163f, 0.964883f),
			rr::RRVec2(0.2733895f, 0.09887506f),
			rr::RRVec2(0.2422106f, -0.463336f),
			rr::RRVec2(-0.5466529f, -0.8083814f),
			rr::RRVec2(0.6191299f, 0.5104562f),
			rr::RRVec2(0.5362201f, 0.7429689f),
			rr::RRVec2(0.3735984f, 0.3863356f),
			rr::RRVec2(0.6939798f, 0.1787464f),
			rr::RRVec2(0.4732445f, 0.1937991f),
			rr::RRVec2(0.8838001f, 0.2504165f),
			rr::RRVec2(0.8430692f, 0.4679047f),
			rr::RRVec2(0.447724f, -0.8855019f),
			rr::RRVec2(0.5618694f, -0.7126608f),
			rr::RRVec2(0.4958969f, -0.4975741f),
			rr::RRVec2(0.8366239f, -0.3825552f),
			rr::RRVec2(0.717171f, -0.5788124f),
			rr::RRVec2(0.5782395f, -0.1434195f),
			rr::RRVec2(0.3609872f, -0.1903974f),
			rr::RRVec2(0.7937168f, -0.00281623f),
			rr::RRVec2(0.9551116f, -0.1922427f),
			rr::RRVec2(0.6263707f, -0.3429523f),
			rr::RRVec2(0.9990594f, 0.01618059f),
			rr::RRVec2(0.3529771f, -0.6476724f)
		};
		int numSamples = NUM_SAMPLES;
		dofProgram3->sendUniform("num_samples",numSamples);
		dofProgram3->sendUniformArray("samples", numSamples, sample);
		glViewport(_sp.viewport[0],_sp.viewport[1],w,h);
		TextureRenderer::renderQuad();
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsDOF

PluginRuntime* PluginParamsDOF::createRuntime(const PluginCreateRuntimeParams& params) const
{
	return new PluginRuntimeDOF(params);
}

}; // namespace
