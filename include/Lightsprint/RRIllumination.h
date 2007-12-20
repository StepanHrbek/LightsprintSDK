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
		virtual void setVertex(unsigned vertex, const RRVec3& color) = 0;

		// Vertex buffer use

		//! Locks the buffer for reading array of all vertices at once. Optional, may return NULL.
		virtual const RRVec3* lockReading() {return NULL;};
		//! Locks the buffer for writing array of all vertices at once. Optional, may return NULL.
		virtual RRVec3* lockWriting() {return NULL;};
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

		//! Sets contents of buffer, all pixels at once.
		virtual void reset(const RRVec4* data) = 0;
		//! \return Width of pixel buffer in pixels.
		virtual unsigned getWidth() const = 0;
		//! \return Height of pixel buffer in pixels.
		virtual unsigned getHeight() const = 0;
		//! Locks the buffer for reading array of all pixels at once. Not mandatory, may return NULL.
		virtual const unsigned* lock() {return NULL;}; // RGBA8
		//! Locks the buffer for reading array of all pixels at once. Not mandatory, may return NULL.
		virtual const RRVec3* lockRGBF() {return NULL;};
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
		static RRIlluminationPixelBuffer* create(unsigned width, unsigned height);

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
	//! Stored values are always in custom scale, which is usually sRGB,
	//! so both inputs and outputs are displayable colors.
	//! \n Map usually contains image of surrounding scene, possibly filtered.
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
		//! \param colors
		//!  Array of 6*size*size values in this order:
		//!  \n size*size values for POSITIVE_X side,
		//!  \n size*size values for NEGATIVE_X side,
		//!  \n size*size values for POSITIVE_Y side,
		//!  \n size*size values for NEGATIVE_Y side,
		//!  \n size*size values for POSITIVE_Z side,
		//!  \n size*size values for NEGATIVE_Z side.
		//!  \n Values are usually interpreted as colors (radiosities in custom scale)
		//!     of distant points in 6*size*size different directions, possibly with filtering applied.
		virtual void setValues(unsigned size, const RRVec3* colors) = 0;

		// Environment map use

		//! Returns value addressed by given direction.
		//! Value is usually interpreted as color (radiosity in custom scale)
		//! of distant point in given direction, possibly with filtering applied.
		//! \n Not mandatory, implementation may always return 0.
		virtual RRVec3 getValue(const RRVec3& direction) const;

		//! Binds texture for use by renderer.
		//! Not mandatory, implementation may do OpenGL bind, DirectX bind or nothing.
		virtual void bindTexture() const;

		virtual ~RRIlluminationEnvironmentMap() {};


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Creates empty environment map in system memory, 6*size*size pixels big.
		//
		//! Created instance is 3D API independent, so also bindTexture() doesn't bind
		//! it for any 3D API.
		static RRIlluminationEnvironmentMap* create(unsigned size);

		//! Loads environment map from disk to system memory.
		//
		//! Created instance is 3D API independent, so also bindTexture() doesn't bind
		//! it for any 3D API.
		//! \n Example1: filename="path/cube_%s.png", cubeSideName={"ft","bk","dn","up","rt","lf"} - Cube is loaded from 6 files.
		//! \n Example2: filename="path/cube.hdr", cubeSideName=NULL - Cube is loaded from 1 file.
		//!    Note that it is possible to load map from float based fileformat in full float precision,
		//!    but existing rendering code expects custom scale values in map, so result
		//!    would be incorrect.
		//! \param filename
		//!  Filename of 1 image or mask of 6 images to be loaded from disk.
		//!  All common file formats are supported.
		//! \param cubeSideName Array of six unique names of cube sides in following order:
		//!  x+ side, x- side, y+ side, y- side, z+ side, z- side.
		//!  Examples: {"0","1","2","3","4","5"}, {"ft","bk","dn","up","rt","lf"}.
		//! \param flipV
		//!  Flip all sides vertically at load time.
		//! \param flipH
		//!  Flip all sides horizontally at load time.
		//! \return
		//!  Returns newly created environment map.
		//!  In case of failure, NULL is returned and details logged via RRReporter.
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

		//! Creates uniform environment, with constant color without regard to direction.
		//
		//! It is suitable for ambient occlusion calculation, getValue() is implemented.
		//! \n It is not suitable for realtime rendering, bind() is empty.
		//! \n Color of environment may be changed by setValues(), first values is taken as a new color.
		//! \param color
		//!  Color (environment radiosity in custom scale) of environment, the same value is returned by getValue().
		static RRIlluminationEnvironmentMap* createUniform(const RRVec3 color = RRVec3(1));

		//! Creates simple sky environment, with user defined colors in upper and lower hemisphere.
		//
		//! It is suitable for precomputed sky lighting, getValue() is implemented.
		//! \n It is not suitable for realtime rendering, bind() is empty.
		//! \n Color of environment may be changed by setValues(), first two values are taken as new colors.
		//! \param upper
		//!  Color (environment radiosity in custom scale) of upper hemisphere,
		//!  the same value is returned by getValue(direction_with_positive_y).
		//! \param lower
		//!  Color (environment radiosity in custom scale) of lower hemisphere,
		//!  the same value is returned by getValue(direction_with_zero_or_negative_y).
		static RRIlluminationEnvironmentMap* createSky(const RRVec3& upper, const RRVec3& lower);
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Storage for object's indirect illumination.
	//
	//! Editor stores calculated illumination here.
	//! Renderer reads illumination from here.
	//! Add one instance to every object in your scene.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRObjectIllumination
	{
	public:
		//! \param numPreImportVertices
		//!  PreImport (original) number of mesh vertices, length of vertex buffer for rendering.
		//!  You may set it 0 for dynamic objects, they don't use vertex buffers so they don't need this information.
		RRObjectIllumination(unsigned numPreImportVertices);
		~RRObjectIllumination();

		//
		// Vertex and pixel buffers, update and render supported for static objects.
		//

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

		//
		// Reflection maps, update and render supported for all objects (but used mostly by dynamic ones).
		//

		//! Diffuse reflection map. Created by you, updated by updateEnvironmentMap(), deleted automatically. May stay NULL.
		RRIlluminationEnvironmentMap* diffuseEnvMap;
		//! Specular reflection map. Created by you, updated by updateEnvironmentMap(), deleted automatically. May stay NULL.
		RRIlluminationEnvironmentMap* specularEnvMap;

		// parameters set by you and read by updateEnvironmentMap():

		//! Size of virtual cube for gathering samples, 16 by default. More = higher precision, slower.
		unsigned gatherEnvMapSize;
		//! Size of diffuse reflection map, 0 for none, 4 by default. More = higher precision, slower.
		unsigned diffuseEnvMapSize;
		//! Size of specular reflection map, 0 for none, 16 by default. More = higher precision, slower.
		unsigned specularEnvMapSize;
		//! World coordinate of object center. To be updated by you when object moves.
		RRVec3 envMapWorldCenter;

	protected:
		//
		// for buffers
		//
		//! PreImport (original) number of mesh vertices, length of vertex buffer for rendering.
		unsigned numPreImportVertices;
		//! Container with all layers.
		void* hiddenLayers;

		//
		// for reflection maps
		//
		friend class RRDynamicSolver;
		RRVec3 cachedCenter;
		unsigned cachedGatherSize;
		unsigned* cachedTriangleNumbers;
		class RRRay* ray6;
	};

} // namespace

#endif
