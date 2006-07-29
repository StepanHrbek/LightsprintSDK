#ifndef RRVISION_RRCORE_H
#define RRVISION_RRCORE_H

#define SUPPORT_TRANSFORMS
#define SUPPORT_SCALE
#define SUPPORT_MIN_FEATURE_SIZE // support merging of near ivertices (to fight needles, hide features smaller than limit)
//#define SUPPORT_INTERPOL // support interpolation, +20% memory required
//#define SUPPORT_DYNAMIC  // support dynamic objects/shadows. off=all vertices in scenespace, no transformations
//#define LIGHTMAP         // generate lightmap for each Triangle + texturemapping
//#define HITS_FIXED       // fixed point hits save lots of memory, possible loss of precision
// note that fixed hits have no hit extension implemeted (used only for dynamic objects)
#define HIT_PTR          & // hits are passed by reference
#define BESTS           200 // how many best shooters to precalculate in one pass. more=faster best() but less accurate

#define CHANNELS         3
#define HITCHANNELS      1 // 1 (CHANNELS only if we support specular reflection that changes light color (eg. polished steel) or specular transmittance that changes light color (eg. colored glass))
#define FACTORCHANNELS   1 // if CLEAN_FACTORS then 1 else CHANNELS

#if CHANNELS==1
#define Channels         real
#elif CHANNELS==3
#define Channels         Vec3
#else		    
#error unsupported CHANNELS
#endif

#if HITCHANNELS==1
#define HitChannels      real
#elif HITCHANNELS==3
#define HitChannels      Vec3
#else
#error unsupported HITCHANNELS
#endif

#if FACTORCHANNELS==1
#define FactorChannels   real
#elif FACTORCHANNELS==3
#define FactorChannels   Vec3
#else
#error unsupported FACTORCHANNELS
#endif

#include <stdarg.h>

#include "geometry.h"
#include "RRVision.h"
#include "interpol.h"

#define STATISTIC(a)
#define STATISTIC_INC(a) STATISTIC(RRScene::getSceneStatistics()->a++)

namespace rr
{

#define DBGLINE
//#define DBGLINE printf("- %s %i\n",__FILE__, __LINE__);

#ifdef HITS_FIXED
 #define HITS_UV_TYPE       U8
 #define HITS_UV_MAX        255 // <=8bit suitable only for fast preview, diverges when triangle gets more than ~1000 hits
 #define HITS_P_TYPE        S8
 #define HITS_P_MAX         127
 //#define HITS_UV_TYPE       U16
 //#define HITS_UV_MAX        65535
 //#define HITS_P_TYPE        S16
 //#define HITS_P_MAX         32767
#else
#endif

#ifndef M_PI
 #define M_PI                ((real)3.14159265358979323846)
#endif

#define SMALL_ENERGY 0.000001f // energy amount small enough to have no impact on scene

// konvence:
//  reset() uvadi objekt do stavu po konstruktoru
//  resetStaticIllumination() uvadi objekt do stavu po nacteni sceny

#ifdef SUPPORT_DYNAMIC
 #define TReflectors DReflectors
#else
 #define TReflectors Reflectors
#endif

#ifndef ONLY_PLAYER


//////////////////////////////////////////////////////////////////////////////
//
// license

extern bool                     licenseStatusValid;
extern RRLicense::LicenseStatus  licenseStatus;

//////////////////////////////////////////////////////////////////////////////
//
// memory

void* realloc(void* p,size_t oldsize,size_t newsize);

//////////////////////////////////////////////////////////////////////////////
//
// globals

extern bool __errors; // was there errors during batch work? used to set result

extern unsigned __frameNumber; // frame number increased after each draw

//////////////////////////////////////////////////////////////////////////////
//
// hit to subtriangle

struct Hit
{
#ifdef HITS_FIXED
	HITS_UV_TYPE u; // 0..HITS_UV_MAX, multiple of r3,l3 in grandpa triangle
	HITS_UV_TYPE v;
	HITS_P_TYPE power; // -HITS_P_MAX..HITS_P_MAX, negative energy is for dynamic shooting
#else
	real    u; // 0..side lengths, multiple of u3,v3 in grandpa triangle
	real    v;
	HitChannels power; // -1..1, negative energy is for dynamic shooting
#endif
	void    setPower(HitChannels apower);
	HitChannels getPower();
	void    setExtensionP(void *ext);
	void   *getExtensionP();
	void    setExtensionR(real r);
	real    getExtensionR();
};

#define IS_POWER(n) ((n)>=-1 && (n)<=1)

//////////////////////////////////////////////////////////////////////////////
//
// hits to one subtriangle

extern unsigned __hitsAllocated;
class Triangle;

class Hits
{
public:
	unsigned hits;
	Hit     *hit;

	Hits();
	~Hits();
	void    reset();
	void    compact();

	void    insert(Hit HIT_PTR ahit);
	real    difBtwAvgHitAnd(Point2 a,Triangle *base);
	//real    avgDifBtwHitAnd(Point2 a);
	bool    doSplit(Point2 centre,real perimeter,Triangle *base);
	real    totalPower();

	private:
		void rawInsert(Hit HIT_PTR ahit);
		void compactImmediate();
#ifdef HITS_FIXED
		U32 sum_u;
		U32 sum_v;
		S32 sum_power;
//		U64 sum_u;
//		U64 sum_v;
//		S64 sum_power;
#else
		real sum_u;
		real sum_v;
		HitChannels sum_power;
#endif
		unsigned hitsAllocated;

#ifdef SUPPORT_DYNAMIC
		bool isDynamic;
	public:
	void    insert(Hit ahit,void *extension);
	real    convertDHitsToHits();
#ifdef SUPPORT_LIGHTMAP
	void    convertDHitsToLightmap(class Lightmap *l,real zoomToLightmap);
#endif
#endif
};

Hits   *allocHitsLevel();
void    freeHitsLevel();

//////////////////////////////////////////////////////////////////////////////
//
// form factor from implicit source to explicit destination

class Node;

class Factor
{
public:
	Node    *destination;
	FactorChannels power; // this fraction of emited energy reaches destination
	               // Q: is modulated by destination's material?
	               // A: CLEAN_FACTORS->NO, !CLEAN_FACTORS->YES

	Factor(Node *destination,FactorChannels apower);
};

//////////////////////////////////////////////////////////////////////////////
//
// all form factors from implicit source

extern unsigned __factorsAllocated;

class Factors
{
public:
	Factors();
	~Factors();
	void    reset();

	unsigned factors();
	void    insert(Factor afactor);
	void    insert(Factors *afactors);
	Factor  get();
	void    remove(Factor *afactor);
	real    contains(Node *destination);
	void    forEach(void (*func)(Factor *factor,va_list ap),...);
	void    forEachDestination(void (*func)(Node *node,va_list ap),...);
	void    removeZeroFactors();

	private:
		unsigned factors24_allocated8;//high24bits=factors, low8bits=ln2(factors allocated), 0=nothing allocated
		unsigned factorsAllocated();
		Factor *factor;
};

//////////////////////////////////////////////////////////////////////////////
//
// thing that shoots and has form factors

class Shooter : public Factors
{
public:
	Shooter();
	~Shooter();
	void    reset(bool resetFactors);

	Channels energyDiffused;
	Channels energyToDiffuse;
	unsigned shotsForFactors;

	Factor  *tmpFactor;     // used only during clustering

	real    accuracy();     // shots done per energy unit
};

#endif

//////////////////////////////////////////////////////////////////////////////
//
// node in hierarchy of clusters, triangles and subtriangles
//
// |clusters|triangles|       |
// |        |---------|       |
// |        |     subtriangles|
// |--------------------------|
// |       nodes              |

extern unsigned __nodesAllocated;

class Node
{
public:
	Node(Node *aparent,Triangle *agrandpa);
	~Node();

	// genealogy, parent and triangle ancestor with absolute coordinates
	Node    *parent;
	Triangle *grandpa;
	Node    *brother();

	// geometry
	real    area; // area in worldspace

#ifndef ONLY_PLAYER
	void    reset(bool resetFactors);

	// form factors and how many shots they were calculated with
	Shooter *shooter;
	real    accuracy();     // shots done per energy unit

	// static energy acumulators
	Channels energyDirect;   // exitance from energy received directly by this node or his subs (not by ancestors)
	Channels energyDirectIncident; // irradiance received directly by this node or his subs (not by ancestors)
	Channels radiosityIndirect();// radiosity received by ancestors
	bool    loadEnergyFromSubs();
	void    propagateEnergyUp();
	Channels getEnergyDynamic(); // gets all energy averaged in more frames

	// dynamic energy acumulators
#ifdef SUPPORT_DYNAMIC
	void    addEnergyDynamic(real e); // adds bit of energy for current frame
	private:
		real    energyDynamicFrame;  // dynamic energy acumulated during one frame
		real    energyDynamicSmooth;  // the same value smoothed using previous value and variance
		real    energyDynamicVariance;
		U8      energyDynamicFrameTime;
		U8      energyDynamicSmoothTime;
	public:
	real    importanceForDynamicShadows();
	real    importanceForDynamicShadows(Bound *objectBound);
#endif

	bool    check();
#endif

	// subnodes
	Node    *sub[2];
	bool    contains(Triangle *t);

	// not aligned rest
	U16     flags;
};

#define FLAG_DIRTY_NODE              1
#define FLAG_DIRTY_IVERTEX           2 // used only when interpolation is turned on
#define FLAG_DIRTY_ALL_SUBNODES      4
#define FLAG_DIRTY_SOME_SUBIVERTICES 8 // used only when interpolation is turned on
#define FLAGS_DIRTY                 15
#define FLAG_OFFERS_CLUSTERING      16
#define FLAG_DENIES_CLUSTERING      32
#define FLAG_WEIGHS_CLUSTERING      64
#define FLAGS_CLUSTERING           112
#define FLAG_IS_REFLECTOR          128
#define FLAG_IS_IN_ORA             256

#define IS_CLUSTER(node)     (!(node)->grandpa)
#define IS_TRIANGLE(node)    ((node)==(node)->grandpa)
#define IS_SUBTRIANGLE(node) ((node)->grandpa && (node)!=(node)->grandpa)

#define CLUSTER(node)        ((Cluster *)(node))
#define TRIANGLE(node)       ((Triangle *)(node))
#define SUBTRIANGLE(node)    ((SubTriangle *)(node))

//////////////////////////////////////////////////////////////////////////////
//
// subtriangle, part of triangle
//
// init(a,b,c):
//  a is stored into s2
//  b is stored into s2+u2
//  c is stored into s2+v2
// ivertex:
//  ivertex(0) returns ivertex at a
//  ivertex(1) returns ivertex at b
//  ivertex(2) returns ivertex at c
// split:
//  rot = number of vertex splitted by triangle split (a=0,b=1,c=2)
//   is calculated from geometry by getSplitVertexSlow()
//   not stored, later calculated from parent/son relations by getSplitVertex()
//   (1% cpu time may be saved by storing rot in 2bits)
//  thus when
//   rot=0 then sub[0].init(a,b,(b+c)/2) sub[1].init(a,(b+c)/2,c)
//   rot=1 then sub[0].init(b,c,(a+c)/2) sub[1].init(b,(a+c)/2,a)

extern unsigned __subtrianglesAllocated;
extern void (*__oraculum)();//doplni vsude ve scene kde to jde splitVertex_rightLeft = informaci jak splitovat kdyz hrozi ze zaokrouhlovaci chybou splitne pokazdy jinak

class SubTriangle : public Node
{
public:
	SubTriangle(SubTriangle *aparent,Triangle *agrandpa);
	~SubTriangle();

	// geometry, SubTriangle position in Triangle space
	Point2  uv[3]; // uv of our vertices in grandpa->u3,v3 ortogonal space
	Vec2    u2,v2; // u2=uv[1]-uv[0], v2=uv[2]-uv[0], helps to speed up some calculations
	real    perimeter();
	Point3  to3d(Point2 a);
	Point3  to3d(int vertex);
	Point3  to3dlo(int vertex);

	void    makeDirty();

	// subtriangles
	// if sub[0] then this triangle is splitted into sub[0] and sub[1]
	//  so that { hit.u,v | splita*u+splitb*v>1 } is sub[0]
	void    splitGeometry(IVertex *asubvertex);
#ifndef ONLY_PLAYER
	void    splitHits(Hits* phits,Hits *phits2);
	bool    wishesToSplitReflector();
#endif
	// enumeration of all subtriangles
	typedef void (EnumSubtrianglesCallback)(SubTriangle* s, IVertex **iv, Channels flatambient, void* context);
	unsigned enumSubtriangles(IVertex **iv, Channels flatambient, EnumSubtrianglesCallback* callback, void* context);

	SubTriangle *brotherSub();
	bool    isRight();
	bool    isRightLeft();
	int     getSplitVertex();
	int     getSplitVertexSlow();
	S8      splitVertex_rightLeft;//-2=dunno a zatim neulozen do .ora, -1=dunno a ulozen, jinak prikaz pro splitGeometry a cache pro getSplitVertexSlow
	IVertex *subvertex;
	private:
		void    createSubvertex(IVertex *asubvertex,int rot);
	public:
	void    removeFromIVertices(Node *node);
	SubTriangle *getNeighbourSubTriangle(int myside,int *nbsside,IVertex *newVertex);
	IVertex *ivertex(int i);
	bool    checkVertices();
	void    installVertices();

	unsigned printGouraud(void *f, IVertex **iv,real scale,Channels flatambient=Channels(0));
	void    drawGouraud(Channels ambient,IVertex **iv,int df);
	void    drawFlat(Channels ambient,int df);

	private:
		real    splita; // sub[0]=subtriangle with splita*u+splitb*v>1
		real    splitb; // sub[1]=subtriangle with splita*u+splitb*v<=1
};

// DF=draw flags
#define DF_REFRESHALL 1
#define DF_TOLIGHTMAP 2

//////////////////////////////////////////////////////////////////////////////
//
// lightmap

#ifdef SUPPORT_LIGHTMAP

extern unsigned __lightmapsAllocated;

class Lightmap
{
public:
	unsigned w;
	unsigned h;
	Point2  uv[3];
	U8      *bitmap;

	bool    isClean;

	Lightmap();
	~Lightmap();

	void setSize(unsigned aw,unsigned ah);
	void Save(char *name);
};

#endif

//////////////////////////////////////////////////////////////////////////////
//
// scale / scalovani
//
// objekt muze byt transformovan matici se scalem, neuniformnim, zapornym
// scale       - asi funguje
// neuniformni - asi funguje
// zaporny     - asi funguje
//
// n3/u3/v3 je ortonormalni baze v objectspace
// u2/v2 jsou v prostoru u3/v3
// splituje se podle u2/v2, takze pri neuniformnim scalu neoptimalne
// area je ve worldspace, takze nedochazi k mnozeni/mizeni energie pri distribuci

//////////////////////////////////////////////////////////////////////////////
//
// triangle, part of cluster and object
//
// init(a,b,c):
//  rotations 0..2 is calculated form geometry
//   =1: init(b,c,a) is called
//   =2: init(c,a,b) is called
//  vertex[3] is filled by {a,b,c}
//  s3,r3,l3 is filled by a,b-a,c-a
//  lightmap.x1,x2,x3 is filled by a,b,c

extern unsigned __trianglesAllocated;
extern unsigned __trianglesWithBadNormal;

class Triangle : public SubTriangle
{
public:
	Triangle();
	~Triangle();
	void    compact();

	// genealogy
#ifdef SUPPORT_TRANSFORMS
	class Object *object;
#endif
	// enumeration of all subtriangles
	unsigned enumSubtriangles(EnumSubtrianglesCallback* callback, void* context);

	// geometry, all in objectspace
	const Vec3* getVertex(unsigned i) const {return qvertex[i];}
	Vec3    getS3() const {return *getVertex(0);} // absolute position of start of base (transformed when dynamic)
	Vec3    getR3() const {return *getVertex(1)-*getVertex(0);} // absolute sidevectors  r3=vertex[1]-vertex[0], l3=vertex[2]-vertex[0] (all transformed when dynamic)
	Vec3    getL3() const {return *getVertex(2)-*getVertex(0);}
	Normal  getN3() const {return qn3;}
	Vec3    getU3() const {return qu3;}
	Vec3    getV3() const {return qv3;}
		private:
		Vec3*       qvertex[3];      // 3x vertex
		Normal      qn3;             // normalized normal vector
		Vec3        qu3,qv3;         // ortonormal base for 2d coordinates in subtriangles
			// hadam 95% ze je dobre ze jsou ortonormalni v objectspace
			// hadam 5% ze by se nekomu vic hodilo world2obj(ortonormlani baze ve worldspace)
		public:
	struct Edge *edge[3];   // edges
	U8      isValid      :1;// triangle is not degenerated
	U8      isInCluster  :1;// triangle is in cluster
	U8      isNeedle     :1;// triangle is needle-shaped, try to hide it by interpolation
	U8      rotations    :2;// how setGeometry(a,b,c) rotated vertices, 0..2, 1 means that vertex={b,c,a}
	S8      setGeometry(Vec3* a,Vec3* b,Vec3* c,const RRMatrix3x4 *obj2world,Normal *n=NULL,int rots=-1);
	Vec3    to3d(Point2 a);
	Vec3    to3d(int vertex);
	SubTriangle *getNeighbourTriangle(int myside,int *nbsside,IVertex *newVertex);
	IVertex *topivertex[3]; // 3x ivertex
	void    removeFromIVertices(Node *node);
		private:
		void    setGeometryCore(Normal *n=NULL);
		public:

	// surface
	const RRSurface *surface;     // material at outer and inner side of Triangle
	Channels setSurface(const RRSurface *s,const Vec3& additionalExitingFlux, bool resetPropagation);
#ifndef ONLY_PLAYER
	Channels getSourceIncidentFlux() {return Channels(sourceExitingFlux.x/MAX(surface->diffuseReflectance[0],0.1f),sourceExitingFlux.y/MAX(surface->diffuseReflectance[1],0.1f),sourceExitingFlux.z/MAX(surface->diffuseReflectance[2],0.1f));} // source incident radiant flux in Watts
	Channels getSourceExitingFlux() {return sourceExitingFlux;} // source exiting radiant flux in Watts
	Channels getSourceIrradiance() {return getSourceIncidentFlux()/area;} // source irradiance in W/m^2
	Channels getSourceExitance() {return getSourceExitingFlux()/area;} // source exitance in W/m^2
		private:
		Channels sourceExitingFlux;   // backup of all scene energy in time 0. Set by setSurface (from ResetStaticIllumination). Used by radiosityGetters "give me onlyPrimary or onlySecondary".
		public:

	// hits
	Hits    hits;           // the most memory consuming struct: set of hits
#endif

#ifdef SUPPORT_LIGHTMAP
	// lightmap
	unsigned subtriangles;  // number of subtriangles
	Lightmap lightmap;      // lightmap
	real    zoomToLightmap; // zoom to fit in lightmap
	void    setLightmapSize(unsigned w);
	void    updateLightmapSize(bool forExport);
	void    updateLightmap();
	void    drawLightmap();
#endif
};

//////////////////////////////////////////////////////////////////////////////
//
// set of triangles

class Cluster;

class Triangles
{
public:
	Triangles();
	~Triangles();
	void    reset();

	void    insert(Triangle *key);
	Triangle *get();
	Triangle *get(real a);
	void    forEach(void (*func)(Triangle *key,va_list ap),...);
	void    holdAmulet();
	void    resurrect();
	Node    *buildClusterHierarchy(bool differentNormals,Cluster *aparent,Cluster *cluster,unsigned *c,unsigned maxc);

	private:
		unsigned trianglesAllocated;
		unsigned trianglesAfterResurrection;
	protected:
		unsigned triangles;
		Triangle **triangle;
};

#ifndef ONLY_PLAYER

//////////////////////////////////////////////////////////////////////////////
//
// cluster, set of triangles, part of object

extern unsigned __clustersAllocated;

class Cluster : public Node
{
public:
	Cluster();
	~Cluster();

	void    makeDirty();
	void    insert(Triangle *t,Triangles *triangles);
	int     buildAround(Edge *e,Cluster *cluster,unsigned *c,unsigned maxc);
	Triangle *getTriangle(real a);
	Triangle *randomTriangle();
	void    removeFromIVertices(Node *node);
};

//////////////////////////////////////////////////////////////////////////////
//
// set of reflectors (light sources and things that reflect some light, no dark things)

class Object;

class Reflectors
{
public:
	unsigned nodes;

	Reflectors();
	~Reflectors();
	void    reset(); // remove all reflectors
	void    resetBest(); // reset acceleration structures for best(), call after big update of primary energies

	bool    check();
	Node    *best(real allEnergyInScene);
	bool    lastBestWantsRefresh() {return refreshing;}
	bool    insert(Node *anode); // returns true when node was inserted (=appended)
	void    insertObject(Object *o);
	void    removeObject(Object *o);
	void    removeSubtriangles();
	bool    findFactorsTo(Node *n);

	private:
		unsigned nodesAllocated;
	protected:
		Node **node;
		void remove(unsigned n);
		// pack of best reflectors (internal cache for best())
		unsigned bests;
		//unsigned bestsReaden;
		Node *bestNode[BESTS];
		bool refreshing; // false = all nodes were selected for distrib, true = for refresh
};

#endif

//////////////////////////////////////////////////////////////////////////////
//
// edge

struct Edge
{
	const Vec3 *vertex[2];
	Triangle   *triangle[2];
	Angle      angle;
	bool       free     :1;
	bool       interpol :1;
};

//////////////////////////////////////////////////////////////////////////////
//
// set of edges

extern unsigned __edgesAllocated;

class Edges
{
public:
	unsigned edges;

	Edges();
	~Edges();
	void    reset();

	void    insert(Edge *key);
	Edge    *get();

	private:
		unsigned edgesAllocated;
		Edge **edge;
};

//////////////////////////////////////////////////////////////////////////////
//
// object, part of scene

class Object
{
public:
	Object(int avertices,int atriangles);
	~Object();

	// unique id for this object, stays the same each time
	unsigned id;

	// object data
	RRObject* importer;
	unsigned vertices;
	unsigned triangles; // primary emitors go first (in DObject)
	unsigned edges;
	Vec3    *vertex;
	Triangle*triangle;
	Edge    *edge;
	void    buildEdges();
	void    buildTopIVertices(unsigned smoothMode);
		private:
		unsigned mergeCloseIVertices(IVertex* ivertex);
		public:
#ifdef ONLY_PLAYER
};

#else
	Channels getVertexIrradiance(unsigned avertex);
	unsigned getTriangleIndex(Triangle* t); // return index of triangle in object, UINT_MAX for invalid input
	IVertex **vertexIVertex; // only for fast approximative getVertexIrradiance
	// IVertex pool
	IVertex *newIVertex();
	void     deleteIVertices();
	IVertex *IVertexPool;
	unsigned IVertexPoolItems;
	unsigned IVertexPoolItemsUsed;

	unsigned clusters;
	Cluster *cluster;
	bool    contains(Triangle *t);
	bool    contains(Cluster *c);
	bool    contains(Node *n);
	void    buildClusters();

	// energies
	Channels objSourceExitingFlux; // primary source exiting radiant flux in Watts
	void    resetStaticIllumination(RRScaler* scaler, bool resetFactors, bool resetPropagation);

	// intersections
	Bound   bound;
	void    detectBounds();
	Triangle* intersection(RRRay& ray, const Point3& eye, const Vec3& direction);

	char    *name;
	bool    check();

#ifdef SUPPORT_TRANSFORMS
	// transformations
	const RRMatrix3x4  *transformMatrix;
	const RRMatrix3x4  *inverseMatrix;
	bool    matrixDirty;
	void    transformBound();
	void    updateMatrices();
#endif
#ifdef SUPPORT_DYNAMIC
	// access to primary emitors
	unsigned trianglesEmiting; // number of first triangles that are primary emitors
#endif

};

#ifdef SUPPORT_DYNAMIC
}
#include "dynamic.h"
namespace rr
{
#endif

//////////////////////////////////////////////////////////////////////////////
//
// scene

class Scene
{
public:
	Scene();
	~Scene();

	Object **object;        // array of objects, first static, then dynamic
	unsigned staticObjects;
	unsigned objects;
	RRSurface *surface;
	unsigned surfaces;
	RRCollider* multiCollider;

	Triangle* intersectionStatic(RRRay& ray, const Point3& eye, const Vec3& direction, Triangle* skip);
	Triangle* intersectionDynobj(RRRay& ray, const Point3& eye, const Vec3& direction, Object *object, Triangle* skip);
	HitChannels rayTracePhoton(Point3 eye,Vec3 direction,Triangle *skip,void *hitExtension,HitChannels power=HitChannels(1));
	Channels  getRadiance(Point3 eye,Vec3 direction,Triangle *skip,Channels power=Channels(1));
	Channels  gatherIrradiance(Point3 eye,Vec3 normal,Triangle *skip,Channels power=Channels(1)); // decreasing power is used only for termination criteria. returns irradiance in W/m^2

	int     turnLight(int whichLight,real intensity); // turns light on/off. just material, no energies modified (use resetStaticIllumination), returns number of lights (emitting materials) in scene

	void    objInsertStatic(Object *aobject);
	void    objRemoveStatic(unsigned o);
	unsigned objNdx(Object *aobject);

	void    freeze(bool yes); // makes multi-object calculation faster, but no object insert/remove/transform is allowed then
	bool    isFrozen();

	void    setScaler(RRScaler* ascaler);
	RRScaler* scaler;


	RRScene::Improvement improveStatic(bool endfunc(void*), void* context);
	void    abortStaticImprovement();
	bool    shortenStaticImprovementIfBetterThan(real minimalImprovement);
	bool    finishStaticImprovement();
	bool    distribute(real maxError);//skonci kdyz nejvetsi mnozstvi nerozdistribuovane energie na jednom facu nepresahuje takovou cast energie lamp (0.001 is ok)
#ifdef SUPPORT_TRANSFORMS
	void    transformObjects();
#endif
#ifdef SUPPORT_DYNAMIC
	void    objInsertDynamic(Object *aobject);
	void    objRemoveDynamic(unsigned o);
	void    objMakeStatic(unsigned o);
	void    objMakeDynamic(unsigned o);
	void    improveDynamic(bool endfunc(void *),void *context);
#endif
	void    iv_forEach(void callback(SubTriangle *s,IVertex *iv,int type));
	void    iv_saveRealFrame(char *name);
	void    iv_cleanErrors();
	void    iv_loadRealFrame(char *name);
	void    iv_addLoadedErrors();
	void    iv_fillEmptyImportants();
	void    iv_markImportants_saveMeshing(unsigned maxvertices,char *namemes);
	void    iv_startSavingBytes(unsigned frames,char *nametga);
	void    iv_saveByteFrame();
	void    iv_savingBytesDone();
	unsigned iv_initLoadingBytes(char *namemes,char *nametga);
	void    iv_loadByteFrame(real frame);
	void    iv_dumpTree(char *name);
	void    iv_dumpVertices(char *name);
	private:
		unsigned iv_savesubs;//tmp set by iv_markImportants,read by iv_startSavingBytes
	public:
	void    draw(RRScene* scene, real quality);
	RRScene::Improvement resetStaticIllumination(bool resetFactors, bool resetPropagation);
	void    updateMatrices();


	// get info
	void    infoScene(char *buf);
	void    infoStructs(char *buf);
	void    infoImprovement(char *buf, int infolevel);
	real    avgAccuracy();
	void    getStats(unsigned* faces, RRReal* sourceExitingFlux, unsigned* rays, RRReal* reflectedIncidentFlux) const;

	private:
		friend class Hits; // GATE
		unsigned allocatedObjects;

		int     phase;
		Node    *improvingStatic;
		Triangles hitTriangles;
		Factors improvingFactors;
		Factors candidatesForClustering;
		void    shotFromToHalfspace(Node *sourceNode);
		void    refreshFormFactorsFromUntil(Node *source,bool endfunc(void *),void *context);
		bool    energyFromDistributedUntil(Node *source,bool endfunc(void *),void *context);

		Channels staticSourceExitingFlux; // primary source exiting radiant flux in Watts, sum of absolute values
		Channels dynamicSourceExitingFlux;
		unsigned shotsForNewFactors;
		unsigned shotsAccumulated;
		unsigned shotsForFactorsTotal;
		unsigned shotsTotal;
		TReflectors staticReflectors; // top nodes in static Triangle trees
		RRMesh** multiObjectMeshes4Delete; // to be deleted with multiCollider

};

void core_Init();
void core_Done();

#endif

} // namespace

#endif
