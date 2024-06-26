// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Radiosity solver - high level.
// --------------------------------------------------------------------------

#include <cmath>
#include <cstdio> // dbg printf
#include "rrcore.h"
#include "../RRStaticSolver/RRStaticSolver.h"

namespace rr
{

#define DBG(a) //a

RRStaticSolver::~RRStaticSolver()
{
	//RR_ASSERT(_CrtIsValidHeapPointer(scene));
	//RR_ASSERT(_CrtIsMemoryBlock(scene,sizeof(Scene),nullptr,nullptr,nullptr));
	delete scene;
}


/////////////////////////////////////////////////////////////////////////////
//
// import geometry


RRStaticSolver* RRStaticSolver::create(RRObject* _object, const RRSolver::SmoothingParameters* _smoothing, bool& _aborting)
{
	if (!_object)
	{
		return nullptr; // no input
	}
	const RRMesh* mesh = _object->getCollider()->getMesh();
	Object *obj = Object::create(mesh->getNumVertices(),mesh->getNumTriangles());
	if (!obj)
	{
		return nullptr; // not enough memory (already reported)
	}
	RRStaticSolver* solver = new RRStaticSolver(_object,_smoothing,obj,_aborting); // obj is filled but not yet adopted
	   
	if (!obj->buildTopIVertices(_smoothing,_aborting))
	{
		delete solver;
		delete obj;
		return nullptr; // not enough memory (already reported) or abort
	}

	solver->scene->objInsertStatic(obj); // obj is adopted
	return solver;
}

RRStaticSolver::RRStaticSolver(RRObject* importer, const RRSolver::SmoothingParameters* smoothing, Object* obj, bool& aborting)
{
	materialEmittanceMultiplier = 1;
	scene=new Scene();
	RR_ASSERT(importer);
	RR_ASSERT(obj);
	RRSolver::SmoothingParameters defaultSmoothing;
	if (!smoothing) smoothing = &defaultSmoothing;
	const RRMesh* mesh = importer->getCollider()->getMesh();
	obj->importer = importer;

	// import triangles
	DBG(printf(" triangles...\n"));
	for (unsigned fi=0;fi<obj->triangles;fi++)
	{
		if (aborting) break;
		RRMesh::Triangle tv;
		mesh->getTriangle(fi,tv);
		const RRMaterial* s=importer->getTriangleMaterial(fi,nullptr,nullptr);
		if (!s) RR_LIMITED_TIMES(1,RRReporter::report(WARN,"At least one triangle has nullptr material -> expect crash.\n"));
		Triangle *t = &obj->triangle[fi];
		RRMesh::TriangleBody body;
		mesh->getTriangleBody(fi,body);
		if (!t->setGeometry(body,smoothing->ignoreSmallerAngle,smoothing->ignoreSmallerArea))
		{
			obj->objSourceExitingFlux+=abs(t->setSurface(s,RRVec3(0),true,materialEmittanceMultiplier));
		}
		else
		{
			t->surface=nullptr; // marks invalid triangles
			t->area=0; // just to have consistency through all invalid triangles
		}
		// initialize isLod0
		RRObject::LodInfo lodInfo;
		importer->getTriangleLod(fi,lodInfo);
		t->isLod0 = (lodInfo.level==0)?1:0;
	}
}


/////////////////////////////////////////////////////////////////////////////
//
// calculate radiosity

RRStaticSolver::Improvement RRStaticSolver::illuminationReset(bool resetFactors, bool resetPropagation, RRReal emissiveMultiplier, const unsigned* directIrradianceCustomRGBA8, const RRReal customToPhysical[256], const RRVec3* directIrradiancePhysicalRGB)
{
	__frameNumber++;
	materialEmittanceMultiplier = emissiveMultiplier;
	return scene->resetStaticIllumination(resetFactors,resetPropagation,emissiveMultiplier,directIrradianceCustomRGBA8,customToPhysical,directIrradiancePhysicalRGB);
}

RRStaticSolver::Improvement RRStaticSolver::illuminationImprove(EndFunc& endfunc)
{
	__frameNumber++;
	return scene->improveStatic(endfunc);
}

RRReal RRStaticSolver::illuminationAccuracy()
{
	return scene->avgAccuracy();
}

//RRReal RRStaticSolver::illuminationConvergence()
//{
//	return scene->getConvergence();
//}


/////////////////////////////////////////////////////////////////////////////
//
// read results

RRVec3 IVertex::getVertexDataFromTriangleData(unsigned questionedTriangle, unsigned questionedVertex012, const RRVec3* perTriangleData, unsigned stride, Triangle* triangles, unsigned numTriangles) const
{
	// prevent NaN (triangle with 1 corner with power=0 gets here in MovingSun+kalasatama.dae)
	if (!powerTopLevel) return RRVec3(0);

	RRVec3 result(0);
	for (unsigned i=0;i<corners;i++)
	{
		unsigned triangleIndex = (unsigned)(getCorner(i).node-triangles);
		RR_ASSERT(triangleIndex<numTriangles);
		RR_ASSERT(getCorner(i).power>=0); // power 0 is not nice, but for simplicity we accept it too
		//RR_ASSERT((*(RRVec3*)(((char*)perTriangleData)+stride*triangleIndex)).avg()>=0); bent normals may be negative
		result += *(RRVec3*)(((char*)perTriangleData)+stride*triangleIndex) * getCorner(i).power;
	}
	return result/powerTopLevel;
}

// per-triangle -> per-vertex smoothing
// works with raw data, no scaling
RRVec3 RRStaticSolver::getVertexDataFromTriangleData(unsigned questionedTriangle, unsigned questionedVertex012, const RRVec3* perTriangleData, unsigned stride) const
{
	RR_ASSERT(perTriangleData);
	RR_ASSERT(questionedVertex012<3);
	RR_ASSERT(questionedTriangle<scene->object->triangles);
	IVertex* ivertex = scene->object->triangle[questionedTriangle].topivertex[questionedVertex012];
	RR_ASSERT(ivertex);
	return ivertex->getVertexDataFromTriangleData(questionedTriangle,questionedVertex012,perTriangleData,stride,scene->object->triangle,scene->object->triangles);
}

bool RRStaticSolver::getTriangleMeasure(unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, const RRColorSpace* colorSpace, RRVec3& out) const
{
	Channels irrad;
	Object* obj;
	Triangle* tri;

	obj = scene->object;
	if (triangle>=obj->triangles)
	{
		RR_ASSERT(0);
		goto zero;
	}
	tri = &obj->triangle[triangle];

	if (!tri->surface)
	{
		goto zero;
	}

	// enhanced by smoothing
	if (vertex<3 && measure.smoothed)
	{
		// measure direct/indirect (without emissivity)
		irrad = tri->topivertex[vertex]->irradiance(measure);
		// measure exiting
		if (measure.exiting)
		{
			irrad = irrad * tri->surface->diffuseReflectance.colorLinear + tri->surface->diffuseEmittance.colorLinear*materialEmittanceMultiplier;
		}
	}
	else
	{
		// measure direct/indirect/exiting (with proper emissivity handling)
		irrad = tri->getMeasure(measure,materialEmittanceMultiplier);
	}

	if (measure.scaled)
	{
		if (colorSpace)
		{
			// colorSpace applied on density, not flux
			colorSpace->fromLinear(irrad);
		}
		else
		{
			// scaling requested but not available
			RR_ASSERT(0);
		}
	}
	if (measure.flux)
	{
		irrad *= tri->area;
	}
	out = irrad;
	return true;
zero:
	out = RRVec3(0);
	return false;
}

} // namespace
