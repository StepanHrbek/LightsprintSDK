#ifndef RRBUFFER_H
#define RRBUFFER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRBuffer.h
//! \brief LightsprintCore | buffer is array of elements (texture, vertex buffer..)
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2009
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cstring> // NULL
#include "RRMemory.h"

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
	//! - vertex buffers by lock(BL_DISCARD_AND_WRITE) (if you implement your own buffer that doesn't support lock, setElement() is used)
	//! - cube maps by lock(BL_DISCARD_AND_WRITE) (if you implement your own buffer that doesn't support lock, setElement() is used)
	//! - lightmaps by setElement()
	//!
	//! LightsprintCore never calls reset(), so it never changes type, size, format or scale of your buffer.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRBuffer : public RRUniformlyAllocatedNonCopyable
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
		//!  \n For physical(linear) scale data, it's recommended to use floating point format to avoid clamping.
		//!  For scaled (sRGB) data, more compact 8bit format is usually sufficient, although it clamps values to 0..1 range.
		//! \param scaled
		//!  Whether buffer data are scaled to custom scale (usually screen colors, sRGB). False for physical(linear) scale.
		//!  When buffer is updated or rendered later, this setting is respected.
		//!  \n\n In greater detail: GI is internally calculated in physical scale, while displays work in sRGB,
		//!  so data must be converted at some point in pipeline.
		//!  True = data are scaled by RRDynamicSolver::updateLightmaps(), increasing CPU load;
		//!  positive sideeffect is that scaled data are suitable even for smaller RGB/RGBA buffers.
		//!  False = data should be scaled later, for example in renderer, thus increasing GPU load.
		//!  In both cases, scaling to sRGB is simple x=pow(x,0.45) operation.
		//!  If you precompute lightmaps once and render them many times, you can save time by setting true,
		//!  data are scaled once. In case of realtime GI where lightmaps are computed once and rendered once,
		//!  you can save time by setting false and scaling data in renderer/shader (GPU is usually faster).
		//! \param data
		//!  Data to be copied into texture. When set to NULL, contents of texture stays uninitialized.
		//!  Format of data is specified by format, interpretation of data is partially specified by scaled.
		//!  Special value 1 creates buffer without any memory allocated for elements
		//!  (it's good when buffer is needed, but its contents is never accessed, e.g. when creating uninitialized texture in rr_gl::Texture).
		//! \return
		//!  True on success, false on failure (invalid parameters).
		virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data) = 0;
		//! Sets single element in buffer. Value is converted to current buffer format.
		//
		//! Index is index into array of all elements, x+y*width+z*width*height.
		//! \n Not mandatory, implementation may be empty.
		virtual void setElement(unsigned index, const RRVec4& element);


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
		//! \return Total amount of system memory occupied by buffer.
		virtual unsigned getMemoryOccupied() const;
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
		//! Locks the buffer for accessing array of all elements at once. Not mandatory, may return NULL.
		//
		//! Behaviour of lock is not defined when buffer is already locked.
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

		//! Optional filename, automatically set when load/save succeeds.
		RRString filename;

		//! Version of data in buffer, incremented each time buffer content changes.
		unsigned version;

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

		//! Creates copy of buffer. Copy is located in system memory and is not connected to its origin, both may be deleted independently.
		RRBuffer* createCopy();

		//! Creates cube texture with specified colors of upper and lower hemisphere.
		//
		//! Set scaled true for colors in custom scale (screen colors), false for physical (linear) scale.
		//! By default, white cube for ambient occlusion is created.
		static RRBuffer* createSky(const RRVec4& upper = RRVec4(1), const RRVec4& lower = RRVec4(1), bool scaled = true);

		//! Loads buffer from disk to system memory.
		//
		//! Examples:
		//! - load("path/lightmap.png") - loads 2d texture (jpg, gif, dds etc)
		//! - load("path/lightmap.vbu") - loads vertex buffer
		//! - load("path/cube_%%s.png", {"bk","ft","dn","up","rt","lf"}) - loads cubemap from 6 files
		//! - load("path/cube.hdr", non-NULL) - loads cubemap from 1 file, expects cross-shaped image with aspect 3:4 or 4:3
		//! - load("path/cube.hdr", NULL) - loads the same file as 2d texture
		//! \param filename
		//!  Filename of 2d image or vertexbuffer or cubemap or mask of 6 images (sides of cubemap) to be loaded from disk.
		//!  All common file formats are supported.
		//!  Proprietary .vbu format is used for vertex buffers.
		//! \param cubeSideName
		//!  Array of six unique names of cube sides in following order:
		//!  x+ side, x- side, y+ side, y- side, z+ side, z- side.
		//!  \n Examples: {"0","1","2","3","4","5"}, {"bk","ft","dn","up","rt","lf"}.
		//!  \n Must be NULL for vertex buffers and 2d textures, non-NULL for cubemaps (even cubemaps in 1 file).
		//! \return
		//!  Returns newly created buffer.
		//!  In case of failure, NULL is returned and details logged via RRReporter.
		//! \remark
		//!  Image load/save is implemented outside LightsprintCore.
		//!  Make samples/Import/ImportFreeImage.cpp part of your project to enable save/load
		//!  or use setLoader() to assign custom code.
		static RRBuffer* load(const char *filename, const char* cubeSideName[6] = NULL);
		//! Loads cube texture from 1 or 6 files to system memory.
		//
		//! This is convenience function working with incomplete information,
		//! it attempts to guess whether you want to load cubemap from 1 file or from 6 files.
		//! It calls load() with guessed parameters.
		//! If you know exactly what to load, call load() yourself and avoid any guesswork.
		//! \param filename
		//!  Filename of 1 cross shaped 4:3 or 3:4 image;
		//!  or filename of one of 6 images that make cube map.
		//!  It should be full filename, e.g. cube_ft.jpg rather than cube_%%s.jpg.
		static RRBuffer* loadCube(const char *filename);

		//! Similar to load(), but loads from disk into existing buffer. Supports user implemented buffers.
		bool reload(const char *filename, const char* cubeSideName[6]);
		//! Similar to loadCube(), but loads from disk into existing buffer. Supports user implemented buffers.
		bool reloadCube(const char *filename);

		//! Rarely used additional save parameters, not necessarily supported by all implementations.
		struct SaveParameters
		{
			char jpegQuality; ///< Our save implementation in LightsprintIO supports 10=low, 25, 50, 75=default, 100=high.
		};
		//! Saves buffer to disk.
		//
		//! Save parameters are similar to load, see load() for examples.
		//! \param filenameMask
		//!  Filename of 1 image/vertexbuffer or mask of 6 images (sides of cubemap) to be saved to disk.
		//!  All common file formats are supported.
		//!  Proprietary .vbu format is used for vertex buffers (it consists of 2 bytes RRBufferFormat, 2 bytes bool scaled, 4 bytes num_vertices, data from buffer).
		//! \param cubeSideName When cubemap is saved, array of six unique names of cube sides in following order:
		//!  x+ side, x- side, y+ side, y- side, z+ side, z- side.
		//!  Examples: {"0","1","2","3","4","5"}, {"bk","ft","dn","up","rt","lf"}.
		//! \param saveParameters
		//!  Rarely used additional parameters, keep NULL for defaults.
		//! \return
		//!  True on successful save of complete buffer.
		//! \remark
		//!  Image load/save is implemented outside LightsprintCore.
		//!  Make samples/Import/ImportFreeImage.cpp part of your project to enable save/load
		//!  or use setLoader() to assign custom code.
		bool save(const char* filenameMask, const char* cubeSideName[6] = NULL, const SaveParameters* saveParameters = NULL);

		//! Type of user defined function that loads buffer from file.
		typedef bool (Loader)(RRBuffer* buffer, const char *filename, const char* cubeSideName[6]);
		//! Type of user defined function that saves buffer to file.
		typedef bool (Saver)(RRBuffer* buffer, const char* filenameMask, const char* cubeSideName[6], const SaveParameters* parameters);
		//! Hooks external code that handles loading images from disk.
		//
		//! Usually called from rr_io::registerLoaders().
		//! Initial state is no code hooked, attempts to load buffer are ignored, reload() returns false.
		//! \return Previous loader.
		static Loader* setLoader(Loader* loader);
		//! Hooks external code that handles saving images to disk.
		//
		//! Usually called from rr_io::registerLoaders().
		//! Initial state is no code hooked, attempts to save buffer are ignored, save() returns false.
		//! \return Previous saver.
		static Saver* setSaver(Saver* saver);

		//! Changes buffer format.
		virtual void setFormat(RRBufferFormat newFormat);
		//! Changes buffer format to floats, RGB to RGBF, RGBA to RGBAF.
		virtual void setFormatFloats();
		//! Changes all colors in buffer to 1-color.
		//
		//! Preserves buffer format.
		//! This operation is lossless for all formats.
		virtual void invert();
		//! Changes all colors in buffer to color*multiplier+addend.
		//
		//! Preserves buffer format.
		//! This operation may be lossy for byte formats (clamped to 0..1 range), use setFormatFloats() for higher precision.
		virtual void multiplyAdd(RRVec4 multiplier, RRVec4 addend);
		//! Flips buffer in x, y and/or z dimension. 
		virtual void flip(bool flipX, bool flipY, bool flipZ);
	};

} // namespace

#endif // RRBUFFER_H
