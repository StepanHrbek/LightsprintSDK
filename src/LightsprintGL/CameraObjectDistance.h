// --------------------------------------------------------------------------
// Camera-object distance measuring helper.
// Copyright (C) 2007-2010 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#include "Lightsprint/GL/Camera.h"

namespace rr_gl
{

////////////////////////////////////////////////////////////////////////
//
// CameraObjectDistance

class CameraObjectDistance
{
public:
	CameraObjectDistance(const rr::RRObject* object, bool water=false, float waterLevel=-1e10f);
	~CameraObjectDistance();

	void addRay(const rr::RRVec3& pos, rr::RRVec3 dir);
	void addPoint(const rr::RRVec3& pos);
	void addCamera(Camera* camera);

	float getDistanceMin() const
	{
		return distMin;
	}
	float getDistanceMax() const
	{
		return distMax;
	}

private:
	const rr::RRObject* object;
	bool water;
	float waterLevel;
	float distMin;
	float distMax;
	rr::RRRay* ray;
};

}; // namespace

