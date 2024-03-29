// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Private attributes of RRSolver.
// --------------------------------------------------------------------------

#ifndef PRIVATE_H
#define PRIVATE_H

#include <vector>
#include "../RRStaticSolver/RRStaticSolver.h"
#include "../RRPackedSolver/RRPackedSolver.h"

namespace rr
{
	class CubeGatheringKit
	{
	public:
		bool inUse;
		RRRay* ray6;
		class ReflectionCubeCollisionHandler* handler6;
		CubeGatheringKit();
		~CubeGatheringKit();
	};

	struct RRSolver::Private
	{
		enum ChangeStrength
		{
			NO_CHANGE,
			SMALL_CHANGE,
			BIG_CHANGE,
		};

		// scene: inputs
		RRObjects  staticObjects;
		RRObjects  dynamicObjects;
		SmoothingParameters smoothing; // for staticObjects only
		// scene: function of inputs
		RRObject*  multiObject;
		bool       forcedMultiObject;
		RRReal     minimalSafeDistance; // minimal distance safely used in current scene, proportional to scene size
		bool       staticSceneContainsLods;
		RRCollider*superCollider;
		unsigned   superColliderMeshVersion;
		bool       superColliderDirtyBig; // set of objects did change
		bool       superColliderDirtySmall; // matrix of existing object did change
		RRObjects  superColliderObjects; // used only temporarily inside getCollider(). stored here to avoid allocation in every getCollider
		RRVec3     superColliderMin;
		RRVec3     superColliderMax;
		RRVec3     superColliderCenter;
		const RRObject* superColliderPlane; // RRCamera::setRangeDynamically() uses it for better near/far

		// lights: inputs
		RRLights   lights;
		RRBuffer*  environment0;
		RRBuffer*  environment1;
		float      environmentAngleRad0;
		float      environmentAngleRad1;
		float      environmentBlendFactor;
		const unsigned* customIrradianceRGBA8; // nullptr or array of getMultiObject()->getCollider()->getMesh()->getNumTriangles() elements

		// scale: inputs
		const RRColorSpace*  colorSpace;
		RRReal     lightMultiplier;
		// scale: function of inputs
		RRReal     customToPhysical[256];

		// calculate
		bool       dirtyCustomIrradiance;
		bool       dirtyMaterials;
		unsigned   dirtyResults; // solution was improved in static or packed solver (this number of times), but we haven't incremented solutionVersion yet. we do this to reduce frequency of lightmap updates
		RRTime     lastInteractionTime;
		RRTime     lastCalcEndTime;
		RRTime     lastReadingResultsTime;
		RRTime     lastGIDirtyBecauseOfVideoTime;
		float      userStep; // avg time spent outside calculate().
		float      calcStep; // avg time spent in calculate().
		float      improveStep; // time to be spent in improve in calculate()
		float      readingResultsPeriodSeconds; // this number of seconds must pass before improvement results in solutionVersion++
		unsigned   readingResultsPeriodSteps; // this number of steps must pass before improvement results in solutionVersion++
		RRStaticSolver*   scene;
		bool       staticSolverCreationFailed; // set after failure so that we don't try again
		unsigned   solutionVersion; // incremented each time we want user to update lightmaps (not after every change in solution, slightly less frequently)
		RRPackedSolver* packedSolver;
		unsigned   materialTransmittanceVersionSum[2];
		CalculateParameters previousCalculateParameters; // copy of previous non-nullptr params sent by user (we pass it to calculate when we call it internally)

		// read results
		struct TriangleVertexPair {unsigned triangleIndex:30;unsigned vertex012:2;TriangleVertexPair(unsigned _triangleIndex,unsigned _vertex012):triangleIndex(_triangleIndex),vertex012(_vertex012){}}; // packed as 30+2 bits is much faster than 32+32 bits
		std::vector<std::vector<TriangleVertexPair> > postVertex2PostTriangleVertex; ///< readResults lookup table for RRSolver. indexed by objectNumber. depends on static objects, must be updated when they change
		std::vector<std::vector<const RRVec3*> > postVertex2Ivertex; ///< readResults lookup table for RRPackedSolver. indexed by 1+objectNumber, 0 is multiObject. depends on static objects and packed solver, must be updated when they change
		CubeGatheringKit cubeGatheringKits[10];

		Private()
		{
			// scene: inputs
			environment0 = nullptr;
			environment1 = nullptr;
			environmentAngleRad0 = 0;
			environmentAngleRad1 = 0;
			environmentBlendFactor = 0;
			// scene: function of inputs
			multiObject = nullptr;
			forcedMultiObject = false;
			staticSceneContainsLods = false;
			superCollider = nullptr;
			superColliderDirtyBig = false;
			superColliderDirtySmall = false;
			superColliderMeshVersion = 0;
			// lights
			customIrradianceRGBA8 = nullptr;

			// scale: inputs
			colorSpace = nullptr;
			lightMultiplier = 1;
			// scale: function of inputs
			for (unsigned i=0;i<256;i++)
			{
				customToPhysical[i] = i/255.f;
			}

			// calculate
			scene = nullptr;
			staticSolverCreationFailed = false;
			dirtyCustomIrradiance = true;
			dirtyMaterials = true;
			dirtyResults = 1;
			lastInteractionTime.addSeconds(-1);
			lastCalcEndTime.addSeconds(0);
			lastReadingResultsTime.addSeconds(-100);
			lastGIDirtyBecauseOfVideoTime.addSeconds(-10);
			userStep = 0;
			calcStep = 0;
			improveStep = 0;
			readingResultsPeriodSeconds = 0;
			readingResultsPeriodSteps = 0;
			solutionVersion = 1; // set solver to 1 so users can start with 0 and solver (even if incremented) will differ
			minimalSafeDistance = 0;
			packedSolver = nullptr;
			materialTransmittanceVersionSum[0] = 0;
			materialTransmittanceVersionSum[1] = 0;
		}
		~Private()
		{
			deleteScene();

			// [#23] inc/dec refcount of environments entering/leaving solver
			RR_SAFE_DELETE(environment0);
			RR_SAFE_DELETE(environment1);
		}
		void deleteScene()
		{
			RR_SAFE_DELETE(packedSolver);
			RR_SAFE_DELETE(scene);
			// if forced (in sceneViewer) -> don't delete custom
			if (forcedMultiObject) multiObject = nullptr;
			RR_SAFE_DELETE(superCollider);
			RR_SAFE_DELETE(multiObject);
			// clear tables that depend on scene (code that fills tables needs them empty)
			postVertex2PostTriangleVertex.clear();
			postVertex2Ivertex.clear();
		}
	};

} // namespace

#endif
