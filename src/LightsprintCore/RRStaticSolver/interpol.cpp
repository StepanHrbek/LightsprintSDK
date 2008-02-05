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

#define TRACE(a) //{if(RRIntersectStats::getInstance()->intersects>=478988) OutputDebugString(a);}

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

IVertex *Object::newIVertex()
{
	if(IVertexPoolItemsUsed>=IVertexPoolItems) 
	{
		IVertex *old=IVertexPool;
		if(!IVertexPoolItems) IVertexPoolItems=128; else IVertexPoolItems*=2;
		IVertexPool=new IVertex[IVertexPoolItems];
		IVertexPoolItemsUsed=1;
		IVertexPool->previousAllocBlock=old; // store pointer to old block on safe place (overwrite unimportant float)
	}
	return &IVertexPool[IVertexPoolItemsUsed++];
}

void Object::deleteIVertices()
{
	while(IVertexPool)
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
	if(poolItemsUsed==1)
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
	cornersAllocatedLn2=3;
	corner=(Corner *)malloc(cornersAllocated()*sizeof(Corner));
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

void IVertex::insert(Triangle* node,bool toplevel,real power,Point3 apoint)
{
	RR_ASSERT(this);
	power *= node->area;
	if(corners==cornersAllocated())
	{
		size_t oldsize=cornersAllocated()*sizeof(Corner);
		__cornersAllocated+=cornersAllocated();
		cornersAllocatedLn2++;
		//__cornersAllocated+=3*cornersAllocated();
		//cornersAllocatedLn2+=2;
		corner=(Corner *)realloc(corner,oldsize,cornersAllocated()*sizeof(Corner));
	}
	for(unsigned i=0;i<corners;i++)
		if(corner[i].node==node)
		{
#ifndef SUPPORT_MIN_FEATURE_SIZE // s min feature size se i triangly smej insertovat vickrat
			RR_ASSERT(!node->grandpa);// pouze clustery se smej insertovat vickrat, power se akumuluje
#endif
			corner[i].power+=power;
			goto label;
		}
	corner[corners].node=node;
	corner[corners].power=power;
	corners++;
      label:
	if(toplevel) powerTopLevel+=power;
}

bool IVertex::contains(Triangle* node)
{
	for(unsigned i=0;i<corners;i++)
		if(corner[i].node==node) return true;
	return false;
}

unsigned IVertex::splitTopLevelByAngleOld(RRVec3 *avertex, Object *obj, float maxSmoothAngle)
{
	// input: ivertex filled with triangle corners (ivertex is installed in all his corners)
	// job: remove this ivertex and install new reduced ivertices (split big into set of smaller)
	// return: number of new ivertices

	// stara verze s chybou projevujici se na gaucich v IS
	// pokud je chyba jen pri min feature size>0,
	// znamena to ze tento alg neni pripraveny na nodu ktera ma v ivertexu vic nez jeden corner

	unsigned numSplitted = 0;
	// remember all places this ivertex is installed in
	IVertex ***topivertex=new IVertex**[3*corners];
	for(unsigned i=0;i<corners;i++)
	{
		RR_ASSERT(corner[i].node);
		RR_ASSERT(corner[i].node->surface);
		unsigned found=0;
		topivertex[i]=NULL;
		topivertex[i+corners]=NULL;
		topivertex[i+2*corners]=NULL;
		for(int k=0;k<3;k++)
		{
			if(corner[i].node->topivertex[k]==this)
			{
#ifndef SUPPORT_MIN_FEATURE_SIZE // s min feature size muze ukazovat vic vrcholu na jeden ivertex
				RR_ASSERT(!found);
#endif
				topivertex[i+found*corners]=&corner[i].node->topivertex[k];
				found++;
			}
		}
		RR_ASSERT(found);
	}
	// for each vertex, reduce it (remove corners with too different normal) and reinstall
	for(unsigned i=0;i<corners;i++)
	{
		IVertex *v;
		// search corner[j] with the same reduction as corner[i]
		// this is slowest code if we open sponza, complexity corners^3 where corners~=30
		for(unsigned j=0;j<i;j++)
		{
			for(unsigned k=0;k<corners;k++)
				if(INTERPOL_BETWEEN(corner[i].node,corner[k].node)
				   !=((*topivertex[j])->contains(corner[k].node)) )
					goto j_differs_i;
			v=*topivertex[j];
			goto install_v;
			j_differs_i:;
		}
		// make new reduction
		v=obj->newIVertex();
		numSplitted++;
		for(unsigned j=0;j<corners;j++)
			if(INTERPOL_BETWEEN(corner[i].node,corner[j].node))
				v->insert(corner[j].node,true,corner[j].power);
		// install reduction
		install_v:
		*topivertex[i]=v;
		if(topivertex[i+corners]) *topivertex[i+corners]=v;
		if(topivertex[i+2*corners]) *topivertex[i+2*corners]=v;
	}
	// make this empty = ready for deletion
	corners=0;
	powerTopLevel=0;
	delete[] topivertex;
	return numSplitted;
}

unsigned IVertex::splitTopLevelByAngleNew(RRVec3 *avertex, Object *obj, float maxSmoothAngle)
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

	unsigned numSplitted = 0;
	//while zbyvaji cornery
	std::list<Corner*> cornersLeft;
	for(unsigned i=0;i<corners;i++) cornersLeft.push_back(&corner[i]);
	while(cornersLeft.size())
	{
		// set=empty
		// zaloz ivertex s timto setem corneru
		IVertex *v = obj->newIVertex();
		numSplitted++;
		// for each zbyvajici corner
restart_iter:
		for(std::list<Corner*>::iterator i=cornersLeft.begin();i!=cornersLeft.end();i++)
		{
			unsigned j;
			//  kdyz ma normalu dost blizkou aspon jednomu corneru ze setu, vloz ho do setu
			if(!v->corners)
				goto insert_i;
			for(j=0;j<v->corners;j++)
				if(INTERPOL_BETWEEN((*i)->node,v->corner[j].node))
			{
insert_i:
				// vloz corner do noveho ivertexu
				v->insert((*i)->node,true,(*i)->power);
				// oprav pointery z nodu na stary ivertex
				Triangle* triangle = (*i)->node;
				for(unsigned k=0;k<3;k++)
					if(triangle->topivertex[k]==this)
						triangle->topivertex[k] = v;
				// odeber z corneru zbyvajicich ke zpracovani
				cornersLeft.erase(i);
				// pro jistotu restartni iteraci, iterator muze byt dead
				goto restart_iter;
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
	if(cacheTime!=(__frameNumber&0x1f) || !cacheValid || cacheDirect!=measure.direct || cacheIndirect!=measure.indirect) // cacheTime is byte:5
	{
		//RR_ASSERT(powerTopLevel);
		// irrad=irradiance in W/m^2
		Channels irrad=Channels(0);

		// full path
		for(unsigned i=0;i<corners;i++)
		{
			Triangle* node=corner[i].node;
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
			irrad+=w*(corner[i].power/corner[i].node->area);
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
	free(corner);
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
	info.center = object->vertex[originalVertexIndex];
	// fill our neighbours
	for(unsigned c=0;c<corners;c++)
	{
		Triangle* tri = corner[c].node;
		unsigned originalPresent = 0;
		for(unsigned i=0;i<3;i++)
		{
			unsigned triVertex = (unsigned)(tri->getVertex(i)-object->vertex);
			if(triVertex!=originalVertexIndex)
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
	for(unsigned c=0;c<aivertex->corners;c++)
	{
		Triangle* tri = aivertex->corner[c].node;
		for(unsigned i=0;i<3;i++)
		{
			if(tri->topivertex[i]==aivertex)
				tri->topivertex[i]=this;
		}
	}
	// move corners
	while(aivertex->corners)
	{
		Corner* c = &aivertex->corner[--aivertex->corners];
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
	for(std::set<unsigned>::const_iterator i=ourVertices.begin();i!=ourVertices.end();i++)
	{
		unsigned ourVertex = *i;
		std::set<unsigned>::iterator invalidNeighbour = neighbourVertices.find(ourVertex);
		if(invalidNeighbour!=neighbourVertices.end())
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

unsigned Object::mergeCloseIVertices(IVertex* ivertex, float minFeatureSize)
// merges close ivertices
// why to do it: eliminates negative effect of needle, both triangles around needle are interpolated as if there is no needle
// returns number of merges (each merge = 1 ivertex reduced)
/* first plan of algorithm:
vertex2ivertex: kazdy vertex ma seznamek do kterych patri ivertexu
ivertices: mam seznam ivertexu, ke kazdemu pridam
  vaha = 1
  seznam sousednich vertexu na ktere se da dojet po hrane
pro kazdy ivertex najdu sousedni ivertexy a vzdalenosti
pokud jsou nejblizsi dva ivertexy dost blizko, spojim je
    nastavit nove teziste
	secist vahy
	spojit seznamy sousednich vertexu
	vyradit ze seznamu vlastni vertexy
	zaktualizovat vertex2ivertex
mozna vznikne potreba interpolovat v ivertexech ne podle corner-uhlu ale i podle corner-plochy (miniaturni a nulovy maj mit nulovou vahu)
  tim by se mohla zlepsit interpolace kdyz se ponechaj degenerovany triangly z tristripu
*/
{
	unsigned ivertices = vertices; // it is expected that ivertex is array of length vertices
	unsigned numReduced = 0;

	// gather all source information for each ivertex
	// - center
	// - set of vertices
	// - set of neighbour vertices
	IVertexInfo* ivertexInfo = new IVertexInfo[ivertices];
	for(unsigned i=0;i<ivertices;i++)
	{
		ivertex[i].fillInfo(this,i,ivertexInfo[i]);
	}
	unsigned* vertex2ivertex = new unsigned[vertices];
	for(unsigned i=0;i<ivertices;i++)
	{
		vertex2ivertex[i] = i;
	}

	// work until done
	unsigned ivertex1Idx = 0;
	while(1)
	{
		// find closest ivertex - ivertex pair
		real minDist = 1e30f;
		unsigned minIVert1 = 0;
		unsigned minIVert2 = 0;
		for(;ivertex1Idx<ivertices;ivertex1Idx++)
			if(ivertexInfo[ivertex1Idx].ourVertices.size())
		{
			RR_ASSERT(ivertexInfo[ivertex1Idx].ivertex->getNumCorners()!=0xfeee);
			RR_ASSERT(ivertexInfo[ivertex1Idx].ivertex->getNumCorners()!=0xcdcd);
			// for each neighbour vertex
			for(std::set<unsigned>::const_iterator i=ivertexInfo[ivertex1Idx].neighbourVertices.begin();i!=ivertexInfo[ivertex1Idx].neighbourVertices.end();i++)
			{
				// convert to neighbour ivertex
				unsigned vertex2Idx = *i;
				unsigned ivertex2Idx = vertex2ivertex[vertex2Idx];
				RR_ASSERT(ivertex1Idx!=ivertex2Idx);
				// measure distance
				real dist = size(ivertexInfo[ivertex1Idx].center-ivertexInfo[ivertex2Idx].center);
				RR_ASSERT(dist); // teoreticky muze nastat kdyz objekt obsahuje vic vertexu na stejnem miste
				// store the best
				if(dist<minDist)
				{
					minDist = dist;
					minIVert1 = ivertex1Idx;
					minIVert2 = ivertex2Idx;
				}
			}
			if(minDist<=minFeatureSize) break;
		}

		// end if not close enough
		if(minDist>minFeatureSize) break;
		numReduced++;

		// merge ivertices: update local temporary vertex2ivertex
		for(std::set<unsigned>::const_iterator i=ivertexInfo[minIVert2].ourVertices.begin();i!=ivertexInfo[minIVert2].ourVertices.end();i++)
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
		if(ivertexInfo[minIVert1].ourVertices.size()>100)
		{
			LIMITED_TIMES(1,warned=true;RRReporter::report(WARN,"Cluster of more than 100 vertices smoothed.\n"));
		}
		if(numReduced>unsigned(vertices*0.9f))
		{
			LIMITED_TIMES(1,warned=true;RRReporter::report(WARN,"More than 90%% of vertices removed in smoothing.\n"));
		}
		if(warned)
		{
			RRVec3 mini,maxi,center;
			importer->getCollider()->getMesh()->getAABB(&mini,&maxi,&center);
			RRReporter::report(INF1,"Scene stats: numVertices=%d  size=%fm x %fm x %fm  minFeatureSize=%fm (check what you enter into RRDynamicSolver::setStaticObjects() or RRStaticSolver())\n",vertices,maxi[0]-mini[0],maxi[1]-mini[1],maxi[2]-mini[2],minFeatureSize);
			if(sum(abs(maxi-mini))>5000) RRReporter::report(WARN,"Possibly scene too big (wrong scale)?\n");
			if(sum(abs(maxi-mini))<0.5) RRReporter::report(WARN,"Possibly scene too small (wrong scale)?\n");
		}
	}

	// delete temporaries
	delete[] vertex2ivertex;
	delete[] ivertexInfo;
	return numReduced;
}
#endif

void Object::buildTopIVertices(unsigned smoothMode, float minFeatureSize, float maxSmoothAngle)
{
	// check
	for(unsigned t=0;t<triangles;t++)
	{
		for(int v=0;v<3;v++)
			RR_ASSERT(!triangle[t].topivertex[v]);
	}

	// build 1 ivertex for each vertex, insert all corners
	IVertex *topivertex=new IVertex[vertices];
	RRMesh* meshImporter = importer->getCollider()->getMesh();
	for(unsigned t=0;t<triangles;t++) if(triangle[t].surface)
	{
		RRMesh::Triangle un_ve; // un_ = unrotated
		meshImporter->getTriangle(t,un_ve);
		for(int ro_v=0;ro_v<3;ro_v++) // ro_ = rotated 
		{
			unsigned un_v = un_ve[ro_v];
			RR_ASSERT(un_v<vertices);
			triangle[t].topivertex[ro_v]=&topivertex[un_v];
			Angle angle=angleBetween(
			  *triangle[t].getVertex((ro_v+1)%3)-*triangle[t].getVertex(ro_v),
			  *triangle[t].getVertex((ro_v+2)%3)-*triangle[t].getVertex(ro_v));
			topivertex[un_v].insert(&triangle[t],true,angle,
			  *triangle[t].getVertex(ro_v)
			  );
		}
	}

	// merge close ivertices into 1 big + 1 empty
	// (empty will be later detected and reported as unused)
	unsigned numIVertices = vertices;
	//printf("IVertices loaded: %d\n",numIVertices);
#ifdef SUPPORT_MIN_FEATURE_SIZE
	// volano jen pokud ma neco delat -> malinka uspora casu
	if(minFeatureSize>0)
	{
		// Pouha existence nasledujiciho radku (mergeCloseIVertices) i kdyz se nikdy neprovadi
		// zpomaluje cube v MSVC o 8%.
		// Nevyresena zahada.
		// Ona fce je jedine misto pouzivajici exceptions, ale exceptions jsou vyple (jejich zapnuti zpomali o dalsich 12%).
		numIVertices -= mergeCloseIVertices(topivertex,minFeatureSize);
		//printf("IVertices after merge close: %d\n",numIVertices);
	}
#endif

	// split ivertices with too different normals
	for(unsigned v=0;v<vertices;v++)
	{
		RRMesh::Vertex vert;
		meshImporter->getVertex(v,vert);
		switch(smoothMode)
		{
			case 0:
				numIVertices += topivertex[v].splitTopLevelByAngleOld((RRVec3*)&vert,this,maxSmoothAngle);
				break;
			case 1:
			default:
				numIVertices += topivertex[v].splitTopLevelByAngleNew((RRVec3*)&vert,this,maxSmoothAngle);
				break;
//			default:
//				numIVertices += topivertex[v].splitTopLevelByNormals((RRVec3*)&vert,this);
		}
		// check that splitted topivertex is no more referenced
		/*for(unsigned t=0;t<triangles;t++) if(triangle[t].surface)
		{
			for(unsigned i=0;i<3;i++)
			{
				RR_ASSERT(triangle[t].topivertex[i]!=&topivertex[v]);
			}
		}*/
	}
	//printf("IVertices after splitting: %d\n",numIVertices);

	// delete local array topivertex
	for(unsigned t=0;t<triangles;t++) if(triangle[t].surface)
	{
		for(unsigned i=0;i<3;i++)
		{
			// check that local array topivertex is not referenced here
			unsigned idx = (unsigned)(triangle[t].topivertex[i]-topivertex);
			RR_ASSERT(idx>=vertices);
		}
	}
	delete[] topivertex;

	// check triangle.topivertex validity
	for(unsigned t=0;t<triangles;t++)
		for(int v=0;v<3;v++)
		{
			if(triangle[t].topivertex[v])
			{
				RR_ASSERT(triangle[t].topivertex[v]->getNumCorners()!=0xcdcd);
				RR_ASSERT(triangle[t].topivertex[v]->getNumCorners()!=0xfeee);
			}
		}
}

} // namespace


