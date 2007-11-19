#ifndef BUGS_H
#define BUGS_H

#include "../LightsprintCore/RRStaticSolver/RRStaticSolver.h"
#include "Lightsprint/GL/Texture.h"

class Bugs
{
public:
	virtual void tick(float seconds) = 0;
	virtual void render() = 0;
	virtual ~Bugs() {};

	static Bugs* create(const rr::RRStaticSolver* ascene, const rr::RRObject* aobject, unsigned anumBugs);
};

#endif
