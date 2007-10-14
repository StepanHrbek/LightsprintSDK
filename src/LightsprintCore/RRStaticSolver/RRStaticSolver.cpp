#include <assert.h>
#include <math.h>
#include <memory.h>
#include "rrcore.h"
#include "clusters.h"
#include "Lightsprint/RRStaticSolver.h"
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
   
RRStaticSolver::RRStaticSolver(RRObject* importer, const SmoothingParameters* smoothing)
{
	scene=new Scene();
	RR_ASSERT(importer);
	if(!importer) return;
	SmoothingParameters defaultSmoothing;
	if(!smoothing) smoothing = &defaultSmoothing;
	RRMesh* meshImporter = importer->getCollider()->getMesh();
	Object *obj=new Object(meshImporter->getNumVertices(),meshImporter->getNumTriangles());
	obj->subdivisionSpeed = smoothing->subdivisionSpeed;
	obj->importer = importer;

	// import vertices
	RR_ASSERT(sizeof(RRMesh::Vertex)==sizeof(Vec3));
	for(unsigned i=0;i<obj->vertices;i++) 
		meshImporter->getVertex(i,*(RRMesh::Vertex*)&obj->vertex[i]);

	// import triangles
	// vzdy vklada/cisluje od nuly nahoru
	DBG(printf(" triangles...\n"));
	int tbot=0;
	for(unsigned fi=0;fi<obj->triangles;fi++) 
	{
		RRMesh::Triangle tv;
		meshImporter->getTriangle(fi,tv);
		const RRMaterial* s=importer->getTriangleMaterial(fi);
		RR_ASSERT(s);
		Triangle *t = &obj->triangle[tbot++];
		RR_ASSERT(t>=obj->triangle && t<&obj->triangle[obj->triangles]);
		// vlozi ho, seridi geometrii atd
		int geom=t->setGeometry(
			&obj->vertex[tv[0]],
			&obj->vertex[tv[1]],
			&obj->vertex[tv[2]],
			NULL,NULL,smoothing->ignoreSmallerAngle,smoothing->ignoreSmallerArea
			);
		if(t->isValid) 
		{
			// this code is on 2 places...
			//  delete this and rather call obj->resetStaticIllumination
			Vec3 additionalIrradiance;
			importer->getTriangleIllumination(fi,RM_IRRADIANCE_PHYSICAL,additionalIrradiance);
			obj->objSourceExitingFlux+=abs(t->setSurface(s,additionalIrradiance,true));
		}
		else
		{
			t->surface=NULL; // marks invalid triangles
			t->area=0; // just to have consistency through all invalid triangles
		}
	}
	   
	// preprocessuje objekt
	DBG(printf(" bounds...\n"));
	{
		DBG(printf(" edges...\n"));
		obj->buildEdges(smoothing->maxSmoothAngle); // build edges only for clusters and/or interpol
	}
	DBG(printf(" ivertices...\n"));
	obj->buildTopIVertices(smoothing->smoothMode,smoothing->minFeatureSize,smoothing->maxSmoothAngle);
	// vlozi objekt do sceny
	scene->objInsertStatic(obj);
}


/////////////////////////////////////////////////////////////////////////////
//
// calculate radiosity

RRStaticSolver::Improvement RRStaticSolver::illuminationReset(bool resetFactors, bool resetPropagation)
{
	if(!licenseStatusValid || licenseStatus!=RRLicense::VALID) return FINISHED;
	__frameNumber++;
	return scene->resetStaticIllumination(resetFactors,resetPropagation);
}

RRStaticSolver::Improvement RRStaticSolver::illuminationImprove(bool endfunc(void*), void* context)
{
	if(!licenseStatusValid || licenseStatus!=RRLicense::VALID) return FINISHED;
	__frameNumber++;
	return scene->improveStatic(endfunc, context);
}

RRReal RRStaticSolver::illuminationAccuracy()
{
	return scene->avgAccuracy()/100;
}


/////////////////////////////////////////////////////////////////////////////
//
// read results

RRVec3 IVertex::getVertexDataFromTriangleData(unsigned questionedTriangle, unsigned questionedVertex012, const RRVec3* perTriangleData, unsigned stride, Triangle* triangles, unsigned numTriangles) const
{
	RRVec3 result = RRVec3(0);
	for(unsigned i=0;i<corners;i++)
	{
		if(IS_TRIANGLE(corner[i].node))
		{
			unsigned triangleIndex = (unsigned)(TRIANGLE(corner[i].node)-triangles);
			RR_ASSERT(triangleIndex<numTriangles);
			result += *(RRVec3*)(((char*)perTriangleData)+stride*triangleIndex) * corner[i].power;
		}
	}
	return result/powerTopLevel;
}

RRVec3 RRStaticSolver::getVertexDataFromTriangleData(unsigned questionedTriangle, unsigned questionedVertex012, const RRVec3* perTriangleData, unsigned stride) const
{
	RR_ASSERT(perTriangleData);
	RR_ASSERT(questionedVertex012<3);
	RR_ASSERT(questionedTriangle<scene->object->triangles);
	IVertex* ivertex = scene->object->triangle[questionedTriangle].topivertex[questionedVertex012];
	return ivertex->getVertexDataFromTriangleData(questionedTriangle,questionedVertex012,perTriangleData,stride,scene->object->triangle,scene->object->triangles);
}

bool RRStaticSolver::getTriangleMeasure(unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, const RRScaler* scaler, RRColor& out) const
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
	if(!licenseStatusValid || licenseStatus!=RRLicense::VALID)
	{
		//RR_ASSERT(0);
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
		irrad = tri->topivertex[vertex]->irradiance(measure);
	}
	else

	// basic, fast
	{
		if(!measure.direct && !measure.indirect) 
		{
			irrad = Channels(0);
		}
		else
		if(measure.direct && !measure.indirect) 
		{
			irrad = tri->getSourceIrradiance();
		}
		else
		if(measure.direct && measure.indirect) 
		{
			irrad = tri->totalIncidentFlux / tri->area;
			// Zde mohl po chvili vypoctu vyplout zaporny vysledek.
			// Neni to hezke, ale zaporne hodnoty muze dodat i klient, takze je tolerujme.
			// Chyba vznika po chvili vypoctu fcss, ale bez pohybu=resetu, porad se jen zlepsuje,
			// je chyba nutna, neslo by to numericky zestabilnit?
		}
		else
		{
			irrad = (tri->totalIncidentFlux - tri->getSourceIncidentFlux()) / tri->area;
			// Zde mohl zaokrouhlovaci chybou vzniknout zaporny vysledek.
			// Neni to hezke, ale zaporne hodnoty muze dodat i klient, takze je tolerujme.
			// Chyba vznika podezrele snadno, prakticky okamzite na zacatku vypoctu,
			// neslo by to numericky zestabilnit?
		}
	}

	if(measure.exiting)
	{
		// diffuse applied on physical scale, not custom scale
		irrad *= tri->surface->diffuseReflectance;
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
	out = RRColor(0);
	STATISTIC_INC(numCallsTriangleMeasureFail);
	return false;
}

unsigned SubTriangle::enumSubtriangles(IVertex **iv, Channels flatambient, RRReal subarea, EnumSubtrianglesCallback* callback, void* context)
{
	if(sub[0])
	{
		IVertex *iv0[3];
		IVertex *iv1[3];
		bool rightleft=isRightLeft();
		int rot=getSplitVertex();
		RR_ASSERT(subvertex);
		RR_ASSERT(subvertex->check());
		iv0[0]=iv[rot];
		iv0[1]=iv[(rot+1)%3];
		iv0[2]=subvertex;
		iv1[0]=iv[rot];
		iv1[1]=subvertex;
		iv1[2]=iv[(rot+2)%3];
		flatambient+=(totalExitingFlux-sub[0]->totalExitingFlux-sub[1]->totalExitingFlux)/area;
		return SUBTRIANGLE(sub[rightleft?0:1])->enumSubtriangles(iv0,flatambient,subarea/2,callback,context)+
			SUBTRIANGLE(sub[rightleft?1:0])->enumSubtriangles(iv1,flatambient,subarea/2,callback,context);
	}
	if(callback)
	{
		callback(this,iv,flatambient,subarea,context);
	}
	return 1;
}

unsigned Triangle::enumSubtriangles(EnumSubtrianglesCallback* callback, void* context)
{
	return SubTriangle::enumSubtriangles(topivertex,Channels(0),area,callback,context);
}

struct SubtriangleIlluminationContext
{
	SubtriangleIlluminationContext(RRRadiometricMeasure ameasure) : measure(ameasure) {};
	RRRadiometricMeasure                   measure;
	const RRScaler*                        scaler;
	RRStaticSolver::SubtriangleIlluminationEater* clientCallback;
	void*                                  clientContext;
};

void buildSubtriangleIllumination(SubTriangle* s, IVertex **iv, Channels flatambient, RRReal subarea, void* context)
{
	SubtriangleIlluminationContext* context2 = (SubtriangleIlluminationContext*)context;
	RRStaticSolver::SubtriangleIllumination si;
	for(unsigned i=0;i<3;i++)
	{
		//!!! je zde chyba ktera se projevi jen pri nekterych typech subdivision
		// fill 2d coords in triangle space
		//  start with uv[] with ortonormal base
		si.texCoord[i] = s->uv[i];
		//  and transform to triangle space
		//  x , y -> x/u2.x-y*v2.x/u2.x/v2.y , y/v2.y
		//  u2,v2 jsou brane z Triangle
		si.texCoord[i] = Vec2(
			si.texCoord[i][0]/s->grandpa->u2[0]-si.texCoord[i][1]*s->grandpa->v2[0]/s->grandpa->u2[0]/s->grandpa->v2[1],
			si.texCoord[i][1]/s->grandpa->v2[1]);
		if(si.texCoord[i][0]<0)
		{
			if(si.texCoord[i][0]<-0.1) {DBG(printf("a%f ",si.texCoord[i][0]));RR_ASSERT(0);}
			si.texCoord[i][0] = 0;
		} else
		if(si.texCoord[i][0]>1)
		{
			if(si.texCoord[i][0]>1.1) {DBG(printf("b%f ",si.texCoord[i][0]));RR_ASSERT(0);}
			si.texCoord[i][0] = 1;
		}
		if(si.texCoord[i][1]<0)
		{
			if(si.texCoord[i][1]<-0.1) {DBG(printf("c%f ",si.texCoord[i][1]));RR_ASSERT(0);}
			si.texCoord[i][1] = 0;
		} else
		if(si.texCoord[i][1]>1)
		{
			if(si.texCoord[i][1]>1.1) {DBG(printf("d%f ",si.texCoord[i][1]));RR_ASSERT(0);}
			si.texCoord[i][1] = 1;
		}
		if(!context2 || licenseStatus!=RRLicense::VALID)
		{
			RR_ASSERT(0);
			return;
		}
		// fill irradiance
		if(context2->measure.smoothed)
			si.measure[i] = iv[i]->irradiance(context2->measure);
		else
			si.measure[i] = flatambient+s->totalExitingFlux/s->area; //!!! ignoruje nastaveni direct/indirect. bere oboje
		// convert irradiance to measure
		if(context2->measure.exiting)
		{
			// diffuse applied on physical scale, not custom scale
			si.measure[i] *= s->grandpa->surface->diffuseReflectance;
		}
		if(context2->measure.scaled && context2->scaler)
		{
			// scaler applied on density, not flux
			context2->scaler->getCustomScale(si.measure[i]);
		}
		if(context2->measure.flux)
		{
			si.measure[i] *= subarea;
		}
		STATISTIC_INC(numCallsTriangleMeasureOk);
	}
	context2->clientCallback(si,context2->clientContext);
}

unsigned RRStaticSolver::getSubtriangleMeasure(unsigned triangle, RRRadiometricMeasure measure, const RRScaler* scaler, SubtriangleIlluminationEater* callback, void* context) const
{
	Object* obj;
	Triangle* tri;

	obj = scene->object;
	if(triangle>=obj->triangles)
	{
		RR_ASSERT(0);
		return 0;
	}
	tri = &obj->triangle[triangle];
	if(!tri->surface)
	{
		SubtriangleIllumination si;
		si.texCoord[0] = Vec2(0,0);
		si.texCoord[1] = Vec2(1,0);
		si.texCoord[2] = Vec2(0,1);
		si.measure[0] = Vec3(0);
		si.measure[1] = Vec3(0);
		si.measure[2] = Vec3(0);
		if(callback) callback(si,context);
		return 1;
	}
	if(!licenseStatusValid)
	{
		RR_ASSERT(0);
		return 0;
	}

	SubtriangleIlluminationContext sic(measure);
	sic.measure = measure;
	sic.scaler = scaler;
	sic.clientCallback = callback;
	sic.clientContext = context;
	return tri->enumSubtriangles(buildSubtriangleIllumination,&sic);
}



/////////////////////////////////////////////////////////////////////////////
//
// misc settings


} // namespace
