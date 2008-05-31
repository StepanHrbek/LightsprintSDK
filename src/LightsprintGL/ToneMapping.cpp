// --------------------------------------------------------------------------
// Tone mapping
// Copyright (C) Stepan Hrbek, Lightsprint, 2008, All rights reserved
// --------------------------------------------------------------------------

#include <GL/glew.h>
#include "Lightsprint/GL/ToneMapping.h"

namespace rr_gl
{

//////////////////////////////////////////////////////////////////////////////
//
// ToneMapping

ToneMapping::ToneMapping(const char* pathToShaders)
{
	bigTexture = NULL;
	smallTexture = NULL;
	textureRenderer = new TextureRenderer(pathToShaders);
}

ToneMapping::~ToneMapping()
{
	delete textureRenderer;
	if(smallTexture)
	{
		delete smallTexture->getBuffer();
		delete smallTexture;
	}
	if(bigTexture)
	{
		delete bigTexture->getBuffer();
		delete bigTexture;
	}
}

void ToneMapping::adjustOperator(rr::RRReal secondsSinceLastAdjustment, rr::RRVec3& brightness, rr::RRReal contrast)
{
	if(!textureRenderer) return;
	int viewport[4];
	glGetIntegerv(GL_VIEWPORT,viewport);
	if(!bigTexture || bigTexture->getBuffer()->getWidth()<(unsigned)viewport[2] || bigTexture->getBuffer()->getHeight()<(unsigned)viewport[3])
	{
		if(bigTexture)
		{
			delete bigTexture->getBuffer();
			delete bigTexture;
		}
		bigTexture = new Texture(rr::RRBuffer::create(rr::BT_2D_TEXTURE,viewport[2],viewport[3],1,rr::BF_RGBA,true,NULL),false,false,GL_NEAREST,GL_NEAREST,GL_REPEAT,GL_REPEAT);
	}
	bigTexture->bindTexture();
	glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,viewport[0],viewport[1],viewport[2],viewport[3]);
	//glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,viewport[0],viewport[1],viewport[2],viewport[3],0); changes size of texture on GPU but doesn't update size in RRBuffer
	const unsigned width = 64;
	const unsigned height = 32;
	unsigned char buf[width*height*3];
	unsigned histo[256];
	unsigned avg = 0;
	textureRenderer->render2D(bigTexture,NULL,0,0,float(width)/viewport[2],float(height)/viewport[3]);
	glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,buf);
	for(unsigned i=0;i<256;i++)
		histo[i] = 0;
	for(unsigned i=0;i<width*height*3;i++)
		histo[buf[i]]++;
	for(unsigned i=0;i<256;i++)
		avg += histo[i]*i;
	avg = avg/(width*height)+1;
	brightness *= pow(128.0f/avg,CLAMPED(secondsSinceLastAdjustment*0.2f,0.0002f,0.3f));
}

}; // namespace
