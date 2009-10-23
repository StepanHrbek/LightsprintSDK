// --------------------------------------------------------------------------
// Scene viewer - entity.
// Copyright (C) 2007-2009 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVENTITY_H
#define SVENTITY_H

#include "Lightsprint/GL/SceneViewer.h"

#ifdef SUPPORT_SCENEVIEWER

#include "SVApp.h"

namespace rr_gl
{


	/////////////////////////////////////////////////////////////////////////////
	//
	// EntityType - identifies entity type in SceneViewer

	enum EntityType
	{
		ST_CAMERA,
		ST_LIGHT,
		ST_OBJECT,
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// EntityId - identifies entity in SceneViewer
	//
	// to be removed, replaced by SVEntity

	struct EntityId
	{
		EntityType type;
		unsigned index;

		EntityId(EntityType _type, unsigned _index)
		{
			type = _type;
			index = _index;
		}
		bool operator ==(const EntityId& a)
		{
			return type==a.type && index==a.index;
		}
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVEntity - entity in SceneViewer
	//
	// entity is not a base class of all classes
	// instead, entity extracts common properties from different classes
	// entities are created from other classes on the fly, for consumers who need them

	struct SVEntity
	{
		EntityType type;
		unsigned index;
		rr::RRVec3 position;
		unsigned icon;

		SVEntity(const rr::RRLight& _light, unsigned _index, rr::RRVec3& _dirlightPosition)
		{
			type = ST_LIGHT;
			index = _index;
			if (_light.type==rr::RRLight::DIRECTIONAL)
			{
				position = _dirlightPosition;
				_dirlightPosition.y += 1;
			}
			else
			{
				position = _light.position;
			}
			icon = _light.type;
		}
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVEntities - all entities in SceneViewer

	class SVEntities : public std::vector<SVEntity>
	{
	public:
		SVEntities(const rr::RRLights& lights, rr::RRVec3 dirlightPosition, const SceneViewerStateEx& svs)
		{
			for (unsigned i=0;i<lights.size();i++)
			{
				push_back(SVEntity(*lights[i],i,dirlightPosition));
			}
		}
	};

	enum SelectEntityAction
	{
		SEA_SELECT,
		SEA_ACTION,
		SEA_ACTION_IF_ALREADY_SELECTED,
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
