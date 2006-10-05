#ifndef RRILLUMINATIONSPACE_H
#define RRILLUMINATIONSPACE_H

//////////////////////////////////////////////////////////////////////////////
//! \file RRIlluminationSpace.h
//! \brief RRIllumination - library for calculated illumination storage
//! \version 2006.10.5
//! \author Copyright (C) Stepan Hrbek, Lightsprint
//! All rights reserved
//////////////////////////////////////////////////////////////////////////////

#include <cassert>
#include "RRMath.h"

namespace rr
{

class RRIrradiancePointAmbient
{
public:
	RRIrradiancePointAmbient()
	{
		irradiance = RRVec3(0);
	}
	//! Irradiance in point in space in W/m^2.
	RRVec3 irradiance;
};

class RRIrradiancePointSH
{
public:
	RRIrradiancePointSH()
	{
		irradiance = RRVec3(0);
	}
	//! Irradiance in point in space in W/m^2.
	//!!!
	RRVec3 irradiance;
};

//! RRIrradianceGrid is 3d array of size[0]*size[1]*size[2] RRIllumPoints
//! uniformly positioned in -1..1,-1..1,-1..1 space.
//! When size=1, pos=0. When size=2, pos=-1 and 1.
template <class RRIrradiancePoint>
class RRIrradianceGrid
{
public:
	RRIrradianceGrid()
	{
		size[0] = size[1] = size[2] = 0;
		data = NULL;
	}
	void setSize(unsigned size0, unsigned size1, unsigned size2)
	{
		delete[] data;
		size[0] = size0;
		size[1] = size1;
		size[2] = size2;
		data = new RRIrradiancePoint[size[0]*size[1]*size[2]];
	}
	// Returns approximated illumination in point in space.
	// For coordinates outside -1..1 range, nearest known 
	// illumination values (in pos=-1 or 1) are used.
	RRIrradiancePoint getIrradiancePoint(const RRVec3& localPos) const
	{
		// no grid -> return no illumination
		if(!size[0] || !size[1] || !size[2] || !data)
		{
			assert(0);
			return RRIrradiancePoint();
		}
		// position outside grid -> clamp
//!!!		localPos.clamp(RRVec3(-1),RRVec3(1));
		// for each axis find nearest coord
		unsigned nearestCoord[3];
		for(unsigned axis=0;axis<3;axis++)
		{
			if(size[0]==1)
			{
				nearestCoord[axis] = 0;
			}
			else
			{
				float a = (size[axis]-1)/2.0f;
				float b = a*(1+1.0f/size[axis]);
				float gridCoord = a*localPos[axis]+b;
				nearestCoord[axis] = (unsigned)gridCoord;
			}
			assert(nearestCoord[axis]<size[axis]);
		}
		// return point
		return getData(nearestCoord[0],nearestCoord[1],nearestCoord[2]);
	}
	~RRIrradianceGrid()
	{
		delete[] data;
	}
protected:
	RRIrradiancePoint& getData(unsigned pos0, unsigned pos1, unsigned pos2) const
	{
		return data[pos0+size[0]*pos1+size[0]*size[1]*pos2];
	}
	unsigned size[3];
	RRIrradiancePoint* data;
};

//! RRIrradianceGridTransformed is RRIrradianceGrid in local space,
//! with worldMatrix that transforms from local space to world space.
template <class RRIrradiancePoint>
class RRIrradianceGridTransformed : public RRIrradianceGrid<RRIrradiancePoint>
{
public:
	void setTransformation(RRMatrix3x4* aworldMatrix, RRMatrix3x4* aworldMatrixInv)
	{
		worldMatrix = *aworldMatrix;
		worldMatrixInv = *aworldMatrixInv;
	}
	RRIrradiancePoint getIrradiancePoint(const RRVec3& worldPos) const
	{
		return RRIrradianceGrid<RRIrradiancePoint>::getIrradiancePoint(worldMatrixInv.transformedPosition(worldPos));
	}
protected:
	RRMatrix3x4 worldMatrix;
	RRMatrix3x4 worldMatrixInv;
};

} // namespace

#endif
