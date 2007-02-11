#ifndef RRILLUMINATION_H
#define RRILLUMINATION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIllumination.h
//! \brief RRIllumination - library for calculated illumination storage
//! \version 2007.2.7
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include "RRMath.h"
#include <cassert>
#include <cstring> // NULL

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#ifdef NDEBUG
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
#ifdef NDEBUG
	#pragma comment(lib,"RRIllumination.lib")
#else
	#pragma comment(lib,"RRIllumination_dd.lib")
#endif
#	endif
#	endif
#endif

#define CLAMPED(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))

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
		RRColorRGBF(RRReal a = 0)
		{
			x = y = z = a;
		}
		RRColorRGBF(RRReal r,RRReal g,RRReal b)
		{
			x = r;
			y = g;
			z = b;
		}
		RRColorRGBF(const RRVec3& a)
		{
			x = a.x;
			y = a.y;
			z = a.z;
		}
		RRColorRGBF toRRColorRGBF() const
		{
			return *this;
		}
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Float RGBA - One of color formats for vertex and pixel buffers.
	//
	// All color formats have default constructor that sets black color.
	// This makes vertex and pixels buffers black after construction.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RRColorRGBAF : public RRVec4
	{
	public:
		RRColorRGBAF(RRReal a = 0)
		{
			x = y = z = w = a;
		}
		RRColorRGBAF(RRReal r,RRReal g,RRReal b,RRReal a)
		{
			x = r;
			y = g;
			z = b;
			w = a;
		}
		RRColorRGBAF(const RRVec3& a)
		{
			x = a.x;
			y = a.y;
			z = a.z;
			w = 0;
		}
		RRColorRGBAF(const RRVec4& a)
		{
			x = a.x;
			y = a.y;
			z = a.z;
			this->w = a.w;
		}
		RRColorRGBF toRRColorRGBF() const
		{
			return RRColorRGBF(x,y,z);
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
			color = (unsigned char)CLAMPED(85*(r+g+b),0,255);
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
		RRReal operator [](int i) const 
		{
			return color/255.0f;
		}
		RRColorRGBF toRRColorRGBF() const
		{
			return RRColorRGBF((*this)[0],(*this)[1],(*this)[2]);
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
			color = (unsigned)CLAMPED(255*r,0,255) + (((unsigned)CLAMPED(255*g,0,255))<<8) + (((unsigned)CLAMPED(255*b,0,255))<<16);
		}
		RRColorRGBA8(RRReal r,RRReal g,RRReal b,RRReal a)
		{
			color = (unsigned)CLAMPED(255*r,0,255) + (((unsigned)CLAMPED(255*g,0,255))<<8) + (((unsigned)CLAMPED(255*b,0,255))<<16) + (((unsigned)CLAMPED(255*a,0,255))<<24);
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
		RRReal operator [](int i) const 
		{
			return ((color>>(i*8))&255)/255.0f;
		}
		RRColorRGBF toRRColorRGBF() const
		{
			return RRColorRGBF((*this)[0],(*this)[1],(*this)[2]);
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

		//! Returns size of vertex buffer in vertices.
		virtual const unsigned getNumVertices() = 0;
		//! Sets value of one element of the buffer.
		//! Must be thread safe.
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
	//! Interface to illumination storage based on 2D pixel buffer, ambient map.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRIlluminationPixelBuffer
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		// Pixel buffer creation

		//! Begins rendering of triangles into ambient map. Must be paired with renderEnd().
		virtual void renderBegin() {};
		//! Description of one illuminated vertex.
		struct IlluminatedVertex
		{
			RRVec2 texCoord; ///< Triangle vertex positions in ambient map.
			RRColorRGBF measure; ///< Triangle vertex illumination.
		};
		//! Description of one illuminated triangle.
		struct IlluminatedTriangle
		{
			IlluminatedVertex iv[3]; ///< Three illuminated vertices forming triangle.
		};
		//! Renders one triangle into map. Must be called inside renderBegin() / renderEnd().
		//
		//! \param it
		//!  Description of single triangle.
		virtual void renderTriangle(const IlluminatedTriangle& it) = 0;
		//! Renders multiple triangles into map. Must be called inside renderBegin() / renderEnd().
		//
		//! \param it
		//!  Array with description of triangles.
		//! \param numTriangles
		//!  Length of it array, number of triangles to be rendered.
		virtual void renderTriangles(const IlluminatedTriangle* it, unsigned numTriangles);
		//! Renders one texel into map. Must be called inside renderBegin() / renderEnd().
		//
		//! \param uv
		//!  Array of 2 elements, texel coordinates in 0..width-1, 0..height-1 range.
		//! \param color
		//!  Color of rendered texel.
		//!  RRRealtimeRadiosity sets irradiance in custom scale here.
		//!  With color r,g,b and probability p that color is correct, RRColorRGBAF(r*p,g*p,b*p,p) is set.
		virtual void renderTexel(const unsigned uv[2], const RRColorRGBAF& color) = 0;
		//! Finishes rendering into ambient map. Must be paired with renderBegin().
		//
		//! Colors with low alpha (probability of correctness) should be processed
		//! and replaced by nearby colors with higher probability.
		//! Colors in form r*p,g*p,b*p,p should be normalized to r,g,b,1.
		virtual void renderEnd() {};

		// Pixel buffer use

		//! \return Width of pixel buffer in pixels.
		virtual unsigned getWidth() const = 0;
		//! \return Height of pixel buffer in pixels.
		virtual unsigned getHeight() const = 0;
		//! Locks the buffer for seeing array of all pixels at once. Optional, may return NULL.
		virtual const RRColorRGBA8* lock() {return NULL;};
		//! Unlocks previously locked buffer.
		virtual void unlock() {};
		//! Binds texture for rendering. Various implementations may do OpenGL bind, Direct3D bind or nothing.
		virtual void bindTexture() {};

		virtual ~RRIlluminationPixelBuffer() {};


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Saves pixel buffer to disk.
		//! Not mandatory, thin implementations may completely skip saving and always return false.
		//! \param filename
		//!  Filename of image to be created on disk.
		//!  Supported file formats are implementation defined.
		//!  Example: "/maps/ambientmap.png"
		//! \return
		//!  True on successful save of complete environment map.
		virtual bool save(const char* filename) {return false;}
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Interface to illumination storage based on environment map.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRIlluminationEnvironmentMap
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		// Environment map creation

		//! Sets illumination values in map.
		//! \param size
		//!  Width and height of one side of cube map.
		//! \param irradiance
		//!  Array of 6*size*size irradiance values in physical scale [W/m^2] in this order:
		//!  \n size*size values for POSITIVE_X side,
		//!  \n size*size values for NEGATIVE_X side,
		//!  \n size*size values for POSITIVE_Y side,
		//!  \n size*size values for NEGATIVE_Y side,
		//!  \n size*size values for POSITIVE_Z side,
		//!  \n size*size values for NEGATIVE_Z side.
		virtual void setValues(unsigned size, RRColorRGBF* irradiance) = 0;

		// Environment map use

		//! Binds texture for use by renderer.
		//! Not mandatory, implementation may do OpenGL bind, DirectX bind or nothing.
		virtual void bindTexture() {};

		virtual ~RRIlluminationEnvironmentMap() {};


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Saves environment map to disk.
		//! Not mandatory, thin implementations may completely skip saving and always return false.
		//! \param filenameMask
		//!  Filename mask of images to be created on disk.
		//!  Supported file formats are implementation defined.
		//!  Implementation is free to create 6 files for 6 cube sides.
		//!  In such case, filename must contain %s wildcard, that will be replaced by cubeSideName.
		//!  Example: "/maps/cube_%s.png".
		//! \param filenameMask Name of image file. Must be in supported format.
		//! \param cubeSideName Array of six unique names of cube sides in following order:
		//!  x+ side, x- side, y+ side, y- side, z+ side, z- side.
		//!  Examples: {"0","1","2","3","4","5"}, {"rt","lf","up","dn","ft","bk"}.
		//! \return
		//!  True on successful save of complete environment map.
		virtual bool save(const char* filenameMask, const char* cubeSideName[6]) {return false;}
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Storage for static object's indirect illumination.
	//
	//! Editor stores calculated illumination here.
	//! Renderer reads illumination from here.
	//! Add one instance to every static object in your scene.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObjectIllumination
	{
	public:
		//! \param numPreImportVertices
		//!  PreImport (original) number of mesh vertices, length of vertex buffer for rendering.
		RRObjectIllumination(unsigned numPreImportVertices);

		//! One layer of illumination (irradiance) values for whole object.
		//
		//! Illumination can be stored in vertex array, ambient map or both.
		//! Multiple channels (e.g. each channel for different light source) can be mixed by renderer.
		struct Channel
		{
			//! Custom vertex array with illumination levels for all object vertices.
			//! May be NULL in which case vertex arrays are not generated and not used by render.
			RRIlluminationVertexBuffer* vertexBuffer;
			//! Custom ambient map with illumination levels for object surface.
			//! May be NULL in which case ambient maps are not generated and not used by render.
			RRIlluminationPixelBuffer* pixelBuffer;
			//! Constructs new channel, always empty.
			//! You can insert vertex array or ambient map or both later.
			Channel()
			{
				vertexBuffer = NULL;
				pixelBuffer = NULL;
			}
			//! Destructs channel, deleting remaining vertex array or ambient map.
			~Channel()
			{
				delete vertexBuffer;
				delete pixelBuffer;
			}
		};

		//! \param channelIndex
		//!  Index of channel you would like to get. Arbitrary unsigned number.
		//!  For now, only channel 0 contains data.
		//! \return Channel of channelIndex. If it doesn't exist yet, it is created.
		Channel* getChannel(unsigned channelIndex);
		//! \return PreImport number of vertices, length of vertex buffer for rendering.
		unsigned getNumPreImportVertices();
		~RRObjectIllumination();
	protected:
		//! PreImport (original) number of mesh vertices, length of vertex buffer for rendering.
		unsigned numPreImportVertices;
		//! Container with all channels.
		void* hiddenChannels;
	};

} // namespace

#endif
