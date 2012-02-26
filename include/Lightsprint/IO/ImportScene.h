//----------------------------------------------------------------------------
//! \file ImportScene.h
//! \brief LightsprintIO | scene import
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2012
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
			#elif _MSC_VER<1600
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2008_sr.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2008_sd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2010_sr.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2010_sd.lib")
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
			#elif _MSC_VER<1600
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2008.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2008_dd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2010.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2010_dd.lib")
				#endif
			#endif
		#endif
	#endif
#endif

#include "Lightsprint/RRScene.h"

namespace rr_io /// LightsprintIO - access to scenes and images on disk
{

//! Registers callbacks for loading and saving scenes, textures and videos in ~75 fileformats (\ref supported_formats).
//
//! After registering loaders and savers, use
//! - RRScene::RRScene(filename) to load 3d scenes
//! - RRScene::save(filename) to save 3d scenes
//! - RRBuffer::load(filename) to load 2d images, cube maps, vertex buffers, videos
//! - RRBuffer::save(filename) to save 2d images, vertex buffers
//! - RRBuffer::load("c@pture") to capture live video
void RR_IO_API registerLoaders();


} // namespace rr_io

#endif
