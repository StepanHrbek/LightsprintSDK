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

		//! Begins rendering of triangles into lightmap. Must be paired with renderEnd().
		virtual void renderBegin() {};
		//! Description of one illuminated vertex.
		struct IlluminatedVertex
		{
			RRVec2 texCoord; ///< Triangle vertex positions in lightmap.
			RRColorRGBF measure; ///< Triangle vertex illumination.
		};
		//! Description of one illuminated triangle.
		struct IlluminatedTriangle
		{
			IlluminatedVertex iv[3]; ///< Three illuminated vertices forming triangle.
		};
		//! Renders one triangle into map. Must be called inside renderBegin() / renderEnd().
		virtual void renderTriangle(const IlluminatedTriangle& it) = 0;
		//! Renders many triangles into map. Must be called inside renderBegin() / renderEnd().
		virtual void renderTriangles(const IlluminatedTriangle* it, unsigned numTriangles);
		//! Finishes rendering of triangles into lightmap. Must be paired with renderBegin().
		virtual void renderEnd() {};

		// Pixel buffer use

		//! Locks the buffer for seeing array of all pixels at once. Optional, may return NULL.
		virtual const RRColorRGBA8* lock() {return NULL;};
		//! Unlocks previously locked buffer.
		virtual void unlock() {};
		//! Binds texture for rendering. Various implementations may do OpenGL bind, DirectX bind or nothing.
		virtual void bindTexture() {};

		virtual ~RRIlluminationPixelBuffer() {};
	};


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
			Channel()
			{
				vertexBuffer = NULL;
				pixelBuffer = NULL;
			}
			~Channel()
			{
				delete vertexBuffer;
				delete pixelBuffer;
			}
			RRIlluminationVertexBuffer* vertexBuffer;
			RRIlluminationPixelBuffer* pixelBuffer;
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
