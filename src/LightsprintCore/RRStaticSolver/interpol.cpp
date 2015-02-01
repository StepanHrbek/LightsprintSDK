// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Smoothing of per-triangle illumination to per-vertex.
// --------------------------------------------------------------------------


#include "rrcore.h"
#include "interpol.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace rr
{

IVertex::IVertex()
{
	RR_ASSERT(this);
	corners=0;
	cornersAllocatedLn2=STATIC_CORNERS_LN2;
	dynamicCorner=NULL;
	powerTopLevel=0;
	cacheTime=__frameNumber-1;
	cacheValid=0;
}

unsigned IVertex::cornersAllocated()
{
	RR_ASSERT(cornersAllocatedLn2<14);
	return 1<<cornersAllocatedLn2;
}

const Corner& IVertex::getCorner(unsigned c) const
{
	RR_ASSERT(c<corners);
	return (c<STATIC_CORNERS)?staticCorner[c]:dynamicCorner[c-STATIC_CORNERS];
}

Corner& IVertex::getCorner(unsigned c)
{
	//!!! find out why this asserts in fcss debug
	//RR_ASSERT(c<corners);
	return (c<STATIC_CORNERS)?staticCorner[c]:dynamicCorner[c-STATIC_CORNERS];
}

void IVertex::insert(Triangle* node,bool toplevel,real power)
{
	RR_ASSERT(this);
	power *= node->area;
	if (corners==cornersAllocated())
	{
		size_t oldsize=(cornersAllocated()-STATIC_CORNERS)*sizeof(Corner);
		cornersAllocatedLn2++;
		//cornersAllocatedLn2+=2;
		dynamicCorner=(Corner *)realloc(dynamicCorner,oldsize,(cornersAllocated()-STATIC_CORNERS)*sizeof(Corner));
	}
	for (unsigned i=0;i<corners;i++)
		if (getCorner(i).node==node)
		{
			getCorner(i).power+=power;
			goto label;
		}
	corners++;
	getCorner(corners-1).node=node;
	getCorner(corners-1).power=power;
label:
	if (toplevel) powerTopLevel+=power;
}

// irradiance in W/m^2, incident power density
// is equal in whole vertex, doesn't depend on corner material
// only measure.direct/indirect is used
Channels IVertex::irradiance(RRRadiometricMeasure measure)
{
	if (cacheTime!=(__frameNumber&0x1f) || !cacheValid || cacheDirect!=measure.direct || cacheIndirect!=measure.indirect) // cacheTime is byte:5
	{
		//RR_ASSERT(powerTopLevel);
		// irrad=irradiance in W/m^2
		Channels irrad=Channels(0);

		// full path
		for (unsigned i=0;i<corners;i++)
		{
			Triangle* node=getCorner(i).node;
			RR_ASSERT(node);
			// a=source+reflected incident flux in watts
			Channels a=node->totalIncidentFlux;
			RR_ASSERT(IS_CHANNELS(a));
			// s=source incident flux in watts
			Channels s=node->getDirectIncidentFlux();
			RR_ASSERT(IS_CHANNELS(s));
			// r=reflected incident flux in watts
			Channels r=a-s;
			RR_ASSERT(IS_CHANNELS(r));
			// w=wanted incident flux in watts
			Channels w=(measure.direct&&measure.indirect)?a:( measure.direct?s: ( measure.indirect?r:Channels(0) ) );
			irrad+=w*(getCorner(i).power/node->area);
			RR_ASSERT(IS_CHANNELS(irrad));
		}

		cache=powerTopLevel?irrad/powerTopLevel:Channels(0);
		clampToZero(cache); //!!! obcas tu vznikaji zaporny hodnoty coz je zcela nepripustny!
		cacheTime=__frameNumber;
		cacheDirect=measure.direct;
		cacheIndirect=measure.indirect;
		cacheValid=1;
	}

	RR_ASSERT(IS_CHANNELS(cache));
	return cache;
}

IVertex::~IVertex()
{
	cornersAllocatedLn2++;
	cornersAllocatedLn2--;
	free(dynamicCorner);
}

bool Object::buildTopIVertices(const RRSolver::SmoothingParameters* smoothing, bool& aborting)
{
	bool outOfMemory = false;

	// check
	for (unsigned t=0;t<triangles;t++)
	{
		for (int v=0;v<3;v++)
			RR_ASSERT(!triangle[t].topivertex[v]);
	}

	// temporarily stitch vertices
	// it's kind of smoothing, stitched vertices share one ivertex, one irradiance
	// we don't want to stitch multiobject, it would require !indexed render (complicated to ensure, slower)
	const RRMesh* originalMesh = importer->getCollider()->getMesh();
	const RRMesh* stitchedMesh = originalMesh->createOptimizedVertices(smoothing->vertexWeldDistance,fabs(smoothing->maxSmoothAngle),0,NULL);
	unsigned stitchedVertices = stitchedMesh->getNumVertices();

	// build 1 ivertex for each stitched vertex, insert all corners
	topivertexArray = new IVertex[stitchedVertices];
	for (unsigned t=0;t<triangles;t++) if (triangle[t].surface)
	{
		if (aborting) break;
		RRMesh::Triangle un_ve;
		stitchedMesh->getTriangle(t,un_ve);
		if (!(un_ve[0]==un_ve[1]||un_ve[0]==un_ve[2]||un_ve[1]==un_ve[2])) // skip triangles degenerated by stitching
		{
			RRMesh::Vertex vertex[3];
			stitchedMesh->getVertex(un_ve[0],vertex[0]);
			stitchedMesh->getVertex(un_ve[1],vertex[1]);
			stitchedMesh->getVertex(un_ve[2],vertex[2]);
			for (int ro_v=0;ro_v<3;ro_v++)
			{
				unsigned un_v = un_ve[ro_v];
				RR_ASSERT(un_v<stitchedVertices);
				triangle[t].topivertex[ro_v]=&topivertexArray[un_v];
				Angle angle=angleBetween(vertex[(ro_v+1)%3]-vertex[ro_v],vertex[(ro_v+2)%3]-vertex[ro_v]);
				topivertexArray[un_v].insert(&triangle[t],true,angle);
			}
		}
	}

	// cleanup
	if (stitchedMesh!=originalMesh)
		delete stitchedMesh;

	return !outOfMemory && !aborting;
}

} // namespace


