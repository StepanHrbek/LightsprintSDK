#ifndef RRENGINE_CORE_H
#define RRENGINE_CORE_H

#include <stdarg.h>
#include <stdlib.h>
#include "geometry.h"
#include "RREngine.h"
#include "interpol.h"

namespace rrEngine
{

//#define SUPPORT_INTERPOL // support interpolation, +20% memory required
//#define SUPPORT_DYNAMIC  // support dynamic objects/shadows. off=all vertices in scenespace, no transformations
//#define LIGHTMAP         // generate lightmap for each Triangle + texturemapping
//#define HITS_FIXED       // fixed point hits save lots of memory, possible loss of precision
			   // note that fixed hits have no hit extension implemeted (used only for dynamic objects)
#define HIT_PTR          & // hits are passed by reference
#define BESTS          200 // how many best shooters to precalculate in one pass. more=faster best() but less accurate

#define DBGLINE
//#define DBGLINE printf("- %s %i\n",__FILE__, __LINE__);

#ifndef MAX
 #define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef MIN
 #define MIN(a,b) ((a)<(b)?(a):(b))
#endif

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
// memory

void* realloc(void* p,size_t oldsize,size_t newsize);

//////////////////////////////////////////////////////////////////////////////
//
// globals

extern bool __errors; // was there errors during batch work? used to set result

extern unsigned __frameNumber; // frame number increased after each draw

extern RRColor __colorFilter;
  // set only via scene::selectColorFilter()
  // barva svetla jehoz sireni ve scene pocitame.
  // spocitat (grayscale) osvetleni zvlast pro r,g,b a slozit je presnejsi nez
  // spocitat (grayscale) jen pro bilou a obarvit to barvou materialu.
  // to proto, ze v prvnim pripade modry povrch skutecne odrazi jen modrou,
  // kdezto ve druhem odrazi bilou.

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
	real    power; // -1..1, negative energy is for dynamic shooting
#endif
	void    setPower(real apower);
	real    getPower();
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
		real sum_power;
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
	real    power; // this part of shooted energy is transferred to destination
	               // Q:is multiplied by destination's diffuseReflectance?
	               // A:NO, it allows us to calculate 3 channels using factors calculated once

	Factor(Node *destination,real apower);
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
	void    reset();

	real    energyDiffused;
	real    energyToDiffuse;
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
class Triangle;

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
	real    area;

#ifndef ONLY_PLAYER
	void    reset();

	// form factors and how many shots they were calculated with
	Shooter *shooter;
	real    accuracy();     // shots done per energy unit

	// static energy acumulators
	real    energyDirect;   // energy received directly by this node or his subs (not by ancestors)
	real    radiosityIndirect();// radiosity received by ancestors
	bool    loadEnergyFromSubs();
	void    propagateEnergyUp();
	real    getEnergyDynamic(); // gets all energy averaged in more frames

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
	Vec2    u2,v2; // u2=uv[1]-uv[0], v2=uv[2]=uv[0], helps to speed up some calculations
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

	unsigned printGouraud(void *f, IVertex **iv,real scale,real flatambient=0);
	void    drawGouraud(real ambient,IVertex **iv,int df);
	void    drawFlat(real ambient,int df);

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
#ifdef SUPPORT_DYNAMIC
	class Object *object;
#endif

	// geometry
	const Vec3* getVertex(unsigned i) {return qvertex[i];}
	Vec3    getS3() {return *getVertex(0);} // absolute position of start of base (transformed when dynamic)
	Vec3    getR3() {return *getVertex(1)-*getVertex(0);} // absolute sidevectors  r3=vertex[1]-vertex[0], l3=vertex[2]-vertex[0] (all transformed when dynamic)
	Vec3    getL3() {return *getVertex(2)-*getVertex(0);}
	Normal  getN3() {return qn3;}
	Vec3    getU3() {return qu3;}
	Vec3    getV3() {return qv3;}
		private:
		Vec3    *qvertex[3];     // 3x vertex
		Normal  qn3;             // normalised normal vector
		Vec3    qu3,qv3;         // ortonormal base for 2d coordinates in subtriangles
		public:
	struct Edge *edge[3];   // edges
	U8      isValid      :1;// triangle is not degenerated
	U8      isInCluster  :1;// triangle is in cluster
	U8      isNeedle     :1;// triangle is needle-shaped, try to hide it by interpolation
	U8      rotations    :2;// how setGeometry(a,b,c) rotated vertices, 0..2, 1 means that vertex={b,c,a}
	S8      setGeometry(Vec3 *a,Vec3 *b,Vec3* c,Normal *n=NULL,int rots=-1);
	Vec3    to3d(Point2 a);
	Vec3    to3d(int vertex);
	SubTriangle *getNeighbourTriangle(int myside,int *nbsside,IVertex *newVertex);
	IVertex *topivertex[3]; // 3x ivertex
	void    removeFromIVertices(Node *node);
	private:
		void    setGeometryCore(Normal *n=NULL);
	public:

	// surface
	RRSurface *surface;       // material at outer and inner side of Triangle
	real    setSurface(RRSurface *s);

#ifndef ONLY_PLAYER
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
	void    reset();

	bool    check();
	Node    *best(bool distributing,real avgAccuracy,real improveBig,real improveInaccurate);
	bool    insert(Node *anode); // returns true when node was inserted (=appended)
	void    insertObject(Object *o);
	void    removeObject(Object *o);
	void    removeSubtriangles();
	bool    findFactorsTo(Node *n);
	unsigned getInstantRadiosityPoints(unsigned points, RRScene::InstantRadiosityPoint* point);

	private:
		unsigned nodesAllocated;
	protected:
		Node **node;
		void remove(unsigned n);
		// pack of best reflectors (internal cache for best())
		unsigned bests;
		Node *bestNode[BESTS];
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
	RRSceneObjectImporter* importer;
	unsigned vertices;
	unsigned triangles; // primary emitors go first (in DObject)
	unsigned edges;
	Triangle *triangle;
	Edge    *edge;
	void    buildEdges();
	void    buildTopIVertices();
#ifdef ONLY_PLAYER
};

#else

	unsigned clusters;
	Cluster *cluster;
	bool    contains(Triangle *t);
	bool    contains(Cluster *c);
	bool    contains(Node *n);
	void    buildClusters();

	// energies
	real    energyEmited;
	void    resetStaticIllumination();

	// intersections
	rrIntersect::RRIntersect*intersector;
	Bound   bound;
	void    detectBounds();
	bool    intersection(Point3 eye,Vec3 direction,Triangle *skip,Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance);

	char    *name;
	bool    check();

#ifdef SUPPORT_DYNAMIC
	// access to primary emitors
	unsigned trianglesEmiting; // number of first triangles that are primary emitors

	// transformations
	const Matrix  *transformMatrix;
	const Matrix  *inverseMatrix;
	bool    matrixDirty;
	void    transformBound();
#endif

};

#ifdef SUPPORT_DYNAMIC
}
#include "dynamic.h"
namespace rrEngine
{
#endif

/*/////////////////////////////////////////////////////////////////////////////
//
// instant radiosity

struct InstantRadiosityPoint
{
	Vec3 pos;
	Vec3 norm;
	Vec3 col;
};*/

//////////////////////////////////////////////////////////////////////////////
//
// scene

extern unsigned __hitsOuter;
extern unsigned __hitsInner;

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
		real    energyEmitedByStatics;
		real    energyEmitedByDynamics;

	bool    intersectionStatic(Point3 eye,Vec3 direction,Triangle *skip,Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance);
	bool    intersectionDynobj(Point3 eye,Vec3 direction,Triangle *skip,Object *object,Triangle **hitTriangle,Hit *hitPoint2d,bool *hitOuterSide,real *hitDistance);
	real    rayTracePhoton(Point3 eye,Vec3 direction,Triangle *skip,void *hitExtension,real power=1);
//	Color   rayTraceCamera(Point3 eye,Vec3 direction,Triangle *skip,Color power=Color(1,1,1));

	char    selectColorFilter(int i);
	int     turnLight(int whichLight,real intensity); // turns light on/off. just material, no energies modified (use resetStaticIllumination), returns number of lights (emitting materials) in scene

	void    objInsertStatic(Object *aobject);
	void    objRemoveStatic(unsigned o);
	unsigned objNdx(Object *aobject);
	bool    improveStatic(bool endfunc(Scene *));
	void    abortStaticImprovement();
	bool    shortenStaticImprovementIfBetterThan(real minimalImprovement);
	bool    finishStaticImprovement();
	bool    distribute(real maxError);//skonci kdyz nejvetsi mnozstvi nerozdistribuovane energie na jednom facu nepresahuje takovou cast energie lamp (0.001 is ok)
#ifdef SUPPORT_DYNAMIC
	void    objInsertDynamic(Object *aobject);
	void    objRemoveDynamic(unsigned o);
	void    objMakeStatic(unsigned o);
	void    objMakeDynamic(unsigned o);
	void    transformObjects();
	void    improveDynamic(bool endfunc(Scene *));
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
	void    draw(real quality);
	void    resetStaticIllumination(bool preserveFactors);

	unsigned getInstantRadiosityPoints(unsigned points, RRScene::InstantRadiosityPoint* point);

	// get info
	void    infoScene(char *buf);
	void    infoImprovement(char *buf);
	real    avgAccuracy();

	private:
		unsigned allocatedObjects;

		int     phase;
		Node    *improvingStatic;
		Triangles hitTriangles;
		Factors improvingFactors;
		Factors candidatesForClustering;
		void    shotFromToHalfspace(Node *sourceNode);
		void    refreshFormFactorsFromUntil(Node *source,real accuracy,bool endfunc(Scene *));
		bool    energyFromDistributedUntil(Node *source,bool endfunc(Scene *));

		unsigned shotsForNewFactors;
		unsigned shotsAccumulated;
		unsigned shotsForFactorsTotal;
		unsigned shotsTotal;
		real    improveBig;
		real    improveInaccurate;
		TReflectors staticReflectors; // top nodes in static Triangle trees
};

void core_Init();
void core_Done();

#endif

} // namespace

#endif
