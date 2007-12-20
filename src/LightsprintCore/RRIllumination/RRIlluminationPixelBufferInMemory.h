// --------------------------------------------------------------------------
// System memory implementation of interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006-2007
// --------------------------------------------------------------------------

#ifndef RRILLUMINATIONPIXELBUFFERINMEMORY_H
#define RRILLUMINATIONPIXELBUFFERINMEMORY_H

#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/GL/Texture.h"

namespace rr
{

	class RRIlluminationPixelBufferInMemory : public RRIlluminationPixelBuffer
	{
	public:
		RRIlluminationPixelBufferInMemory(const char* filename, unsigned width, unsigned height);
		virtual void reset(const RRVec4* data);
		virtual unsigned getWidth() const;
		virtual unsigned getHeight() const;
		virtual const unsigned* lock(); // RGBA8
		virtual const RRVec3* lockRGBF();
		virtual void unlock();
		virtual void bindTexture() const;
		virtual bool save(const char* filename);
		virtual ~RRIlluminationPixelBufferInMemory();
	private:
		friend RRIlluminationPixelBuffer;
		rr_gl::Texture* texture;
		unsigned char* lockedPixels;
	};

} // namespace

#endif
