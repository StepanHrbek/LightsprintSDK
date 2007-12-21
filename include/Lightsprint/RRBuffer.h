#ifndef RRBUFFER_H
#define RRBUFFER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRBuffer.h
//! \brief LightsprintCore | buffer is array of elements (texture, vertex buffer..)
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cstring> // NULL
#include "RRMemory.h"

#define CLAMPED(a,min,max) (((a)<(min))?min:(((a)>(max)?(max):(a))))

namespace rr
{

	//! Buffer type. Implementation is not required to support all of them.
	enum RRBufferType
	{
		BT_VERTEX_BUFFER, ///< vertex buffer, 1d array. Used for realtime indirect lighting, per-vertex lightmaps.
		BT_1D_TEXTURE,    ///< 1d texture, 1d array. Not used.
		BT_2D_TEXTURE,    ///< 2d texture, 2d array. Used for lightmaps, ambient maps, bent normal maps.
		BT_3D_TEXTURE,    ///< 3d texture, 3d array. Not used.
		BT_CUBE_TEXTURE,  ///< cube texture, 3d array. Used for diffuse and specular environment maps.
	};

	//! Buffer format. Implementation is not required to support all of them.
	enum RRBufferFormat
	{
		BF_RGB,   ///< 8bit rgb, 24bits per pixel
		BF_RGBA,  ///< 8bit rgba, 32bits per pixel
		BF_RGBF,  ///< float rgb, 96bits per pixel
		BF_RGBAF, ///< float rgba, 128bits per pixel
		BF_DEPTH, ///< depth, implementation defined precision
	};

	//! Buffer lock. Implementation is not required to support all of them.
	enum RRBufferLock
	{
		BL_READ,              ///< Lock for reading only.
		BL_READ_WRITE,        ///< Lock for reading and writing.
		BL_DISCARD_AND_WRITE, ///< Discards old data and locks buffer (now with uninitialized data) for writing. You should overwrite whole buffer otherwise it stays partially uninitialized.
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	//! Buffer, array of elements.
	//
	//! 1d buffers are used for vertex buffers.
	//! 2d buffers are used for 2d textures.
	//! 3d buffers are used for cube textures.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRBuffer : public RRUniformlyAllocated
	{
	public:

		//////////////////////////////////////////////////////////////////////////////
		// Interface for setting
		//////////////////////////////////////////////////////////////////////////////

		//! Sets size and contents of buffer.
		//
		//! Textures initialized as cube stay cube textures, 2d stay 2d.
		//! \param type
		//!  Requested type of buffer.
		//! \param width
		//!  Requested width of buffer.
		//! \param height
		//!  Requested height of buffer.
		//!  Must equal to width for cube texture.
		//! \param depth
		//!  Requested depth of buffer.
		//!  Must equal to width for cube texture.
		//! \param format
		//!  Format of data.
		//!  Implementation is not required to support all data formats.
		//! \param data
		//!  Data to be loaded(copied) into texture. When set to NULL, contents of texture stays uninitialized.
		//!  Format of data is specified by format.
		virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, const unsigned char* data) = 0;
		//! Sets single element in buffer. Value is converted to current buffer format (alpha=0 when necessary).
		//
		//! Index is index into array of all elements, x+y*width+z*width*height.
		//! \n Not mandatory, implementation may be empty.
		virtual void setElement(unsigned index, const RRVec3& element);


		//////////////////////////////////////////////////////////////////////////////
		// Interface for reading
		//////////////////////////////////////////////////////////////////////////////

		//! \return Type of buffer, e.g. BT_VERTEX_BUFFER.
		virtual RRBufferType getType() const = 0;
		//! \return Width of buffer: number of vertices for vertex buffer, width in pixels for texture.
		virtual unsigned getWidth() const = 0;
		//! \return Height of buffer: 1 for vertex buffer, height in pixels for texture.
		virtual unsigned getHeight() const = 0;
		//! \return Depth of buffer: 1 for vertex buffer, 1 for 2d texture, 6 for cube texture.
		virtual unsigned getDepth() const = 0;
		//! \return Format of buffer, e.g. BF_RGBF.
		virtual RRBufferFormat getFormat() const = 0;
		//! \return Number of bits in one element, e.g. 96 for BF_RGBF, implementation defined for BF_DEPTH.
		virtual unsigned getElementBits() const;
		//! Returns value addressed by given integer coordinates as regular 3d array.
		//
		//! Index is index into array of all elements, x+y*width+z*width*height.
		//! Out of range indices are reported as error.
		//! \n Not mandatory, implementation may always return 0.
		virtual RRVec4 getElement(unsigned index) const;
		//! Returns value addressed by given float coordinates.
		//
		//! For cube texture, coordinates are direction from center, not necessarily normalized.
		//! For other buffers, coordinates are array indices in 0..1 range covering whole buffer.
		//! Out of range indices are wrapped to 0..1.
		//! \n Not mandatory, implementation may always return 0.
		//! Used by offline solver to read illumination from environment (cube map).
		virtual RRVec4 getElement(const RRVec3& coord) const;
		//! Locks the buffer for reading array of all elements at once. Not mandatory, may return NULL.
		//
		//! For writing into buffer, use reset().
		//! \n Behaviour of lock is not defined when buffer is already locked.
		//! \return
		//!  Pointer to array of all width*height*depth elements, in format specified by getFormat().
		virtual unsigned char* lock(RRBufferLock lock);
		//! Unlocks previously locked buffer.
		virtual void unlock();

		//////////////////////////////////////////////////////////////////////////////
		// Misc
		//////////////////////////////////////////////////////////////////////////////

		RRBuffer();
		virtual ~RRBuffer() {};

		//! For your private use, not accessed by LightsprintCore. Initialized to NULL.
		//! LightsprintGL sets it to Texture*.
		void* customData;


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Creates buffer in system memory.
		static RRBuffer* create(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, const unsigned char* data);

		//! Creates cube texture with specified colors of upper and lower hemisphere.
		static RRBuffer* createSky(const RRVec4& upper, const RRVec4& lower);

		//! Creates cube texture with specified color.
		static RRBuffer* createSky(RRVec4 color = RRVec4(1));

		//! Loads buffer from disk to system memory.
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
		static RRBuffer* load(const char *filename, const char* cubeSideName[6] = NULL, bool flipV = false, bool flipH = false);

		//! Similar to load(), but loads from disk into existing buffer. Supports user implemented buffers.
		virtual bool reload(const char *filename, const char* cubeSideName[6], bool flipV = false, bool flipH = false);

		//! Saves buffer to disk.
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
		virtual bool save(const char* filenameMask, const char* cubeSideName[6]=NULL);
	};

} // namespace

#endif // RRBUFFER_H
