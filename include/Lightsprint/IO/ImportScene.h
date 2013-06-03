//----------------------------------------------------------------------------
//! \file ImportScene.h
//! \brief LightsprintIO | scene import
//! \author Copyright (C) Stepan Hrbek, Lightsprint 2006-2013
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
			#elif _MSC_VER<1700
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2010_sr.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2010_sd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2012_sr.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2012_sd.lib")
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
			#elif _MSC_VER<1700
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2010.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2010_dd.lib")
				#endif
			#else
				#ifdef NDEBUG
					#pragma comment(lib,"LightsprintIO.vs2012.lib")
				#else
					#pragma comment(lib,"LightsprintIO.vs2012_dd.lib")
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
//! - rr::RRScene::RRScene("filename.ext") to load 3d scenes
//! - rr::RRScene::save("filename.ext") to save 3d scenes
//! - rr::RRBuffer::load("filename.ext") to load 2d images, cube maps, vertex buffers, videos
//! - rr::RRBuffer::save("filename.ext") to save 2d images, vertex buffers
//! - rr::RRBuffer::load("c@pture") to capture live video
//!
//! \param argc
//!  Copy of your main() argument. Used only for import isolation on non-Windows platforms, can be 0 otherwise.
//! \param argv
//!  Copy of your main() argument. Used only for import isolation on non-Windows platforms, can be NULL otherwise.
//! \param step Keep empty.
//!  Only if you need to register your own isolated loader for fileformat already supported by LightsprintIO, all without modifying LightsprintIO,
//!  call registerLoaders(1); registerYourLoader(); registerLoaders(2); isolateSceneLoaders();
void RR_IO_API registerLoaders(int argc, char** argv, unsigned step = 0);

//! Isolates scene loaders, makes them run in separated processes.
//
//! Isolation increases robustness of main application, by protecting it
//! against various ill effects of low quality importers.
//! Although no problems are known within Lightsprint's importers, imagine that one day in future,
//! you run e.g. into memory corruption problem. Without isolated importers,
//! any part of codebase can be responsible for corruption, it's tough problem to debug. With isolation,
//! importers are ruled out as a source of problem (importers make around 50% of lines of code in Lightsprint SDK).
//!
//! When isolating loaders, your program startup should look like \code
//! register_your_non_isolated_loaders();
//! rr_io::registerLoaders();
//! register_your_isolated_loaders();
//! rr_io::isolateSceneLoaders();
//! \endcode
//!
//! Isolation works by rerunning our own executable as a separated process with special arguments each time scene needs to be imported.
//! This function checks for special arguments and if found, it converts the scene to .rr3 and exits.
//! Therefore you should run this function as soon in your application as possible (before all slow steps), and you should let it exit, if it needs to.
//! Caller then imports newly created .rr3, and deletes it.
//! If isolated process fails to import the scene or write it back as .rr3, caller tries to import the scene without isolation.
//!
//! .rr3 fileformat is never isolated, it is used for transferring data between processes.
//!
//! \param argc
//!  Copy of your main() argument. Used only on non-Windows platforms, can be 0 otherwise.
//! \param argv
//!  Copy of your main() argument. Used only on non-Windows platforms, can be NULL otherwise.
void RR_IO_API isolateSceneLoaders(int argc, char** argv);


} // namespace rr_io

#endif
