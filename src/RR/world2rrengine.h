#ifndef _WORLD2RRENGINE_H
#define _WORLD2RRENGINE_H

#include "World.h"
#include "RREngine.h"

rrVision::RRScene *convert_world2scene(WORLD *w, char *material_mgf, rrCollider::RRCollider::IntersectTechnique intersectTechnique);

#endif
