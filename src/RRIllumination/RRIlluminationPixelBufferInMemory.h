#include "RRIllumination.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Illumination storage in pixel buffer in system memory.
	//
	//! Uses software rasterizer.
	//! Template parameter Color specifies format of one element in pixel buffer.
	//! It can be RRColorRGBF, RRColorRGBA8, RRColorI8.
	//
	//////////////////////////////////////////////////////////////////////////////

	template <class Color>
	class RRIlluminationPixelBufferInMemory : public RRIlluminationPixelBuffer
	{
	public:
		RRIlluminationPixelBufferInMemory(unsigned awidth, unsigned aheight)
		{
			width = awidth;
			height = aheight;
			pixels = new Color[width*height];
		}
		const Color* lock()
		{
			return pixels;
		}
		virtual void renderBegin()
		{
			for(unsigned i=0;i<width*height;i++)
			{
				pixels[i] = Color(1,1,0);
			}
		}
		virtual void renderTriangle(const IlluminatedTriangle& it);
		virtual void renderEnd()
		{
			for(unsigned j=0;j<height-1;j++)
			for(unsigned i=0;i<width-1;i++)
			{
				if(pixels[width*j+i]==Color(1,1,0))
				{
					if(pixels[width*j+i+1]!=Color(1,1,0)) pixels[width*j+i] = pixels[width*j+i+1]; else
					if(pixels[width*(j+1)+i]!=Color(1,1,0)) pixels[width*j+i] = pixels[width*(j+1)+i];
				}
			}
			for(unsigned j=height;--j>0;)
			for(unsigned i=width;--i>0;)
			{
				if(pixels[width*j+i]==Color(1,1,0))
				{
					if(pixels[width*j+i-1]!=Color(1,1,0)) pixels[width*j+i] = pixels[width*j+i-1]; else
					if(pixels[width*(j-1)+i]!=Color(1,1,0)) pixels[width*j+i] = pixels[width*(j-1)+i];
				}
			}
		}
		~RRIlluminationPixelBufferInMemory()
		{
			delete[] pixels;
		}
	private:
		unsigned width;
		unsigned height;
		Color*   pixels;
	};

} // namespace
