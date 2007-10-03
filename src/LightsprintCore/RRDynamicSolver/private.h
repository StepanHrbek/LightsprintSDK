#ifndef PRIVATE_H
#define PRIVATE_H

#include <vector>
#include "Lightsprint/RRDynamicSolver.h"
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
		RRObjects  dobjects;
		RRLights   lights;
		const RRIlluminationEnvironmentMap* environment;
		RRStaticSolver::SmoothingParameters smoothing;
		// scene: function of inputs
		RRObject*  multiObjectCustom;
		RRObjectWithPhysicalMaterials* multiObjectPhysical;
		RRObjectWithIllumination* multiObjectPhysicalWithIllumination;
		RRReal     minimalSafeDistance; // minimal distance safely used in current scene, proportional to scene size

		// detect
		unsigned*  detectedCustomRGBA8;

		// scale: inputs
		RRScaler*  scaler;
		RRReal     boostDetectedDirectIllumination;
		// scale: function of inputs
		RRReal     customToPhysical[256];

		// calculate
		bool       dirtyMaterials;
		bool       dirtyStaticSolver;
		ChangeStrength dirtyLights; // 0=no light change, 1=small light change, 2=strong light change
		bool       dirtyResults;
		TIME       lastInteractionTime;
		TIME       lastCalcEndTime;
		TIME       lastReadingResultsTime;
		float      userStep; // avg time spent outside calculate().
		float      calcStep; // avg time spent in calculate().
		float      improveStep; // time to be spent in improve in calculate()
		float      readingResultsPeriod;
		RRStaticSolver*   scene;
		unsigned   solutionVersion;
		RRPackedSolver* packedSolver;

		// read results
		struct TriangleVertexPair {unsigned triangleIndex:30;unsigned vertex012:2;TriangleVertexPair(unsigned _triangleIndex,unsigned _vertex012):triangleIndex(_triangleIndex),vertex012(_vertex012){}}; // packed as 30+2 bits is much faster than 32+32 bits
		std::vector<std::vector<TriangleVertexPair> > preVertex2PostTriangleVertex; ///< readResults lookup table for RRDynamicSolver
		std::vector<std::vector<const RRVec3*> > preVertex2Ivertex; ///< readResults lookup table for RRPackedSolver

		Private()
		{
			// scene: inputs
			environment = NULL;
			// scene: function of inputs
			multiObjectCustom = NULL;
			multiObjectPhysical = NULL;
			multiObjectPhysicalWithIllumination = NULL;

			// detect
			detectedCustomRGBA8 = NULL;

			// scale: inputs
			scaler = NULL;
			boostDetectedDirectIllumination = 1;
			// scale: function of inputs
			for(unsigned i=0;i<256;i++)
			{
				customToPhysical[i] = i/255.f;
			}

			// calculate
			scene = NULL;
			dirtyMaterials = true;
			dirtyStaticSolver = true;
			dirtyLights = BIG_CHANGE;
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
			delete packedSolver;
			delete scene;
			delete multiObjectPhysicalWithIllumination;
			if(multiObjectPhysical!=multiObjectCustom) delete multiObjectPhysical; // no scaler -> physical == custom
			delete multiObjectCustom;
		}
	};

} // namespace

#endif
