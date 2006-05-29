#ifndef _WORLD2RRVISION_H
#define _WORLD2RRVISION_H

#include "World.h"
#include "RRVision.h"

rr::RRScene *convert_world2scene(WORLD *w, char *material_mgf, rr::RRCollider::IntersectTechnique intersectTechnique);

#endif
