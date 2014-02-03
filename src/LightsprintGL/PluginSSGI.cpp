// --------------------------------------------------------------------------
// Screen space global illumination postprocess.
// Copyright (C) 2012-2013 Stepan Hrbek, Lightsprint. All rights reserved.
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
	Program* ssgiProgram3;

public:

	PluginRuntimeSSGI(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps)
	{
		// NEAREST separates foreground/background depths well, but estimating pixel depth in shader becomes very inaccurate for steps close to 1px (corridor gets dark in distance)
		// LINEAR solves "corridor gets dark in distance", but there are shadows behind edges (processing background fragment: when step hits edge of foreground, sampled depth can look like something close to background, and is accepted as shadow caster)
		// a) NEAREST for initial dz calculation, LINEAR for the rest of shader
		//    pouzit GL_ARB_sampler_objects
		//    --nic neresi, problem je s LINEAR v casti ktera by zustala LINEAR
		// b) vsude NEAREST, ale v druhy casti shaderu upravit delty v shaderu aby se vzdy cetlo ze stredu texelu
		//    ? neni jasny jak najednou zacilit na sted texelu v obou osach
		// b) vsude NEAREST, ale v druhy casti shaderu vzdy sklouznout do nejblizsiho stredu texelu a pokusit se upravit vypocet aby vse sedelo
		//    - sklouznuti zmeni smer, nepujde nic predpocitat pro smer
		// c) vsude NEAREST, ale v druhy casti shaderu simulovat LINEAR a neinterpolovat tam kde se hodne lisi
		//    - skoro 4x vic lookupu a hodne porovnavani, dost zpomali
		// d) predrenderovat normaly
		//    --nic neresi, problem je s LINEAR Z
		// e) paralelne pocitat s LINEAR i NEAREST, vzit to cemu vyjde mensi occlusion
		//    (- vysoka pravdepodobnost ze aspon jeden z nich nekde nenajde occlusion, chyba pak bude videt ve vysledku
		//      nicmene chybejici occlusion bude asi min rusit nez prebyvajici)
		//    - zustava tenka silueta
#if 1
		bigColor = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigColor2 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigColor3 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigDepth = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false,GL_LINEAR,GL_LINEAR,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
#else
		bigColor = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigColor2 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigColor3 = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_RGBA,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
		bigDepth = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,16,16,1,rr::BF_DEPTH,true,RR_GHOST_BUFFER),false,false,GL_NEAREST,GL_NEAREST,GL_CLAMP_TO_EDGE,GL_CLAMP_TO_EDGE);
#endif
		ssgiUberProgram = UberProgram::create(rr::RRString(0,L"%lsssgi.vs",pathToShaders.w_str()),rr::RRString(0,L"%lsssgi.fs",pathToShaders.w_str()));
		ssgiProgram1 = ssgiUberProgram ? ssgiUberProgram->getProgram("#define PASS 1\n") : NULL;
		ssgiProgram2 = ssgiUberProgram ? ssgiUberProgram->getProgram("#define PASS 2\n") : NULL;
		ssgiProgram3 = ssgiUberProgram ? ssgiUberProgram->getProgram("#define PASS 3\n") : NULL;
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
		
		if (!bigColor || !bigColor2 || !bigDepth || !ssgiProgram1 || !ssgiProgram2 || !ssgiProgram3)
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
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
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

		if (0)
		{
		// varianta: paralelne pocitat s LINEAR i NEAREST, vzit to cemu vyjde mensi occlusion
			FBO oldFBOState = FBO::getState();
			glViewport(0,0,w,h);
			
			// bigColor2 = SSGI(GL_LINEAR)
			bigDepth->bindTexture();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			bigColor->bindTexture();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,bigColor2);
			TextureRenderer::renderQuad();
			
			// bigColor3 = SSGI(GL_NEAREST)
			bigDepth->bindTexture();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			bigColor->bindTexture();
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,bigColor3);
			TextureRenderer::renderQuad();

			// bigColor = max(bigColor2,bigColor3)
			ssgiProgram = ssgiProgram3;
			ssgiProgram->useIt();
			ssgiProgram->sendTexture("colorMap",bigColor2);
			ssgiProgram->sendTexture("depthMap",bigColor3);
			oldFBOState.restore();
glViewport(_sp.viewport[0],_sp.viewport[1],w,h);
//			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,bigColor);
		PreserveFlag p4(GL_BLEND,GL_TRUE);
		PreserveBlendFunc p5;
		glBlendFunc(GL_ZERO,GL_SRC_COLOR);
			TextureRenderer::renderQuad();

			oldFBOState.restore();
			return;
		}

		if (1) // 0=noise, 1=blur
		{
			FBO oldFBOState = FBO::getState();
			FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,bigColor2);
			glViewport(0,0,w,h);
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
			ssgiProgram->sendUniform("tPixelSize",1.0f/w,0.f);
			ssgiProgram->sendUniform("mDepthRange",eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())),eye.getFar()/(eye.getFar()-eye.getNear()));
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
			ssgiProgram->sendUniform("tPixelSize",1.0f/w,0.f);
			//ssgiProgram->sendUniform("mDepthRange",eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())),eye.getFar()/(eye.getFar()-eye.getNear()));
			TextureRenderer::renderQuad();

			// blur smallColor2 to render target
			oldFBOState.restore();
			ssgiProgram->sendTexture("depthMap",bigDepth);
			ssgiProgram->sendTexture("colorMap",bigColor);
			ssgiProgram->sendTexture("aoMap",bigColor3);
			ssgiProgram->sendUniform("tPixelSize",0.f,1.0f/h);
			//ssgiProgram->sendUniform("mDepthRange",eye.getNear()*eye.getFar()/((eye.getFar()-eye.getNear())),eye.getFar()/(eye.getFar()-eye.getNear()));
			}
			glViewport(_sp.viewport[0],_sp.viewport[1],w,h);
		}
		PreserveFlag p4(GL_BLEND,GL_TRUE);
		PreserveBlendFunc p5;
		glBlendFunc(GL_ZERO,GL_SRC_COLOR);
		TextureRenderer::renderQuad();
		//if (_textureRenderer)
		//	_textureRenderer->render2D(bigDepth,NULL,1,0,0.35f,0.3f,0.3f,-1);
	}
};


/////////////////////////////////////////////////////////////////////////////
//
// PluginParamsSSGI

PluginRuntime* PluginParamsSSGI::createRuntime(const rr::RRString& pathToShaders, const rr::RRString& pathToMaps) const
{
	return new PluginRuntimeSSGI(pathToShaders, pathToMaps);
}

}; // namespace
