// --------------------------------------------------------------------------
// Scene viewer - light icons.
// Copyright (C) 2007-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVENTITYICONS_H
#define SVENTITYICONS_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRCollider.h"
#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/GL/UberProgram.h"
#include "SVEntity.h"
#include <vector>

namespace rr_gl
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVEntities - collection of entites/icons

	class SVEntities : public std::vector<SVEntity>
	{
	public:
		float iconSize;

		void addLights(const rr::RRLights& lights, rr::RRVec3 dirlightPosition);
		void markSelected(const EntityIds& selectedEntityIds);
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVEntityIcons - can render and intersect SVEntities

	class SVEntityIcons
	{
	public:
		SVEntityIcons(const char* pathToMaps, UberProgram* uberProgram);
		~SVEntityIcons();

		// inputs: entities, ray->rayXxx
		// outputs: ray->hitXxx
		// sideeffects: ray->rayLengthMax is lost
		bool intersectIcons(const SVEntities& entities, rr::RRRay* ray);

		void renderIcons(const SVEntities& entities, const rr::RRCamera& eye);

		bool isOk() const;
	private:
		// icon vertices are computed in worldspace to simplify ray-icon intersections
		void getIconWorldVertices(const SVEntity& entity, rr::RRVec3 eyePos, rr::RRVec3 vertex[4]);

		// inputs: t, ray->rayXxx
		// outputs: ray->hitXxx
		bool intersectTriangle(const rr::RRMesh::TriangleBody* t, rr::RRRay* ray);

		// inputs: entity, ray->rayXxx
		// outputs: ray->hitXxx
		bool intersectIcon(const SVEntity& entity, rr::RRRay* ray);

		void renderIcon(const SVEntity& entity, const rr::RRCamera& eye);

		rr::RRBuffer* icon[IC_LAST]; // indexed by IconCode
		Program* programIcons;
	};
 
}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
