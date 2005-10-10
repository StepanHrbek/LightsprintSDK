
#ifndef RRENGINE_INTERPOL_H
#define RRENGINE_INTERPOL_H

#include <set>

namespace rrEngine
{

/*
INTERPOL_BETWEEN tells if it's good idea to interpolate between two triangles
 different surface -> yes (was no, but it was in time when exitances were interpolated. now we interpolate irradiances)
 (angle too big) * (areas of too different size) -> no
If we interpolate between areas of too different size, small dark tri + large lit tri would
 go both to grey which makes scene much darker than it should be.
*/
#define INTERPOL_BETWEEN_A(t1,t2,angle) (angle<=(MIN(t1->area,t2->area)/(t1->area+t2->area)*2+0.2f)*MAX_INTERPOL_ANGLE /*&& t1->grandpa->surface==t2->grandpa->surface*/)
#define INTERPOL_BETWEEN(t1,t2)         INTERPOL_BETWEEN_A(t1,t2,angleBetweenNormalized(t1->grandpa->getN3(),t2->grandpa->getN3()))

#ifdef SUPPORT_MIN_FEATURE_SIZE
	// vypnuto protoze posouva vertexy do pozice jejich ivertexu, coz pri mergovani blizkych ivertexu neni zadouci
#else
	#define IV_POINT // +2%space, precise coords without blackpixels (no 2d->3d transforms)
#endif

extern real MAX_INTERPOL_ANGLE; // max angle between interpolated neighbours

//////////////////////////////////////////////////////////////////////////////
//
// interpolated vertex

extern unsigned __iverticesAllocated;
extern unsigned __cornersAllocated;

class Node;
class Object;

struct Corner
{
	Node *node;
	real power;
};

struct IVertexInfo
// used by: merge close ivertices
{
	void               absorb(IVertexInfo& info2); // this <- this+info2, info2 <- 0
	class IVertex*     ivertex;                    // ivertex this information came from
	Point3             center;                     // center of our vertices
	std::set<unsigned> ourVertices;                // our vertices
	std::set<unsigned> neighbourVertices;          // neighbours of our vertices (na ktere se da od nasich dojet po jedne hrane)
};

class IVertex
{
public:
	IVertex();
	~IVertex();

	real    error;
	void    loadCache(Channels r);
#ifdef ONLY_PLAYER
	Channels player_cache;
	U8      player_detail;
#endif

	void    insert(Node *node,bool toplevel,real power,Point3 apoint=Point3(0,0,0));
	void    insertAlsoToParents(Node *node,bool toplevel,real power,Point3 apoint=Point3(0,0,0));
	bool    contains(Node *node);
	unsigned splitTopLevel(Vec3 *avertex, Object *obj);
	void    makeDirty();
	bool    hasExitance() {return powerTopLevel!=0;}
	Channels irradiance();
	Channels exitance(Node* corner);
	bool    remove(Node *node,bool toplevel);
	bool    isEmpty();

	// used by: merge close ivertices
	void    fillInfo(Object* object, unsigned originalVertexIndex, IVertexInfo& info);
	void    absorb(IVertex* aivertex);
	unsigned getNumCorners() {return corners;}

	bool    check();
	bool    check(Point3 apoint);
#ifdef IV_POINT
	Point3  point; // higher comfort, +2%space (+60%IVertex)
#endif

	bool    loaded:1;   // jen pro ucely grabovani/loadovani fak
	bool    saved:1;    // jen pro ucely grabovani/loadovani fak
	bool    important:1;// jen pro ucely grabovani/loadovani fak

	private:
		U8       cacheTime:5; // fix __frameNumber&0x1f if you change :5
		U8       cacheValid:1;
		U8       cornersAllocatedLn2:7;
		U16      corners;
		Channels cache;	// cached irradiance
		unsigned cornersAllocated();
		real     powerTopLevel;
		Corner   *corner; // pole corneru tvoricich tento ivertex
		Channels getClosestIrradiance();
};

//////////////////////////////////////////////////////////////////////////////
//
// .ora oraculum
// akcelerovany orakulum, poradi otazek je znamo a jsou predpocitany v poli

extern bool ora_filling;
extern bool ora_reading;

void ora_filling_init();
void ora_fill(S8 *ptr);
void ora_filling_done(char *filename);

void ora_reading_init(char *filename);
S8   ora_read();
void ora_reading_done();

} // namespace

#endif

