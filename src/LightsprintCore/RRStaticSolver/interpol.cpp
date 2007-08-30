
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

/*#include <windows.h>
#undef assert
#define RR_ASSERT(a) {if(!(a)) DebugBreak();}
//#define RR_ASSERT(a) {if(!(a) && IsDebuggerPresent()) DebugBreak();}
char *FS(char *fmt, ...)
{
	static char msg[1000];
	va_list argptr;
	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);
	return msg;
}*/
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

//#define LOG_LOADING_MES
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
		if(old) *(IVertex**)IVertexPool=old;
	}
	return &IVertexPool[IVertexPoolItemsUsed++];
}

void Object::deleteIVertices()
{
	while(IVertexPool)
	{
		IVertex *old=*(IVertex**)IVertexPool;
		delete[] IVertexPool;
		IVertexPool=old;
	}
	IVertexPool=0;
	IVertexPoolItems=0;
	IVertexPoolItemsUsed=0;
}

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
#ifdef IV_POINT
	point=Point3(0,0,0);
#endif
	cacheTime=__frameNumber-1;
	cacheValid=0;
	__iverticesAllocated++;
	__cornersAllocated+=cornersAllocated();
	loaded=false;
	saved=false;
	important=false;
	error=0;//musi nulovat protoze novy ivertexy vznikaj i v dobe kdy se zrovna errory akumulujou
}

unsigned IVertex::cornersAllocated()
{
	RR_ASSERT(cornersAllocatedLn2<14);
	return 1<<cornersAllocatedLn2;
}

bool IVertex::check()
{
	RR_ASSERT(corners!=0xfeee);
	RR_ASSERT(corners!=0xcdcd);
	return true;//powerTopLevel>0;
}

bool IVertex::check(Point3 apoint)
{
	// kdyz transformuje objekty misto strel, kontrolni pointy
	// v ivertexech zustanou neztransformovany a nelze je checkovat
#ifdef IV_POINT
	if(size2(point)) RR_ASSERT(size2(point-apoint)<MAX_NEIGHBOUR_DISTANCE);
#endif
	return true;
}

void IVertex::insert(Node *node,bool toplevel,real power,Point3 apoint)
{
	RR_ASSERT(this);
	// new interpolation scheme "power*=node->area" now doesn't work with subdivision
	if(node->grandpa->object->subdivisionSpeed==0)
		power *= node->area;

#ifdef IV_POINT
	if(size2(apoint))
	{
		//RR_ASSERT(power<M_PI+0.001); had sense before power*=node->area
		if(size2(point)==0) point=apoint; else
		  RR_ASSERT(size2(point-apoint)<MAX_NEIGHBOUR_DISTANCE);
	}
#endif
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


void IVertex::insertAlsoToParents(Node *node,bool toplevel,real power,Point3 apoint)
{
	insert(node,toplevel,power,apoint);
	while((node=node->parent)) insert(node,false,power);
}

bool IVertex::contains(Node *node)
{
	for(unsigned i=0;i<corners;i++)
		if(corner[i].node==node) return true;
	return false;
}

unsigned IVertex::splitTopLevelByAngleOld(Vec3 *avertex, Object *obj, float maxSmoothAngle)
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
		RR_ASSERT(corner[i].node->grandpa);
		RR_ASSERT(corner[i].node->grandpa->surface);
		unsigned found=0;
		topivertex[i]=NULL;
		topivertex[i+corners]=NULL;
		topivertex[i+2*corners]=NULL;
		for(int k=0;k<3;k++)
		{
			if(TRIANGLE(corner[i].node)->topivertex[k]==this)
			{
#ifndef SUPPORT_MIN_FEATURE_SIZE // s min feature size muze ukazovat vic vrcholu na jeden ivertex
				RR_ASSERT(!found);
#endif
				topivertex[i+found*corners]=&TRIANGLE(corner[i].node)->topivertex[k];
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
#ifdef IV_POINT
		v->point=point;
#endif
		for(unsigned j=0;j<corners;j++)
			if(INTERPOL_BETWEEN(corner[i].node,corner[j].node))
				v->insertAlsoToParents(corner[j].node,true,corner[j].power);
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

unsigned IVertex::splitTopLevelByAngleNew(Vec3 *avertex, Object *obj, float maxSmoothAngle)
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
#ifdef IV_POINT
		v->point = point;
#endif
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
				v->insertAlsoToParents((*i)->node,true,(*i)->power);
				// oprav pointery z nodu na stary ivertex
				Node* node = (*i)->node;
				if(IS_TRIANGLE(node))
				{
					Triangle* triangle = TRIANGLE(node);
					for(unsigned k=0;k<3;k++)
						if(triangle->topivertex[k]==this)
							triangle->topivertex[k] = v;
				}
				else
					RR_ASSERT(0);
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

/*
unsigned IVertex::splitTopLevelByNormals(Vec3 *avertex, Object *obj)
{
	// input: ivertex filled with triangle corners (ivertex is installed in all his corners)
	// job: remove this ivertex and install new reduced ivertices (split big into set of smaller)
	// return: number of new ivertices

	// vsechny cornery vlozi do setu radiciho podle normaly
	// vzniknou skupinky se stejnou normalou
	// z kazde skupinky udela jeden ivertex

	// spoleha se tu na to, ze multiObjekt kdyz slouci vic vertexu do jednoho,
	//  vsem trianglevertexum ktere je sdilely da stejnou normalu, nejlepe vzniklou zprumerovanim starych
	// multiObjekt to ale nedela
	// spoleha se tedy na to, ze se vzdy slouci jen vertexy uvnitr roviny
	//  pro vertexy mimo rovinu (napr na valci) budou vysledky spatne, vznikne nesesmoothovana hrana

	// pro kazdy corner si zjistime jeho normalu
	struct CornerNormal
	{
		Corner* corner;
		Vec3 normal;
		int operator <(const CornerNormal& b) const
		{
			return memcmp(&normal,&b.normal,sizeof(normal));
		}
	};
	std::set<CornerNormal> cornersLeft;
	for(unsigned i=0;i<corners;i++) 
	{
		CornerNormal tmp;
		tmp.corner = &corner[i];
		RRObject::TriangleNormals normals;
		obj->importer->getTriangleNormals(obj->getTriangleIndex(corner[i].node->grandpa),normals);
		tmp.normal = normals[?]; nemam u corneru ulozeno ktery vrchol nody to je, zde to uz nelze zjistit, bylo by nutne to sem nekudy pracne protlacit
		cornersLeft.insert(tmp);
	}
	unsigned numSplitted = 0;
	//while zbyvaji cornery
	while(cornersLeft.size())
	{
		// set=empty
		// zaloz ivertex s timto setem corneru
		IVertex *v = obj->newIVertex();
		Vec3 vNormal;
		numSplitted++;
#ifdef IV_POINT
		v->point = point;
#endif
		// for each zbyvajici corner
restart_iter:
		for(std::set<CornerNormal>::iterator i=cornersLeft.begin();i!=cornersLeft.end();i++)
		{
			if(!v->corners || (*i).normal==vNormal)
			{
					vNormal = (*i).normal;
					// vloz corner do noveho ivertexu
					v->insertAlsoToParents((*i).corner->node,true,(*i).corner->power);
					// oprav pointery z nodu na stary ivertex
					Node* node = (*i).corner->node;
					if(IS_TRIANGLE(node))
					{
						Triangle* triangle = TRIANGLE(node);
						for(unsigned k=0;k<3;k++)
							if(triangle->topivertex[k]==this)
								triangle->topivertex[k] = v;
					}
					else
						RR_ASSERT(0);
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
*/

void IVertex::makeDirty()
{
	for(unsigned i=0;i<corners;i++)
		corner[i].node->flags|=FLAG_DIRTY_IVERTEX+FLAG_DIRTY_SOME_SUBIVERTICES;
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

		// fast path for realtime radiosity
		//RR_ASSERT(measure.indirect && !measure.direct && !subdivisionEnabled);
		//for(unsigned i=0;i<corners;i++)
		//{
		//	Node* node=corner[i].node;
		//	irrad += (node->totalIncidentFlux-TRIANGLE(node)->getSourceIncidentFlux())*(corner[i].power/corner[i].node->area);
		//}

		// full path
		for(unsigned i=0;i<corners;i++)
		{
			Node* node=corner[i].node;
			RR_ASSERT(node);
			// a=source+reflected incident flux in watts
			Channels a=node->totalIncidentFlux;
			RR_ASSERT(IS_CHANNELS(a));
			if(node->sub[0])
			{
				RR_ASSERT(node->sub[1]);
				a-=node->sub[0]->totalIncidentFlux+node->sub[1]->totalIncidentFlux;
			}
			RR_ASSERT(IS_CHANNELS(a));
			// s=source incident flux in watts
			Channels s=IS_TRIANGLE(node) ? TRIANGLE(node)->getSourceIncidentFlux() : Channels(0);
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

// exitance in W/m^2, exitting power density
// differs for different corners, depends on corner material
Channels IVertex::exitance(Node* corner)
{
	return irradiance(
		RRRadiometricMeasure(+0,+0,+0,1,1) // don't care if it's exiting, scaled or flux
		)*corner->grandpa->surface->diffuseReflectance;
}

void IVertex::loadCache(Channels r)
{
	cache=r;
	cacheTime=__frameNumber;
	cacheValid=1;
}

bool IVertex::remove(Node *node,bool toplevel)
// returns true when last corner removed (-> ivertex should be deleted)
//!!! vsechna volani nastavuji do toplevel false
{
	bool removed=false;
	for(unsigned i=0;i<corners;i++)
		if(corner[i].node==node)
		{
			if(toplevel) powerTopLevel-=corner[i].power;
			RR_ASSERT(powerTopLevel>-0.0000001);
			RR_ASSERT(powerTopLevel>-0.00001);
			corner[i]=corner[--corners];
			removed=true;
		}
//!!!	RR_ASSERT(IS_CLUSTER(node) || removed);// cluster je insertovan a odebiran vickrat (kazdy triangl pridava svou power), prvni odber z IVertexu odebere celou power, ostatni odbery nechame projit naprazdno a nekricime ze je to chyba
// assertuje pri ruseni sceny ve fcss koupelna4, ale zadny leak to nezpusobuje
	//if(powerTopLevel<0.00001) corners=0;
	return corners==0;
}

bool IVertex::isEmpty()
{
	return corners==0;
}

IVertex::~IVertex()
{
	//!!!RR_ASSERT(corners==0);
	free(corner);
	__cornersAllocated-=cornersAllocated();
	__iverticesAllocated--;
}

//////////////////////////////////////////////////////////////////////////////
//
// subtriangle, part of triangle

IVertex *SubTriangle::ivertex(int i)
{
/*
	RR_ASSERT(i>=0 && i<3);
	if(IS_TRIANGLE(this)) return TRIANGLE(this)->topivertex[i];
	RR_ASSERT(parent);
	RR_ASSERT(parent->sub[0]);
	RR_ASSERT(parent->sub[1]);
	SubTriangle *pa=SUBTRIANGLE(parent);
	bool right=isRight();
	int rot=pa->getSplitVertex();
	IVertex *tmp;
	switch(i)
	{
	   case 0:tmp = pa->ivertex(rot); break;
	   case 1:if(right) tmp = pa->ivertex((rot+1)%3); else tmp = pa->subvertex; break;
	   default:RR_ASSERT(i==2);
		  if(right) tmp = pa->subvertex; else tmp = pa->ivertex((rot+2)%3);
	}
	return tmp;
*/
	RR_ASSERT(IS_PTR(this));
	RR_ASSERT(i>=0 && i<3);
	IVertex *tmp;
	if(IS_TRIANGLE(this))
	{
		RR_ASSERT(TRIANGLE(this)->surface);
		tmp = TRIANGLE(this)->topivertex[i];
		TRACE(FS("(%x,%d,%x,%d)",this,i,tmp,RRIntersectStats::getInstance()->intersects));
		RR_ASSERT(tmp);
	}
	else
	{
		RR_ASSERT(parent);
		RR_ASSERT(parent->sub[0]);
		RR_ASSERT(parent->sub[1]);
//		SubTriangle *pa=SUBTRIANGLE(parent);
//		bool right=isRight();
		switch(i)
		{
			case 0:
				{
				SubTriangle *pa=SUBTRIANGLE(parent);
				int rot=pa->getSplitVertex();
				tmp = pa->ivertex(rot);
				RR_ASSERT(tmp);
				break;
				}
			case 1:
				{
				SubTriangle *pa=SUBTRIANGLE(parent);
				if(isRight())
				{
					int rot=pa->getSplitVertex();
					tmp = pa->ivertex((rot+1)%3);
					RR_ASSERT(tmp);
				}
				else 
				{
					tmp = pa->subvertex;
					RR_ASSERT(tmp);
				}
				break;
				}
			default:
				{
				RR_ASSERT(i==2);
				SubTriangle *pa=SUBTRIANGLE(parent);
				if(isRight())
				{
					tmp = pa->subvertex;
					RR_ASSERT(tmp);
				}
				else
				{
					int rot=pa->getSplitVertex();
					tmp = pa->ivertex((rot+2)%3);
					RR_ASSERT(tmp);
				}
				}
		}
	}
	RR_ASSERT(tmp);
	return tmp;
}

bool SubTriangle::checkVertices()
{
	RR_ASSERT(this);
	if(IS_TRIANGLE(this)) RR_ASSERT(TRIANGLE(this)->surface);
	IVertex *v;

	TRACE("\n0");
	v=ivertex(0);
	TRACE("1\n");
	RR_ASSERT(this);
	RR_ASSERT(v);
	v->check(to3d(0));

	v=ivertex(1);
	RR_ASSERT(this);
	RR_ASSERT(v);
	v->check(to3d(1));

	v=ivertex(2);
	RR_ASSERT(this);
	RR_ASSERT(v);
	v->check(to3d(2));

	return true;
}

void SubTriangle::installVertices()
{
	IVertex *v;
	v=ivertex(0);RR_ASSERT(v);
	v->insert(this,false,angleBetween(u2,v2),to3d(0));
	v=ivertex(1);RR_ASSERT(v);
	v->insert(this,false,angleBetween(v2-u2,-u2),to3d(1));
	v=ivertex(2);RR_ASSERT(v);
	v->insert(this,false,angleBetween(-v2,u2-v2),to3d(2));
}

SubTriangle *downWhereSideSplits(SubTriangle *s,int *side,IVertex *newVertex)
//insertne vsechny pod this az po ten co vraci
//this ne
{
	real sideLength1;
	switch(*side)
	{
		case 0:sideLength1=size(s->u2);break;
		case 1:sideLength1=size(s->v2-s->u2);break;
		case 2:sideLength1=size(s->v2);break;
		default:RR_ASSERT(0);
	}
	while(true)
	{
		RR_ASSERT(s);
		int splitVertex=s->getSplitVertexSlow();
		if(*side==(splitVertex+1)%3) break;
		s->splitGeometry(NULL);
		s=SUBTRIANGLE(s->sub[( (*side==splitVertex) == s->isRightLeft() )?0:1]);
		*side=(*side==splitVertex)?0:2;
		newVertex->insert(s,false,M_PI);
	}
	real sideLength2;
	switch(*side)
	{
		case 0:sideLength2=size(s->u2);break;
		case 1:sideLength2=size(s->v2-s->u2);break;
		case 2:sideLength2=size(s->v2);break;
		default:RR_ASSERT(0);
	}
	RR_ASSERT(fabs(sideLength1-sideLength2)<0.0001);
	RR_ASSERT(*side==(s->getSplitVertexSlow()+1)%3);
	return s;
}

SubTriangle *Triangle::getNeighbourTriangle(int myside,int *nbsside,IVertex *newVertex)// myside 0=u2, 1=v2-u2, 2=-v2
//insertne tenhle, to co vraci a vsechno nad nima
{
	SubTriangle *neighbourSub=NULL;
	if(edge[myside] && edge[myside]->interpol)
	{
		Triangle *neighbourTri=edge[myside]->triangle[edge[myside]->triangle[0]==this?1:0];
		newVertex->insertAlsoToParents(neighbourTri,true,M_PI);
		if(neighbourTri->edge[0]==edge[myside])
		{
			*nbsside=0;
		}
		else
		if(neighbourTri->edge[1]==edge[myside])
		{
			*nbsside=1;
		}
		else
		{
			RR_ASSERT(neighbourTri->edge[2]==edge[myside]);
			*nbsside=2;
		}
		RR_ASSERT(getVertex(myside)==neighbourTri->getVertex((*nbsside+1)%3));
		RR_ASSERT(getVertex((myside+1)%3)==neighbourTri->getVertex(*nbsside));
		neighbourSub=downWhereSideSplits(neighbourTri,nbsside,newVertex);
	}
	newVertex->insertAlsoToParents(this,true,M_PI);
	return neighbourSub;
}

SubTriangle *SubTriangle::getNeighbourSubTriangle(int myside,int *nbsside,IVertex *newVertex)// myside 0=u2, 1=v2-u2, 2=-v2
//insertne tenhle, to co vraci a vsechno nad nima
{
#ifndef NDEBUG
	Point3 my_a=to3d(myside);
	Point3 my_b=to3d((myside+1)%3);
#endif
	// is this triangle?
	if(IS_TRIANGLE(this))
	{
		SubTriangle *neighbour=TRIANGLE(this)->getNeighbourTriangle(myside,nbsside,newVertex);
#ifndef NDEBUG
		if(neighbour)
		{
			Point3 nb_a=neighbour->to3d((*nbsside+1)%3);
			Point3 nb_b=neighbour->to3d(*nbsside);
			RR_ASSERT(size(nb_a-my_a)<MAX_NEIGHBOUR_DISTANCE);
			RR_ASSERT(size(nb_b-my_b)<MAX_NEIGHBOUR_DISTANCE);
			RR_ASSERT(*nbsside==(neighbour->getSplitVertexSlow()+1)%3);
		}
#endif
		return neighbour;
	}
	// is brother neighbour?
	newVertex->insert(this,false,M_PI);
	bool right=isRight();
	if((myside==0 && !right) || (myside==2 && right))
	{
		*nbsside=right?0:2;
		newVertex->insertAlsoToParents(parent,true,2*M_PI);
		SubTriangle *neighbour=brotherSub();
		newVertex->insert(neighbour,false,M_PI);
		neighbour=downWhereSideSplits(neighbour,nbsside,newVertex);
#ifndef NDEBUG
		Point3 nb_a=neighbour->to3d((*nbsside+1)%3);
		Point3 nb_b=neighbour->to3d(*nbsside);
		RR_ASSERT(size(nb_a-my_a)<MAX_NEIGHBOUR_DISTANCE);
		RR_ASSERT(size(nb_b-my_b)<MAX_NEIGHBOUR_DISTANCE);
		RR_ASSERT(*nbsside==(neighbour->getSplitVertexSlow()+1)%3);
#endif
		return neighbour;
	}
	// none -> ask parent for help
	/*
	myside==0 && !right   brother
	myside==1 && !right   plus=+1 pulka
	myside==2 && !right   plus=+2
	myside==0 &&  right   plus=+0
	myside==1 &&  right   plus=+1 pulka
	myside==2 &&  right   brother
	*/
	int passide=(SUBTRIANGLE(parent)->getSplitVertex()+myside)%3;
	SubTriangle *neighbour=SUBTRIANGLE(parent)->getNeighbourSubTriangle(passide,nbsside,newVertex);
	if(neighbour)
	{
		if(myside==1)
		{
			RR_ASSERT(neighbour->sub[0]);
			RR_ASSERT(SUBTRIANGLE(parent)->subvertex==neighbour->subvertex);
			RR_ASSERT((neighbour->getSplitVertex()+1)%3==*nbsside);
			neighbour=SUBTRIANGLE(neighbour->sub[(neighbour->isRightLeft()==right)?1:0]);
			newVertex->insert(neighbour,false,M_PI);
			*nbsside=1;
			neighbour=downWhereSideSplits(neighbour,nbsside,newVertex);
			RR_ASSERT(*nbsside==(neighbour->getSplitVertexSlow()+1)%3);
		}
#ifndef NDEBUG
		Point3 nb_a=neighbour->to3d((*nbsside+1)%3);
		Point3 nb_b=neighbour->to3d(*nbsside);
		RR_ASSERT(size(nb_a-my_a)<MAX_NEIGHBOUR_DISTANCE);
		RR_ASSERT(size(nb_b-my_b)<MAX_NEIGHBOUR_DISTANCE);
		RR_ASSERT(*nbsside==(neighbour->getSplitVertexSlow()+1)%3);
#endif
	}
	return neighbour;
}

void SubTriangle::createSubvertex(IVertex *asubvertex,int rot)
{
#ifdef IV_POINT
	Point3 apoint=(ivertex((rot+1)%3)->point+ivertex((rot+2)%3)->point)/2;
#endif
	RR_ASSERT(!subvertex);
	if(asubvertex)
	{
		RR_ASSERT(IS_PTR(asubvertex));
		subvertex=asubvertex;
		RR_ASSERT(subvertex->check(to3d(SUBTRIANGLE(sub[0])->uv[isRightLeft()?2:1])));
#ifdef IV_POINT
		RR_ASSERT(subvertex->point==apoint);
#endif
	}
	else
	{
		subvertex=grandpa->object->newIVertex();
#ifdef IV_POINT
		subvertex->point=apoint;
#endif
#ifndef NDEBUG
		RR_ASSERT(rot>=0 && rot<=2);
		Point3 my_a=to3d((rot+1)%3);
		Point3 my_b=to3d((rot+2)%3);
#endif
		int nbsside;
		SubTriangle *neighbour=getNeighbourSubTriangle((rot+1)%3,&nbsside,subvertex);
		if(neighbour)
		{
			RR_ASSERT(!neighbour->sub[0]);
			RR_ASSERT(!neighbour->subvertex);
			RR_ASSERT(neighbour->checkVertices());
			RR_ASSERT(nbsside==(neighbour->getSplitVertexSlow()+1)%3);
			switch((nbsside+2)%3/*==neighbour->getSplitVertexSlow()*/)
			{
				case 0:RR_ASSERT(subvertex->check(neighbour->to3d(neighbour->uv[0]+neighbour->u2/2+neighbour->v2/2)));break;
				case 1:RR_ASSERT(subvertex->check(neighbour->to3d(neighbour->uv[0]+neighbour->v2/2)));break;
				case 2:RR_ASSERT(subvertex->check(neighbour->to3d(neighbour->uv[0]+neighbour->u2/2)));break;
			}
			neighbour->splitGeometry(subvertex);
#ifndef NDEBUG
			RR_ASSERT(nbsside>=0 && nbsside<=2);
			Point3 nb_a=neighbour->to3d((nbsside+1)%3);
			Point3 nb_b=neighbour->to3d(nbsside);
			RR_ASSERT(size(nb_a-my_a)<MAX_NEIGHBOUR_DISTANCE);
			RR_ASSERT(size(nb_b-my_b)<MAX_NEIGHBOUR_DISTANCE);
#endif
		}
	}
	RR_ASSERT(SUBTRIANGLE(sub[0])->checkVertices());
	RR_ASSERT(SUBTRIANGLE(sub[1])->checkVertices());
	// uses subvertex->point, must be called after we set it
	RR_ASSERT(this);
	SUBTRIANGLE(sub[1])->installVertices();
	SUBTRIANGLE(sub[0])->installVertices();
}

void SubTriangle::removeFromIVertices(Node *node)
{
	if(subvertex)
		subvertex->remove(node,false);
	if(sub[0])
	{
		SUBTRIANGLE(sub[0])->removeFromIVertices(node);
		SUBTRIANGLE(sub[1])->removeFromIVertices(node);
	}
}

void Triangle::removeFromIVertices(Node *node)
{
	topivertex[0]->remove(node,false);
	topivertex[1]->remove(node,false);
	topivertex[2]->remove(node,false);
	SubTriangle::removeFromIVertices(node);
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
		RR_ASSERT(IS_TRIANGLE(corner[c].node));
		if(!IS_TRIANGLE(corner[c].node)) continue;
		Triangle* tri = TRIANGLE(corner[c].node);
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
		if(IS_TRIANGLE(aivertex->corner[c].node)) // we handle only triangles as we know only triangles get here
		{
			Triangle* tri = TRIANGLE(aivertex->corner[c].node);
			for(unsigned i=0;i<3;i++)
			{
				if(tri->topivertex[i]==aivertex)
					tri->topivertex[i]=this;
			}

		} else RR_ASSERT(0);
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
			RRReporter::report(INF1,"Scene stats: numVertices=%d  size=%fm x %fm x %fm  minFeatureSize=%fm (check what you enter into RRDynamicSolver::setObjects() or RRStaticSolver())\n",vertices,maxi[0]-mini[0],maxi[1]-mini[1],maxi[2]-mini[2],minFeatureSize);
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
		RR_ASSERT(!triangle[t].subvertex);
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
	check();
	// volano jen pokud ma neco delat -> malinka uspora casu
	if(minFeatureSize>0)
	{
		// Pouha existence nasledujiciho radku (mergeCloseIVertices) i kdyz se nikdy neprovadi
		// zpomaluje cube v MSVC o 8%.
		// Nevyresena zahada.
		// Ona fce je jedine misto pouzivajici exceptions, ale exceptions jsou vyple (jejich zapnuti zpomali o dalsich 12%).
		numIVertices -= mergeCloseIVertices(topivertex,minFeatureSize);
		check();
		//printf("IVertices after merge close: %d\n",numIVertices);
	}
#endif

	// split ivertices with too different normals
	int unusedVertices=0;
	for(unsigned v=0;v<vertices;v++)
	{
		RRMesh::Vertex vert;
		meshImporter->getVertex(v,vert);
		if(!topivertex[v].check(vert)) unusedVertices++;
		switch(smoothMode)
		{
			case 0:
				numIVertices += topivertex[v].splitTopLevelByAngleOld((Vec3*)&vert,this,maxSmoothAngle);
				break;
			case 1:
			default:
				numIVertices += topivertex[v].splitTopLevelByAngleNew((Vec3*)&vert,this,maxSmoothAngle);
				break;
//			default:
//				numIVertices += topivertex[v].splitTopLevelByNormals((Vec3*)&vert,this);
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
	check();

	// report unused vertices
	if(unusedVertices)
	{
		fprintf(stderr,"Scene contains %i never used vertices.\n",unusedVertices);
	}

	/*/ for each vertex, store pointer to one of ivertices
	// helper only for fast approximate render
	memset(vertexIVertex,0,sizeof(vertexIVertex[0])*vertices);
	for(unsigned t=0;t<triangles;t++) if(triangle[t].surface)
	{
		// ro_=rotated, un_=unchanged,original from importer
		RRMesh::Triangle un_ve;
		meshImporter->getTriangle(t,un_ve);
		for(int ro_v=0;ro_v<3;ro_v++)
		{
			//RR_ASSERT(triangle[t].topivertex[v]);
			RR_ASSERT(!triangle[t].topivertex[ro_v] || triangle[t].topivertex[ro_v]->error!=-45);
			unsigned un_v = un_ve[(ro_v+triangle[t].rotations)%3];
			RR_ASSERT(un_v<vertices);
			vertexIVertex[un_v]=triangle[t].topivertex[ro_v]; // un[un]=ro[ro]
		}
	}*/

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
	check();

	// check vertexIVertex validity
	for(unsigned v=0;v<vertices;v++)
	{
		//!!! happens with generated ball, reason unknown
		//RR_ASSERT(vertexIVertex[v]);
	}

	// check triangle.topivertex validity
	check();
	for(unsigned t=0;t<triangles;t++)
		for(int v=0;v<3;v++)
		{
			//RR_ASSERT(triangle[t].topivertex[v]); no topivertex allowed for invalid triangles (surface=NULL)
			RR_ASSERT(!triangle[t].topivertex[v] || triangle[t].topivertex[v]->error!=-45);
			if(triangle[t].topivertex[v])
			{
				RR_ASSERT(triangle[t].topivertex[v]->getNumCorners()!=0xcdcd);
				RR_ASSERT(triangle[t].topivertex[v]->getNumCorners()!=0xfeee);
			}
		}

	check();
}

} // namespace


