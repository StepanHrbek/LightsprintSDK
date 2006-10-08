#ifndef RRREALTIMERADIOSITY_H
#define RRREALTIMERADIOSITY_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRRealtimeRadiosity.h
//! \brief RRRealtimeRadiosity - library for calculating radiosity in dynamic scenes
//! \version 2006.10.5
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include <vector>
#include "RRVision.h"

#ifdef _MSC_VER
#	ifdef RR_STATIC
		// use static library
		#if defined(RR_RELEASE) || (defined(NDEBUG) && !defined(RR_DEBUG))
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
#pragma comment(lib,"RRRealtimeRadiosity.lib")
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
	//! and load it with scene objects.
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
		void setObjects(Objects& objects, float stitchDistance = 0.01f);
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
		//! you don't repeatedly render scene that doesn't change.
		//! Note that user can't see any difference, as image stays unchanged in both cases.
		//! We use these periods for more intense calculations (99% of time is spent here).
		//! We recognize these periods from you not calling reportLightChange() and reportIlluminationUse().
		//! Longer time spent in calculation() leads to higher quality.
		//!
		//! When scene is rendered permanently, only 20-50% of time is spent calculating.
		//!
		//! Night edition will add partial precalculations running transparently on background.
		//! Without changes in your code, you will get much higher quality.
		//! \param requests
		//!  Sum of manual requests. See enum Request for all possible individual requests.
		//! \return
		//!  IMPROVED when any vertex or pixel buffer was updated with improved illumination.
		//!  NOT_IMPROVED otherwise. FINISHED = exact solution was reached, no further calculations are necessary.
		RRScene::Improvement calculate(unsigned requests=0);

		//! Reports that appearance of one or more materials has changed.
		//!
		//! Call this when you change material properties in your material editor.
		void reportMaterialChange();
		//! Reports that position/rotation/shape of one or more objects has changed.
		//
		//! Call this when some object in scene moves or changes shape.
		//! Not used in any demo yet, not fully tested. This will be fixed in next version.
		void reportGeometryChange();
		//! Reports that position/rotation/shape of one or more lights has changed.
		//
		//! Call this when any light in scene changes any property, so that direct illumination changes.
		//! \param strong Hint for solver, was change in illumination strong, does illumination 
		//!  change significantly? Good hint improves performance.
		void reportLightChange(bool strong);
		//! Reports that calculated illumination has been used (eg. for rendering).
		//
		//! Call at each render where you use calculated radiosity.
		//! Without such report, illumination is updated less often, which saves time for calculations.
		void reportIlluminationUse();
		//! Reports that user interacts and needs maximal responsiveness.
		//
		//! This can be used for example when user walks through scene where nothing changes.
		//! Calculate() stops all actions for short period of time and gives 100% of CPU time to you.
		void reportCriticalInteraction();
		//! Reports that critical interactions end.
		//
		//! This can be used for example at the end of user's walk through or manipulation with scene.
		//! If you regularly call calculate(), it immediately restarts its activities
		//! and spends more CPU time.
		void reportEndOfInteractions();

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

		//! Adjusts RRScene parameters each time new scene is created. May be reimplemented by you.
		virtual void adjustScene();
		//! Called automatically for each interesting event, you may reimplement it and watch what happens inside.
		//! Debugging aid.
		virtual void reportAction(const char* action) const;

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
		bool       dirtyMaterials;
		bool       dirtyGeometry;
		ChangeStrength dirtyLights; // 0=no light change, 1=small light change, 2=strong light change
		bool       dirtyResults;
		long       lastIlluminationUseTime;
		long       lastCriticalInteractionTime;
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

} // namespace

#endif
