#ifndef RRILLUMINATION_H
#define RRILLUMINATION_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIllumination.h
//! \brief LightsprintCore | storage for calculated illumination
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <cstring> // NULL
#include "RRMemory.h"

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
		RRColorRGBAF(const RRVec3p& a)
		{
			x = a.x;
			y = a.y;
			z = a.z;
			w = a.w;
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

	class RR_API RRIlluminationVertexBuffer : public RRUniformlyAllocated
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		// Vertex buffer creation

		//! Returns size of vertex buffer in vertices.
		virtual unsigned getNumVertices() const = 0;
		//! Sets value of one element of the buffer.
		//! Must be thread safe.
		virtual void setVertex(unsigned vertex, const RRColorRGBF& color) = 0;

		// Vertex buffer use

		//! Locks the buffer for accessing array of all vertices at once. Optional, may return NULL.
		virtual const RRColorRGBF* lock() {return NULL;};
		//! Unlocks previously locked buffer.
		virtual void unlock() {};

		virtual ~RRIlluminationVertexBuffer() {};

		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		// Creates and returns vertex buffer in system memory. It is graphics API independent.
		static RRIlluminationVertexBuffer* createInSystemMemory(unsigned numVertices);

		//! Loads RRIlluminationVertexBuffer stored on disk by createInSystemMemory()->save().
		//
		//! Note that other future implementations may use incompatible formats
		//! and provide their own load().
		static RRIlluminationVertexBuffer* load(const char* filename, unsigned expectedNumVertices);

		//! Saves vertex buffer to disk.
		//! Not mandatory, thin implementations may completely skip saving and always return false.
		//! \param filename
		//!  Filename of vertex buffer to be created on disk.
		//!  Supported file formats are implementation defined.
		//!  Example: "data/object1.vbu"
		//! \return
		//!  True on successful save of vertex buffer.
		virtual bool save(const char* filename) {return false;}
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Interface to illumination storage based on 2D pixel buffer.
	//
	//! Used for lightmaps with direct illumination and 
	//! ambient maps with indirect illumination.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRIlluminationPixelBuffer : public RRUniformlyAllocated
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		// Pixel buffer creation

		//! Begins rendering of triangles into pixel buffer. Must be paired with renderEnd().
		//! Used only while calculating pixel buffers, called by RRDynamicSolver.
		//! \n\n Audience: Used internally by RRDynamicSolver.
		virtual void renderBegin() {};
		//! Description of one illuminated vertex.
		//! \n\n Audience: Used internally by RRDynamicSolver.
		struct IlluminatedVertex
		{
			RRVec2 texCoord; ///< Triangle vertex positions in pixel buffer.
			RRColorRGBF measure; ///< Triangle vertex illumination.
		};
		//! Description of one illuminated triangle.
		//! \n\n Audience: Used internally by RRDynamicSolver.
		struct IlluminatedTriangle
		{
			IlluminatedVertex iv[3]; ///< Three illuminated vertices forming triangle.
		};
		//! Renders one triangle into pixel buffer. Must be called inside renderBegin() / renderEnd().
		//
		//! Used only while realtime calculating pixel buffers, called by RRDynamicSolver.
		//! \param it
		//!  Description of single triangle.
		virtual void renderTriangle(const IlluminatedTriangle& it) = 0;
		//! Renders multiple triangles into pixel buffer. Must be called inside renderBegin() / renderEnd().
		//
		//! Used only while realtime calculating pixel buffers, called by RRDynamicSolver.
		//! \param it
		//!  Array with description of triangles.
		//! \param numTriangles
		//!  Length of it array, number of triangles to be rendered.
		virtual void renderTriangles(const IlluminatedTriangle* it, unsigned numTriangles);
		//! Renders one texel into pixel buffer. Must be called inside renderBegin() / renderEnd().
		//
		//! Used only while non-realtime calculating pixel buffers, called by RRDynamicSolver.
		//! \param uv
		//!  Array of 2 elements, texel coordinates in 0..width-1, 0..height-1 range.
		//! \param color
		//!  Color of rendered texel.
		//!  RRDynamicSolver sets irradiance in custom scale here.
		//!  With color r,g,b and importance i, RRColorRGBAF(r*i,g*i,b*i,i) is provided.
		//!  Importance is real number in 0..1 range, 1 for securely detected colors,
		//!  less than 1 for partially unsecure colors, e.g. texels partially inside object.
		//!  0 for completely unsecure texels (it's also possible to not render them at all).
		virtual void renderTexel(const unsigned uv[2], const RRColorRGBAF& color) = 0;
		//! Finishes rendering into pixel buffer. Must be paired with renderBegin().
		//
		//! Used only while calculating pixel buffers, called by RRDynamicSolver.
		//!
		//! Colors with low alpha (probability of correctness) should be processed
		//! and replaced by nearby colors with higher probability.
		//! Colors in form r*p,g*p,b*p,p should be normalized to r,g,b,1.
		//! \n\n Changes state of rendering pipeline: could change state related to shader,
		//!  could reset render target to default backbuffer.
		//! \n\n Audience: Used internally by RRDynamicSolver.
		//! \param preferQualityOverSpeed
		//!  Set true when used in precalculator, for high quality.
		//!  Set false when used in realtime process, for high speed.
		virtual void renderEnd(bool preferQualityOverSpeed) {};

		// Pixel buffer use

		//! \return Width of pixel buffer in pixels.
		virtual unsigned getWidth() const = 0;
		//! \return Height of pixel buffer in pixels.
		virtual unsigned getHeight() const = 0;
		//! Locks the buffer for seeing array of all pixels at once. Not mandatory, may return NULL.
		virtual const RRColorRGBA8* lock() {return NULL;};
		//! Unlocks previously locked buffer.
		virtual void unlock() {};
		//! Binds pixel buffer for rendering. Not mandatory,
		//! various implementations may do OpenGL texture bind, Direct3D bind or nothing.
		virtual void bindTexture() const {};

		virtual ~RRIlluminationPixelBuffer() {};


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Creates and returns pixel buffer for illumination storage in system memory.
		//
		//! Doesn't depend on any 3D API, operates in system memory only,
		//! so bindTexture() doesn't bind it for any 3D API.
		//! \param width Width of pixel buffer/texture.
		//! \param height Height of pixel buffer/texture.
		//! \param spreadForegroundColor
		//!  How far foreground (used) colors spread into background (unused) regions.
		//!  For lightmaps that are bilinearly filtered at application time, set 2 or higher
		//!  to prevent background color leaking into foreground.
		//!  For lightmaps that are unfiltered at application time, set 1 or higher.
		//!  Set high enough (e.g. 1000) to fill whole background by nearest foreground color.
		//! \param backgroundColor
		//!  Color of unused background pixels.
		//! \param smoothBackground
		//!  Smooth foreground-background transition.
		static RRIlluminationPixelBuffer* create(unsigned width, unsigned height, unsigned spreadForegroundColor=2, RRColorRGBAF backgroundColor=RRColorRGBAF(0), bool smoothBackground = false);

		//! Loads pixel buffer stored on disk as 2d image.
		//
		//! Doesn't depend on any 3D API, operates in system memory only,
		//! so bindTexture() doesn't bind it for any 3D API.
		//! \param filename
		//!  Filename of image to be loaded from disk.
		//!  Supported file formats include jpg, png, dds, hdr, gif, tga.
		//!  Example: "/maps/ambientmap.png"
		static RRIlluminationPixelBuffer* load(const char* filename);

		//! Saves pixel buffer to disk.
		//
		//! Not mandatory, thin implementations may completely skip saving and always return false.
		//! \param filename
		//!  Filename of image to be created on disk.
		//!  Supported file formats are implementation defined.
		//!  Example: "/maps/ambientmap.png"
		//! \return
		//!  True on successful save of pixel buffer.
		virtual bool save(const char* filename) = 0;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Interface to illumination storage based on environment map.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRIlluminationEnvironmentMap : public RRUniformlyAllocated
	{
	public:
		//////////////////////////////////////////////////////////////////////////////
		// Interface
		//////////////////////////////////////////////////////////////////////////////

		// Environment map creation

		//! Sets all illumination values in map.
		//! \param size
		//!  Width and height of one side of cube map.
		//! \param irradiance
		//!  Array of 6*size*size irradiance values in custom scale in this order:
		//!  \n size*size values for POSITIVE_X side,
		//!  \n size*size values for NEGATIVE_X side,
		//!  \n size*size values for POSITIVE_Y side,
		//!  \n size*size values for NEGATIVE_Y side,
		//!  \n size*size values for POSITIVE_Z side,
		//!  \n size*size values for NEGATIVE_Z side.
		virtual void setValues(unsigned size, const RRColorRGBF* irradiance) = 0;

		// Environment map use

		//! Returns value addressed by given direction.
		//! Not mandatory, implementation may always return 0.
		virtual RRColorRGBF getValue(const RRVec3& direction) const;

		//! Binds texture for use by renderer.
		//! Not mandatory, implementation may do OpenGL bind, DirectX bind or nothing.
		virtual void bindTexture() const;

		virtual ~RRIlluminationEnvironmentMap() {};


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Creates empty environment map in system memory.
		//
		//! Created instance is 3D API independent, so also bindTexture() doesn't bind
		//! it for any 3D API.
		static RRIlluminationEnvironmentMap* create(unsigned width);

		//! Loads environment map from disk to system memory.
		//
		//! Created instance is 3D API independent, so also bindTexture() doesn't bind
		//! it for any 3D API.
		static RRIlluminationEnvironmentMap* load(const char *filename, const char* cubeSideName[6],
			bool flipV = false, bool flipH = false);

		//! Saves environment map to disk.
		//
		//! Not mandatory, thin implementations may completely skip saving and always return false.
		//! \param filenameMask
		//!  Filename mask of images to be created on disk.
		//!  Supported file formats are implementation defined.
		//!  Implementation is free to create 6 files for 6 cube sides.
		//!  In such case, filename must contain \%s wildcard, that will be replaced by cubeSideName.
		//!  Example: "path/cube_%s.png".
		//! \param cubeSideName Array of six unique names of cube sides in following order:
		//!  x+ side, x- side, y+ side, y- side, z+ side, z- side.
		//!  Examples: {"0","1","2","3","4","5"}, {"ft","bk","dn","up","rt","lf"}.
		//! \return
		//!  True on successful save of complete environment map.
		virtual bool save(const char* filenameMask, const char* cubeSideName[6]);

		//! Creates uniform environment, with constant irradiance without regard to direction.
		//
		//! It is suitable for ambient occlusion calculation.
		//! \n It is not suitable for rendering, bind() is empty.
		//! \n Color of environment may be changed by setValues(), first element of values is taken as
		//!    a new irradiance.
		//! \param irradiance
		//!  Irradiance in environment, the same value is returned by getValue().
		static RRIlluminationEnvironmentMap* createUniform(const RRColorRGBF irradiance = RRColorRGBF(1));

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
		//! Multiple layers (e.g. one layer per light source) can be mixed by renderer.
		struct Layer
		{
			//! Custom vertex array with illumination levels for all object vertices.
			//! May be NULL in which case vertex arrays are not generated and not used by render.
			RRIlluminationVertexBuffer* vertexBuffer;
			//! Custom ambient map with illumination levels for object surface.
			//! May be NULL in which case ambient maps are not generated and not used by render.
			RRIlluminationPixelBuffer* pixelBuffer;
			//! Constructs new layer, always empty.
			//! You can insert vertex array or ambient map or both later.
			Layer()
			{
				vertexBuffer = NULL;
				pixelBuffer = NULL;
			}
			//! Destructs layer, deleting remaining vertex array or ambient map.
			~Layer()
			{
				delete vertexBuffer;
				delete pixelBuffer;
			}
		};

		//! \param layerNumber
		//!  Index of layer you would like to get. Arbitrary unsigned number.
		//! \return Layer of layerNumber. If it doesn't exist yet, it is created.
		Layer* getLayer(unsigned layerNumber);
		const Layer* getLayer(unsigned layerNumber) const;
		//! \return PreImport number of vertices, length of vertex buffer for rendering.
		unsigned getNumPreImportVertices() const;
		~RRObjectIllumination();
	protected:
		//! PreImport (original) number of mesh vertices, length of vertex buffer for rendering.
		unsigned numPreImportVertices;
		//! Container with all layers.
		void* hiddenLayers;
	};

} // namespace

#endif
