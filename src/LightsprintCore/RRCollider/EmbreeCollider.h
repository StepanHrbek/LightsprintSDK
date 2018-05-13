//----------------------------------------------------------------------------
// Copyright (C) 1999-2016 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// EmbreeCollider class - Embree based colliders.
// --------------------------------------------------------------------------

#ifndef COLLIDER_EMBREECOLLIDER_H
#define COLLIDER_EMBREECOLLIDER_H

#ifdef SUPPORT_EMBREE

#include "Lightsprint/RRCollider.h"

namespace rr
{

RRCollider* createEmbreeCollider(const RRMesh* mesh, RRCollider::IntersectTechnique intersectTechnique, bool& aborting);
RRCollider* createEmbreeMultiCollider(const RRObjects& objects, RRCollider::IntersectTechnique intersectTechnique, bool& aborting);

} //namespace

#endif
#endif
