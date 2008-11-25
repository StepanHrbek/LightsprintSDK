// --------------------------------------------------------------------------
// Scene viewer - ray log for debugging.
// Copyright (C) Stepan Hrbek, Lightsprint, 2007-2008, All rights reserved
// --------------------------------------------------------------------------

#ifndef SVRAYLOG_H
#define SVRAYLOG_H

#include "Lightsprint/RRCollider.h"

namespace rr_gl
{

	class SVRayLog
	{
	public:
		enum {MAX_RAYS=10000};
		struct Ray
		{
			rr::RRVec3 begin;
			rr::RRVec3 end;
			bool infinite;
			bool unreliable;
		};
		static Ray log[MAX_RAYS];
		static unsigned size;

		static void push_back(const rr::RRRay* ray, bool hit);
	};

}; // namespace

#endif
