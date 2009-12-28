//----------------------------------------------------------------------------
//! \file ImportScene.h
//! \brief LightsprintIO | scene import
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2009
//! All rights reserved
//----------------------------------------------------------------------------

#ifndef IMPORTSCENE_H
#define IMPORTSCENE_H

#ifdef _MSC_VER
	#ifdef RR_IO_STATIC
		// build or use static library
		#define RR_IO_API
	#elif defined(RR_IO_BUILD)
		// build dll
		#define RR_IO_API __declspec(dllexport)
		#pragma warning(disable:4251) // stop MSVC warnings
	#else
		// use dll
		#define RR_IO_API __declspec(dllimport)
		#pragma warning(disable:4251) // stop MSVC warnings
	#endif
#else
	// build or use static library
	#define RR_IO_API
#endif

// autolink library when external project includes this header
#ifdef _MSC_VER
	#if !defined(RR_IO_MANUAL_LINK) && !defined(RR_IO_BUILD)
		#ifdef RR_IO_STATIC
			// use static library
			#if _MSC_VER<1400
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2003_sr.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2003_sd.lib")
				#endif
			#elif _MSC_VER<1500
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2005_sr.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2005_sd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2008_sr.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2008_sd.lib")
				#endif
			#endif
		#else
			// use dll
			#if _MSC_VER<1400
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2003.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2003_dd.lib")
				#endif
			#elif _MSC_VER<1500
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2005.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2005_dd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2008.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2008_dd.lib")
				#endif
			#endif
		#endif
	#endif
#endif

#include "Lightsprint/RRScene.h"

namespace rr_io /// LightsprintIO - access to scenes and images on disk
{

//! Registers callbacks for loading and saving textures using FreeImage 3rd party library.
//
//! After registering loaders, use
//! - RRScene::RRScene(filename) to load 3d scenes
//! - RRBuffer::load(filename) to load 2d images, cube maps, vertex buffers
//! 
//! Fully supported 3d formats:
//! - .dae (Collada)
//! - .gsa (Gamebryo)
//!
//! Partially supported 3d formats:
//! - .3ds
//! - .bsp (Quake3)
//! - .obj
//! - .mgf
//!
//! Supported 2d formats, cube maps, vertex buffers:
//! - .jpg
//! - .png
//! - .hdr (float colors)
//! - .exr (float colors)
//! - .tga
//! - .bmp
//! - .dds
//! - .tif
//! - .jp2
//! - .gif
//! - .ico
//! - .vbu (vertex buffer)
//! - etc, see FreeImage for complete list
void RR_IO_API registerLoaders();


} // namespace rr_io

#endif
