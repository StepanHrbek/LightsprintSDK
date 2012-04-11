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
#include "Lightsprint/GL/TextureRenderer.h"
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
		void addXYZ(rr::RRVec3 center, IconCode transformation, const rr::RRCamera& eye);
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVEntityIcons - can render and intersect SVEntities

	class SVEntityIcons
	{
	public:
		SVEntityIcons(const char* pathToMaps, UberProgram* uberProgram);
		bool isOk() const;
		void renderIcons(const SVEntities& entities, TextureRenderer* textureRenderer, const rr::RRCamera& eye, const rr::RRCollider* supercollider, rr::RRRay* ray);
		const SVEntity* intersectIcons(const SVEntities& entities, rr::RRVec2 mousePositionInWindow);
		~SVEntityIcons();

	private:
		rr::RRBuffer* icon[IC_LAST]; // indexed by IconCode
		Program* programArrows;
		std::vector<std::pair<const SVEntity*,rr::RRVec4> > piwIconRectangles; // piw=positionInWindow in -1..1 range, filled by renderIcons(), read by intersectIcons()
	};
 
}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
