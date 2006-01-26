
#include <assert.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

/*#include <windows.h>
#undef assert
#define assert(a) {if(!(a)) DebugBreak();}
//#define assert(a) {if(!(a) && IsDebuggerPresent()) DebugBreak();}
char *FS(char *fmt, ...)
{
	static char msg[1000];
	va_list argptr;
	va_start (argptr,fmt);
	vsprintf (msg,fmt,argptr);
	va_end (argptr);
	return msg;
}*/
#define TRACE(a) //{if(rrCollider::RRIntersectStats::getInstance()->intersects>=478988) OutputDebugString(a);}

#include "rrcore.h"
#include "interpol.h"

namespace rrVision
{

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
	assert(this);
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
	assert(cornersAllocatedLn2<14);
	return 1<<cornersAllocatedLn2;
}

bool IVertex::check()
{
	assert(corners!=0xfeee);
	assert(corners!=0xcdcd);
	return true;//powerTopLevel>0;
}

bool IVertex::check(Point3 apoint)
{
	// kdyz transformuje objekty misto strel, kontrolni pointy
	// v ivertexech zustanou neztransformovany a nelze je checkovat
#ifdef IV_POINT
	if(size2(point)) assert(size2(point-apoint)<MAX_NEIGHBOUR_DISTANCE);
#endif
	return true;
}

void IVertex::insert(Node *node,bool toplevel,real power,Point3 apoint)
{
	assert(this);
	// new interpolation scheme "power*=node->area" now doesn't works with subdivision
	if(RRGetStateF(RRSSF_SUBDIVISION_SPEED)==0)
		power *= node->area;

	if(node->grandpa && node->grandpa->isNeedle) power=0; // ignorovat prispevky jehel
#ifdef IV_POINT
	if(size2(apoint))
	{
		//assert(power<M_PI+0.001); had sense before power*=node->area
		if(size2(point)==0) point=apoint; else
		  assert(size2(point-apoint)<MAX_NEIGHBOUR_DISTANCE);
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
			assert(!node->grandpa);// pouze clustery se smej insertovat vickrat, power se akumuluje
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

unsigned IVertex::splitTopLevel(Vec3 *avertex, Object *obj)
{
	// input: ivertex filled with triangle corners (ivertex is installed in all his corners)
	// job: remove this ivertex and install new reduced ivertices

	unsigned numSplitted = 0;
	// remember all places this ivertex is installed in
	IVertex ***topivertex=new IVertex**[3*corners];
	for(unsigned i=0;i<corners;i++)
	{
		assert(corner[i].node);
		assert(corner[i].node->grandpa);
		assert(corner[i].node->grandpa->surface);
		unsigned found=0;
		topivertex[i]=NULL;
		topivertex[i+corners]=NULL;
		topivertex[i+2*corners]=NULL;
		for(int k=0;k<3;k++)
		{
			if(TRIANGLE(corner[i].node)->topivertex[k]==this)
			{
#ifndef SUPPORT_MIN_FEATURE_SIZE // s min feature size muze ukazovat vic vrcholu na jeden ivertex
				assert(!found);
#endif
				topivertex[i+found*corners]=&TRIANGLE(corner[i].node)->topivertex[k];
				found++;
			}
		}
		assert(found);
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

#ifndef ONLY_PLAYER

void IVertex::makeDirty()
{
	for(unsigned i=0;i<corners;i++)
		corner[i].node->flags|=FLAG_DIRTY_IVERTEX+FLAG_DIRTY_SOME_SUBIVERTICES;
}

// irradiance in W/m^2, incident power density
// is equal in whole vertex, doesn't depend on corner material
Channels IVertex::irradiance()
{
	bool getSource=RRGetState(RRSS_GET_SOURCE)!=0;
	bool getReflected=RRGetState(RRSS_GET_REFLECTED)!=0;
	if(cacheTime!=(__frameNumber&0x1f) || !cacheValid) // cacheTime is byte:5
	{
		//assert(powerTopLevel);
		// irrad=irradiance in W/m^2
		Channels irrad=Channels(0);
		for(unsigned i=0;i<corners;i++)
		{
			Node* node=corner[i].node;
			assert(node);
			// a=source+reflected incident flux in watts
			Channels a=node->energyDirectIncident+node->getEnergyDynamic();
			assert(IS_CHANNELS(a));
			if(node->sub[0])
			{
				assert(node->sub[1]);
				a-=node->sub[0]->energyDirectIncident+node->sub[1]->energyDirectIncident;
			}
			assert(IS_CHANNELS(a));
			// s=source incident flux in watts
			Channels s=IS_TRIANGLE(node) ? TRIANGLE(node)->getSourceIncidentFlux() : Channels(0);
			assert(IS_CHANNELS(s));
			// r=reflected incident flux in watts
			Channels r=a-s;
			assert(IS_CHANNELS(r));
			// w=wanted incident flux in watts
			Channels w=(getSource&&getReflected)?a:( getSource?s: ( getReflected?r:Channels(0) ) );
			irrad+=w/corner[i].node->area*corner[i].power
				/*/ corner[i].node->grandpa->surface->diffuseReflectanceColor*/;
			assert(IS_CHANNELS(irrad));
		}
		cache=powerTopLevel?irrad/powerTopLevel:getClosestIrradiance();//hack for ivertices inside needle - quick search for nearest valid value
		cacheTime=__frameNumber;
		cacheValid=1;
	}
	clampToZero(cache); //!!! obcas tu vznikaji zaporny hodnoty coz je zcela nepripustny!
	assert(IS_CHANNELS(cache));
	return cache;
}

// exitance in W/m^2, exitting power density
// differs for different corners, depends on corner material
Channels IVertex::exitance(Node* corner)
{
	return irradiance()*corner->grandpa->surface->diffuseReflectanceColor;
}

void IVertex::loadCache(Channels r)
{
	cache=r;
	cacheTime=__frameNumber;
	cacheValid=1;
}

#endif

bool IVertex::remove(Node *node,bool toplevel)
// returns true when last corner removed (-> ivertex should be deleted)
//!!! vsechna volani nastavuji do toplevel false
{
	bool removed=false;
	for(unsigned i=0;i<corners;i++)
		if(corner[i].node==node)
		{
			if(toplevel) powerTopLevel-=corner[i].power;
			assert(powerTopLevel>-0.0000001);
			assert(powerTopLevel>-0.00001);
			corner[i]=corner[--corners];
			removed=true;
		}
	assert(IS_CLUSTER(node) || removed);// cluster je insertovan a odebiran vickrat (kazdy triangl pridava svou power), prvni odber z IVertexu odebere celou power, ostatni odbery nechame projit naprazdno a nekricime ze je to chyba
	//if(powerTopLevel<0.00001) corners=0;
	return corners==0;
}

bool IVertex::isEmpty()
{
	return corners==0;
}

IVertex::~IVertex()
{
	//!!!assert(corners==0);
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
	assert(i>=0 && i<3);
	if(IS_TRIANGLE(this)) return TRIANGLE(this)->topivertex[i];
	assert(parent);
	assert(parent->sub[0]);
	assert(parent->sub[1]);
	SubTriangle *pa=SUBTRIANGLE(parent);
	bool right=isRight();
	int rot=pa->getSplitVertex();
	IVertex *tmp;
	switch(i)
	{
	   case 0:tmp = pa->ivertex(rot); break;
	   case 1:if(right) tmp = pa->ivertex((rot+1)%3); else tmp = pa->subvertex; break;
	   default:assert(i==2);
		  if(right) tmp = pa->subvertex; else tmp = pa->ivertex((rot+2)%3);
	}
	return tmp;
*/
	assert(IS_PTR(this));
	assert(i>=0 && i<3);
	IVertex *tmp;
	if(IS_TRIANGLE(this))
	{
		assert(TRIANGLE(this)->surface);
		tmp = TRIANGLE(this)->topivertex[i];
		TRACE(FS("(%x,%d,%x,%d)",this,i,tmp,rrCollider::RRIntersectStats::getInstance()->intersects));
		assert(tmp);
	}
	else
	{
		assert(parent);
		assert(parent->sub[0]);
		assert(parent->sub[1]);
//		SubTriangle *pa=SUBTRIANGLE(parent);
//		bool right=isRight();
		switch(i)
		{
			case 0:
				{
				SubTriangle *pa=SUBTRIANGLE(parent);
				int rot=pa->getSplitVertex();
				tmp = pa->ivertex(rot);
				assert(tmp);
				break;
				}
			case 1:
				{
				SubTriangle *pa=SUBTRIANGLE(parent);
				if(isRight())
				{
					int rot=pa->getSplitVertex();
					tmp = pa->ivertex((rot+1)%3);
					assert(tmp);
				}
				else 
				{
					tmp = pa->subvertex;
					assert(tmp);
				}
				break;
				}
			default:
				{
				assert(i==2);
				SubTriangle *pa=SUBTRIANGLE(parent);
				if(isRight())
				{
					tmp = pa->subvertex;
					assert(tmp);
				}
				else
				{
					int rot=pa->getSplitVertex();
					tmp = pa->ivertex((rot+2)%3);
					assert(tmp);
				}
				}
		}
	}
	assert(tmp);
	return tmp;
}

bool SubTriangle::checkVertices()
{
	assert(this);
	if(IS_TRIANGLE(this)) assert(TRIANGLE(this)->surface);
	IVertex *v;

	TRACE("\n0");
	v=ivertex(0);
	TRACE("1\n");
	assert(this);
	assert(v);
	v->check(to3d(0));

	v=ivertex(1);
	assert(this);
	assert(v);
	v->check(to3d(1));

	v=ivertex(2);
	assert(this);
	assert(v);
	v->check(to3d(2));

	return true;
}

void SubTriangle::installVertices()
{
	IVertex *v;
	v=ivertex(0);assert(v);
	v->insert(this,false,angleBetween(u2,v2),to3d(0));
	v=ivertex(1);assert(v);
	v->insert(this,false,angleBetween(v2-u2,-u2),to3d(1));
	v=ivertex(2);assert(v);
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
		default:assert(0);
	}
	while(true)
	{
		assert(s);
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
		default:assert(0);
	}
	assert(fabs(sideLength1-sideLength2)<0.0001);
	assert(*side==(s->getSplitVertexSlow()+1)%3);
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
			assert(neighbourTri->edge[2]==edge[myside]);
			*nbsside=2;
		}
		assert(getVertex(myside)==neighbourTri->getVertex((*nbsside+1)%3));
		assert(getVertex((myside+1)%3)==neighbourTri->getVertex(*nbsside));
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
			assert(size(nb_a-my_a)<MAX_NEIGHBOUR_DISTANCE);
			assert(size(nb_b-my_b)<MAX_NEIGHBOUR_DISTANCE);
			assert(*nbsside==(neighbour->getSplitVertexSlow()+1)%3);
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
		assert(size(nb_a-my_a)<MAX_NEIGHBOUR_DISTANCE);
		assert(size(nb_b-my_b)<MAX_NEIGHBOUR_DISTANCE);
		assert(*nbsside==(neighbour->getSplitVertexSlow()+1)%3);
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
			assert(neighbour->sub[0]);
			assert(SUBTRIANGLE(parent)->subvertex==neighbour->subvertex);
			assert((neighbour->getSplitVertex()+1)%3==*nbsside);
			neighbour=SUBTRIANGLE(neighbour->sub[(neighbour->isRightLeft()==right)?1:0]);
			newVertex->insert(neighbour,false,M_PI);
			*nbsside=1;
			neighbour=downWhereSideSplits(neighbour,nbsside,newVertex);
			assert(*nbsside==(neighbour->getSplitVertexSlow()+1)%3);
		}
#ifndef NDEBUG
		Point3 nb_a=neighbour->to3d((*nbsside+1)%3);
		Point3 nb_b=neighbour->to3d(*nbsside);
		assert(size(nb_a-my_a)<MAX_NEIGHBOUR_DISTANCE);
		assert(size(nb_b-my_b)<MAX_NEIGHBOUR_DISTANCE);
		assert(*nbsside==(neighbour->getSplitVertexSlow()+1)%3);
#endif
	}
	return neighbour;
}

void SubTriangle::createSubvertex(IVertex *asubvertex,int rot)
{
#ifdef IV_POINT
	Point3 apoint=(ivertex((rot+1)%3)->point+ivertex((rot+2)%3)->point)/2;
#endif
	assert(!subvertex);
	if(asubvertex)
	{
		assert(IS_PTR(asubvertex));
		subvertex=asubvertex;
		assert(subvertex->check(to3d(SUBTRIANGLE(sub[0])->uv[isRightLeft()?2:1])));
#ifdef IV_POINT
		assert(subvertex->point==apoint);
#endif
	}
	else
	{
		subvertex=new IVertex();
#ifdef IV_POINT
		subvertex->point=apoint;
#endif
#ifndef NDEBUG
		assert(rot>=0 && rot<=2);
		Point3 my_a=to3d((rot+1)%3);
		Point3 my_b=to3d((rot+2)%3);
#endif
		int nbsside;
		SubTriangle *neighbour=getNeighbourSubTriangle((rot+1)%3,&nbsside,subvertex);
		if(neighbour)
		{
			assert(!neighbour->sub[0]);
			assert(!neighbour->subvertex);
			assert(neighbour->checkVertices());
			assert(nbsside==(neighbour->getSplitVertexSlow()+1)%3);
			switch((nbsside+2)%3/*==neighbour->getSplitVertexSlow()*/)
			{
				case 0:assert(subvertex->check(neighbour->to3d(neighbour->uv[0]+neighbour->u2/2+neighbour->v2/2)));break;
				case 1:assert(subvertex->check(neighbour->to3d(neighbour->uv[0]+neighbour->v2/2)));break;
				case 2:assert(subvertex->check(neighbour->to3d(neighbour->uv[0]+neighbour->u2/2)));break;
			}
			neighbour->splitGeometry(subvertex);
#ifndef NDEBUG
			assert(nbsside>=0 && nbsside<=2);
			Point3 nb_a=neighbour->to3d((nbsside+1)%3);
			Point3 nb_b=neighbour->to3d(nbsside);
			assert(size(nb_a-my_a)<MAX_NEIGHBOUR_DISTANCE);
			assert(size(nb_b-my_b)<MAX_NEIGHBOUR_DISTANCE);
#endif
		}
	}
	assert(SUBTRIANGLE(sub[0])->checkVertices());
	assert(SUBTRIANGLE(sub[1])->checkVertices());
	// uses subvertex->point, must be called after we set it
	assert(this);
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

#ifndef ONLY_PLAYER
void Cluster::removeFromIVertices(Node *node)
{
	assert(sub[0]);
	assert(sub[1]);
	if(IS_CLUSTER(sub[0])) CLUSTER(sub[0])->removeFromIVertices(node); else TRIANGLE(sub[0])->removeFromIVertices(node);
	if(IS_CLUSTER(sub[1])) CLUSTER(sub[1])->removeFromIVertices(node); else TRIANGLE(sub[1])->removeFromIVertices(node);
}
#endif

#ifdef SUPPORT_MIN_FEATURE_SIZE
void IVertex::fillInfo(Object* object, unsigned originalVertexIndex, IVertexInfo& info)
// used by: merge close ivertices
// fills structure with information about us
// expects ivertex on the beginning of its life - contains all corners around 1 vertex,
//  originalVertexIndex is index of that vertex in object->vertex array
{
	//assert(corners);
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
		assert(IS_TRIANGLE(corner[c].node));
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
		assert(originalPresent==1);
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

		} else assert(0);
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
	assert(this!=&info2);
	// set center
	unsigned verticesTotal = (unsigned)(ourVertices.size()+info2.ourVertices.size());
	center = (center*(real)ourVertices.size() + info2.center*(real)info2.ourVertices.size()) / (real)verticesTotal;
	// set ourVertices
	ourVertices.insert(info2.ourVertices.begin(),info2.ourVertices.end());
	info2.ourVertices.erase(info2.ourVertices.begin(),info2.ourVertices.end());
	assert(ourVertices.size()==verticesTotal);
	assert(info2.ourVertices.size()==0);
	// set neighbourVertices
	neighbourVertices.insert(info2.neighbourVertices.begin(),info2.neighbourVertices.end());
	info2.neighbourVertices.erase(info2.neighbourVertices.begin(),info2.neighbourVertices.end());
	assert(info2.neighbourVertices.size()==0);
	for(std::set<unsigned>::const_iterator i=ourVertices.begin();i!=ourVertices.end();i++)
	{
		unsigned ourVertex = *i;
		std::set<unsigned>::iterator invalidNeighbour = neighbourVertices.find(ourVertex);
		if(invalidNeighbour!=neighbourVertices.end())
		{
			neighbourVertices.erase(invalidNeighbour);
			//invalidNeighbour = neighbourVertices.find(ourVertex);
			//assert(invalidNeighbour==neighbourVertices.end());
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

unsigned Object::mergeCloseIVertices(IVertex* ivertex)
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
			assert(ivertexInfo[ivertex1Idx].ivertex->getNumCorners()!=0xfeee);
			assert(ivertexInfo[ivertex1Idx].ivertex->getNumCorners()!=0xcdcd);
			// for each neighbour vertex
			for(std::set<unsigned>::const_iterator i=ivertexInfo[ivertex1Idx].neighbourVertices.begin();i!=ivertexInfo[ivertex1Idx].neighbourVertices.end();i++)
			{
				// convert to neighbour ivertex
				unsigned vertex2Idx = *i;
				unsigned ivertex2Idx = vertex2ivertex[vertex2Idx];
				assert(ivertex1Idx!=ivertex2Idx);
				// measure distance
				real dist = size(ivertexInfo[ivertex1Idx].center-ivertexInfo[ivertex2Idx].center);
				assert(dist); // teoreticky muze nastat kdyz objekt obsahuje vic vertexu na stejnem miste
				// store the best
				if(dist<minDist)
				{
					minDist = dist;
					minIVert1 = ivertex1Idx;
					minIVert2 = ivertex2Idx;
				}
			}
			if(minDist<=RRGetStateF(RRSSF_MIN_FEATURE_SIZE)) break;
		}

		// end if not close enough
		if(minDist>RRGetStateF(RRSSF_MIN_FEATURE_SIZE)) break;
		numReduced++;

		// merge ivertices: update local temporary vertex2ivertex
		for(std::set<unsigned>::const_iterator i=ivertexInfo[minIVert2].ourVertices.begin();i!=ivertexInfo[minIVert2].ourVertices.end();i++)
		{
			assert(vertex2ivertex[*i] == minIVert2);
			vertex2ivertex[*i] = minIVert1;
		}

		// merge ivertices: rehook object persistent triangle->topivertex[0..2] from absorbed ivertex to absorbant ivertex
		// merge ivertices: update object persistent ivertices
		// while all other code in this method modifies temporary local data,
		//  this modifies also persistent object data
		assert(ivertexInfo[minIVert1].ivertex->getNumCorners()!=0xfeee);
		assert(ivertexInfo[minIVert1].ivertex->getNumCorners()!=0xcdcd);
		ivertexInfo[minIVert1].absorb(ivertexInfo[minIVert2]);
	}

	// delete temporaries
	delete[] vertex2ivertex;
	delete[] ivertexInfo;
	return numReduced;
}
#endif

void Object::buildTopIVertices()
{
	// check
	for(unsigned t=0;t<triangles;t++)
	{
		assert(!triangle[t].subvertex);
		for(int v=0;v<3;v++)
			assert(!triangle[t].topivertex[v]);
	}

	// build 1 ivertex for each vertex, insert all corners
	IVertex *topivertex=new IVertex[vertices];
	rrCollider::RRMeshImporter* meshImporter = importer->getCollider()->getImporter();
	for(unsigned t=0;t<triangles;t++) if(triangle[t].surface)
	{
		rrCollider::RRMeshImporter::Triangle un_ve; // un_ = unrotated
		meshImporter->getTriangle(t,un_ve);
		for(int ro_v=0;ro_v<3;ro_v++) // ro_ = rotated 
		{
			unsigned un_v = un_ve[(ro_v+triangle[t].rotations)%3];
			assert(un_v<vertices);
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
	if(RRGetStateF(RRSSF_MIN_FEATURE_SIZE)>0)
	{
		// Pouha existence nasledujiciho radku (mergeCloseIVertices) i kdyz se nikdy neprovadi
		// zpomaluje cube v MSVC o 8%.
		// Nevyresena zahada.
		// Ona fce je jedine misto pouzivajici exceptions, ale exceptions jsou vyple (jejich zapnuti zpomali o dalsich 12%).
		numIVertices -= mergeCloseIVertices(topivertex);
		check();
		//printf("IVertices after merge close: %d\n",numIVertices);
	}
#endif

	// split ivertices with too different normals
	int unusedVertices=0;
	for(unsigned v=0;v<vertices;v++)
	{
		rrCollider::RRMeshImporter::Vertex vert;
		meshImporter->getVertex(v,vert);
		if(!topivertex[v].check(vert)) unusedVertices++;
		numIVertices += topivertex[v].splitTopLevel((Vec3*)&vert,this);
		// check that splitted topivertex is no more referenced
		/*for(unsigned t=0;t<triangles;t++) if(triangle[t].surface)
		{
			for(unsigned i=0;i<3;i++)
			{
				assert(triangle[t].topivertex[i]!=&topivertex[v]);
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

	// for each vertex, store pointer to one of ivertices
	// helper only for fast approximate render
	memset(vertexIVertex,0,sizeof(vertexIVertex[0])*vertices);
	for(unsigned t=0;t<triangles;t++) if(triangle[t].surface)
	{
		// ro_=rotated, un_=unchanged,original from importer
		rrCollider::RRMeshImporter::Triangle un_ve;
		meshImporter->getTriangle(t,un_ve);
		for(int ro_v=0;ro_v<3;ro_v++)
		{
			//assert(triangle[t].topivertex[v]);
			assert(!triangle[t].topivertex[ro_v] || triangle[t].topivertex[ro_v]->error!=-45);
			unsigned un_v = un_ve[(ro_v+triangle[t].rotations)%3];
			assert(un_v<vertices);
			vertexIVertex[un_v]=triangle[t].topivertex[ro_v]; // un[un]=ro[ro]
		}
	}

	// delete local array topivertex
	for(unsigned t=0;t<triangles;t++) if(triangle[t].surface)
	{
		for(unsigned i=0;i<3;i++)
		{
			// check that local array topivertex is not referenced here
			unsigned idx = (unsigned)(triangle[t].topivertex[i]-topivertex);
			assert(idx>=vertices);
		}
	}
	delete[] topivertex;
	check();

	// check vertexIVertex validity
	for(unsigned v=0;v<vertices;v++)
	{
		//!!! happens with generated ball, reason unknown
		//assert(vertexIVertex[v]);
	}

	// check triangle.topivertex validity
	check();
	for(unsigned t=0;t<triangles;t++)
		for(int v=0;v<3;v++)
		{
			//assert(triangle[t].topivertex[v]); no topivertex allowed for invalid triangles (surface=NULL)
			assert(!triangle[t].topivertex[v] || triangle[t].topivertex[v]->error!=-45);
			if(triangle[t].topivertex[v])
			{
				assert(triangle[t].topivertex[v]->getNumCorners()!=0xcdcd);
				assert(triangle[t].topivertex[v]->getNumCorners()!=0xfeee);
			}
		}

	check();
}





#ifndef ONLY_PLAYER

//////////////////////////////////////////////////////////////////////////////
//
// .ora oraculum
// akcelerovany orakulum, poradi otazek je znamo a jsou predpocitany v poli

// zde se vytvari pole pointeru na odpovedi, pri ukladani uz by mely bejt
//  vsechny vyplneny. pocita ze kazdej subtriangl zavola fill jen jednou.

bool    ora_filling=false;
static S8 **ora_ptr=NULL;
static unsigned ora_ptrs=0;

extern void ora_filling_init()
{
	ora_ptrs=0;
	ora_ptr=new S8*[100000];
	for(unsigned i=0;i<100000;i++) ora_ptr[i]=NULL;
	ora_filling=true;
}

extern void ora_fill(S8 *ptr)
{
	assert(ora_ptrs<100000);
	ora_ptr[ora_ptrs++]=ptr;
}

extern void ora_filling_done(char *filename)
{
	FILE *f=fopen(filename,"wb");
	if(!f) return;
	for(unsigned i=0;i<ora_ptrs;i++)
	{
		assert(ora_ptr[i]);
		fwrite(ora_ptr[i],1,1,f);
	}
	fclose(f);
	delete[] ora_ptr;
	ora_ptr=NULL;
	ora_ptrs=0;
	ora_filling=false;
}

// zde se nacitaj hodnoty z orakula. pocita ze kazdej subtriangl zavola read
//  jen jednou.

bool    ora_reading=false;
static S8 *ora_val=NULL;
static unsigned ora_vals=0;
static unsigned ora_pos=0;

extern void ora_reading_init(char *filename)
{
	ora_vals=0;
	ora_val=NULL;
	ora_pos=0;
	FILE *f=fopen(filename,"rb");
	if(!f) return;
	fseek(f,0,SEEK_END);
	ora_vals=ftell(f);
	ora_val=new S8[ora_vals];
	fseek(f,0,SEEK_SET);
	fread(ora_val,1,ora_vals,f);
	fclose(f);
	ora_reading=true;
}

extern S8 ora_read()
{
	assert(ora_pos<ora_vals);
	return ora_val[ora_pos++];
}

extern void ora_reading_done()
{
	assert(ora_pos==ora_vals);
	delete[] ora_val;
	ora_val=NULL;
	ora_vals=0;
	ora_reading=false;
}

//////////////////////////////////////////////////////////////////////////////
//
// saving/loading precalculated ivertex data, oraculum

static unsigned iv_depth=0;

static void iv_loadReal(SubTriangle *s,IVertex *iv,int type);

static void iv_callSubIvertices(SubTriangle *s,void callback(SubTriangle *s,IVertex *iv,int type))
{
	iv_depth++;
#ifdef LOG_LOADING_MES
	if(callback==iv_loadReal)
	  printf("iv: ");
#endif
	callback(s,s->subvertex,3);
#ifdef LOG_LOADING_MES
	if(callback==iv_loadReal)
	  printf("sub[0]: ");
#endif
	if(s->sub[0]) iv_callSubIvertices(SUBTRIANGLE(s->sub[0]),callback);
#ifdef LOG_LOADING_MES
	if(callback==iv_loadReal)
	  printf("sub[1]: ");
#endif
	if(s->sub[1]) iv_callSubIvertices(SUBTRIANGLE(s->sub[1]),callback);
	callback(s,NULL,4);//ohlasi vynorovani ze subtrianglu
	iv_depth--;
}

static void iv_callIvertices(Triangle *t,void callback(SubTriangle *s,IVertex *iv,int type))
{
#ifdef LOG_LOADING_MES
	if(callback==iv_loadReal)
	  printf("%f %f %f  %f %f %f  %f %f %f\n",
	    t->vertex[0]->x,t->vertex[0]->y,t->vertex[0]->z,
	    t->vertex[1]->x,t->vertex[1]->y,t->vertex[1]->z,
	    t->vertex[2]->x,t->vertex[2]->y,t->vertex[2]->z);
#endif
	callback(t,t->topivertex[0],0);
	callback(t,t->topivertex[1],1);
	callback(t,t->topivertex[2],2);
	iv_callSubIvertices(t,callback);
}

static int iv_cmpobj(const void *e1, const void *e2)
{
	return (*(Object **)e1)->id-(*(Object **)e2)->id;
}

// type=0,1,2...tolikaty topivertex, 3...subvertex, 4...oznameni vynoreni
void Scene::iv_forEach(void callback(SubTriangle *s,IVertex *iv,int type))
{
	// objekty musi prochazet vzdycky ve stejnym poradi
	// a to i pri kazdym dalsim spusteni
	Object **obj=new Object*[objects];
	memcpy(obj,object,sizeof(Object *)*objects);
	qsort(obj,objects,sizeof(Object *),iv_cmpobj);

	for(unsigned o=0;o<objects;o++)
	    for(unsigned t=0;t<obj[o]->triangles;t++)
		iv_callIvertices(&obj[o]->triangle[t],callback);
	delete[] obj;
}

static void iv_cleanFlags(SubTriangle *s,IVertex *iv,int type)
{
	if(iv)
	{
		iv->loaded=false;
		iv->saved=false;
	}
}

static void iv_setFlagImportant(SubTriangle *s,IVertex *iv,int type)
{
	if(iv) iv->important=true;
}

static FILE *iv_f;
static FILE *iv_fall;
#define iv_FW(var) fwrite(&(var),sizeof(var),1,iv_f)
#define iv_FALLW(var) fwrite(&(var),sizeof(var),1,iv_fall)
#define iv_FR(var) fread(&(var),sizeof(var),1,iv_f)
#define iv_FALLR(var) fread(&(var),sizeof(var),1,iv_fall)
static bool iv_realsInside;
static SubTriangle *iv_sub=NULL;

// ulozi informace o meshingu pro important ivertexy
// pri iv_realsInside je prolozi realama z ivertexu (=ulozi frame)
// pri !iv_realsInside je prolozi byteerrorama z subveretxu (=ulozi .mes)

static void iv_saveReal(SubTriangle *s,IVertex *iv,int type)
{
	// prochazi vetvi ktera neni important, takze nic nedela,
	//  jen ceka dokud se nevynori zpatky do korene ty vetve
	if(iv_sub)
	{
		if(s==iv_sub)
		{
			iv_sub=NULL;
			assert(type==4);
		}
		return;
	}
	// vynorovani ze subtrianglu nemusi ukladat
	if(type==4) return;
	// ulozi bajt co delat
	U8 b;
	// kdyz narazi na neimportant iv, prejde do rezimu cekani na vynoreni
	if(iv && !iv->important)
	{
		iv_sub=s;//(poznamena si s a ceka dokud nezacne vynorovani)
		b=0;// a tvrdi ze zadne dalsi vnorovani uz tu neni
	}
	else
	if(!iv) b=0; // zadne dalsi vnorovani uz tu neni
	  else if(iv->saved) b=1; // ivertex uz byl ulozen
	  else b=10; // ivertex ulozime ted
	// v topivertexu k b pricte kolikaty topivertex to je,
	// v subvertex do nej navic zakodujeme deleni subtrianglu
	if(b)
	{
		b+=type;
		if(type==3) b+=s->getSplitVertex()+(s->isRightLeft()?3:0);
	}
	iv_FW(b);
	if(b>=10)
	{
		if(iv_realsInside)
		{
			Channels r=iv->exitance(s);
			iv_FW(r);
		}
		else
		if(type==3)//byteerror neuklada pro topvertexy
		{
			U8 be=(U8)(iv->error);
			iv_FW(be);
		}
		iv->saved=true;
	}
}

// ulozi uplne vsechny ivertexy
// vsem nastavi flag saved, ostatni nemeni

void Scene::iv_saveRealFrame(char *name)
{
	iv_f=fopen(name,"wb");
	if(!iv_f) {fprintf(stderr,"Unable to grab to %s.\n",name);__errors=1;return;}
	// zapise dulezity parametry prostredi
	iv_FW(shotsTotal);//kolik statickejch paprsku vystrelil na frame
	#ifdef SUPPORT_DYNAMIC
	iv_FW(__lightShotsPerDynamicFrame); //kolik dynamickejch paprsku vystrelil na frame
	#else
	int tmp0=0; iv_FW(tmp0);
	#endif
	// vsechny ivertexy oznaci za neulozeny a dulezity
	iv_forEach(iv_cleanFlags);
	iv_forEach(iv_setFlagImportant);
	// prochazi triangly a subtriangly a neulozeny dulezity ivertexy uklada
	iv_realsInside=true;
	iv_forEach(iv_saveReal);
	// hotovo
	fclose(iv_f);
}

// byl nacten udaj o ivertexu ale nechceme ho prochazet do hloubky.
// tato fce preskace vsechno co se tyka jeho synu.
// fce je volana pouze z iv_findSplitInfo a ta pouze z iv_splitDetector
//  a ta pouze z iv_loadReal,
//  ktera nastavi iv_realsInside (zda cteme .000 frame nebo .mes)

void iv_skipLoadingSubTree()
{
	unsigned todo=2;//ma pred sebou dva syny ktery musi projit (treba 2x0)
	do
	{
	  U8 b;
	  iv_FR(b);
	  switch(b)
	  {
	    case 0:todo--;break;// zadnej ivertex tu neni
	    case 1:// ivertex uz byl nahran
	    case 2:// pokracuje se jeho dvema synama
	    case 3:
	    case 4:
	    case 5:
	    case 6:
	    case 7:
	    case 8:
	    case 9:todo++;break;
	    case 10:// ivertex nahrajeme ted
	    case 11:// pokracuje se jeho dvema synama
	    case 12:
	    case 13:
	    case 14:
	    case 15:
	    case 16:
	    case 17:
	    case 18:if(iv_realsInside) {real r;iv_FR(r);assert(IS_NUMBER(r));}
	            else if(b>=13){U8 be;iv_FR(be);}//byteerror nenacita z topvertexu
	            todo++;break;
	    default: assert(0);
	  }
	}
	while(todo);
}

// znovu projde cely prave otevreny iv_f a vsude kde se dozvi jak
//  splitovat to nastavi do cache v s.
// fce je volana pouze z iv_splitDetector a ta pouze z iv_loadReal,
//  ktera nastavi iv_realsInside (zda cteme .000 frame nebo .mes)

static int iv_infosfound;

static void iv_findSplitInfo(SubTriangle *s,IVertex *iv,int type)
{
	// prochazi vetvi ktera na disku neni, takze nic nedela,
	//  jen ceka dokud se nevynori zpatky do korene ty vetve
	if(iv_sub)
	{
	  if(s==iv_sub)
	  {
	    iv_sub=NULL;
	    assert(type==4);
	  }
	  return;
	}
	// ostatni vynorovani uz zcela ignoruje
	if(type==4) return;
	// nacte bajt z disku a kouka co dal
	U8 b;
	real r;
	iv_FR(b);
	switch(b)
	{
	  case 0:// zadne dalsi vnorovani uz tu nema byt
	    iv_sub=s;//(poznamena si s a ceka dokud nezacne vynorovani)
	    break;
	  case 1:// topivertexy nas ale vubec nezajimaj
	  case 2:
	  case 3:
	    break;
	  case 10:
	  case 11:
	  case 12:
	    if(iv_realsInside)
	    {
	      iv_FR(r);
	      assert(IS_NUMBER(r));
	    }
	    //byteerror nenacita z topvertexu
	    break;
	  case 4:// ivertex uz by tady byl nahran
	  case 5:
	  case 6:
	  case 7:
	  case 8:
	  case 9:
	    if(!iv) iv_skipLoadingSubTree();
	    assert(s->splitVertex_rightLeft<0 || s->splitVertex_rightLeft==b-4);
	    if(s->splitVertex_rightLeft<0) iv_infosfound++;
	    s->splitVertex_rightLeft=b-4;
	    break;
	  case 13:// ivertex by mel byt nahran tady
	  case 14:
	  case 15:
	  case 16:
	  case 17:
	  case 18:
	    if(iv_realsInside)
	    {
	      iv_FR(r);
	      assert(IS_NUMBER(r));
	    }
	    else
	    //byteerror nacita ze subivertexu a tam zrovna jsme
	    {
	      U8 be;
	      iv_FR(be);
	    }
	    if(!iv) iv_skipLoadingSubTree();
	    assert(s->splitVertex_rightLeft<0 || s->splitVertex_rightLeft==b-13);
	    if(s->splitVertex_rightLeft<0) iv_infosfound++;
	    s->splitVertex_rightLeft=b-13;
	    break;
	  default:
	    assert(0);
	}
}

static Scene *iv_scene;
static int iv_meshingStartOffset;

static void iv_splitDetector()
{
	DBGLINE
	fpos_t pos;
	fgetpos(iv_f,&pos);
	fseek(iv_f,iv_meshingStartOffset,SEEK_SET);
	iv_infosfound=0;
	iv_scene->iv_forEach(iv_findSplitInfo);
	fsetpos(iv_f,&pos);
	//fprintf(stderr,"found %i infos\n",iv_infosfound);
}

static unsigned iv_ivertices=0;
static IVertex **iv_realptrtable=NULL;
static unsigned iv_flagsLoaded;

// nacte informace o meshingu
// pri iv_realsInside uvnitr ocekava a nacte i realy z ivertexu (=nacte frame)

static void iv_loadReal(SubTriangle *s,IVertex *iv,int type)
{
#ifdef LOG_LOADING_MES
	if(type==4) printf("ret\n");
#endif
	// prochazi vetvi ktera na disku neni, takze nic nedela,
	//  jen ceka dokud se nevynori zpatky do korene ty vetve
	if(iv_sub)
	{
	  if(s==iv_sub)
	  {
	    iv_sub=NULL;
	    assert(type==4);
	  }
	  return;
	}
	// ostatni vynorovani uz zcela ignoruje
	if(type==4) return;
	// nacte bajt z disku a kouka co dal
	U8 b;
	Channels r;
	U8 be;//byte error
	iv_FR(b);
#ifdef LOG_LOADING_MES
	printf("iv=%i type=%i b=%i",iv?1:0,type,b);
#endif
	switch(b)
	{
	  case 0:// zadne dalsi vnorovani uz tu nema byt
	    iv_sub=s;//(poznamena si s a ceka dokud nezacne vynorovani)
	    break;
	  case 1:// ivertex uz byl nahran
	  case 2:
	  case 3:
	  case 4:
	  case 5:
	  case 6:
	  case 7:
	  case 8:
	  case 9:
	    if(type==3)
		{
	      // zkontroluje ze deleni s odpovida tomu s jakym to bylo ulozeno
	      assert(b==1+type+s->getSplitVertex()+(s->isRightLeft()?3:0));
		}
	    else
		{
	      // zkontroluje ze jde o odpovidajici topivertex
	      assert(b==1+type);
		}
	    assert(iv);
	    assert(iv->loaded);
	    break;
	  case 10:// ivertex nahrajeme ted
	  case 11:
	  case 12:
	  case 13:
	  case 14:
	  case 15:
	  case 16:
	  case 17:
	  case 18:
	    if(iv_realsInside)
	    {
	      iv_FR(r);
	      assert(IS_CHANNELS(r));
	    }
	    else
	    if(type==3)//byteerror nenacita z topvertexu
	    {
	      iv_FR(be);
	    }
	    if(!iv)
	    {
	      assert(type==3);
	      // tady chce naseknout subtriangl zpusobem nactenym ze souboru
	      //  ale ono by to mohlo automaticky naseknout i sousedy
	      //  a spatne, driv nez dojdem k nim a nactem jak sekat je.
	      // sileny reseni:
	      //  jen subtrianglu reknem jak se ma sekat a jdem dal.
	      //  po nacteni vstupu nekdo projde strom pruchodem do sirky
	      //  a az narazi na patro s oznamenim jak sekat, naseka to tam.
	      //  opakuje se nacitani souboru dokud takhle nezpracuje
	      //  vsechny patra (pri kazdym nacteni ubyde aspon jedno).
	      //  jinej vyklad: nacita cyklem pres vsechny patra vzdy
	      //   jen to jedno.
	      //  jenze to nejde protoze ivertex je ve dvou ruznejch patrech!
	      // fungujici reeni:
	      //  splitGeometry kdykoliv potrebuje splitovat zavola
	      //  nas callback a ten mu novym pruchodem vstupu zjisti jak
	      assert(!__oraculum);
#ifndef LOG_LOADING_MES
	      __oraculum=iv_splitDetector;//orakulum jak splitovat ostatni
#endif
	      assert(s->splitVertex_rightLeft==b-10-type || s->splitVertex_rightLeft<0);
	      s->splitVertex_rightLeft=b-10-type;//prikaz jak splitnout tenhle (jen urychleny orakulum)
	      s->splitGeometry(NULL);
	      __oraculum=NULL;
	      iv=SUBTRIANGLE(s)->subvertex;
#ifdef LOG_LOADING_MES
	      printf(" iv=%i",iv?1:0);
#endif
	      // zkontroluje ze deleni s odpovida tomu s jakym to bylo ulozeno
	      assert(b==10+type+s->getSplitVertex()+(s->isRightLeft()?3:0));
	    }
	    assert(iv);
	    if(type==3)
	    {
	      assert(iv==SUBTRIANGLE(s)->subvertex);
	      // zkontroluje ze deleni s odpovida tomu s jakym to bylo ulozeno
	      assert(b==10+type+s->getSplitVertex()+(s->isRightLeft()?3:0));
	    }
	    else
	    {
	      // zkontroluje ze jde o odpovidajici topivertex
	      assert(b==10+type);
	    }
	    assert(!iv->loaded);
	    if(iv_realsInside)
	      // loadovani .000 framu - nacte obsah ivertexu
	      iv->loadCache(r);
	    else
	    {
	      // loadovani .mes - plni si tabulku vsech ivertexu
	      assert(iv_flagsLoaded<iv_ivertices);
	      iv_realptrtable[iv_flagsLoaded]=iv;
	      if(type==3) iv->error=be;//ze subvertexu nacet byteerror
	      iv_flagsLoaded++;
	    }
	    iv->loaded=true;
	    break;
	  default:
	    assert(0);
	}
#ifdef LOG_LOADING_MES
	printf("\n");
#endif
}

static void iv_cleanError(SubTriangle *s,IVertex *iv,int type)
{
	if(iv) iv->error=0;
}

static void iv_cleanFlagImportant(SubTriangle *s,IVertex *iv,int type)
{
	if(iv) iv->important=false;
}

void Scene::iv_cleanErrors()
{
	// vsem ivertexum vynuluje error
	iv_forEach(iv_cleanError);
}

void Scene::iv_loadRealFrame(char *name)
{
	iv_f=fopen(name,"rb");
	if(!iv_f) {fprintf(stderr,"Unable to load %s.\n",name);__errors=1;return;}
	// precte dulezity parametry prostredi
	unsigned u;
	iv_FR(u);//kolik statickejch paprsku vystrelil na frame
	iv_FR(u);//kolik dynamickejch paprsku vystrelil na frame
	// vsechny ivertexy oznaci za nenacteny
	iv_forEach(iv_cleanFlags);
	// prochazi triangly a subtriangly a nenacteny ivertexy nacita
	iv_scene=this;
	iv_realsInside=true;
	iv_meshingStartOffset=8;
	iv_forEach(iv_loadReal);
	// kontrola ze dosel na konec souboru
	fgetc(iv_f);
	assert(feof(iv_f));
	// hotovo
	fclose(iv_f);
}

static real iv_error(SubTriangle *s,IVertex *iv)
{
	assert(s);
	assert(iv);
	assert(iv==s->subvertex);
	assert(iv->loaded);
	bool isRL=s->isRightLeft();
	Channels r1=SUBTRIANGLE(s->sub[isRL?0:1])->ivertex(1)->exitance(s);
	Channels r2=SUBTRIANGLE(s->sub[isRL?1:0])->ivertex(2)->exitance(s);
	assert(s->area>=0);
	return sum(abs(r1-r2))*s->area;
}

static void iv_addLoadedError(SubTriangle *s,IVertex *iv,int type)
{
	if(iv && iv->loaded && type==3)
	{
		// zvyssi error o chybu jaka by vznikla zrusenim subvertexu
		iv->error+=iv_error(s,iv);
	}
}

void Scene::iv_addLoadedErrors()
{
	// nactenejm ivertexum zvysi error
	iv_forEach(iv_addLoadedError);
}

static void iv_fillEmptyImportant(SubTriangle *s,IVertex *iv,int type)
{
	// vyplni nenastaveny ivertexy prumernou hodnotou sousedu
	if(iv && iv->important && !iv->loaded)
	{
		assert(type==3);
		bool isRL=s->isRightLeft();
		Channels r=(SUBTRIANGLE(s->sub[isRL?0:1])->ivertex(1)->exitance(s)+SUBTRIANGLE(s->sub[isRL?1:0])->ivertex(2)->exitance(s))/2;
		iv->loadCache(r);
		iv->loaded=true;
	}
}

void Scene::iv_fillEmptyImportants()
{
	// a nenacteny ivertexy dogeneruje aby tam neco bylo az se to bude
	// sejvovat do tga
	iv_forEach(iv_fillEmptyImportant);
}

//static unsigned iv_count;
//static unsigned iv_deletables;
static unsigned iv_tops;//pocet topvertexu
static unsigned iv_subs;//pocet subivertexu

// pocita ivertexy a nastavi jim important

static void iv_countAndSetFlagImportant(SubTriangle *s,IVertex *iv,int type)
{
	if(iv && !iv->important)
	{
		iv->important=true;
		if(type==3) iv_subs++;else iv_tops++;
	}
}

// propaguje nahoru velky errory aby nikdy nemel
// subvertex vetsi nez subvertex otce (topivertexy nemeni)

static unsigned iv_propagations;

static void iv_propagateErrors(SubTriangle *s,IVertex *iv,int type)
{
	// propaguje error nahoru, zaridi ze vyssi ivertexy maj vyssi error.
	// o topivertexy se nestara
	if(IS_SUBTRIANGLE(s) && s->subvertex && !iv /*...propaguje jen pri vynorovani*/)
	{
		SubTriangle *parent=SUBTRIANGLE(s->parent);
		assert(s->subvertex);
		assert(parent->subvertex);
		assert(IS_NUMBER(s->subvertex->error));
		assert(IS_NUMBER(parent->subvertex->error));
		if(parent->subvertex->error<=s->subvertex->error)
		{
//*fprintf(stderr," %i(%f,",(int)s,parent->subvertex->error);
			// zde potrebujeme dat do parent->subvertex->error malinko vic nez je v s->subvertex->error
			// protoze to pak budeme sortit podle erroru a vertexy s nizkym errorem neulozime do tga
			real add=0.001f;
			do
			{
				parent->subvertex->error=s->subvertex->error+add;
				add*=10;
			} while(parent->subvertex->error<=s->subvertex->error);
			iv_propagations++;
//*fprintf(stderr,"%f,%f)",s->subvertex->error,parent->subvertex->error);
		}
	}
}

static SubTriangle **iv_array;
static unsigned iv_inArray;

static void iv_saveSubsToArray(SubTriangle *s,IVertex *iv,int type)
{
	if(iv && type==3 && !iv->saved)
	{
		iv->saved=true;
		iv_array[iv_inArray++]=s;
	}
}

static void iv_cleanFlagSaved(SubTriangle *s,IVertex *iv,int type)
{
	if(iv) iv->saved=false;
}

int iv_cmperr(const void *e1, const void *e2)
{
	// radi od nejvetsich k nejmensim errorum
	assert((*(SubTriangle **)e1)->subvertex);
	assert((*(SubTriangle **)e2)->subvertex);
	if((*(SubTriangle **)e2)->subvertex->error-(*(SubTriangle **)e1)->subvertex->error<0) return -1;
	if((*(SubTriangle **)e2)->subvertex->error-(*(SubTriangle **)e1)->subvertex->error>0) return 1;
	return 0;
}

void Scene::iv_markImportants_saveMeshing(unsigned maxvertices,char *namemes)
{
	// vsem ivertexum nastavi important a spocita kolik jich je
	// a zpropaguje velky errory nahoru at nema syn vetsi nez otec
	iv_tops=0;
	iv_subs=0;
	iv_forEach(iv_cleanFlagImportant);
	iv_forEach(iv_countAndSetFlagImportant);
	unsigned prev_propagations;
	iv_propagations=1<<30;
	do
	{
		prev_propagations=iv_propagations;
		iv_propagations=0;
		iv_forEach(iv_propagateErrors);
//*                fprintf(stderr,"%i\n",iv_propagations);
	}
	while(iv_propagations && iv_propagations<prev_propagations);
	// to za && je jen hack at se tu nezacykli kdyz je v hierarchii ivertexu
	//  cyklus a propagace chyb jim propaguje porad dokola
	//  (cyklus v ivertexech byt nema a je treba zjistit kdo ho tam vytvari)

	// vsechny subvertexy nastrka do pole a sesorti podle erroru
	iv_array=new SubTriangle*[iv_subs];
	iv_inArray=0;
	iv_forEach(iv_saveSubsToArray);
	assert(iv_inArray==iv_subs);
	qsort(iv_array,iv_subs,sizeof(IVertex *),iv_cmperr);

	// zjisti kolik subvertexu bude ukladat
	iv_savesubs=MAX(0,MIN(maxvertices-iv_tops,iv_subs));

	// ukladanejm subvertexum nastavi error
	// tak aby mel prvni 0 az linearne k poslednimu 255.
	// to se pak ulozi jako byteerror, jako ukazatel dulezitosti.
	for(unsigned i=0;i<iv_savesubs;i++)
		iv_array[i]->subvertex->error=255.f*i/iv_savesubs;
	// ostatnim zrusi importance aby se neukladaly
	for(unsigned i=iv_savesubs;i<iv_subs;i++)
		iv_array[i]->subvertex->important=false;

	delete[] iv_array;
	// ulozi meshing
	iv_f=fopen(namemes,"wb");
	if(!iv_f) {fprintf(stderr,"Unable to export meshing to %s.\n",namemes);__errors=1;return;}
	iv_forEach(iv_cleanFlagSaved);
	iv_realsInside=false;
	iv_forEach(iv_saveReal);
	fclose(iv_f);
}

void Scene::iv_startSavingBytes(unsigned frames,char *nametga)
{
	// zacne ukladat tga (zapise hlavicku)
	iv_fall=fopen(nametga,"wb");
	if(!iv_fall) {fprintf(stderr,"Unable to export table to %s.\n",nametga);__errors=1;return;}
	U32 i=0x00030000;
	iv_FALLW(i);
	i=0x18010000;
	iv_FALLW(i);
	i=0;
	iv_FALLW(i);
	U16 j=iv_tops+iv_savesubs;//kolik ivertexu je ve framu (sirka)
	iv_FALLW(j);
	j=frames;//kolik framu je v sekvenci (vyska)
	iv_FALLW(j);
	j=8;
	iv_FALLW(j);
}

//extern real getBrightness(real color);//vypujcka z rr.cpp

static void iv_saveImportantByte(SubTriangle *s,IVertex *iv,int type)
{
	if(iv && iv->important && !iv->saved)
	{
		U8 b;
//takto by misto silne nedulezitych bajtu ukladal 255
//ale neni to nutne protoze rizeni kvality uz mame
//if(!iv->loaded || (type==3 && iv_error(s,iv)<1))
//b=255;else
		//!!!b=(U8)(254.99*getBrightness(iv->exitance()));
#if CHANNELS == 1
		b=(U8)iv->exitance();
#else
		b=0; //!!!
#endif
		iv_FALLW(b);
		iv->saved=true;
	}
}

void Scene::iv_saveByteFrame()
{
	iv_forEach(iv_cleanFlagSaved);
	iv_forEach(iv_saveImportantByte);
}

void Scene::iv_savingBytesDone()
{
	fclose(iv_fall);
}

static unsigned iv_frames=0;
static U8 *iv_bytetable=NULL;

unsigned Scene::iv_initLoadingBytes(char *namemes,char *nametga)
{
	delete[] iv_bytetable;
	delete[] iv_realptrtable;
	iv_bytetable=NULL;
	iv_realptrtable=NULL;
	iv_frames=0;
	// nacte pole bytes
	iv_fall=fopen(nametga,"rb");
	if(!iv_fall) {fprintf(stderr,"Unable to load table from %s.",nametga);__errors=1;return 0;}
	U32 i;
	iv_FALLR(i);
	iv_FALLR(i);
	iv_FALLR(i);
	U16 j;
	iv_FALLR(j);
	iv_ivertices=j;
	iv_FALLR(j);
	iv_frames=j;
	iv_FALLR(j);
	iv_bytetable=new U8[iv_frames*iv_ivertices];
	fread(iv_bytetable,sizeof(U8),iv_frames*iv_ivertices,iv_fall);
	fclose(iv_fall);
	// nacte ktery ivertices budou loadovany z pole bytes
	iv_f=fopen(namemes,"rb");
	if(!iv_f) {fprintf(stderr,"Unable to load meshing from %s.",namemes);__errors=1;return 0;}
	iv_realptrtable=new IVertex*[iv_ivertices];
	iv_forEach(iv_cleanFlags);
	iv_forEach(iv_cleanError);//do subvertex->erroru za chvili nacte byteerror
	iv_flagsLoaded=0;
	iv_scene=this;
	iv_realsInside=false;
	iv_meshingStartOffset=0;
	assert(iv_sub==NULL);
	iv_forEach(iv_loadReal);
	assert(iv_flagsLoaded==iv_ivertices);
	fclose(iv_f);
	return iv_frames;
}

void Scene::iv_loadByteFrame(real frame)
{
	assert(iv_realptrtable);
	assert(iv_bytetable);
	frame=fmod(fmod(frame,(real)iv_frames)+iv_frames,(real)iv_frames);
	unsigned framefloor=(int)frame;
	assert(framefloor<iv_frames);
	for(unsigned iv=0;iv<iv_ivertices;iv++)
	{
		real r=(framefloor+1-frame)*iv_bytetable[iv+framefloor*iv_ivertices]
		      +(frame-framefloor)*iv_bytetable[iv+((framefloor+1)%iv_frames)*iv_ivertices];
		iv_realptrtable[iv]->loadCache(Channels(r/256));
	}
}

static void iv_dump(SubTriangle *s,IVertex *iv,int type)
{
	if(type!=0) return;
	assert(IS_TRIANGLE(s));
	fprintf(iv_f,"%f %f %f  %f %f %f  %f %f %f\n",
	  TRIANGLE(s)->getVertex(0)->x,TRIANGLE(s)->getVertex(0)->y,TRIANGLE(s)->getVertex(0)->z,
	  TRIANGLE(s)->getVertex(1)->x,TRIANGLE(s)->getVertex(1)->y,TRIANGLE(s)->getVertex(1)->z,
	  TRIANGLE(s)->getVertex(2)->x,TRIANGLE(s)->getVertex(2)->y,TRIANGLE(s)->getVertex(2)->z);
}

void Scene::iv_dumpTree(char *name)
{
	iv_f=fopen(name,"wb");
	if(!iv_f) {fprintf(stderr,"Unable to dump tree to %s.",name);__errors=1;return;}
	iv_forEach(iv_dump);
	fclose(iv_f);
}

//////////////////////////////////////////////////////////////////////////////
//
// fighting needles

// used to find closest ivertex with valid radiosity (powerTopLevel!=0)

static IVertex *ne_iv;
static IVertex *ne_bestIv;
static real     ne_bestDist;
#ifdef IV_POINT
static Point3   ne_pos;
#else
static Point2   ne_pos;

static void iv_findIvPos(SubTriangle *s,IVertex *iv,int type)
{
	if(iv==ne_iv) switch(type)
	{
		case 0: ne_pos=s->uv[0]; break;
		case 1: ne_pos=s->uv[1]; break;
		case 2: ne_pos=s->uv[2]; break;
		case 3: ne_pos=SUBTRIANGLE(s->sub[0])->uv[0]; break;
	}
}
#endif

static void iv_findIvClosestToPos(SubTriangle *s,IVertex *iv,int type)
{
	if(!iv || iv==ne_iv || !iv->hasExitance()) return;
#ifdef IV_POINT
	Point3 pos=iv->point;
#else
	Point2 pos;
	switch(type)
	{
		case 0: pos=s->uv[0]; break;
		case 1: pos=s->uv[1]; break;
		case 2: pos=s->uv[2]; break;
		case 3: pos=SUBTRIANGLE(s->sub[0])->uv[s->isRightLeft()?2:1]; break;
		default: return;
	}
	iv->check(s->to3d(pos));
#endif
	real dist=size2(pos-ne_pos);
	if(dist<ne_bestDist)
	{
		ne_bestDist=dist;
		ne_bestIv=iv;
	}
}

Channels IVertex::getClosestIrradiance()
// returns irradiance of ivertex closest to this
{
	if(RRGetState(RRSS_FIGHT_NEEDLES)<2) return Channels(0); // pokud nebojujem s jehlama, vertexum bez powerTopLevel (zrejme nekde v jehlach na okraji sceny) dame barvu 0
	// kdyz 2 jehly sousedej, sdilej ivertex s powertoplevel==0
	//  kde nektery cornery spadaj do jednoho, jiny do druhyho
	// projdem vsechny
	ne_iv=this;
	ne_bestIv=NULL;
	ne_bestDist=1e20f;
	Node *tested1=NULL;
	Node *tested2=NULL;
	for(unsigned i=0;i<corners;i++)
	{
		Node *n=corner[i].node;
		if(!n || !n->grandpa) continue;
		if(n==tested1 || n==tested2) continue;
#ifdef IV_POINT
		ne_pos=point;
#else
		ne_pos.x=-1024;
		iv_callIvertices(n->grandpa,iv_findIvPos);
		assert(ne_pos.x!=-1024);
#endif
		iv_callIvertices(n->grandpa,iv_findIvClosestToPos);
		if(!tested1) tested1=n; else tested2=n;
	}
	static Channels prev=Channels(1e10);
	if(ne_bestIv) prev=ne_bestIv->irradiance();
	return prev;
}

#endif

} // namespace


