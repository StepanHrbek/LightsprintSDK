// --------------------------------------------------------------------------
// Tone mapping
// Copyright (C) 2008-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdlib>
#include <GL/glew.h>
#include "Lightsprint/GL/ToneMapping.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "Lightsprint/GL/FBO.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// ToneMapping

ToneMapping::ToneMapping(const char* pathToShaders)
{
	bigTexture = NULL;
	smallTexture = NULL;
}

ToneMapping::~ToneMapping()
{
	if (smallTexture)
	{
		delete smallTexture->getBuffer();
		delete smallTexture;
	}
	if (bigTexture)
	{
		delete bigTexture->getBuffer();
		delete bigTexture;
	}
}

void ToneMapping::adjustOperator(TextureRenderer* textureRenderer, rr::RRReal secondsSinceLastAdjustment, rr::RRVec3& brightness, rr::RRReal contrast, rr::RRReal targetIntensity)
{
	if (!textureRenderer) return;
	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);

	if (!bigTexture) bigTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,1,1,1,rr::BF_RGB,true,NULL),false,false,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
	bigTexture->bindTexture();
	int bwidth = viewport[2];
	int bheight = viewport[3];
	glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB,0,0,bwidth,bheight,0);

	const unsigned swidth = 32;
	const unsigned sheight = 32;
	unsigned char buf[swidth*sheight*3];
	unsigned histo[256];
	unsigned avg = 0;
	if (!smallTexture)
	{
		smallTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,swidth,sheight,1,rr::BF_RGB,true,NULL),false,false,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
	}
	FBO oldFBOState = FBO::getState();
	FBO::setRenderTarget(GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,smallTexture);
	glViewport(0,0,swidth,sheight);
	textureRenderer->render2D(bigTexture,NULL,1,0,0,1,1);
	glReadPixels(0,0,swidth,sheight,GL_RGB,GL_UNSIGNED_BYTE,buf);
	oldFBOState.restore();
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	for (unsigned i=0;i<256;i++)
		histo[i] = 0;
	for (unsigned i=0;i<swidth*sheight*3;i+=3)
		histo[RR_MAX3(buf[i],buf[i+1],buf[i+2])]++;
	for (unsigned i=0;i<256;i++)
		avg += histo[i]*i;
	if (avg==0)
	{
		// completely black scene
		//  extremely low brightness? -> reset it to 1
		//  disabled lighting? -> avoid increasing brightness ad infinitum
		brightness = rr::RRVec3(1);
	}
	else
	{
		avg = avg/(swidth*sheight)+1;
		if (histo[255]>=swidth*sheight*7/10) avg = 1000; // at least 70% of screen overshot, adjust faster
		brightness *= pow(targetIntensity*255/avg,RR_CLAMPED(secondsSinceLastAdjustment*0.15f,0.0002f,0.2f));
	}
	//rr::RRReporter::report(rr::INF1,"%d\n",avg);
}

}; // namespace
