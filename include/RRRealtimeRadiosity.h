#ifndef RRREALTIMERADIOSITY_H
#define RRREALTIMERADIOSITY_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRRealtimeRadiosity.h
//! \brief RRRealtimeRadiosity - library for calculating radiosity in dynamic scenes
//! \version 2006.11.16
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <vector>
#include "RRVision.h"

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#ifdef NDEBUG
			#pragma comment(lib,"RRRealtimeRadiosity_sr.lib")
		#else
			#pragma comment(lib,"RRRealtimeRadiosity_sd.lib")
		#endif
#	else
#	ifdef RR_DLL_BUILD_REALTIMERADIOSITY
		// build dll
#		undef RR_API
#		define RR_API __declspec(dllexport)
#	else // use dll
#ifdef NDEBUG
	#pragma comment(lib,"RRRealtimeRadiosity.lib")
#else
	#pragma comment(lib,"RRRealtimeRadiosity_dd.lib")
#endif
#	endif
#	endif
#endif

#include "RRIllumination.h"

namespace rr
{

	//////////////////////////////////////////////////////////////////////////////
	//
	//  RRRealtimeRadiosity
	//! Radiosity solver for interactive applications.
	//
	//! Usage: Create one instance at the beginning of interactive session
	//! and load it with all static objects in scene.
	//! Call calculate() at each frame, it will spend some time
	//! by improving illumination.
	//! When scene changes, report it using report* methods.
	//!
	//! Few methods remain to be implemented by you.
	//! You can get reports what happens inside if you reimplement reportAction().
	//!
	//! You can check sample HelloRealtimeRadiosity where we use it.
	//!
	//! It is not allowed to create and use multiple instances at the same time.
	//!
	//! Thread safe:
	//! updateEnvironmentMaps() yes, may be called from multiple threads at the same time.
	//! Other methods no, may be called from multiple threads, but not at the same time.
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRRealtimeRadiosity
	{
	public:
		RRRealtimeRadiosity();
		virtual ~RRRealtimeRadiosity();

		//! Set scaler used by this scene i/o operations. This is option for your convenience. See RRScaler for details.
		void setScaler(RRScaler* scaler);
		//! Returns scaler used by this scene i/o operations.
		const RRScaler* getScaler() const;

		//! One static 3d object with storage space for calculated illumination.
		typedef std::pair<RRObject*,RRObjectIllumination*> Object;
		//! Container for all static objects present in scene.
		typedef std::vector<Object> Objects;
		//! Sets static contents of scene, all objects at once.
		//! \param objects
		//!  Static contents of your scene, set of static objects.
		//!  Objects should not move (in 3d space) during our lifetime.
		//!  Object's getSurface should return values in custom scale.
		//! \param smoothing
		//!  Static scene illumination smoothing.
		//!  Set NULL for default values.
		void setObjects(Objects& objects, const RRScene::SmoothingParameters* smoothing);
		//! Returns number of static objects in scene.
		unsigned getNumObjects() const;
		//! Returns i-th static object in scene.
		RRObject* getObject(unsigned i);
		//! Returns illumination of i-th static object in scene.
		RRObjectIllumination* getIllumination(unsigned i);


		//! Optional request codes for calculate().
		//! Calculate() knows when to update which buffer etc,
		//! but you can override his logic by these manual requests.
		enum Request
		{
			UPDATE_VERTEX_BUFFERS = 1,
			UPDATE_PIXEL_BUFFERS = 2,
		};
		//! Calculates and improves indirect illumination on static objects.
		//
		//! To be called once per frame while rendering. To be called even when
		//! rendering is paused, calculation runs on background.
		//!
		//! With architect edition (no precalculations), it helps to improve performance if
		//! you don't repeatedly render scene that doesn't change, for example in game editor.
		//! Note that user can't see any difference, you only save time and power 
		//! by not rendering the same image again.
		//! Calculation() uses these periods for more intense calculations,
		//! creating higher quality illumination.
		//! It detects these periods from you not calling reportLightChange() and reportInteraction().
		//!
		//! When scene is rendered permanently, only 20-50% of CPU time is spent calculating.
		//!
		//! Night edition will add partial precalculations running transparently on background.
		//! Without changes in your code, you will get much higher quality.
		//! \param requests
		//!  Sum of manual requests. See enum Request for all possible individual requests.
		//! \return
		//!  IMPROVED when any vertex or pixel buffer was updated with improved illumination.
		//!  NOT_IMPROVED otherwise. FINISHED = exact solution was reached, no further calculations are necessary.
		RRScene::Improvement calculate(unsigned requests=0);
		//! Calculates and updates environment maps for dynamic object at given position.
		//
		//! Generates specular and diffuse environment maps with object's global illumination.
		//! \n- specular map is to be sampled (by reflected direction) in object's glossy pixels
		//! \n- diffuse map is to be sampled (by surface normal) in object's rough pixels
		//!
		//! Thread safe: yes, may be called from multiple threads at the same time.
		//!
		//! \param objectCenter
		//!  Center of your dynamic object in world space coordinates.
		//! \param gatherSize
		//!  Number of samples gathered from scene will be gatherSize*gatherSize*6.
		//!  It doesn't have to be power of two.
		//!  Set higher for higher quality, lower for higher speed.
		//!  Size 16 is good, but 4 could be enough if you don't need specular map.
		//! \param specularMap
		//!  Your custom environment map for rendering object's specular reflection.
		//!  To be filled with calculated data in custom scale.
		//!  Set to NULL for no specular reflection.
		//! \param specularSize
		//!  Size of environment map for rendering object's specular reflection.
		//!  Map will have 6*specularSize*specularSize pixels.
		//!  If your custom specularMap requires size to be power of two, make sure you enter correct size here.
		//!  Use smaller size for faster update.
		//!  Size 16 is good.
		//! \param diffuseMap
		//!  Your custom environment map for rendering object's diffuse reflection.
		//!  To be filled with calculated data in custom scale.
		//!  Set to NULL for no diffuse reflection.
		//! \param diffuseSize
		//!  Size of environment map for rendering object's diffuse reflection.
		//!  Map will have 6*diffuseSize*diffuseSize pixels.
		//!  If your custom diffuseMap requires size to be power of two, make sure you enter correct size here.
		//!  Use smaller size for faster update.
		//!  Size 4 is good.
		//! \param HDR
		//!  True = physically correct calculation.
		//!  False = fast calculation clamped to 8bits per channel.
		void updateEnvironmentMaps(RRVec3 objectCenter, unsigned gatherSize,
			unsigned specularSize, RRIlluminationEnvironmentMap* specularMap,
			unsigned diffuseSize, RRIlluminationEnvironmentMap* diffuseMap,
			bool HDR);

		//! Reports that appearance of one or more materials has changed.
		//!
		//! Call this when you change material properties in your material editor.
		void reportMaterialChange();
		//! Reports that position/rotation/shape of one or more lights has changed.
		//
		//! Call this when any light in scene changes any property, so that direct illumination changes.
		//! \param strong Hint for solver, was change in illumination strong, does illumination 
		//!  change significantly? Good hint improves performance.
		void reportLightChange(bool strong);
		//! Reports interaction between user and application.
		//
		//! This is useful for better CPU utilization in non-interactive periods of life
		//! of your application.
		//! Call it each time user hits key, moves mouse etc, or animation frame is played.
		//! \n\n When called often,
		//! solver calculates illumination in small batches to preserve realtime performance.
		//! This is typical mode for games.
		//! \n\n When not called at all,
		//! there's no interaction between user and application
		//! and solver increases calculation batches up to 100ms at once to better utilize CPU.
		//! This happens for example in game editor, when level designer stops moving mouse.
		void reportInteraction();

		//! Returns multiObject created by merging all objects present in scene.
		//! MultiObject is not created before you insert objects and call calculate().
		RRObject* getMultiObjectCustom();
		//! As getMultiObjectCustom, but with surfaces converted to physical space.
		RRObjectWithPhysicalSurfaces* getMultiObjectPhysical();
		//! As getMultiObjectPhysical, but with space for storage of detected direct illumination.
		RRObjectWithIllumination* getMultiObjectPhysicalWithIllumination();
		//! Returns the scene.
		//! Scene is not created before you insert objects and call calculate().
		const RRScene* getScene();

	protected:
		//! Autodetects material properties of all materials present in scene.
		//
		//! To be implemented by you.
		//! New values must appear in RRObjects already present in scene.
		virtual void detectMaterials() = 0;
		//! Autodetects direct illumination on all faces in scene.
		//
		//! To be implemented by you.
		//! \return You may fail by returning false, you will get another chance next time.
		virtual bool detectDirectIllumination() = 0;

		//! Returns new vertex buffer (for indirect illumination) in your custom format.
		//
		//! Default implementation allocates 3 floats per vertex in RAM.
		//! This is good for editor, but you may want to use 4 bytes per vertex in game to save memory.
		//! You may even use monochromatic (1 float) format if you don't need color bleeding.
		virtual RRIlluminationVertexBuffer* newVertexBuffer(unsigned numVertices);
		//! Returns new pixel buffer (for ambient map) in your custom format.
		//! Default implementation returns NULL.
		virtual RRIlluminationPixelBuffer* newPixelBuffer(RRObject* object);

	private:
		enum ChangeStrength
		{
			NO_CHANGE,
			SMALL_CHANGE,
			BIG_CHANGE,
		};
		// calculate
		Objects    objects;
		RRScene::SmoothingParameters smoothing;
		bool       dirtyMaterials;
		bool       dirtyGeometry;
		ChangeStrength dirtyLights; // 0=no light change, 1=small light change, 2=strong light change
		bool       dirtyResults;
		long       lastInteractionTime;
		long       lastCalcEndTime;
		long       lastReadingResultsTime;
		float      userStep; // avg time spent outside calculate().
		float      calcStep; // avg time spent in calculate().
		float      improveStep; // time to be spent in improve in calculate()
		float      readingResultsPeriod;
		RRObject*  multiObjectCustom;
		RRObjectWithPhysicalSurfaces* multiObjectPhysical;
		RRObjectWithIllumination* multiObjectPhysicalWithIllumination;
		RRScene*   scene;
		RRScene::Improvement calculateCore(unsigned requests, float improveStep);
		// read results
		RRScaler*  scaler;
		void       updateVertexLookupTable();
		std::vector<std::vector<std::pair<unsigned,unsigned> > > preVertex2PostTriangleVertex; ///< readResults lookup table
		void       readVertexResults();
		void       readPixelResults();
		unsigned   resultChannelIndex;
	};


	//////////////////////////////////////////////////////////////////////////////
	//
	// Sanity checks.
	//
	// First step of your application should be
	//   if(!RR_INTERFACE_OK) terminate_with_message(RR_INTERFACE_MISMATCH_MSG);
	//////////////////////////////////////////////////////////////////////////////

	//! Returns id of interface offered by library.
	RR_API unsigned RR_INTERFACE_ID_LIB();
	// Returns id of interface expected by app.
	#define RR_INTERFACE_ID_APP() unsigned( sizeof(rr::RRRealtimeRadiosity) + 0 )
	//! Returns if interface matches. False = dll mismatch, app should be terminated.
	#define RR_INTERFACE_OK (RR_INTERFACE_ID_APP()==rr::RR_INTERFACE_ID_LIB())
	//! Returns description of interface offered by library + compile date.
	RR_API char* RR_INTERFACE_DESC_LIB();
	// Returns description of interface expected by app + compile date.
	#if defined(NDEBUG) && defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "RELEASE_STATIC (" __DATE__ " " __TIME__ ")"
	#elif defined(NDEBUG) && !defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "RELEASE_DLL (" __DATE__ " " __TIME__ ")"
	#elif !defined(NDEBUG) && defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "DEBUG_STATIC (" __DATE__ " " __TIME__ ")"
	#elif !defined(NDEBUG) && !defined(RR_STATIC)
	#define RR_INTERFACE_DESC_APP() "DEBUG_DLL (" __DATE__ " " __TIME__ ")"
	#endif
	// Returns description of version mismatch.
	#define RR_INTERFACE_MISMATCH_MSG "RRRealtimeRadiosity version mismatch.\nLibrary has interface: %d %s\nApplication expects  : %d %s\n",rr::RR_INTERFACE_ID_LIB(),rr::RR_INTERFACE_DESC_LIB(),RR_INTERFACE_ID_APP(),RR_INTERFACE_DESC_APP()

} // namespace

#endif
