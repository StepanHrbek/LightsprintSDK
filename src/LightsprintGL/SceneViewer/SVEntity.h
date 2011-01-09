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
		bool bright;

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
			bright = _light.enabled;
		}
	};

	/////////////////////////////////////////////////////////////////////////////
	//
	// SVEntities - all entities in SceneViewer

	class SVEntities : public std::vector<SVEntity>
	{
	public:
		void addLights(const rr::RRLights& lights, rr::RRVec3 dirlightPosition)
		{
			for (unsigned i=0;i<lights.size();i++)
			{
				if (lights[i])
				{
					SVEntity entity(*lights[i],i,dirlightPosition);
					if (lights[i]->name=="Flashlight")
					{
						// makes icon invisible without deleting it
						// we need all light icons present so that they can be indexed like lights in solver
						entity.position.x += 1e10f;
					}
					push_back(entity);
				}
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
