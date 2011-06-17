// --------------------------------------------------------------------------
// Scene viewer - entity.
// Copyright (C) 2007-2011 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVENTITY_H
#define SVENTITY_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVApp.h"
#include <vector>

namespace rr_gl
{


	/////////////////////////////////////////////////////////////////////////////
	//
	// EntityType - identifies entity type in SceneViewer

	enum EntityType
	{
		ST_FIRST,
		ST_CAMERA,
		ST_LIGHT,
		ST_STATIC_OBJECT,
		ST_DYNAMIC_OBJECT,
		ST_LAST
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// IconCode

	enum IconCode
	{
		IC_POINT = 0,
		IC_SPOT,
		IC_DIRECTIONAL,
		IC_LAST
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

		EntityId()
		{
			type = ST_FIRST;
			index = 0;
		}
		EntityId(EntityType _type, unsigned _index)
		{
			type = _type;
			index = _index;
		}
		bool operator ==(const EntityId& a) const
		{
			return type==a.type && index==a.index;
		}
		bool operator !=(const EntityId& a) const
		{
			return !(a==*this);
		}
		bool isOk()
		{
			return type>ST_FIRST && type<ST_LAST;
		}
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVEntity - entity/icon in SceneViewer
	//
	// entity is not a base class of all classes
	// instead, entity extracts common properties from different classes
	// entities are created from other classes on the fly, for consumers who need them

	struct SVEntity : public EntityId
	{
		rr::RRVec3 position;
		float iconSize;
		IconCode iconCode;
		bool bright;
		bool selected;
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
