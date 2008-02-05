// --------------------------------------------------------------------------
// Object adapter.
// Copyright 2005-2008 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include "RRObjectWithPhysicalMaterials.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObject

RRObjectWithPhysicalMaterials* RRObject::createObjectWithPhysicalMaterials(const RRScaler* scaler)
{
	/*
	//!!! zatim nejde zoptimalizovat, protoze update() neni v RRObjectu
	if(!scaler)
	{
		RR_ASSERT(0);
		return this;
	}
	*/
	return this ? new RRObjectWithPhysicalMaterialsImpl(this,scaler) : NULL;
}

} // namespace
