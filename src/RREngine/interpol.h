
#ifndef _INTERPOL_H
#define _INTERPOL_H

namespace rrEngine
{

#ifdef PACK
#pragma pack(1) // recognized by GCC and MSVC
#endif

#define INTERPOL_BETWEEN_A(t1,t2,angle) (angle<=MAX_INTERPOL_ANGLE && t1->grandpa->surface==t2->grandpa->surface)
#define INTERPOL_BETWEEN(t1,t2)         INTERPOL_BETWEEN_A(t1,t2,angleBetweenNormalized(t1->grandpa->n3,t2->grandpa->n3))
#define IV_POINT // +2%space, precise coords without blackpixels (no 2d->3d transforms)

extern real MAX_INTERPOL_ANGLE; // max angle between interpolated neighbours

//////////////////////////////////////////////////////////////////////////////
//
// interpolated vertex

extern unsigned __iverticesAllocated;
extern unsigned __cornersAllocated;

class Node;

struct Corner
{
	Node *node;
	real power;
};

class IVertex
{
public:
	IVertex();
       ~IVertex();

	real    error;
	void    loadCache(real r);
#ifdef ONLY_PLAYER
	real    player_cache;
	byte    player_detail;
#endif

	void    insert(Node *node,bool toplevel,real power,Point3 apoint=Point3(0,0,0));
	void    insertAlsoToParents(Node *node,bool toplevel,real power,Point3 apoint=Point3(0,0,0));
	bool    contains(Node *node);
	void    splitTopLevel(Vertex *avertex);
	void    makeDirty();
	bool    hasRadiosity() {return powerTopLevel!=0;}
	real    radiosity();
	bool    remove(Node *node,bool toplevel);
	bool    isEmpty();
	bool    check();
	bool    check(Point3 apoint);
#ifdef IV_POINT
	Point3  point; // higher comfort, +2%space (+60%IVertex)
#endif

	bool    loaded:1;   // jen pro ucely grabovani/loadovani fak
	bool    saved:1;    // jen pro ucely grabovani/loadovani fak
	bool    important:1;// jen pro ucely grabovani/loadovani fak

	private:
		U8       cacheTime:5;
		U8       cornersAllocatedLn2;
		U16      corners;
		real     cache;
		unsigned cornersAllocated();
		real     powerTopLevel;
		Corner   *corner; // pole corneru tvoricich tento ivertex
		real     getClosestRadiosity();
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

