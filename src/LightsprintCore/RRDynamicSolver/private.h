// --------------------------------------------------------------------------
// Private attributes of dynamic GI solver.
// Copyright 2006-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef PRIVATE_H
#define PRIVATE_H

#include <vector>
#include "../RRStaticSolver/RRStaticSolver.h"
#include "../RRPackedSolver/RRPackedSolver.h"
#include "Lightsprint/GL/Timer.h"

namespace rr
{

	struct RRDynamicSolver::Private
	{
		enum ChangeStrength
		{
			NO_CHANGE,
			SMALL_CHANGE,
			BIG_CHANGE,
		};

		// scene: inputs
		RRObjects  objects;
		SmoothingParameters smoothing;
		// scene: function of inputs
		RRObject*  multiObjectCustom;
		bool       forcedMultiObjectCustom;
		RRObjectWithPhysicalMaterials* multiObjectPhysical;
		RRReal     minimalSafeDistance; // minimal distance safely used in current scene, proportional to scene size
		bool       staticSceneContainsEmissiveMaterials;
		bool       staticSceneContainsLods;

		// lights: inputs
		RRLights   lights;
		const RRBuffer* environment;
		const unsigned* customIrradianceRGBA8;

		// scale: inputs
		const RRScaler*  scaler;
		RRReal     boostCustomIrradiance;
		// scale: function of inputs
		RRReal     customToPhysical[256];

		// calculate
		bool       dirtyCustomIrradiance;
		bool       dirtyMaterials;
		bool       dirtyResults;
		TIME       lastInteractionTime;
		TIME       lastCalcEndTime;
		TIME       lastReadingResultsTime;
		float      userStep; // avg time spent outside calculate().
		float      calcStep; // avg time spent in calculate().
		float      improveStep; // time to be spent in improve in calculate()
		float      readingResultsPeriod;
		RRStaticSolver*   scene;
		bool       staticSolverCreationFailed; // set after failure so that we don't try again
		unsigned   solutionVersion;
		RRPackedSolver* packedSolver;

		// read results
		struct TriangleVertexPair {unsigned triangleIndex:30;unsigned vertex012:2;TriangleVertexPair(unsigned _triangleIndex,unsigned _vertex012):triangleIndex(_triangleIndex),vertex012(_vertex012){}}; // packed as 30+2 bits is much faster than 32+32 bits
		std::vector<std::vector<TriangleVertexPair> > preVertex2PostTriangleVertex; ///< readResults lookup table for RRDynamicSolver. depends on static objects, must be updated when they change
		std::vector<std::vector<const RRVec3*> > preVertex2Ivertex; ///< readResults lookup table for RRPackedSolver. indexed by 1+objectNumber, 0 is multiObject. depends on static objects and packed solver, must be updated when they change

		Private()
		{
			// scene: inputs
			environment = NULL;
			// scene: function of inputs
			multiObjectCustom = NULL;
			forcedMultiObjectCustom = false;
			multiObjectPhysical = NULL;
			staticSceneContainsEmissiveMaterials = false;
			staticSceneContainsLods = false;

			// lights
			customIrradianceRGBA8 = NULL;

			// scale: inputs
			scaler = NULL;
			boostCustomIrradiance = 1;
			// scale: function of inputs
			for (unsigned i=0;i<256;i++)
			{
				customToPhysical[i] = i/255.f;
			}

			// calculate
			scene = NULL;
			staticSolverCreationFailed = false;
			dirtyCustomIrradiance = true;
			dirtyMaterials = true;
			dirtyResults = true;
			lastInteractionTime = 0;
			lastCalcEndTime = 0;
			lastReadingResultsTime = 0;
			userStep = 0;
			calcStep = 0;
			improveStep = 0;
			readingResultsPeriod = 0;
			solutionVersion = 1;
			minimalSafeDistance = 0;
			packedSolver = NULL;
		}
		~Private()
		{
			deleteScene();
		}
		void deleteScene()
		{
			RR_SAFE_DELETE(packedSolver);
			RR_SAFE_DELETE(scene);
			// be careful:
			// 1. physical==custom (when no scaler) -> don't delete physical
			if (multiObjectPhysical==multiObjectCustom) multiObjectPhysical = NULL;
			// 2. forced (in sceneViewer) -> don't delete custom
			if (forcedMultiObjectCustom) multiObjectCustom = NULL;
			// 3. both -> don't delete 
			RR_SAFE_DELETE(multiObjectCustom);
			RR_SAFE_DELETE(multiObjectPhysical);
			// clear tables that depend on scene (code that fills tables needs them empty)
			preVertex2PostTriangleVertex.clear();
			preVertex2Ivertex.clear();
		}
	};

} // namespace

#endif
