#ifndef RRREALTIMERADIOSITY_H
#define RRREALTIMERADIOSITY_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRRealtimeRadiosity.h
//! \brief RRRealtimeRadiosity - library for calculating radiosity in dynamic scenes
//! \version 2006.8
//! \author Copyright (C) Lightsprint
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
#	ifdef RR_DLL_BUILD_CALCULATOR
		// build dll
#		undef RR_API
#		define RR_API __declspec(dllexport)
#	else // use dll
#pragma comment(lib,"RRRealtimeRadiosity.lib")
#	endif
#	endif
#endif

#include "RRIllumPrecalculated.h"

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
	//
	//////////////////////////////////////////////////////////////////////////////

	class RR_API RRRealtimeRadiosity
	{
	public:
		RRRealtimeRadiosity();
		virtual ~RRRealtimeRadiosity();

		//! One 3d object with storage space for calculated illumination.
		typedef std::pair<RRObject*,RRObjectIllumination*> Object;
		//! Container for all objects present in scene.
		typedef std::vector<Object> Objects;
		//! Sets contents of scene, all objects at once.
		void setObjects(Objects& objects, float stitchDistance = 0.01f);
		//! Returns i-th object in scene.
		RRObject* getObject(unsigned i);
		//! Returns illumination of i-th object in scene.
		RRObjectIllumination* getIllumination(unsigned i);


		//! Calculates and improves indirect illumination on objects.
		//
		//! To be called once per frame while rendering. To be called even when
		//! rendering is paused, calculation runs on background.
		//!
		//! With architect edition (no precalculations), it's expected that there are 
		//! periods when scene doesn't change so you don't even render it.
		//! We use these periods for more intense calculations (99% of time is spent here).
		//! We recognize these periods from you not calling reportLightChange() and reportIlluminationUse().
		//!
		//! When scene is rendered permanently, only 20-50% of time is spent calculating
		//! and quality goes down.
		//!
		//! Night edition will add partial precalculations running transparently on background.
		//! Without changes in your code, you will get much higher quality especially when
		//! scene is rendered permanently.
		RRScene::Improvement calculate();

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
		//!
		//! If you regularly call calculate(), it stops all actions for short period 
		//! of time and gives 100% of CPU time to you.
		void reportCriticalInteraction();
		//! Reports that critical interactions end.
		//
		//! This can be used for example at the end of user's walk through or manipulation with scene.
		//! If you regularly call calculate(), it immediately restarts its activities
		//! and spends more CPU time.
		void reportEndOfInteractions();

		//! Returns multiObject created by merging all objects present in scene.
		//! MultiObject is not created before you insert objects and call calculate().
		const RRObjectAdditionalIllumination* getMultiObject();
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

		//! All objects in scene.
		Objects    objects;
		//! The multiObject created by merging all objects.
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
		long       lastIlluminationUseTime;
		long       lastCriticalInteractionTime;
		long       lastCalcEndTime;
		long       lastReadingResultsTime;
		float      userStep; // avg time spent outside calculate().
		float      calcStep; // avg time spent in calculate().
		float      improveStep; // time to be spent in improve in calculate()
		float      readingResultsPeriod;
		RRScene::Improvement calculateCore(float improveStep);
		// read results
		void       updateVertexLookupTable();
		std::vector<std::vector<std::pair<unsigned,unsigned> > > preVertex2PostTriangleVertex; ///< readResults lookup table
		void       readVertexResults();
		unsigned   resultChannelIndex;
	};

} // namespace

#endif
