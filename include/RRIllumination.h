#ifndef RRILLUMINATION_H
#define RRILLUMINATION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIllumination.h
//! \brief RRIllumination - library for calculated illumination storage
//! \version 2006.9
//! \author Copyright (C) Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRMath.h"
#include <cassert>
#include <cstring> // NULL

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#if defined(RR_RELEASE) || (defined(NDEBUG) && !defined(RR_DEBUG))
			#pragma comment(lib,"RRIllumination_sr.lib")
		#else
			#pragma comment(lib,"RRIllumination_sd.lib")
		#endif
#	else
#	ifdef RR_DLL_BUILD_ILLUMINATION
		// build dll
#		undef RR_API
#		define RR_API __declspec(dllexport)
#	else // use dll
#pragma comment(lib,"RRIllumination.lib")
#	endif
#	endif
#endif


#define RR_DEVELOPMENT_LIGHTMAP //!!!

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Float RGB - One of color formats for vertex and pixel buffers.
	//
	// All color formats have default constructor that sets black color.
	// This makes vertex and pixels buffers black after construction.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRColorRGBF : public RRVec3
	{
	public:
		RRColorRGBF()
		{
			x = y = z = 0;
		}
		RRColorRGBF(const RRVec3& a)
		{
			x = a.x;
			y = a.y;
			z = a.z;
		}
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! 8bit intensity - One of color formats for vertex and pixel buffers.
	//
	//////////////////////////////////////////////////////////////////////////////

	struct RRColorI8
	{
		RRColorI8()
		{
			color = 0;
		}
		RRColorI8(RRReal r,RRReal g,RRReal b)
		{
			color = (unsigned char)(255/3*(r+g+b));
		}
		const RRColorI8& operator =(const RRColorRGBF& a)
		{
			return *this = RRColorI8(a[0],a[1],a[2]);
		}
		bool operator ==(const RRColorI8& a)
		{
			return color==a.color;
		}
		bool operator !=(const RRColorI8& a)
		{
			return color!=a.color;
		}
		unsigned char color;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! RGBA8, total 32bits - One of color formats for vertex and pixel buffers.
	//
	//////////////////////////////////////////////////////////////////////////////

	struct RRColorRGBA8
	{
		RRColorRGBA8()
		{
			color = 0;
		}
		RRColorRGBA8(RRReal r,RRReal g,RRReal b)
		{
			color = ((unsigned char)(255*r)&255) + (((unsigned char)(255*g)&255)<<8) + (((unsigned char)(255*b)&255)<<16);
		}
		const RRColorRGBA8& operator =(const RRColorRGBF& a)
		{
			return *this = RRColorRGBA8(a[0],a[1],a[2]);
		}
		bool operator ==(const RRColorRGBA8& a)
		{
			return color==a.color;
		}
		bool operator !=(const RRColorRGBA8& a)
		{
			return color!=a.color;
		}
		unsigned color;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Interface to illumination storage based on vertex buffer.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRIlluminationVertexBuffer
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		// Vertex buffer creation

		//! Sets size of the buffer. Content may be lost.
		virtual void setSize(unsigned numVertices) = 0;
		//! Sets value of one element of the buffer.
		virtual void setVertex(unsigned vertex, const RRColorRGBF& color) = 0;

		// Vertex buffer use

		//! Locks the buffer for seeing array of all vertices at once. Optional, may return NULL.
		virtual const RRColorRGBF* lock() {return NULL;};
		//! Unlocks previously locked buffer.
		virtual void unlock() {};

		virtual ~RRIlluminationVertexBuffer() {};


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// Creates and returns vertex buffer in system memory. It is graphics API independent.
		static RRIlluminationVertexBuffer* createInSystemMemory(unsigned numVertices);
	};



#ifdef RR_DEVELOPMENT_LIGHTMAP
	//////////////////////////////////////////////////////////////////////////////
	//
	//! Interface to illumination storage based on pixel buffer, lightmap.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRIlluminationPixelBuffer
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		// Pixel buffer creation

		//! Sets size of buffer. Content may be lost.
		virtual void setSize(unsigned width, unsigned height) = 0;
		//! Marks all pixels as unused. Content may be lost.
		virtual void renderBegin() {};
		//! Description of one illuminated triangle.
		struct IlluminatedTriangle
		{
			RRVec2 texCoord[3]; ///< Triangle vertices positions in triangle space, triangle vertex0 is in 0,0, vertex1 is in 1,0, vertex2 is in 0,1.
			RRColorRGBF measure[3]; ///< Triangle vertices illumination.
		};
		//! Renders one triangle into map. Marks all triangle pixels as used. All other pixels stay unchanged.
		virtual void renderTriangle(const IlluminatedTriangle& it) = 0;
		//! Filters map so that unused pixels close to used pixels get their color (may be also marked as used). Used pixels stay unchanged.
		virtual void renderEnd() {};

		// Pixel buffer use

		//! Locks the buffer for seeing array of all pixels at once. Optional, may return NULL.
		virtual const RRColorRGBA8* lock() {return NULL;};
		//! Unlocks previously locked buffer.
		virtual void unlock() {};
		//! Binds texture for rendering. Various implementations may do OpenGL bind, DirectX bind or nothing.
		virtual void bindTexture() {};

		virtual ~RRIlluminationPixelBuffer() {};


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// Creates and returns pixel buffer in system memory. It is graphics API independent.
		static RRIlluminationPixelBuffer* createInSystemMemory(unsigned width, unsigned height);
		// Creates and returns pixel buffer in OpenGL texture. Is bindable. Depends on OpenGL.
		static RRIlluminationPixelBuffer* createInOpenGL(unsigned width, unsigned height);
	};
#endif

	//////////////////////////////////////////////////////////////////////////////
	//
	//! Storage for object's indirect illumination.
	//
	//! Editor stores calculated illumination here.
	//! Renderer reads illumination from here.
	//! Also unwrap for illumination maps may be stored here.
	//! Add one instance to each of your objects.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObjectIllumination
	{
	public:
		//! Enter PreImport number of vertices, length of vertex buffer for rendering.
		//! Vertex buffer will be created for PreImport vertices, so it is not suitable for MultiObject.
		RRObjectIllumination(unsigned anumPreImportVertices);

		struct Channel
		{
			Channel(unsigned anumVertices)
			{
				vertexBuffer = NULL;
#ifdef RR_DEVELOPMENT_LIGHTMAP
				pixelBuffer = NULL;
#endif
			}
			~Channel()
			{
				delete vertexBuffer;
#ifdef RR_DEVELOPMENT_LIGHTMAP
				delete pixelBuffer;
#endif
			}
			RRIlluminationVertexBuffer* vertexBuffer;
#ifdef RR_DEVELOPMENT_LIGHTMAP
			RRIlluminationPixelBuffer* pixelBuffer;
#endif
		};

		Channel* getChannel(unsigned channelIndex);
		unsigned getNumPreImportVertices();
		~RRObjectIllumination();
	protected:
		unsigned numPreImportVertices; ///< PreImport number of vertices, length of vertex buffer for rendering.
		void* hiddenChannels;
	};

} // namespace

#endif
