#ifndef _WORLD2RRVISION_H
#define _WORLD2RRVISION_H

#include "World.h"
#include "Lightsprint/RRStaticSolver.h"

rr::RRStaticSolver *convert_world2scene(WORLD *w, char *material_mgf, rr::RRCollider::IntersectTechnique intersectTechnique);

#endif
