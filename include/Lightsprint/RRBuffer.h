#ifndef RRBUFFER_H
#define RRBUFFER_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRBuffer.h
//! \brief LightsprintCore | buffer is array of elements (texture, vertex buffer..)
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2007-2011
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cstring> // NULL
#include "RRFileLocator.h"

namespace rr
{

	//! Buffer type. Implementation is not required to support all of them.
	enum RRBufferType
	{
		BT_VERTEX_BUFFER, ///< Vertex buffer, 1d array of width elements. Used for object's realtime indirect lighting, precomputed per-vertex lighting.
		BT_2D_TEXTURE,    ///< 2d texture or video, 2d array of width*height elements. Used for object's precomputed lightmaps, ambient occlusion maps, bent normal maps, material textures and videos.
		BT_CUBE_TEXTURE,  ///< Cube texture, 3d array of size*size*6 elements. Used for scene environment and for object's diffuse and specular reflection maps.
	};

	//! Buffer format. Implementation is not required to support all of them.
	enum RRBufferFormat
	{
		BF_RGB,   ///< Integer RGB, 24bits per pixel. Format of textures natively supported by virtually all hardware. Usually not suitable for vertex buffers in hardware. Ideal for textures in custom scale (screen colors), not suitable for data in physical (linear) scale due to limited precision.
		BF_BGR,   ///< Integer BGR, 24bits per pixel. For DirectX interoperability.
		BF_RGBA,  ///< Integer RGBA, 32bits per pixel. Format natively supported by virtually all hardware. Ideal for data in custom scale (screen colors), not suitable for data in physical (linear) scale due to limited precision.
		BF_RGBF,  ///< Floating point RGB, 96bits per pixel. High precision, suitable for any data, but some old GPUs don't support textures in this format. Ideal for vertex buffers in physical (linear) scale.
		BF_RGBAF, ///< Floating point RGBA, 128bits per pixel. High precision, suitable for any data, but some old GPUs don't support textures in this format.
		BF_DEPTH, ///< Depth, implementation defined precision.
		BF_DXT1,  ///< DXT1 compressed, can't be accessed per-pixel.
		BF_DXT3,  ///< DXT3 compressed, can't be accessed per-pixel.
		BF_DXT5,  ///< DXT5 compressed, can't be accessed per-pixel.
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
	//! \section buf_types Buffer types
	//!  - 1d buffers are used for vertex buffers.
	//!  - 2d buffers are used for 2d textures and videos.
	//!  - 3d buffers are used for cube textures.
	//!
	//! \section buf_dx_gl Using buffers in OpenGL/DirectX
	//!  Two approaches exist for using buffers in DirectX/OpenGL renderer
	//!  -# Use existing RRBuffer that stores data in system memory. Each time buffer version changes,
	//!     copy data from buffer to DirectX/OpenGL texture.
	//!     rr_gl::getTexture() implements this behaviour, it is flexible and fast.
	//!  -# Subclass RRBuffer, make it store data directly in DirectX/OpenGL texture.
	//!     It is less flexible, but it saves system memory and it could be faster if used with care.
	//!     For an example of RRBuffer subclass, see e.g. RRBufferDirectShow implemented in LightsprintIO.
	//!
	//! \section buf_update How solvers update buffers
	//!  When implementing custom RRBuffer subclass, it may help to know how solvers update buffers
	//!  - vertex buffers are updated by lock(BL_DISCARD_AND_WRITE), with fallback to setElement() if lock() fails
	//!  - cube maps are updated by lock(BL_DISCARD_AND_WRITE), with fallback to setElement() if lock() fails
	//!  - lightmaps are updated by setElement()
	//!  - reset() is never called, so your type, size, format and scale are preserved
	//!
	//! \section buf_sharing How buffers are cached/shared
	//!  Buffers loaded from disk may be shared to save memory and time.
	//!  Sharing is automatic, you mostly don't have to care about it, but it's still good to know the rules,
	//!  they are shown by example:
	//!  \code
	//!   a = RRBuffer::load("foo/bar.avi");  // a loaded from disk
	//!   b = RRBuffer::load("foo/bar.avi");  // b found in cache, b==a
	//!   c = RRBuffer::load("foo\\bar.avi"); // c loaded from disk, because filename differs
	//!   a->setElement(0,RRVec4(0));         // modifies content of a==b
	//!   d = RRBuffer::load("foo/bar.avi");  // d loaded from disk, a==b removed from cache, because content differs
	//!   delete a;                           // no memory freed, it's still in use by b
	//!   delete b;                           // memory freed
	//!   delete d;                           // no memory freed, d stays in cache
	//!   e = RRBuffer::load("foo/bar.avi");  // e found in cache, e==d
	//!   e->play();                          // starts playing image to buffer, audio to speakers
	//!   //e->stop();                        // here we forget to stop
	//!   delete e;                           // no memory freed, e stays in cache, still playing to speakers
	//!   // someone overwrites foo/bar.avi
	//!   f = RRBuffer::load("foo/bar.avi");  // f loaded from disk, e deleted from cache, memory freed, stops playing, because file's write time did change
	//!                                       // (note that file's write time is not tracked for cubemaps stored in 6 files, they are assumed to never change on disk, let us know if it is a problem)
	//!  \endcode
	//!
	//! \section buf_capture Live video capture
	//!  LightsprintIO implements support for live video capture into 2d buffer.
	//!  With LightsprintIO callbacks registered, live video capture is started by
	//!  opening imaginary file "c@pture".
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
		//!  Special value RR_GHOST_BUFFER creates buffer without any memory allocated for elements
		//!  (it's good when buffer is needed, but its contents is never accessed, e.g. when creating uninitialized texture in rr_gl::Texture).
		//! \return
		//!  True on success, false on failure (invalid parameters).
		virtual bool reset(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data) = 0;
		#define RR_GHOST_BUFFER (const unsigned char*)1
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
		//! \return Width*height*depth.
		unsigned getNumElements() const;
		//! \return Format of buffer, e.g. BF_RGBF.
		virtual RRBufferFormat getFormat() const = 0;
		//! \return False when buffer data are in physical (linear) scale, true for data in custom scale (screen colors, sRGB).
		virtual bool getScaled() const = 0;
		//! \return Size of buffer in bytes, pure buffer size without several fixed bytes of class size.
		//
		//! In case of video, size of one uncompressed frame is calculated.
		//! In case of RR_GHOST_BUFFER, 0 is returned as no memory is allocated.
		virtual unsigned getBufferBytes() const;
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
		//! Coordinates are array indices in 0..1 range covering whole buffer.
		//! Out of range indices are wrapped to 0..1.
		virtual RRVec4 getElementAtPosition(const RRVec3& position) const;
		//! Returns environment sample addressed by given direction (not necessarily normalized).
		//
		//! 2d texture is interpreted as 360*180 degree panorama.
		//! Cube texture is interpreted as standard cube.
		virtual RRVec4 getElementAtDirection(const RRVec3& direction) const;
		//! Locks the buffer for accessing array of all elements at once. Not mandatory, may return NULL.
		//
		//! Behaviour of lock is not defined when buffer is already locked.
		//! \return
		//!  Pointer to array of all width*height*depth elements, in format specified by getFormat().
		virtual unsigned char* lock(RRBufferLock lock);
		//! Unlocks previously locked buffer.
		virtual void unlock();


		//////////////////////////////////////////////////////////////////////////////
		// Interface for video
		//////////////////////////////////////////////////////////////////////////////

		//! For video/capture/animated buffers, returns true if buffer content was updated and version changed.
		virtual bool update() {return false;}
		//! For video buffers, starts playing buffer. To update content of playing buffer, call update().
		virtual void play() {}
		//! For video buffers, stops playing buffer and rewinds to the beginning.
		virtual void stop() {}
		//! For video buffers, pauses playing buffer.
		virtual void pause() {}
		//! \return Duration of dynamic content (video) in seconds, 0 for static content (image, vertex colors), -1 for unlimited dynamic content (video capture).
		virtual float getDuration() const {return 0;}


		//////////////////////////////////////////////////////////////////////////////
		// Misc
		//////////////////////////////////////////////////////////////////////////////

		RRBuffer();
		virtual ~RRBuffer() {};

		//! Optional filename, automatically set when load/save succeeds.
		RRString filename;

		//! Version of data in buffer, modified each time buffer content changes.
		unsigned version;

		//! For your private use, not accessed by LightsprintCore. Initialized to NULL.
		//
		//! If you use LightsprintGL, you should not modify it,
		//! it is set to Texture*.
		void* customData;


		//////////////////////////////////////////////////////////////////////////////
		// Tools for creation/copying
		//////////////////////////////////////////////////////////////////////////////

		//! Creates buffer in system memory. See reset() for parameter details. Returns NULL when parameters are invalid or allocation fails.
		static RRBuffer* create(RRBufferType type, unsigned width, unsigned height, unsigned depth, RRBufferFormat format, bool scaled, const unsigned char* data);

		//! Creates reference to the same buffer. Both buffer and reference must be deleted (in any order).
		//
		//! It is not thread safe, must not be called concurrently for one buffer.
		//! It may be called concurrently for different buffers.
		virtual RRBuffer* createReference() = 0;
		//! Returns number of references to this instance, for debugging only.
		virtual unsigned getReferenceCount() = 0;

		//! Creates copy of buffer. Copy is located in system memory and is completely separated, both buffers may contain different data. Copy of video contains single frame.
		RRBuffer* createCopy();
		//! Copies contents of buffer. Buffer format and scale are preserved, data are converted as necessary.
		//
		//! \param destination
		//!  Destination buffer. Must have the same width, height, depth, may differ in format, scale.
		//! \param scaler
		//!  Scaler used if buffers differ in scale. May be NULL for no conversion.
		//! \return True on success.
		bool copyElementsTo(RRBuffer* destination, const class RRScaler* scaler) const;

		//! Creates cube texture with specified colors of upper and lower hemisphere.
		//
		//! Set scaled true for colors in custom scale (screen colors), false for physical (linear) scale.
		//! By default, white cube for ambient occlusion is created.
		static RRBuffer* createSky(const RRVec4& upper = RRVec4(1), const RRVec4& lower = RRVec4(1), bool scaled = true);


		//////////////////////////////////////////////////////////////////////////////
		// Tools for loading/saving
		//////////////////////////////////////////////////////////////////////////////

		//! Loads buffer from disk to system memory.
		//
		//! Examples:
		//! - load("path/lightmap.png") - loads 2d texture (jpg, gif, dds etc)
		//! - load("path/lightmap.vbu") - loads vertex buffer
		//! - load("path/cube_%%s.png", {"bk","ft","dn","up","rt","lf"}) - loads cubemap from 6 files
		//! - load("path/cube.hdr", non-NULL) - loads cubemap from 1 file, expects cross-shaped image with aspect 3:4 or 4:3
		//! - load("path/cube.hdr", NULL) - loads the same file as 2d texture
		//! - load("path/video.avi") - initializes video streamed to 2d texture, streaming is started by play()
		//! - load("c@pture") - initializes live video capture to 2d texture, capturing is started by play()
		//! \param filename
		//!  Filename of 2d image or vertexbuffer or cubemap or mask of 6 images (sides of cubemap) to be loaded from disk.
		//!  All common file formats are supported.
		//!  Proprietary .vbu format is used for vertex buffers.
		//! \param cubeSideName
		//!  Array of six unique names of cube sides in following order:
		//!  x+ side, x- side, y+ side, y- side, z+ side, z- side.
		//!  \n Examples: {"0","1","2","3","4","5"}, {"bk","ft","dn","up","rt","lf"}.
		//!  \n Must be NULL for vertex buffers and 2d textures, non-NULL for cubemaps (even cubemaps in 1 file).
		//! \param fileLocator
		//!  NULL = load will be attempted only from filename.
		//!  Non-NULL = load will be attempted from paths offered by fileLocator.
		//! \return
		//!  Returns newly created buffer.
		//!  In case of failure, NULL is returned and details logged via RRReporter.
		//! \remark
		//!  Image load/save is implemented outside LightsprintCore.
		//!  Make samples/Import/ImportFreeImage.cpp part of your project to enable save/load
		//!  or use setLoader() to assign custom code.
		static RRBuffer* load(const RRString& filename, const char* cubeSideName[6] = NULL, const RRFileLocator* fileLocator = NULL);
		//! Loads texture from 1 or 6 files to system memory, converting it to cubemap if possible.
		//
		//! This is convenience function working with incomplete information,
		//! it attempts to guess whether you want to load texture from 1 file or from 6 files.
		//! It calls load() with guessed parameters.
		//! If you know exactly what to load, call load() yourself and avoid any guesswork.
		//! \param filename
		//!  It could be one of
		//!  - one of 6 images that make cube map; they are loaded into cubemap
		//!  - cross shaped 4:3 or 3:4 image; is loaded into cubemap
		//!  - any other 2d image; is loaded into 2d map
		//!  It should be full filename, e.g. cube_ft.jpg rather than cube_%%s.jpg.
		//! \param fileLocator
		//!  NULL = load will be attempted only from filename.
		//!  Non-NULL = load will be attempted from paths offered by fileLocator.
		static RRBuffer* loadCube(const RRString& filename, const RRFileLocator* fileLocator = NULL);
		//! Similar to load(), but loads from disk into existing buffer.
		//
		//! Default implementation uses buffer's load() and reset()
		//! to load and copy single static frame, it does not work for videos.
		virtual bool reload(const RRString& filename, const char* cubeSideName[6], const RRFileLocator* fileLocator);

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
		bool save(const RRString& filenameMask, const char* cubeSideName[6] = NULL, const SaveParameters* saveParameters = NULL);

		//! Type of user defined function that loads content from file into new buffer.
		typedef RRBuffer* (Loader)(const RRString& filename, const char* cubeSideName[6]);
		//! Type of user defined function that saves buffer contents to file.
		typedef bool (Saver)(RRBuffer* buffer, const RRString& filenameMask, const char* cubeSideName[6], const SaveParameters* parameters);
		//! Hooks external code that handles loading content from files into new buffers.
		//
		//! Usually called from rr_io::registerLoaders().
		//! Initial state is no code hooked, attempts to load buffer are ignored, load() returns NULL.
		static void registerLoader(Loader* loader);
		//! Hooks external code that handles saving images to disk.
		//
		//! Usually called from rr_io::registerLoaders().
		//! Initial state is no code hooked, attempts to save buffer are ignored, save() returns false.
		static void registerSaver(Saver* saver);


		//////////////////////////////////////////////////////////////////////////////
		// Tools for pixel manipulation
		//////////////////////////////////////////////////////////////////////////////

		//! Changes buffer format.
		virtual void setFormat(RRBufferFormat newFormat);
		//! Changes buffer format to floats, RGB to RGBF, RGBA to RGBAF.
		virtual void setFormatFloats();

		//! Clears buffer to clearColor.
		virtual void clear(RRVec4 clearColor = RRVec4(0));
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
		//! Changes all colors in buffer to pow(color*brightness,gamma).
		//
		//! Preserves buffer format.
		//! This operation may be lossy for byte formats (clamped to 0..1 range), use setFormatFloats() for higher precision.
		virtual void brightnessGamma(RRVec4 brightness, RRVec4 gamma);
		//! Fills mini and maxi with extreme values found in buffer.
		virtual void getMinMax(RRVec4* mini, RRVec4* maxi);


		//////////////////////////////////////////////////////////////////////////////
		// Tools for lightmap postprocessing
		//////////////////////////////////////////////////////////////////////////////

		//! Applies gaussian blur to lightmap, even across seams in unwrap.
		//
		//! Reads and preserves connectivity information stored by lightmap baker to alpha channel.
		//! \param sigma
		//!  Amount of smoothing, reasonable values are around 1.
		//! \param wrap
		//!  True = smooth through lightmap boundaries.
		//! \param object
		//!  Object this lightmap is for, used only for smoothing across unwrap seams.
		//!  When NULL, separated unwrap regions are smoothed separately, unwrap seams stay visible in lightmap.
		//! \return
		//!  True on success, may fail when allocation fails or buffer is not 2d texture.
		virtual bool lightmapSmooth(float sigma, bool wrap, const class RRObject* object);
		//! Fills in unused lightmap texels relevant when bilinearly interpolating lightmap.
		//
		//! Reads connectivity information stored by lightmap baker to alpha channel.
		//! Sets alpha in newly colored texels to 0.001f.
		//! \param wrap
		//!  True = grow through lightmap boundaries.
		//! \return
		//!  False when lightmap is empty (all texels have alpha<0.002) or not 2d texture.
		virtual bool lightmapGrowForBilinearInterpolation(bool wrap);
		//! Fills in unused lightmap texels in proximity of used ones, may help when mipmapping or compressing lightmap.
		//
		//! Expects used texels to have alpha>0.
		//! Sets alpha in newly colored texels to 0.001f.
		//! \param distance
		//!  Distance in pixels, how deep into unused regions to grow used colors.
		//! \param wrap
		//!  True = grow through lightmap boundaries.
		//! \return
		//!  True on success, may fail when allocation fails or buffer is not 2d texture.
		virtual bool lightmapGrow(unsigned distance, bool wrap);
		//! Fills unused texels in lightmap by backgroundColor.
		//
		//! Expects used texels to have alpha>0.
		//! \param backgroundColor
		//!  Color (and alpha) to set to all unused texels.
		//! \return
		//!  True on success, may fail when buffer is not 2d texture.
		virtual bool lightmapFillBackground(RRVec4 backgroundColor);

	protected:
		//! Deletes last reference to buffer from cache.
		//
		//! To be called from delete operator of all RRBuffer implementations when refCount is 1.
		//! Without this function, deleted images would stay in cache and next load from the same filename
		//! would be super fast. This is however rarely needed, freeing memory is more important,
		//! so we explicitly delete buffer from cache.
		void deleteFromCache();
	};

} // namespace

#endif // RRBUFFER_H
