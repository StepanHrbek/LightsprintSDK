// --------------------------------------------------------------------------
// Scene viewer - light icons.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVLIGHTICONS_H
#define SVLIGHTICONS_H

#include "Lightsprint/GL/SceneViewer.h"

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRCollider.h"
#include "Lightsprint/GL/Camera.h"
#include "Lightsprint/GL/UberProgram.h"

namespace rr_gl
{

	class SVLightIcons
	{
	public:
		SVLightIcons(const char* pathToMaps, UberProgram* uberProgram);
		~SVLightIcons();

		// inputs: lights, ray->rayXxx
		// outputs: ray->hitXxx
		// sideeffects: ray->rayLengthMax is lost
		bool intersect(const rr::RRLights& lights, rr::RRRay* ray, rr::RRVec3 dirlightPosition, float iconSize);

		void render(const rr::RRLights& lights, const Camera& eye, unsigned selectedIndex, rr::RRVec3 dirlightPosition, float iconSize);

		bool isOk() const;
	private:
		// icon vertices are computed in worldspace to simplify ray-icon intersections
		void getIconWorldVertices(const rr::RRLight& light, rr::RRVec3 eyePos, rr::RRVec3 vertex[4], rr::RRVec3& dirlightPosition, float iconSize);

		// inputs: t, ray->rayXxx
		// outputs: ray->hitXxx
		bool intersectTriangle(const rr::RRMesh::TriangleBody* t, rr::RRRay* ray);

		// inputs: light, ray->rayXxx
		// outputs: ray->hitXxx
		bool intersectIcon(const rr::RRLight& light, rr::RRRay* ray, rr::RRVec3& dirlightPosition, float iconSize);

		void renderIcon(const rr::RRLight& light, const Camera& eye, rr::RRVec3& dirlightPosition, float iconSize);

		rr::RRBuffer* icon[3]; // indexed by RRLight::Type
		Program* program;
	};
 
}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
