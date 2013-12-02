// --------------------------------------------------------------------------
// Scene viewer - entity.
// Copyright (C) 2007-2013 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#ifndef SVENTITY_H
#define SVENTITY_H

#ifdef SUPPORT_SCENEVIEWER

#include "Lightsprint/GL/SceneViewer.h"
#include "SVApp.h"
#include <set>

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
		ST_OBJECT,
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
		IC_MOVEMENT,
		IC_ROTATION,
		IC_SCALE,
		IC_STATIC,
		IC_X,
		IC_Y,
		IC_Z,
		IC_LAST
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// EntityId - identifies entity in SceneViewer

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
		bool operator <(const EntityId& a) const
		{
			return type<a.type || (type==a.type && index<a.index);
		}
		bool isOk()
		{
			return type>ST_FIRST && type<ST_LAST;
		}
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// EntityIds

	class EntityIds : public std::set<EntityId>
	{
	public:
		EntityIds() {}
		EntityIds(EntityId e) {insert(e);}
		// why not put RRVec3 center here?
		// because we would have to recalculate it each time something moves
		// it's easier if all who need center calculate it themselves
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
		rr::RRVec3 parentPosition; // if different from position, arrow is rendered from parentPosition
		float iconSize;
		IconCode iconCode;
		bool bright;
		bool selected;
	};

	enum SelectEntityAction
	{
		SEA_NOTHING, // SVFrame::selectEntityInTreeAndUpdatePanel() does not change selection in tree, while still updating panels
		SEA_SELECT,
		SEA_ACTION,
	};

}; // namespace

#endif // SUPPORT_SCENEVIEWER

#endif
