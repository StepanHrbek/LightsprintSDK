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
	#if !defined(RR_MANUAL_LINK) && !defined(RR_IO_BUILD)
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

#include "Lightsprint/RRDynamicSolver.h"

namespace rr_io
{

/////////////////////////////////////////////////////////////////////////////
//
// Scene

//! Loads scene from file. Supported formats:
//! - .dae (collada)
//! - .3ds
//! - .bsp (quake3)
//! - .obj
//! - .mgf
class RR_IO_API ImportScene : public rr::RRUniformlyAllocatedNonCopyable
{
public:
	//! Loads .dae .3ds .bsp .obj .mgf scene from file.
	//
	//! \param filename
	//!  Filename of scene.
	//! \param scale
	//!  If it is .3ds/.obj, geometry is scaled by scale3dsObj.
	//!  .3ds/.obj formats don't contain information about units,
	//!  different files might need different scale to convert to meters.
	//! \param stripPaths
	//!  You can flatten eventual directory structure in texture names
	//!  and search all textures in the same directory as scene file.
	ImportScene(const char* filename, float scale = 1, bool stripPaths = false);
	~ImportScene();

	const rr::RRObjects* getObjects() {return objects;}
	const rr::RRLights* getLights() {return lights;}

protected:
	rr::RRObjects*    objects;
	rr::RRLights*     lights;
	class Model_3DS*  scene_3ds;
	struct TMapQ3*    scene_bsp;
	class FCDocument* scene_dae;
};

//! Registers callbacks for loading and saving textures using FreeImage 3rd party library.
void RR_IO_API setImageLoader();

} // namespace rr_io

#endif
