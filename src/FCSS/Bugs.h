#ifndef BUGS_H
#define BUGS_H

#include "Lightsprint/RRVision.h"
#include "Lightsprint/DemoEngine/Texture.h"

class Bugs
{
public:
	virtual void tick(float seconds) = 0;
	virtual void render() = 0;
	virtual ~Bugs() {};

	static Bugs* create(const rr::RRScene* ascene, const rr::RRObject* aobject, unsigned anumBugs);
};

#endif
