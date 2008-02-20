#ifndef RRBUFFER_H
#define RRBUFFER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRBuffer.h
//! \brief LightsprintCore | buffer is array of elements (texture, vertex buffer..)
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2008
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
		BT_VERTEX_BUFFER, ///< Vertex buffer, 1d array of width elements. Used for object's realtime indirect lighting, precomputed per-vertex lighting.
		BT_2D_TEXTURE,    ///< 2d texture, 2d array of width*height elements. Used for object's precomputed lightmaps, ambient occlusion maps, bent normal maps.
		BT_CUBE_TEXTURE,  ///< Cube texture, 3d array of size*size*6 elements. Used for scene environment and for object's diffuse and specular reflection maps.
	};

	//! Buffer format. Implementation is not required to support all of them.
	enum RRBufferFormat
	{
		BF_RGB,   ///< Integer RGB, 24bits per pixel. Format of textures natively supported by virtually all hardware. Usually not suitable for vertex buffers in hardware. Ideal for textures in custom scale (screen colors), not suitable for data in physical (linear) scale due to limited precision.
		BF_RGBA,  ///< Integer RGBA, 32bits per pixel. Format natively supported by virtually all hardware. Ideal for data in custom scale (screen colors), not suitable for data in physical (linear) scale due to limited precision.
		BF_RGBF,  ///< Floating point RGB, 96bits per pixel. High precision, suitable for any data, but some old GPUs don't support textures in this format. Ideal for vertex buffers in physical (linear) scale.
		BF_RGBAF, ///< Floating point RGBA, 128bits per pixel. High precision, suitable for any data, but some old GPUs don't support textures in this format.
		BF_DEPTH, ///< Depth, implementation defined precision.
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
	//!
	//! DirectX/OpenGL renderers can use buffers in two ways:
	//! -# Let Lightsprint calculate lighting into buffers. Each time you request Lightsprint
	//!    to calculate new data, lock() them and copy them to your DirectX/OpenGL buffer, e.g. texture.
	//!    See rr_gl::Texture::reset() and rr_gl::getTexture() as an example.
	//!    It is usually easier way and it performs very well.
	//! -# Implement your own RRBuffer so that computed data are stored directly
	//!    in DirectX/OpenGL buffer.
	//!    It is more complicated, but it could be more efficient in some cases.
	//!    It's possible to simplify subclassing by using helper RRBuffer::create() instance
	//!    internally in subclass instance.
	//!
	//! Functions used by LightsprintCore to update buffers
	//! - vertex buffers by lock(); or setElement() when lock() returns NULL
	//! - environment maps by reset(), but future version will use lock() too to avoid memcpy
	//! - lightmaps by setElement()
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
		//! \param type
		//!  Requested type of buffer.
		//! \param width
		//!  Requested width of buffer. Set to number of vertices for BT_VERTEX_BUFFER.
		//! \param height
		//!  Requested height of buffer. Set 1 for BT_VERTEX_BUFFER. Set equal to width for BT_CUBE_TEXTURE.
		//! \param depth
		//!  Requested depth of buffer.
		//!  Set 1 for BT_VERTEX_BUFFER and BT_2D_TEXTURE. Set 6 for BT_CUBE_TEXTURE.
		//! \param format
		//!  Format of data.
		//!  Implementation is not required to support all data formats.
		//! \param scaled
		//!  Whether buffer data are in custom scale (usually screen colors, sRGB). False for physical(linear) scale.
		//!  When buffer gets updated or rendered later, this setting should be respected.
		//! \param data
		//!  Data to be loaded(copied) into texture. When set to NULL, contents of texture stays uninitialized.
		//!  Format of data is specified by format, interpretation of data is partially specified by scaled.
		//! \return
		//!  True on success, false on failure (invalid parameters).
		virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data) = 0;
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
		//! \return False when buffer data are in physical (linear) scale, true for data in custom scale (screen colors, sRGB).
		virtual bool getScaled() const = 0;
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
		//
		//! If you use LightsprintGL, you should not modify it,
		//! it is set to Texture*.
		void* customData;


		//////////////////////////////////////////////////////////////////////////////
		// Tools
		//////////////////////////////////////////////////////////////////////////////

		//! Creates buffer in system memory. See reset() for parameter details. Returns NULL when parameters are invalid.
		static RRBuffer* create(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data);

		//! Creates cube texture with specified colors of upper and lower hemisphere.
		//
		//! Set scaled true for colors in custom scale (screen colors), false for physical (linear) scale.
		//! By default, white cube for ambient occlusion is created.
		static RRBuffer* createSky(const RRVec4& upper = RRVec4(1), const RRVec4& lower = RRVec4(1), bool scaled = true);

		//! Loads buffer from disk to system memory.
		//
		//! \n Example1: load("path/lightmap.png") - loads 2d texture (jpg, gif, dds etc)
		//! \n Example2: load("path/lightmap.vbu") - loads vertex buffer
		//! \n Example3: load("path/cube_%s.png", {"ft","bk","dn","up","rt","lf"}) - loads cubemap from 6 files
		//! \n Example4: load("path/cube.hdr", NULL) - loads cubemap from 1 file
		//! \param filename
		//!  Filename of 1 image/vertexbuffer or mask of 6 images (sides of cubemap) to be loaded from disk.
		//!  All common file formats are supported.
		//!  Proprietary .vbu format is used for vertex buffers.
		//! \param cubeSideName When cubemap is loaded, array of six unique names of cube sides in following order:
		//!  x+ side, x- side, y+ side, y- side, z+ side, z- side.
		//!  Examples: {"0","1","2","3","4","5"}, {"ft","bk","dn","up","rt","lf"}.
		//! \param flipV
		//!  Flip all sides vertically at load time.
		//! \param flipH
		//!  Flip all sides horizontally at load time.
		//! \return
		//!  Returns newly created buffer.
		//!  In case of failure, NULL is returned and details logged via RRReporter.
		static RRBuffer* load(const char *filename, const char* cubeSideName[6] = NULL, bool flipV = false, bool flipH = false);

		//! Similar to load(), but loads from disk into existing buffer. Supports user implemented buffers.
		bool reload(const char *filename, const char* cubeSideName[6], bool flipV = false, bool flipH = false);

		//! Saves buffer to disk.
		//
		//! Save parameters are similar to load, see load() for examples.
		//! \param filenameMask
		//!  Filename of 1 image/vertexbuffer or mask of 6 images (sides of cubemap) to be saved to disk.
		//!  All common file formats are supported.
		//!  Proprietary .vbu format is used for vertex buffers (it consists of 2 bytes RRBufferFormat, 2 bytes bool scaled, 4 bytes num_vertices, data from buffer).
		//! \param cubeSideName When cubemap is saved, array of six unique names of cube sides in following order:
		//!  x+ side, x- side, y+ side, y- side, z+ side, z- side.
		//!  Examples: {"0","1","2","3","4","5"}, {"ft","bk","dn","up","rt","lf"}.
		//! \return
		//!  True on successful save of complete buffer.
		bool save(const char* filenameMask, const char* cubeSideName[6]=NULL);

		//! Sets functions that load and save buffer, usually by calling external image manipulation library.
		//
		//! Example of callbacks: samples/Import/ImportFreeImage.cpp.
		//! \n If functions are not set, attempts to load/save buffer are silently ignored.
		static void setLoader(bool (*reload)(RRBuffer* buffer, const char *filename, const char* cubeSideName[6], bool flipV, bool flipH), bool (*save)(RRBuffer* buffer, const char* filenameMask, const char* cubeSideName[6]));
	};

} // namespace

#endif // RRBUFFER_H
