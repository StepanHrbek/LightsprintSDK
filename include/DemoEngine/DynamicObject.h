// --------------------------------------------------------------------------
// DemoEngine
// DynamicObject, 3d object with dynamic global illumination.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef DYNAMICOBJECT_H
#define DYNAMICOBJECT_H

#include "Model_3DS.h"
#include "RRIlluminationEnvironmentMapInOpenGL.h"


//////////////////////////////////////////////////////////////////////////////
//
// Dynamic object

class DE_API DynamicObject
{
public:
	static DynamicObject* create(const char* filename,float scale)
	{
		DynamicObject* d = new DynamicObject();
		if(d->model.Load(filename,scale) && d->getModel().numObjects)
		{
			Model_3DS::Vector center = d->model.GetCenter();
			d->localCenter = rr::RRVec3(center.x,center.y,center.z);
			return d;
		}
		if(!d->getModel().numObjects) printf("Model %s contains no objects.",filename);
		delete d;
		return NULL;
	}
	const Model_3DS& getModel()
	{
		return model;
	}
	const rr::RRVec3& getLocalCenter()
	{
		return localCenter;
	}
	rr::RRIlluminationEnvironmentMap* getSpecularMap()
	{
		return &specularMap;
	}
	rr::RRIlluminationEnvironmentMap* getDiffuseMap()
	{
		return &diffuseMap;
	}
private:
	DynamicObject() {}
	Model_3DS model;
	rr::RRVec3 localCenter;
	rr::RRIlluminationEnvironmentMapInOpenGL specularMap;
	rr::RRIlluminationEnvironmentMapInOpenGL diffuseMap;
};

#endif
