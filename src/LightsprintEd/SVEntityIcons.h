// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - light icons.
// --------------------------------------------------------------------------

#ifndef SVENTITYICONS_H
#define SVENTITYICONS_H

#include "Lightsprint/Ed/Ed.h"
#include "Lightsprint/RRLight.h"
#include "Lightsprint/RRCollider.h"
#include "Lightsprint/GL/UberProgram.h"
#include "Lightsprint/GL/UberProgramSetup.h"
#include "Lightsprint/GL/TextureRenderer.h"
#include "SVEntity.h"
#include <vector>

namespace rr_ed
{

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVEntities - collection of entites/icons

	class SVEntities : public std::vector<SVEntity>
	{
	public:
		float iconSize; // size in meters

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
		SVEntityIcons(wxString pathToMaps, rr_gl::UberProgram* uberProgram);
		bool isOk() const;
		void renderIcons(const SVEntities& entities, rr_gl::TextureRenderer* textureRenderer, const rr::RRCamera& eye, const rr::RRCollider* supercollider, rr::RRCollisionHandler* collisionHandler, const SceneViewerStateEx& svs);
		const SVEntity* intersectIcons(const SVEntities& entities, rr::RRVec2 mousePositionInWindow);
		~SVEntityIcons();

	private:
		rr::RRBuffer* icon[IC_LAST]; // indexed by IconCode
		rr_gl::UberProgramSetup uberProgramSetup;
		rr_gl::Program* programArrows;
		std::vector<std::pair<const SVEntity*,rr::RRVec4> > piwIconRectangles; // piw=positionInWindow in -1..1 range, filled by renderIcons(), read by intersectIcons()
	};
 
}; // namespace

#endif
