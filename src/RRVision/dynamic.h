#ifdef SUPPORT_DYNAMIC

#ifndef RRVISION_DYNAMIC_H
#define RRVISION_DYNAMIC_H

#if CHANNELS != 1
#error dynamic.* is not adjusted for CHANNELS!=1
#endif

#ifndef SUPPORT_TRANSFORMS
#error SUPPORT_DYNAMIC requires SUPPORT_TRANSFORMS
#endif

#include "rrcore.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// globals

extern U8 d_drawDynamicHits; // kreslit dynamicky zasahy? (vs. konvertit je na dynamickou energii a kreslit tu)

//////////////////////////////////////////////////////////////////////////////
//
// struct describing one "reflector -> dynamic object" link

struct ReflToDynobj
{
	void    initFrame(Node *refl);
	real    energyR;    // vsechna energie letici z reflektoru
	real    powerR2DSum;// suma dilu energie leticich z reflektoru smerem k boundingsfere (je jich lightShots)
	bool    visible;    // our object is visible
	int     lightShots; // how many rays was shot to object boundingsphere
	int     lightHits;  // how many rays went through whole scene and hit our dynamic object
	int     shadeShots; // how many rays continued as shadow rays
	real    lightShotsP;// UNUSED
	real    lightHitsP; // UNUSED
	real    shadeShotsP;// UNUSED
	real    shadeHitsP; // how many shadow rays went through static scene and hit visible triangle * power * importance
	real    rawAccuracy;
	real    lightAccuracy;
	void    updateLightAccuracy(Shooter *reflector);
	real    shadeAccuracy();

	void    setImportance(real imp);
	real    getImportance();

	private:
		real    importance;
};

//////////////////////////////////////////////////////////////////////////////
//
// set of reflectors (light sources and things that reflect light)
// dynamic version - first go nodesImportant, then others
//                 - gimmeLinks selects most important links for dyn.shooting

class Scene;

class DReflectors : public Reflectors
{
public:
	DReflectors(Scene *ascene);
	~DReflectors();
	void    reset();

	bool    insert(Node *anode);
	void    removeObject(Object *o);
	void    removeSubtriangles();
	void    updateListForDynamicShadows();
	unsigned gimmeLinks(unsigned (shootFunc1)(Scene *scene,Node *refl,Object *dynobj,ReflToDynobj* r2d,unsigned shots),
	                    unsigned (shootFunc2)(Scene *scene,Node *refl,unsigned shots),
	                    bool endfunc(void *),void *context);

	private:
		Scene *scene;
		ReflToDynobj *r2d;
		unsigned r2dAllocated; // # of ReflToDynobj in r2d in objects
		unsigned nodesImportant; // nodes casting dynamic shadows
		unsigned scanpos1; // this node casting dyn.shad. will be checked for possible removing from dyn.shad.
		unsigned scanpos2; // this node not casting dyn.shad. will be checked for possible inserting to dyn.shad.
		real maxImportance; // maximal detected importance
		real importanceLevelToAccept; // 0..1 where 1 means maximal detected importance, more important refls will be accepted to cast shadows
		real importanceLevelToDeny; // 0..1 where 1 means maximal detected importance, less important refls will be denied to cast shadows
		real importance(unsigned n);
		void swapReflectors(unsigned n1,unsigned n2);
		void remove(unsigned n);
		void updateImportant(unsigned n);
		void updateUnimportant(unsigned n);
};

extern unsigned __lightShotsPerDynamicFrame;
extern unsigned __shadeShotsPerDynamicFrame;

} // namespace

#endif

#endif
