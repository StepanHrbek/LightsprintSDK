#include "RRObjectWithPhysicalSurfaces.h"

namespace rr
{

//////////////////////////////////////////////////////////////////////////////
//
// RRObject

RRObjectWithPhysicalSurfaces* RRObject::createObjectWithPhysicalSurfaces(const RRScaler* scaler)
{
	/*
	//!!! zatim nejde zoptimalizovat, protoze update() neni v RRObjectu
	if(!scaler)
	{
		assert(0);
		return this;
	}
	*/
	return new RRObjectWithPhysicalSurfacesImpl(this,scaler);
}

} // namespace
