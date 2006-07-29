#include <assert.h>
#include <math.h>
#include <memory.h>
#include "rrcore.h"
#include "RRVision.h"

namespace rr
{

#define DBG(a) //a
#define scene ((Scene*)_scene)

/*
1-sided vs 2-sided surface

Expected behaviour:

 Exactly as mgfdoc specifies.

 What does mgfdoc say:

 The sides entity is used to set the number of sides for the current material.
 If a surface is two-sided, then it will appear
 identical when viewed from either the front or the back.
 If a surface is one-sided,
 then it appears invisible when viewed from the back side.
 This means
 that a transmitting object will affect the light coming in through the
 front surface and ignore the characteristics of the back surface,
 unless the index of refraction is set.
 If the index of refraction is set, then the object will act as a
 solid piece of dielectric material.
 In either case, the transmission properties of the exiting surface
 should be the same as the incident surface for the model to be
 physically valid.
 The default number of sides is two.

Real behaviour:

 I'm not sure if I understand it.
 Let's independently specify our behaviour:

 1. 1-sided surface is surface of solid object from real world.
    If it has no transmittance, surface can't be hit from the inner side.
    (=There is darkness inside real world solid objects.)
    But if it happens, energy should be thrown away and ray discontinued.
    Presence of such rays may be visualised by
    -sides1outer:receive=0 -sides1inner:receive=1 -sides2outer:receive=0 -sides2inner:receive=0

    Let's imagine some possibilities:
      a) rays that hit inner side are thrown away
      b) rays that hit inner side continue as if nothing happened

    Our implementation makes a) by default.
    b) is available by -sides1inner:catch=0

 2. There is no 2-sided surface in real world.
    2-sided is probably something solid, thin object with >0 thickness.
    Or CG hack without real-world sense.
    What if we have 2-sided surface enlightened only from one side?

    Let's imagine possibilities how to handle it:
      a) dark from the other side (thin metal layer)
      b) equally enlightened from (and emiting to) both sides (thin cloth)
      c) appear equally enlightened from both sides but emit light only to outer side (CG hack)

    Our implementation makes c) by default.
    b) is available by -sides2inner:emit=1
    a) would require some coding and more memory


RRSideBits sideBits[3][2]={
	{{0},{0}},
	{{1,1,1,1,1,1},{0,0,1,0,0,0}}, // definition of default 1-sided (front side, back side)
	{{1,1,1,1,1,1},{1,0,1,1,1,1}}, // definition of default 2-sided (front side, back side)
};
*/

RRScene::RRScene()
{
	_scene=new Scene();
}

RRScene::~RRScene()
{
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
//    +ubyde kontrola fyzikalni legalnosti v rrapi, legalnost zaridi RRSurface::validate();
// b) behem pouzivani neustale kontrolovat ze neco nepreteklo
//    -sezere moc vykonu
// c) zkontrolovat na zacatku a pak duverovat
//    +ubyde kontrola fyzikalni legalnosti v rrapi, legalnost zaridi RRSurface::validate();
   
RRScene::ObjectHandle RRScene::objectCreate(RRObject* importer, unsigned smoothMode)
{
	assert(importer);
	if(!importer) return UINT_MAX;
	RRMesh* meshImporter = importer->getCollider()->getMesh();
	Object *obj=new Object(meshImporter->getNumVertices(),meshImporter->getNumTriangles());
	obj->importer = importer;

	// import vertices
	assert(sizeof(RRMesh::Vertex)==sizeof(Vec3));
	for(unsigned i=0;i<obj->vertices;i++) 
		meshImporter->getVertex(i,*(RRMesh::Vertex*)&obj->vertex[i]);

#ifdef SUPPORT_TRANSFORMS
	obj->transformMatrix=importer->getWorldMatrix();
	obj->inverseMatrix=importer->getInvWorldMatrix();
	// vyzada si prvni transformaci
	obj->matrixDirty=true;
#endif

	// import triangles
	// od nuly nahoru insertuje emitory, od triangles-1 dolu ostatni
	DBG(printf(" triangles...\n"));
	int tbot=0;
// bohuzel pak nesedej triangle_handly, poradi se ted uz nesmi menit
//#ifdef SUPPORT_DYNAMIC
//	int ttop=obj->triangles-1;
//#endif
	for(unsigned fi=0;fi<obj->triangles;fi++) 
	{
		RRMesh::Triangle tv;
		meshImporter->getTriangle(fi,tv);
		unsigned si = importer->getTriangleSurface(fi);
		const RRSurface* s=importer->getSurface(si);
		assert(s);
		// rozhodne zda vlozit face dolu mezi emitory nebo nahoru mezi ostatni
		Triangle *t;
//#ifdef SUPPORT_DYNAMIC
//		if(s && s->diffuseEmittance) t=&obj->triangle[tbot++]; else t=&obj->triangle[ttop--];
//#else
		t=&obj->triangle[tbot++];
//#endif
		assert(t>=obj->triangle && t<&obj->triangle[obj->triangles]);
		// vlozi ho, seridi geometrii atd
/*		RRMesh::Vertex v[3];
		meshImporter->getVertex(tv[0],v[0]);
		meshImporter->getVertex(tv[1],v[1]);
		meshImporter->getVertex(tv[2],v[2]);*/
		int geom=t->setGeometry(
			&obj->vertex[tv[0]],
			&obj->vertex[tv[1]],
			&obj->vertex[tv[2]],
#ifdef SUPPORT_TRANSFORMS
			obj->transformMatrix
#else
			NULL
#endif
			);
		if(t->isValid) 
		{
			// this code is on 2 places...
			//  delete this and rather call obj->resetStaticIllumination
			Vec3 sumExitance;
			importer->getTriangleAdditionalMeasure(fi,RM_EXITANCE,sumExitance);
			if(scene->scaler) sumExitance = Vec3(
				scene->scaler->getStandard(sumExitance.x), // getOriginal=getWattsPerSquareMeter
				scene->scaler->getStandard(sumExitance.y),
				scene->scaler->getStandard(sumExitance.z));
			obj->objSourceExitingFlux+=abs(t->setSurface(s,sumExitance,true));
		}
		else
		{
			t->surface=NULL; // marks invalid triangles
			t->area=0; // just to have consistency through all invalid triangles
		}
	}
	   
#ifdef SUPPORT_DYNAMIC
	obj->trianglesEmiting=tbot;
#endif
	// preprocessuje objekt
	DBG(printf(" bounds...\n"));
	obj->detectBounds();
	{
		DBG(printf(" edges...\n"));
		obj->buildEdges(); // build edges only for clusters and/or interpol
	}
	if(RRScene::getState(USE_CLUSTERS))
	{
		DBG(printf(" clusters...\n"));
		obj->buildClusters(); 
		// clusters first, ivertices then (see comment in Cluster::insert)
	}
	DBG(printf(" ivertices...\n"));
	obj->buildTopIVertices(smoothMode);
	// priradi objektu jednoznacny a pri kazdem spusteni stejny identifikator
	obj->id=0;//!!!
	obj->name=NULL;
#ifdef SUPPORT_TRANSFORMS
	obj->transformBound();
#endif
	// vlozi objekt do sceny
#ifdef SUPPORT_DYNAMIC
	if (0) 
	{
		scene->objInsertDynamic(obj); 
		return -1-scene->objects;
	}
	else
#endif
	{
		scene->objInsertStatic(obj);
		return scene->objects-1;
	}
}



/////////////////////////////////////////////////////////////////////////////
//
// calculate radiosity

RRScene::Improvement RRScene::illuminationReset(bool resetFactors, bool resetPropagation)
{
	if(!licenseStatusValid || licenseStatus!=RRLicense::VALID) return FINISHED;
	__frameNumber++;
	scene->updateMatrices();
	return scene->resetStaticIllumination(resetFactors,resetPropagation);
}

RRScene::Improvement RRScene::illuminationImprove(bool endfunc(void*), void* context)
{
	if(!licenseStatusValid || licenseStatus!=RRLicense::VALID) return FINISHED;
	__frameNumber++;
	return scene->improveStatic(endfunc, context);
}

RRReal RRScene::illuminationAccuracy()
{
	return scene->avgAccuracy()/100;
}


/////////////////////////////////////////////////////////////////////////////
//
// read results

bool RRScene::getTriangleMeasure(ObjectHandle object, unsigned triangle, unsigned vertex, RRRadiometricMeasure measure, RRColor& out)
{
	Channels irrad;
	RRScaler* scaler;
	Object* obj;
	Triangle* tri;

	// pokus nejak kompenzovat ze jsem si ve freeze interne zrusil n objektu a nahradil je 1 multiobjektem
	//if(isFrozen())
	//{
	//	if(scene->objects!=1) 
	//	{
	//		assert(0);
	//		return NULL;
	//	}
	//	Object* obj = scene->object[0];
	//	obj->importer->getCollider()->getMesh()->get
	//}
	if(object<0 || object>=scene->objects) 
	{
		assert(0);
		goto error;
	}
	obj = scene->object[object];
	if(triangle>=obj->triangles)
	{
		assert(0);
		goto error;
	}
	if(!licenseStatusValid || licenseStatus!=RRLicense::VALID)
	{
		assert(0);
		goto error;
	}
	tri = &obj->triangle[triangle];
	if(!tri->surface)
	{
		goto zero;
	}

	// enhanced by final gathering
	if(vertex<3 && RRScene::getState(GET_FINAL_GATHER))
	{
		vertex=(vertex+3-tri->rotations)%3;

		Channels reflIrrad = Channels(0);
		if(RRScene::getState(GET_REFLECTED))
		{
			// get normal
			RRObject* objectImporter = obj->importer;
			RRObject::TriangleNormals normals;
			objectImporter->getTriangleNormals(triangle,normals);
			Vec3 normal = normals.norm[vertex];
			assert(fabs(size2(normal)-1)<0.001);
			// get point
			RRMesh* meshImporter = objectImporter->getCollider()->getMesh();
			RRMesh::Triangle triangleIndices;
			meshImporter->getTriangle(triangle,triangleIndices);
			unsigned vertexIdx = triangleIndices[vertex];
			RRMesh::Vertex vertexBody;
			meshImporter->getVertex(vertexIdx,vertexBody);
			Vec3 point = vertexBody;
			// transform to worldspace
			const RRMatrix3x4* world = objectImporter->getWorldMatrix();
			if(world)
			{
				world->transformPosition(point);
				world->transformDirection(normal);
				normal *= 1/size(normal);
				assert(fabs(size2(normal)-1)<0.001);
			}
			//!!! solved by fudge 1mm offset in gatherIrradiance. point += normal*0.001f; //!!! fudge number. to avoid immediate hits of other faces connected to vertex. could be done also by more advanced skipTriangle with list of all faces connected to vertex
			// get irradiance
			reflIrrad = scene->gatherIrradiance(point,normal,tri);
		}
		irrad = (RRScene::getState(GET_SOURCE)?tri->getSourceIrradiance():Channels(0)) + reflIrrad; // irradiance in W/m^2
	}
	else

	// enhanced by smoothing
	if(vertex<3 && RRScene::getState(GET_SMOOTH))
	{
		vertex=(vertex+3-tri->rotations)%3;

		Channels reflIrrad = Channels(0);
		if(RRScene::getState(GET_REFLECTED))
		{
			unsigned oldSource = RRScene::setState(GET_SOURCE,0);
			reflIrrad = tri->topivertex[vertex]->irradiance();
			RRScene::setState(GET_SOURCE,oldSource);
		}
		irrad = (RRScene::getState(GET_SOURCE)?tri->getSourceIrradiance():Channels(0)) + reflIrrad; // irradiance in W/m^2
	}
	else

	// basic, fast
	{
		if(!RRScene::getState(GET_SOURCE) && !RRScene::getState(GET_REFLECTED)) 
			irrad = Channels(0);
		else
		if(RRScene::getState(GET_SOURCE) && !RRScene::getState(GET_REFLECTED)) 
			irrad = tri->getSourceIrradiance();
		else
		if(RRScene::getState(GET_SOURCE) && RRScene::getState(GET_REFLECTED)) 
			irrad = (tri->energyDirectIncident + tri->getEnergyDynamic()) / tri->area;
		else
			irrad = (tri->energyDirectIncident + tri->getEnergyDynamic()) / tri->area - tri->getSourceIrradiance();
	}

	// scaler may be applied only on irradiance/exitance
	scaler = scene->scaler;
	if(scaler)
	{
		irrad[0] = scaler->getScaled(irrad[0]);
		irrad[1] = scaler->getScaled(irrad[1]);
		irrad[2] = scaler->getScaled(irrad[2]);
	}
	switch(measure)
	{
		case RM_INCIDENT_FLUX:
			goto error; // not supported yet
		case RM_IRRADIANCE:
			out = irrad;
			STATISTIC_INC(numCallsTriangleMeasureOk);
			return true;
		case RM_EXITING_FLUX:
			goto error; // not supported yet
		case RM_EXITANCE:
			out = irrad * tri->surface->diffuseReflectance;
			STATISTIC_INC(numCallsTriangleMeasureOk);
			return true;
	}
error:
	assert(0);
zero:
	out = RRColor(0);
	STATISTIC_INC(numCallsTriangleMeasureFail);
	return false;
}

unsigned SubTriangle::enumSubtriangles(IVertex **iv, Channels flatambient, EnumSubtrianglesCallback* callback, void* context)
{
	if(sub[0])
	{
		IVertex *iv0[3];
		IVertex *iv1[3];
		bool rightleft=isRightLeft();
		int rot=getSplitVertex();
		assert(subvertex);
		assert(subvertex->check());
		iv0[0]=iv[rot];
		iv0[1]=iv[(rot+1)%3];
		iv0[2]=subvertex;
		iv1[0]=iv[rot];
		iv1[1]=subvertex;
		iv1[2]=iv[(rot+2)%3];
		flatambient+=(energyDirect+getEnergyDynamic()-sub[0]->energyDirect-sub[1]->energyDirect)/area;
		return SUBTRIANGLE(sub[rightleft?0:1])->enumSubtriangles(iv0,flatambient,callback,context)+
			SUBTRIANGLE(sub[rightleft?1:0])->enumSubtriangles(iv1,flatambient,callback,context);
	}
	if(callback)
	{
		callback(this,iv,flatambient,context);
	}
	return 1;
}

unsigned Triangle::enumSubtriangles(EnumSubtrianglesCallback* callback, void* context)
{
	return SubTriangle::enumSubtriangles(topivertex,Channels(0),callback,context);
}

struct SubtriangleIlluminationContext
{
	RRRadiometricMeasure                   measure;
	RRScaler*                              scaler;
	RRScene::SubtriangleIlluminationEater* clientCallback;
	void*                                  clientContext;
};

void buildSubtriangleIllumination(SubTriangle* s, IVertex **iv, Channels flatambient, void* context)
{
	SubtriangleIlluminationContext* context2 = (SubtriangleIlluminationContext*)context;
	RRScene::SubtriangleIllumination si;
	for(unsigned i=0;i<3;i++)
	{
		// fill 2d coords in triangle space
		//  start with uv[] with ortonormal base
		si.texCoord[i] = s->uv[(i+3-s->grandpa->rotations)%3];
		//  and transform to triangle space
		//  x , y -> x/u2.x-y*v2.x/u2.x/v2.y , y/v2.y
		//  u2,v2 jsou brane z Triangle
		si.texCoord[i] = Vec2(si.texCoord[i][0]/s->grandpa->u2[0]-si.texCoord[i][1]*s->grandpa->v2[0]/s->grandpa->u2[0]/s->grandpa->v2[1],si.texCoord[i][1]/s->grandpa->v2[1]);
		// fill irradiance
		if(RRScene::getState(RRScene::GET_SMOOTH))
			si.measure[i] = iv[i]->irradiance();
		else
			si.measure[i] = flatambient+s->energyDirect/s->area;
		// scale irradiance
		if(context2->scaler)
		{
			for(unsigned j=0;j<3;j++)
			{
				si.measure[i][j] = context2->scaler->getScaled(si.measure[i][j]);
			}
		}
		if(!context2 || licenseStatus!=RRLicense::VALID)
		{
			assert(0);
			return;
		}
		// convert irradiance to measure
		switch(context2->measure)
		{
			case RM_INCIDENT_FLUX:
				assert(0);
				STATISTIC_INC(numCallsTriangleMeasureFail);
				break;
			case RM_IRRADIANCE:
				STATISTIC_INC(numCallsTriangleMeasureOk);
				break;
			case RM_EXITING_FLUX:
				assert(0);
				STATISTIC_INC(numCallsTriangleMeasureFail);
				break;
			case RM_EXITANCE:
				si.measure[i] *= s->grandpa->surface->diffuseReflectance;
				STATISTIC_INC(numCallsTriangleMeasureOk);
				break;
		}
	}
	context2->clientCallback(si,context2->clientContext);
}

unsigned  RRScene::getSubtriangleMeasure(ObjectHandle object, unsigned triangle, RRRadiometricMeasure measure, SubtriangleIlluminationEater* callback, void* context)
{
	Object* obj;
	Triangle* tri;

	if(object<0 || object>=scene->objects) 
	{
		assert(0);
		return 0;
	}
	obj = scene->object[object];
	if(triangle>=obj->triangles)
	{
		assert(0);
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
		assert(0);
		return 0;
	}

	SubtriangleIlluminationContext sic;
	sic.measure = measure;
	sic.scaler = scene->scaler;
	sic.clientCallback = callback;
	sic.clientContext = context;
	return tri->enumSubtriangles(buildSubtriangleIllumination,&sic);
}



/////////////////////////////////////////////////////////////////////////////
//
// misc settings

void RRScene::setScaler(RRScaler* scaler)
{
	scene->setScaler(scaler);
}

static unsigned RRSSValueU[RRScene::SSU_LAST];
static RRReal   RRSSValueF[RRScene::SSF_LAST];

void RRScene::resetStates()
{
	memset(RRSSValueU,0,sizeof(RRSSValueU));
	memset(RRSSValueF,0,sizeof(RRSSValueF));
	setState(RRScene::GET_SOURCE,1);
	setState(RRScene::GET_REFLECTED,1);
	setState(RRScene::GET_SMOOTH,1);
	// development
	setState(RRScene::USE_CLUSTERS,0);
	setState(RRScene::GET_FINAL_GATHER,0);
	setState(RRScene::FIGHT_NEEDLES,0);

	setStateF(RRScene::MIN_FEATURE_SIZE,0);
	setStateF(RRScene::MAX_SMOOTH_ANGLE,M_PI/10+0.01f);
	// development
	setStateF(RRScene::SUBDIVISION_SPEED,0);
	setStateF(RRScene::IGNORE_SMALLER_AREA,SMALL_REAL);
	setStateF(RRScene::IGNORE_SMALLER_ANGLE,0.001f);
	setStateF(RRScene::FIGHT_SMALLER_AREA,0.01f);
	setStateF(RRScene::FIGHT_SMALLER_ANGLE,0.01f);
	//setStateF(RRScene::SUBDIVISION_SPEED,0);
	//setStateF(RRScene::MIN_FEATURE_SIZE,10.037f); //!!!
}

unsigned RRScene::getState(SceneStateU state)
{
	if(state<0 || state>=SSU_LAST) 
	{
		assert(0);
		return 0;
	}
	return RRSSValueU[state];
}

unsigned RRScene::setState(SceneStateU state, unsigned value)
{
	if(state<0 || state>=SSU_LAST) 
	{
		assert(0);
		return 0;
	}
	unsigned tmp = RRSSValueU[state];
	RRSSValueU[state] = value;
	return tmp;
}

real RRScene::getStateF(SceneStateF state)
{
	if(state<0 || state>=SSF_LAST) 
	{
		assert(0);
		return 0;
	}
	return RRSSValueF[state];
}

real RRScene::setStateF(SceneStateF state, real value)
{
	if(state<0 || state>=SSF_LAST) 
	{
		assert(0);
		return 0;
	}
	real tmp = RRSSValueF[state];
	RRSSValueF[state] = value;
	return tmp;
}


} // namespace
