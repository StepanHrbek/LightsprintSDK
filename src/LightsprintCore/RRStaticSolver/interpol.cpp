// --------------------------------------------------------------------------
// Smoothing.
// Copyright 2000-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------


#ifdef _MSC_VER
#pragma warning(disable:4530) // rucim za mistni operace se std::set a std::list ze nehodi exception. s vypnutymi exceptions je rr o 2.5% rychlejsi
#endif

#include "rrcore.h"
#include "interpol.h"

#include <assert.h>
#include <list>
#include <math.h>
#include <memory.h>
#include <set>
#include <stdio.h>
#include <stdlib.h>

#define TRACE(a) //{if (RRIntersectStats::getInstance()->intersects>=478988) OutputDebugString(a);}

namespace rr
{

struct IVertexInfo
// used by: merge close ivertices
{
	void               absorb(IVertexInfo& info2); // this <- this+info2, info2 <- 0
	class IVertex*     ivertex;                    // ivertex this information came from
	Point3             center;                     // center of our vertices
	std::set<unsigned> ourVertices;                // our vertices
	std::set<unsigned> neighbourVertices;          // neighbours of our vertices (na ktere se da od nasich dojet po jedne hrane)
};

#define MAX_NEIGHBOUR_DISTANCE 0.01

//////////////////////////////////////////////////////////////////////////////
//
// ivertex pool

void Object::deleteIVertices()
{
	while (IVertexPool)
	{
		IVertex *old=IVertexPool->previousAllocBlock;
		delete[] IVertexPool;
		IVertexPool=old;
	}
	IVertexPool=0;
	IVertexPoolItems=0;
	IVertexPoolItemsUsed=0;
}

/*IVertexPoolIterator::IVertexPoolIterator(const Object* object)
{
	pool = object->IVertexPool;
	poolItems = object->IVertexPoolItems;
	poolItemsUsed = object->IVertexPoolItemsUsed;
}

IVertex* IVertexPoolIterator::getNext()
{
	if (poolItemsUsed==1)
	{
		pool = *(IVertex**)pool;
		poolItemsUsed = poolItems = poolItems/2;
	}
	return pool ? pool[poolItemsUsed] : NULL;
}*/


//////////////////////////////////////////////////////////////////////////////
//
// all subtriangles around vertex

unsigned __iverticesAllocated=0;
unsigned __cornersAllocated=0;

IVertex::IVertex()
{
	RR_ASSERT(this);
	corners=0;
	cornersAllocatedLn2=STATIC_CORNERS_LN2;
	dynamicCorner=NULL;
	powerTopLevel=0;
	cacheTime=__frameNumber-1;
	cacheValid=0;
	__iverticesAllocated++;
	__cornersAllocated+=cornersAllocated();
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
		__cornersAllocated+=cornersAllocated();
		cornersAllocatedLn2++;
		//__cornersAllocated+=3*cornersAllocated();
		//cornersAllocatedLn2+=2;
		dynamicCorner=(Corner *)realloc(dynamicCorner,oldsize,(cornersAllocated()-STATIC_CORNERS)*sizeof(Corner));
	}
	for (unsigned i=0;i<corners;i++)
		if (getCorner(i).node==node)
		{
#ifndef SUPPORT_MIN_FEATURE_SIZE // s min feature size se i triangly smej insertovat vickrat
			RR_ASSERT(!node->grandpa);// pouze clustery se smej insertovat vickrat, power se akumuluje
#endif
			getCorner(i).power+=power;
			goto label;
		}
	corners++;
	getCorner(corners-1).node=node;
	getCorner(corners-1).power=power;
label:
	if (toplevel) powerTopLevel+=power;
}

bool IVertex::contains(Triangle* node)
{
	for (unsigned i=0;i<corners;i++)
		if (getCorner(i).node==node) return true;
	return false;
}

unsigned IVertex::splitTopLevelByAngleNew(RRVec3 *avertex, Object *obj, float maxSmoothAngle, bool& outOfMemory)
{
	// input: ivertex filled with triangle corners (ivertex is installed in all his corners)
	// job: remove this ivertex and install new reduced ivertices (split big into set of smaller)
	// return: number of new ivertices

	//while zbyvaji cornery
	// set=empty
	// for each zbyvajici corner
	//  kdyz ma normalu dost blizkou aspon jednomu corneru ze setu, vloz ho do setu
	//  zaloz ivertex s timto setem corneru
	//  NEDOKONALE, vrchol jehlanu bude mit jednu barvu

	const RRMesh* mesh = obj->importer->getCollider()->getMesh();

	unsigned numSplitted = 0;
	//while zbyvaji cornery
	std::list<Corner*> cornersLeft;
	for (unsigned i=0;i<corners;i++) cornersLeft.push_back(&getCorner(i));
	while (cornersLeft.size())
	{
		// set=empty
		// zaloz ivertex s timto setem corneru
		IVertex *v = obj->newIVertex();
		if (!v)
		{
			outOfMemory = true;
			break;
		}
		numSplitted++;
		// for each zbyvajici corner
restart_iter:
		for (std::list<Corner*>::iterator i=cornersLeft.begin();i!=cornersLeft.end();i++)
		{
			unsigned t1,t2;
			RRMesh::TriangleBody body1,body2;
			RRVec3 n1,n2;
			unsigned j;
			//  kdyz ma normalu dost blizkou aspon jednomu corneru ze setu, vloz ho do setu
			if (!v->corners)
				goto insert_i;

			t1 = ARRAY_ELEMENT_TO_INDEX(obj->triangle,(*i)->node);
			mesh->getTriangleBody(t1,body1);
			n1 = orthogonalTo(body1.side1,body1.side2).normalized();

			for (j=0;j<v->corners;j++)
			{
				t2 = ARRAY_ELEMENT_TO_INDEX(obj->triangle,v->getCorner(j).node);
				mesh->getTriangleBody(t2,body2);
				n2 = orthogonalTo(body2.side1,body2.side2).normalized();

				if (angleBetweenNormalized(n1,n2)<=maxSmoothAngle)
				{
insert_i:
					// vloz corner do noveho ivertexu
					v->insert((*i)->node,true,(*i)->power);
					// oprav pointery z nodu na stary ivertex
					Triangle* triangle = (*i)->node;
					for (unsigned k=0;k<3;k++)
						if (triangle->topivertex[k]==this)
							triangle->topivertex[k] = v;
					// odeber z corneru zbyvajicich ke zpracovani
					cornersLeft.erase(i);
					// pro jistotu restartni iteraci, iterator muze byt dead
					goto restart_iter;
				}
			}
		}
	}
	// make this empty = ready for deletion
	corners=0;
	powerTopLevel=0;
	return numSplitted;
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
		STATISTIC_INC(numIrradianceCacheMisses);
	}
	else
		STATISTIC_INC(numIrradianceCacheHits);

	RR_ASSERT(IS_CHANNELS(cache));
	return cache;
}

IVertex::~IVertex()
{
	cornersAllocatedLn2++;
	cornersAllocatedLn2--;
	free(dynamicCorner);
	__cornersAllocated-=cornersAllocated();
	__iverticesAllocated--;
}

IVertex *Triangle::ivertex(int i)
{
	RR_ASSERT(IS_PTR(this));
	RR_ASSERT(i>=0 && i<3);
	RR_ASSERT(surface);
	IVertex *tmp = topivertex[i];
	TRACE(FS("(%x,%d,%x,%d)",this,i,tmp,RRIntersectStats::getInstance()->intersects));
	RR_ASSERT(tmp);
	return tmp;
}

#ifdef SUPPORT_MIN_FEATURE_SIZE
void IVertex::fillInfo(Object* object, unsigned originalVertexIndex, IVertexInfo& info)
// used by: merge close ivertices
// fills structure with information about us
// expects ivertex on the beginning of its life - contains all corners around 1 vertex,
//  originalVertexIndex is index of that vertex in object->vertex array
{
	//RR_ASSERT(corners);
	// corners==0 means ivertex without any corner
	// it's probably in vertex that is not referenced by any triangle
	// it's not nice, but let's try to live with such

	// fill original ivertex
	info.ivertex = this;
	// fill our vertices
	info.ourVertices.insert(originalVertexIndex);
	// fill center
	const RRMesh* mesh = object->importer->getCollider()->getMesh();
	mesh->getVertex(originalVertexIndex,info.center);
	// fill our neighbours
	for (unsigned c=0;c<corners;c++)
	{
		Triangle* tri = getCorner(c).node;
		unsigned originalPresent = 0;
		unsigned triangleIndex = ARRAY_ELEMENT_TO_INDEX(object->triangle,tri);
		RRMesh::Triangle triangleVertexIndices;
		mesh->getTriangle(triangleIndex,triangleVertexIndices);
		for (unsigned i=0;i<3;i++)
		{
			unsigned triVertex = triangleVertexIndices[i];
			if (triVertex!=originalVertexIndex)
				info.neighbourVertices.insert(triVertex);
			else
				originalPresent++;
		}
		RR_ASSERT(originalPresent==1);
	}
}

void IVertex::absorb(IVertex* aivertex)
// used by: merge close ivertices
// this <- this+aivertex
// aivertex <- 0
{
	// rehook nodes referencing aivertex to us
	for (unsigned c=0;c<aivertex->corners;c++)
	{
		Triangle* tri = aivertex->getCorner(c).node;
		for (unsigned i=0;i<3;i++)
		{
			if (tri->topivertex[i]==aivertex)
				tri->topivertex[i]=this;
		}
	}
	// move corners
	while (aivertex->corners)
	{
		Corner* c = &aivertex->getCorner(--aivertex->corners);
		insert(c->node,false,c->power);
	}
}

void IVertexInfo::absorb(IVertexInfo& info2)
// used by: merge close ivertices
// this <- this+info2
// info2 <- 0
// all nodes referencing info2.ivertex are rehooked to us
{
	RR_ASSERT(this!=&info2);
	// set center
	unsigned verticesTotal = (unsigned)(ourVertices.size()+info2.ourVertices.size());
	center = (center*(real)ourVertices.size() + info2.center*(real)info2.ourVertices.size()) / (real)verticesTotal;
	// set ourVertices
	ourVertices.insert(info2.ourVertices.begin(),info2.ourVertices.end());
	info2.ourVertices.erase(info2.ourVertices.begin(),info2.ourVertices.end());
	RR_ASSERT(ourVertices.size()==verticesTotal);
	RR_ASSERT(info2.ourVertices.size()==0);
	// set neighbourVertices
	neighbourVertices.insert(info2.neighbourVertices.begin(),info2.neighbourVertices.end());
	info2.neighbourVertices.erase(info2.neighbourVertices.begin(),info2.neighbourVertices.end());
	RR_ASSERT(info2.neighbourVertices.size()==0);
	for (std::set<unsigned>::const_iterator i=ourVertices.begin();i!=ourVertices.end();i++)
	{
		unsigned ourVertex = *i;
		std::set<unsigned>::iterator invalidNeighbour = neighbourVertices.find(ourVertex);
		if (invalidNeighbour!=neighbourVertices.end())
		{
			neighbourVertices.erase(invalidNeighbour);
			//invalidNeighbour = neighbourVertices.find(ourVertex);
			//RR_ASSERT(invalidNeighbour==neighbourVertices.end());
		}
	}
	/*set_difference(
		neighbourVertices.begin(),neighbourVertices.end(),
		ourVertices.begin(),ourVertices.end(),
		neighbourVertices.begin()
		);*/
	// modification of object data!
	// ivertex absorbs corners of info2.ivertex
	ivertex->absorb(info2.ivertex);
}

unsigned Object::mergeCloseIVertices(IVertex* ivertex, float minFeatureSize, bool& aborting)
// merges close ivertices
// why to do it: eliminates negative effect of needle, both triangles around needle are interpolated as if there is no needle
// returns number of merges (each merge = 1 ivertex reduced)
{
	unsigned ivertices = vertices; // it is expected that ivertex is array of length vertices
	unsigned numReduced = 0;

	// gather all source information for each ivertex
	// - center
	// - set of vertices
	// - set of neighbour vertices
	IVertexInfo* ivertexInfo = new IVertexInfo[ivertices];
	for (unsigned i=0;i<ivertices;i++)
	{
		ivertex[i].fillInfo(this,i,ivertexInfo[i]);
	}
	unsigned* vertex2ivertex = new unsigned[vertices];
	for (unsigned i=0;i<ivertices;i++)
	{
		vertex2ivertex[i] = i;
	}

	// work until done
	unsigned ivertex1Idx = 0;
	while (!aborting)
	{
		// find closest ivertex - ivertex pair
		real minDist = 1e30f;
		unsigned minIVert1 = 0;
		unsigned minIVert2 = 0;
		for (;ivertex1Idx<ivertices;ivertex1Idx++)
			if (ivertexInfo[ivertex1Idx].ourVertices.size())
		{
			RR_ASSERT(ivertexInfo[ivertex1Idx].ivertex->getNumCorners()!=0xfeee);
			RR_ASSERT(ivertexInfo[ivertex1Idx].ivertex->getNumCorners()!=0xcdcd);
			// for each neighbour vertex
			for (std::set<unsigned>::const_iterator i=ivertexInfo[ivertex1Idx].neighbourVertices.begin();i!=ivertexInfo[ivertex1Idx].neighbourVertices.end();i++)
			{
				// convert to neighbour ivertex
				unsigned vertex2Idx = *i;
				unsigned ivertex2Idx = vertex2ivertex[vertex2Idx];
				RR_ASSERT(ivertex1Idx!=ivertex2Idx);
				// measure distance
				real dist = size(ivertexInfo[ivertex1Idx].center-ivertexInfo[ivertex2Idx].center);
				RR_ASSERT(dist); // teoreticky muze nastat kdyz objekt obsahuje vic vertexu na stejnem miste
				// store the best
				if (dist<minDist)
				{
					minDist = dist;
					minIVert1 = ivertex1Idx;
					minIVert2 = ivertex2Idx;
				}
			}
			if (minDist<=minFeatureSize) break;
		}

		// end if not close enough
		if (minDist>minFeatureSize) break;
		numReduced++;

		// merge ivertices: update local temporary vertex2ivertex
		for (std::set<unsigned>::const_iterator i=ivertexInfo[minIVert2].ourVertices.begin();i!=ivertexInfo[minIVert2].ourVertices.end();i++)
		{
			RR_ASSERT(vertex2ivertex[*i] == minIVert2);
			vertex2ivertex[*i] = minIVert1;
		}

		// merge ivertices: rehook object persistent triangle->topivertex[0..2] from absorbed ivertex to absorbant ivertex
		// merge ivertices: update object persistent ivertices
		// while all other code in this method modifies temporary local data,
		//  this modifies also persistent object data
		RR_ASSERT(ivertexInfo[minIVert1].ivertex->getNumCorners()!=0xfeee);
		RR_ASSERT(ivertexInfo[minIVert1].ivertex->getNumCorners()!=0xcdcd);
		ivertexInfo[minIVert1].absorb(ivertexInfo[minIVert2]);

		// sanity check
		bool warned = false;
		if (ivertexInfo[minIVert1].ourVertices.size()>100)
		{
			LIMITED_TIMES(1,warned=true;RRReporter::report(WARN,"Cluster of more than 100 vertices smoothed.\n"));
		}
		if (numReduced>unsigned(vertices*0.9f))
		{
			LIMITED_TIMES(1,warned=true;RRReporter::report(WARN,"More than 90%% of vertices removed in smoothing.\n"));
		}
		if (warned)
		{
			RRVec3 mini,maxi,center;
			importer->getCollider()->getMesh()->getAABB(&mini,&maxi,&center);
			RRReporter::report(INF1,"Scene stats: numVertices=%d  size=%fm x %fm x %fm  minFeatureSize=%fm (check what you enter into RRDynamicSolver::setStaticObjects() or RRStaticSolver())\n",vertices,maxi[0]-mini[0],maxi[1]-mini[1],maxi[2]-mini[2],minFeatureSize);
			if (sum(abs(maxi-mini))>5000) RRReporter::report(WARN,"Possibly scene too big (wrong scale)?\n");
			if (sum(abs(maxi-mini))<0.5) RRReporter::report(WARN,"Possibly scene too small (wrong scale)?\n");
		}
	}

	// delete temporaries
	delete[] vertex2ivertex;
	delete[] ivertexInfo;
	return numReduced;
}
#endif

bool Object::buildTopIVertices(float minFeatureSize, float maxSmoothAngle, bool& aborting)
{
	bool outOfMemory = false;

	// check
	for (unsigned t=0;t<triangles;t++)
	{
		for (int v=0;v<3;v++)
			RR_ASSERT(!triangle[t].topivertex[v]);
	}

	// build 1 ivertex for each vertex, insert all corners
	IVertex *topivertex=new IVertex[vertices];
	const RRMesh* mesh = importer->getCollider()->getMesh();
	for (unsigned t=0;t<triangles;t++) if (triangle[t].surface)
	{
		if (aborting) break;
		RRMesh::Triangle un_ve;
		mesh->getTriangle(t,un_ve);
		RRMesh::Vertex vertex[3];
		mesh->getVertex(un_ve[0],vertex[0]);
		mesh->getVertex(un_ve[1],vertex[1]);
		mesh->getVertex(un_ve[2],vertex[2]);
		for (int ro_v=0;ro_v<3;ro_v++)
		{
			unsigned un_v = un_ve[ro_v];
			RR_ASSERT(un_v<vertices);
			triangle[t].topivertex[ro_v]=&topivertex[un_v];
			Angle angle=angleBetween(vertex[(ro_v+1)%3]-vertex[ro_v],vertex[(ro_v+2)%3]-vertex[ro_v]);
			topivertex[un_v].insert(&triangle[t],true,angle);
		}
	}

	// merge close ivertices into 1 big + 1 empty
	// (empty will be later detected and reported as unused)
	unsigned numIVertices = vertices;
	//printf("IVertices loaded: %d\n",numIVertices);
#ifdef SUPPORT_MIN_FEATURE_SIZE
	// volano jen pokud ma neco delat -> malinka uspora casu
	if (minFeatureSize>0 && !aborting)
	{
		// Pouha existence nasledujiciho radku (mergeCloseIVertices) i kdyz se nikdy neprovadi
		// zpomaluje cube v MSVC o 8%.
		// Nevyresena zahada.
		// Ona fce je jedine misto pouzivajici exceptions, ale exceptions jsou vyple (jejich zapnuti zpomali o dalsich 12%).
		numIVertices -= mergeCloseIVertices(topivertex,minFeatureSize,aborting);
		//printf("IVertices after merge close: %d\n",numIVertices);
	}
#endif

	// split ivertices with too different normals
	for (unsigned v=0;v<vertices;v++)
	{
		if (aborting) break;
		RRMesh::Vertex vert;
		mesh->getVertex(v,vert);
		numIVertices += topivertex[v].splitTopLevelByAngleNew((RRVec3*)&vert,this,maxSmoothAngle,outOfMemory);
		if (outOfMemory) break;
		// check that splitted topivertex is no more referenced
		/*for (unsigned t=0;t<triangles;t++) if (triangle[t].surface)
		{
			for (unsigned i=0;i<3;i++)
			{
				RR_ASSERT(triangle[t].topivertex[i]!=&topivertex[v]);
			}
		}*/
	}
	//printf("IVertices after splitting: %d\n",numIVertices);

	// delete local array topivertex
	for (unsigned t=0;t<triangles;t++) if (triangle[t].surface)
	{
		for (unsigned i=0;i<3;i++)
		{
			// check that local array topivertex is not referenced here
			unsigned idx = (unsigned)(triangle[t].topivertex[i]-topivertex);
			if (idx<vertices)
			{
				if (aborting || outOfMemory)
				{
					// it could be broken after abort, fix it
					triangle[t].topivertex[i] = NULL;
				}
				else
				{
					// it must not be broken otherwise
					RR_ASSERT(0);
				}
			}
		}
	}
	delete[] topivertex;

	// check triangle.topivertex validity
	for (unsigned t=0;t<triangles;t++)
		for (int v=0;v<3;v++)
		{
			if (triangle[t].topivertex[v])
			{
				RR_ASSERT(triangle[t].topivertex[v]->getNumCorners()!=0xcdcd);
				RR_ASSERT(triangle[t].topivertex[v]->getNumCorners()!=0xfeee);
			}
		}

	return !outOfMemory && !aborting;
}

} // namespace


