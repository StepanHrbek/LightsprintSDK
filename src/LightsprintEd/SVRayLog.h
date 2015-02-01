// --------------------------------------------------------------------------
// Copyright (C) 1999-2015 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// Scene viewer - ray log for debugging.
// --------------------------------------------------------------------------

#ifndef SVRAYLOG_H
#define SVRAYLOG_H

#include "Lightsprint/Ed/Ed.h"
#include "Lightsprint/RRCollider.h"

namespace rr_ed
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
