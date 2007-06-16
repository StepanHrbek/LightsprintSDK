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
	return new RRObjectWithPhysicalMaterialsImpl(this,scaler);
}

} // namespace
