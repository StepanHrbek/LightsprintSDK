// --------------------------------------------------------------------------
// OpenGL implementation of ambient map interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2007
// --------------------------------------------------------------------------

#include <cstdio>
#include "RRIlluminationPixelBufferInMemory.h"

//#define DIAGNOSTIC // kazdy texel dostane barvu podle toho kolikrat do nej bylo zapsano
//#define DIAGNOSTIC_RED_UNRELIABLE // ukaze nespolehlivy texely cervene

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationPixelBufferInMemory

RRIlluminationPixelBufferInMemory::RRIlluminationPixelBufferInMemory(const char* filename, unsigned awidth, unsigned aheight)
{
	if(filename)
	{
		texture = de::Texture::loadM(filename,NULL,false,false);
	}
	else
	{
		texture = de::Texture::createM(NULL,awidth,aheight,false,de::Texture::TF_RGBA);
	}
	renderedTexels = NULL;
}

void RRIlluminationPixelBufferInMemory::renderBegin()
{
	if(renderedTexels) 
	{
		RR_ASSERT(0);
		return;
	}
}

void RRIlluminationPixelBufferInMemory::renderTriangle(const IlluminatedTriangle& it)
{
	LIMITED_TIMES(1,RRReporter::report(RRReporter::WARN,"RRIlluminationPixelBuffer::renderTriangle() not implemented by this instance."));
}

void RRIlluminationPixelBufferInMemory::renderTexel(const unsigned uv[2], const rr::RRColorRGBAF& color)
{
	if(!renderedTexels)
	{
		renderedTexels = new rr::RRColorRGBAF[texture->getWidth()*texture->getHeight()];
	}
	if(uv[0]>=texture->getWidth())
	{
		RR_ASSERT(0);
		return;
	}
	if(uv[1]>=texture->getHeight())
	{
		RR_ASSERT(0);
		return;
	}

	renderedTexels[uv[0]+uv[1]*texture->getWidth()] += color;
}

void filter(RRColorRGBAF* inputs,RRColorRGBAF* outputs,unsigned width,unsigned height)
{
	unsigned size = width*height;
#pragma omp parallel for schedule(static)
	for(int i=0;i<(int)size;i++)
	{
		RRColorRGBAF c = inputs[i];
		if(c[3]>=0.99f)
		{
			outputs[i] = c;
		}
		else
		{
			RRColorRGBAF c1 = inputs[(i+1)%size];
			RRColorRGBAF c2 = inputs[(i-1)%size];
			RRColorRGBAF c3 = inputs[(i+width)%size];
			RRColorRGBAF c4 = inputs[(i-width)%size];
			RRColorRGBAF c5 = inputs[(i+1+width)%size];
			RRColorRGBAF c6 = inputs[(i+1-width)%size];
			RRColorRGBAF c7 = inputs[(i-1+width)%size];
			RRColorRGBAF c8 = inputs[(i-1-width)%size];

			RRColorRGBAF cc = ( c + (c1+c2+c3+c4)*0.25f + (c5+c6+c7+c8)*0.166f )*1.3f;

			outputs[i] = cc/MAX(cc.w,1.0f);
		}
	}
}

void RRIlluminationPixelBufferInMemory::renderEnd(bool preferQualityOverSpeed)
{
	if(!renderedTexels)
	{
		RRReporter::report(RRReporter::WARN,"No texels rendered into map, bad unwrap or low calculation quality?\n");
		return;
	}

	// normalize texels
	#pragma omp parallel for schedule(static)
	for(int i=0;i<(int)(texture->getWidth()*texture->getHeight());i++)
	{
		if(renderedTexels[i].w)
		{
			renderedTexels[i] /= renderedTexels[i][3];
			renderedTexels[i][3] = 1; //missing vec4 operators
		}
#ifdef DIAGNOSTIC_RED_UNRELIABLE
		else
		{
			renderedTexels[i] = rr::RRColorRGBAF(1,0,0,0);//!!!
		}
#endif
	}

#ifdef DIAGNOSTIC
	// convert to visible colors
	unsigned diagColors[] =
	{
		0,0xff0000ff,0xff00ff00,0xffff0000,0xffffff00,0xffff00ff,0xff00ffff,0xffffffff
	};
	for(unsigned i=0;i<texture->getWidth()*texture->getHeight();i++)
	{
		renderedTexels[i].color = diagColors[CLAMPED(renderedTexels[i].color&255,0,7)];
	}
#endif

#ifndef DIAGNOSTIC_RED_UNRELIABLE
	// alloc second workspace
	RRColorRGBAF* workspaceTexels = new rr::RRColorRGBAF[texture->getWidth()*texture->getHeight()];

	// filter map
	for(int pass=0;pass<(preferQualityOverSpeed?2:2);pass++)
	{
		filter(renderedTexels,workspaceTexels,texture->getWidth(),texture->getHeight());
		filter(workspaceTexels,renderedTexels,texture->getWidth(),texture->getHeight());
	}

	// copy to texture
	texture->reset(texture->getWidth(),texture->getHeight(),de::Texture::TF_RGBAF,(const unsigned char*)renderedTexels,false);

	// free workspace
	SAFE_DELETE_ARRAY(workspaceTexels);
	SAFE_DELETE_ARRAY(renderedTexels);
#endif
}

unsigned RRIlluminationPixelBufferInMemory::getWidth() const
{
	return texture->getWidth();
}

unsigned RRIlluminationPixelBufferInMemory::getHeight() const
{
	return texture->getHeight();
}

void RRIlluminationPixelBufferInMemory::bindTexture() const
{
	texture->bindTexture();
}

bool RRIlluminationPixelBufferInMemory::save(const char* filename)
{
	return texture->save(filename,NULL);
}

RRIlluminationPixelBufferInMemory::~RRIlluminationPixelBufferInMemory()
{
	delete[] renderedTexels;
}


/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationPixelBuffer

RRIlluminationPixelBuffer* RRIlluminationPixelBuffer::create(unsigned w, unsigned h)
{
	return new RRIlluminationPixelBufferInMemory(NULL,w,h);
}

RRIlluminationPixelBuffer* RRIlluminationPixelBuffer::load(const char* filename)
{
	RRIlluminationPixelBufferInMemory* illum = new RRIlluminationPixelBufferInMemory(filename,0,0);
	if(!illum->texture)
		SAFE_DELETE(illum);
	return illum;
}

} // namespace
