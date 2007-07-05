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

RRIlluminationPixelBufferInMemory::RRIlluminationPixelBufferInMemory(const char* filename, unsigned _width, unsigned _height, unsigned _spreadForegroundColor, RRColorRGBAF _backgroundColor, bool _smoothBackground)
{
	if(filename)
	{
		texture = de::Texture::loadM(filename,NULL,false,false);
	}
	else
	{
		texture = de::Texture::createM(NULL,_width,_height,false,de::Texture::TF_RGBA);
	}
	renderedTexels = NULL;
	spreadForegroundColor = _spreadForegroundColor;
	backgroundColor = _backgroundColor;
	smoothBackground = _smoothBackground;
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

void filter(RRColorRGBAF* inputs,RRColorRGBAF* outputs,unsigned width,unsigned height,bool* _changed)
{
	unsigned size = width*height;
	bool changed = false;
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

			changed = true;
		}
	}
	*_changed = changed;
}

void RRIlluminationPixelBufferInMemory::renderEnd(bool preferQualityOverSpeed)
{
	if(!renderedTexels)
	{
		RRReporter::report(RRReporter::WARN,"No texels rendered into map, bad unwrap or low calculation quality?\n");
		return;
	}

	unsigned numTexels = texture->getWidth()*texture->getHeight();

	// normalize texels
	//  magnifies noise in low reliability texels
	//  but prevents unwanted leaks from distant areas into low reliability areas
	// note: can be deleted, incoming alpha should be always 0 or 1 so this should be no op
	#pragma omp parallel for schedule(static)
	for(int i=0;i<(int)numTexels;i++)
	{
		if(renderedTexels[i][3])
		{
			renderedTexels[i] /= renderedTexels[i][3];
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
	for(unsigned i=0;i<numTexels;i++)
	{
		renderedTexels[i].color = diagColors[CLAMPED(renderedTexels[i].color&255,0,7)];
	}
#endif

#ifndef DIAGNOSTIC_RED_UNRELIABLE
	// alloc second workspace
	RRColorRGBAF* workspaceTexels = new rr::RRColorRGBAF[numTexels];

	// spread foreground color
	bool changed = true;
	for(unsigned pass=0;pass<spreadForegroundColor && changed;pass++)
	{
		filter(renderedTexels,workspaceTexels,texture->getWidth(),texture->getHeight(),&changed);
		filter(workspaceTexels,renderedTexels,texture->getWidth(),texture->getHeight(),&changed);
	}

	// free second workspace
	SAFE_DELETE_ARRAY(workspaceTexels);

	// set background color
	for(unsigned i=0;i<numTexels;i++)
	{
		RRReal alpha = renderedTexels[i][3];
		if(smoothBackground)
		{
			if(alpha<1)
			{
				renderedTexels[i] = renderedTexels[i] + backgroundColor*(1-alpha);
			}

		}
		else
		{
			if(alpha<=0)
			{
				renderedTexels[i] = backgroundColor;
			}
			else
			{
				renderedTexels[i] = renderedTexels[i]/alpha;
			}
		}
	}

	// copy to texture
	texture->reset(texture->getWidth(),texture->getHeight(),de::Texture::TF_RGBAF,(const unsigned char*)renderedTexels,false);

	// free workspace
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

RRIlluminationPixelBuffer* RRIlluminationPixelBuffer::create(unsigned w, unsigned h, unsigned spreadForegroundColor, RRColorRGBAF backgroundColor, bool smoothBackground)
{
	return new RRIlluminationPixelBufferInMemory(NULL,w,h,spreadForegroundColor,backgroundColor,smoothBackground);
}

RRIlluminationPixelBuffer* RRIlluminationPixelBuffer::load(const char* filename)
{
	RRIlluminationPixelBufferInMemory* illum = new RRIlluminationPixelBufferInMemory(filename,0,0,2,RRColorRGBAF(0),false);
	if(!illum->texture)
		SAFE_DELETE(illum);
	return illum;
}

} // namespace
