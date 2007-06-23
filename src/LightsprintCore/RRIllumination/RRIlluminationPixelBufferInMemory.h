// --------------------------------------------------------------------------
// System memory implementation of interface rr::RRIlluminationPixelBuffer.
// Copyright (C) Stepan Hrbek, Lightsprint, 2006-2007
// --------------------------------------------------------------------------

#ifndef RRILLUMINATIONPIXELBUFFERINMEMORY_H
#define RRILLUMINATIONPIXELBUFFERINMEMORY_H

#include "Lightsprint/RRIllumination.h"
#include "Lightsprint/DemoEngine/Texture.h"

namespace rr
{

	class RRIlluminationPixelBufferInMemory : public RRIlluminationPixelBuffer
	{
	public:
		RRIlluminationPixelBufferInMemory(const char* filename, unsigned width, unsigned height, unsigned spreadForegroundColor, RRColorRGBAF backgroundColor, bool smoothBackground);
		virtual void renderBegin();
		virtual void renderTriangle(const IlluminatedTriangle& it);
		virtual void renderTexel(const unsigned uv[2], const RRColorRGBAF& color);
		virtual void renderEnd(bool preferQualityOverSpeed);
		virtual unsigned getWidth() const;
		virtual unsigned getHeight() const;
		virtual void bindTexture() const;
		virtual bool save(const char* filename);
		virtual ~RRIlluminationPixelBufferInMemory();
	private:
		friend RRIlluminationPixelBuffer;
		RRColorRGBAF* renderedTexels;
		de::Texture* texture;
		unsigned spreadForegroundColor;
		RRColorRGBAF backgroundColor;
		bool smoothBackground;
	};

} // namespace

#endif
