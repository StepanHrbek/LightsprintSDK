// --------------------------------------------------------------------------
// Radiosity solver - high level.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <assert.h>
#include <math.h>
#include <memory.h>
#include "rrcore.h"
#include "../RRStaticSolver/RRStaticSolver.h"
#include <stdio.h> // dbg printf

namespace rr
{

#define DBG(a) //a

RRStaticSolver::~RRStaticSolver()
{
	//assert(_CrtIsValidHeapPointer(scene));
	//assert(_CrtIsMemoryBlock(scene,sizeof(Scene),NULL,NULL,NULL));
	delete scene;
}


/////////////////////////////////////////////////////////////////////////////
//
// import geometry

// je dulezite neakceptovat scenu kde neco odrazi vic jak 99%
// 
// 1. kdyz si dva facy se 100% odrazivosti v nektery slozce pinkaj foton tam a zpet
//    a DISTRIB_LEVEL_HIGH vynucuje neustale dalsi pinkani (protoze foton je dost velky),
//    je nutne to poznat a prejit na refresh. ted to znamena zastaveni vypoctu.
// 2. kdyz si facy pinkaj cim dal vic fotonu protoze matrose odrazej vic nez 1,
//    je nutne to poznat a skoncit s vhodnou hlaskou. ted to znamena nesmyslne vysledky.
// 3. i 99.9% odrazivost ma vazne nasledky. ted to mozna zpusobuje nezadouci indirecty ktere bez resetu nezmizej.
//    koupelna: vzajemnym pinkanim pod kouli se naakumuluje mnoho zluteho svetla.
//    kdyz refreshnu, zmizi. bez refreshe se ale likviduje velice obtizne,
//    pouze opetovnym dlouhym pinkanim opacnym smerem.
//    je nutne maximalni odrazivost srazit co nejniz aby tento jev zeslabl, pod 98%?
//
// jak to ale zaridit, kdyz si schovavam pointery na surface a ten muze byt zlym importerem kdykoliv zmenen?
//!!! zatim nedoreseno
// a) udelat si kopie vsech surfacu
//    -sezere trosku pameti a casu
//    +maximalni bezpeci, kazda zmena surfacu za behu je fatalni, i banalni zmena odrazivosti z 0.5 na 0.4 to rozbije
//    +ubyde kontrola fyzikalni legalnosti v rrapi, legalnost zaridi RRMaterial::validate();
// b) behem pouzivani neustale kontrolovat ze neco nepreteklo
//    -sezere moc vykonu
// c) zkontrolovat na zacatku a pak duverovat
//    +ubyde kontrola fyzikalni legalnosti v rrapi, legalnost zaridi RRMaterial::validate();

RRStaticSolver* RRStaticSolver::create(RRObject* _object, const RRDynamicSolver::SmoothingParameters* _smoothing, bool& _aborting)
{
	if(!_object)
	{
		return NULL; // no input
	}
	RRMesh* mesh = _object->getCollider()->getMesh();
	Object *obj = Object::create(mesh->getNumVertices(),mesh->getNumTriangles());
	if(!obj)
	{
		return NULL; // not enough memory (already reported)
	}
	RRStaticSolver* solver = new RRStaticSolver(_object,_smoothing,obj,_aborting); // obj is filled but not yet adopted
	   
	if(!obj->buildTopIVertices(_smoothing->minFeatureSize,_smoothing->maxSmoothAngle,_aborting))
	{
		delete solver;
		delete obj;
		return NULL; // not enough memory (already reported) or abort
	}

	solver->scene->objInsertStatic(obj); // obj is adopted
	return solver;
}

RRStaticSolver::RRStaticSolver(RRObject* importer, const RRDynamicSolver::SmoothingParameters* smoothing, Object* obj, bool& aborting)
{
	scene=new Scene();
	RR_ASSERT(importer);
	RR_ASSERT(obj);
	RRDynamicSolver::SmoothingParameters defaultSmoothing;
	if(!smoothing) smoothing = &defaultSmoothing;
	RRMesh* mesh = importer->getCollider()->getMesh();
	obj->importer = importer;

	// import triangles
	DBG(printf(" triangles...\n"));
	for(unsigned fi=0;fi<obj->triangles;fi++)
	{
		if(aborting) break;
		RRMesh::Triangle tv;
		mesh->getTriangle(fi,tv);
		const RRMaterial* s=importer->getTriangleMaterial(fi,NULL,NULL);
		RR_ASSERT(s);
		Triangle *t = &obj->triangle[fi];
		RRMesh::TriangleBody body;
		mesh->getTriangleBody(fi,body);
		if(!t->setGeometry(body,smoothing->ignoreSmallerAngle,smoothing->ignoreSmallerArea))
		{
			obj->objSourceExitingFlux+=abs(t->setSurface(s,RRVec3(0),true));
		}
		else
		{
			t->surface=NULL; // marks invalid triangles
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

RRStaticSolver::Improvement RRStaticSolver::illuminationReset(bool resetFactors, bool resetPropagation, unsigned* directIrradiancePhysicalRGBA8, RRReal customToPhysical[256], RRVec3* directIrradiancePhysicalRGB)
{
	__frameNumber++;
	return scene->resetStaticIllumination(resetFactors,resetPropagation,directIrradiancePhysicalRGBA8,customToPhysical,directIrradiancePhysicalRGB);
}

RRStaticSolver::Improvement RRStaticSolver::illuminationImprove(bool endfunc(void*), void* context)
{
	__frameNumber++;
	return scene->improveStatic(endfunc, context);
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
	RRVec3 result = RRVec3(0);
	for(unsigned i=0;i<corners;i++)
	{
		unsigned triangleIndex = (unsigned)(getCorner(i).node-triangles);
		RR_ASSERT(triangleIndex<numTriangles);
		RR_ASSERT(getCorner(i).power>0);
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

// Returns triangle measure, any combination of measure.direct/indirect/exiting.
// Works as if measure.scaled=0, measure.flux=0.
RRVec3 Triangle::getMeasure(RRRadiometricMeasure measure) const
{
	RR_ASSERT(surface);

	if(!measure.direct && !measure.indirect)
	{
		return RRVec3(0);
	}
	else
	if(measure.direct && !measure.indirect)
	{
		if(measure.exiting)
		{
			return getDirectExitance();
		}
		else
		{
			return getDirectIrradiance();
		}
	}
	else
	if(measure.direct && measure.indirect) 
	{
		if(measure.exiting)
		{
			return getTotalExitance();
		}
		else
		{
			return getTotalIrradiance();
		}
	}
	else
	{
		if(measure.exiting)
		{
			return getIndirectExitance();
		}
		else
		{
			return getIndirectIrradiance();
		}
	}
}

bool RRStaticSolver::getTriangleMeasure(unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, const RRScaler* scaler, RRVec3& out) const
{
	Channels irrad;
	Object* obj;
	Triangle* tri;

	obj = scene->object;
	if(triangle>=obj->triangles)
	{
		RR_ASSERT(0);
		goto zero;
	}
	tri = &obj->triangle[triangle];

	if(!tri->surface)
	{
		goto zero;
	}

	// enhanced by smoothing
	if(vertex<3 && measure.smoothed)
	{
		// measure direct/indirect (without emissivity)
		irrad = tri->topivertex[vertex]->irradiance(measure);
		// measure exiting
		if(measure.exiting)
		{
			irrad *= tri->surface->diffuseReflectance;
		}
	}
	else
	{
		// measure direct/indirect/exiting (with proper emissivity handling)
		irrad = tri->getMeasure(measure);
	}

	if(measure.scaled)
	{
		if(scaler)
		{
			// scaler applied on density, not flux
			scaler->getCustomScale(irrad);
		}
		else
		{
			// scaling requested but not available
			RR_ASSERT(0);
		}
	}
	if(measure.flux)
	{
		irrad *= tri->area;
	}
	out = irrad;
	STATISTIC_INC(numCallsTriangleMeasureOk);
	return true;
zero:
	out = RRVec3(0);
	STATISTIC_INC(numCallsTriangleMeasureFail);
	return false;
}

unsigned RRStaticSolver::getSubtriangleMeasure(unsigned triangle, RRRadiometricMeasure measure, const RRScaler* scaler, SubtriangleIlluminationEater* callback, void* context) const
{
	return 0;
}

} // namespace
