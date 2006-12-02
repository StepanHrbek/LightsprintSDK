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
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRRealtimeRadiosity
	{
	public:
		RRRealtimeRadiosity();
		virtual ~RRRealtimeRadiosity();

		//! One static 3d object with storage space for calculated illumination.
		typedef std::pair<RRObject*,RRObjectIllumination*> Object;
		//! Container for all static objects present in scene.
		typedef std::vector<Object> Objects;
		//! Sets static contents of scene, all objects at once.
		//! \param stitchDistance
		//!  Distance in world units. Vertices with lower or equal distance will be stitched into one vertex.
		//!  Zero stitches only identical vertices, negative value means no action.
		//! \param minFeatureSize
		//!  Distance in world units. Smaller features will be smoothed. This could be imagined as a kind of blur.
		//!  Use 0 for no smoothing and watch for possible artifacts in areas with small geometry details
		//!  and 'needle' triangles. Increase until artifacts disappear.
		//!  15cm (0.15 for game with 1m units) could be good for typical interior game.
		//! \param maxSmoothAngle
		//!  Angle in radians, controls automatic smoothgrouping.
		//!  Edges with smaller angle between face normals are smoothed out.
		//!  Optimal value depends on your geometry, but reasonable value could be 0.33.
		void setObjects(Objects& objects, float stitchDistance = 0.01f, float minFeatureSize = 0.15f, float maxSmoothAngle = 0.33f);
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
		//! Calculates and updates environment map for dynamic object at given position.
		//
		//! Generates multiple LODs, with max size selected by you, smallest size always 2.
		//! \n You can use multiple LODs for roughness mapping - select LOD for each pixel according to roughness map.
		//! \n Using smaller LOD creates effect of rough surface with diffuse reflection,
		//! using greater LOD creates effect of smooth surface with specular reflection.
		//! \param environmentMap
		//!  Your custom environment map. To be filled with calculated data.
		//! \param maxSize
		//!  Size of maximal LOD of cubemap to be calculated; containing 6*size*size pixels.
		//!  If your custom environmentMap requires size to be power of two, make sure you enter correct size here.
		//!  Use smaller size for faster update.
		//!  Size 4 is optimal for diffuse reflection.
		//!  Size 16 is optimal for both diffuse and specular reflections.
		//! \param objectCenter
		//!  Center of your dynamic object in world space coordinates.
		void updateEnvironmentMap(RRIlluminationEnvironmentMap* environmentMap, unsigned maxSize, RRVec3 objectCenter);

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
		RRObjectAdditionalIllumination* getMultiObject();
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

		//! Called each time new RRScene is created (could be more than once per life).
		//! Default implementation is empty.
		//! You can reimplement it for example to change scene scaler.
		virtual void onSceneInit();

		//! Returns new vertex buffer (for indirect illumination) in your custom format.
		//
		//! Default implementation allocates 3 floats per vertex in RAM.
		//! This is good for editor, but you may want to use 4 bytes per vertex in game to save memory.
		//! You may even use monochromatic (1 float) format if you don't need color bleeding.
		virtual RRIlluminationVertexBuffer* newVertexBuffer(unsigned numVertices);
		//! Returns new pixel buffer (for ambient map) in your custom format.
		//! Default implementation returns NULL.
		virtual RRIlluminationPixelBuffer* newPixelBuffer(RRObject* object);

		//! All objects in scene.
		Objects    objects;
		//! The multiObject created by merge of all objects.
		RRObjectAdditionalIllumination* multiObject;
		//! The scene used for radiosity calculations.
		RRScene*   scene;

	private:
		enum ChangeStrength
		{
			NO_CHANGE,
			SMALL_CHANGE,
			BIG_CHANGE,
		};
		// calculate
		float      stitchDistance;
		float      minFeatureSize;
		float      maxSmoothAngle;
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
		RRObject*  multiObjectBase;
		RRScene::Improvement calculateCore(unsigned requests, float improveStep);
		// read results
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
