// --------------------------------------------------------------------------
// Scene viewer - light icons.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVENTITYICONS_H
#define SVENTITYICONS_H

#include "Lightsprint/GL/SceneViewer.h"

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRCollider.h"
#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/GL/UberProgram.h"
#include "SVEntity.h"

namespace rr_gl
{

	class SVEntityIcons
	{
	public:
		SVEntityIcons(const char* pathToMaps, UberProgram* uberProgram);
		~SVEntityIcons();

		// inputs: entities, ray->rayXxx, iconSize
		// outputs: ray->hitXxx
		// sideeffects: ray->rayLengthMax is lost
		bool intersectIcons(const SVEntities& entities, rr::RRRay* ray, float iconSize);

		void renderIcons(const SVEntities& entities, const Camera& eye, unsigned selectedIndex, float iconSize);

		bool isOk() const;
	private:
		// icon vertices are computed in worldspace to simplify ray-icon intersections
		void getIconWorldVertices(const SVEntity& entity, rr::RRVec3 eyePos, rr::RRVec3 vertex[4], float iconSize);

		// inputs: t, ray->rayXxx
		// outputs: ray->hitXxx
		bool intersectTriangle(const rr::RRMesh::TriangleBody* t, rr::RRRay* ray);

		// inputs: entity, ray->rayXxx
		// outputs: ray->hitXxx
		bool intersectIcon(const SVEntity& entity, rr::RRRay* ray, float iconSize);

		void renderIcon(const SVEntity& entity, const Camera& eye, float iconSize);

		rr::RRBuffer* icon[4]; // indexed by Entity::icon
		Program* program;
	};
 
}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
