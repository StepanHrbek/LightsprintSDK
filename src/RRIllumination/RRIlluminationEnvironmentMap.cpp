#include "Lightsprint/RRIllumination.h"

#include "Lightsprint/RRDebug.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Sky


/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMap

RRColorRGBF RRIlluminationEnvironmentMap::getValue(const RRVec3& direction) const
{
	RRReporter::report(RRReporter::WARN,"RRIlluminationEnvironmentMap::getValue: Not implemented.");
	RR_ASSERT(0);
	return RRColorRGBF(0);
};

void RRIlluminationEnvironmentMap::bindTexture() const
{
	RRReporter::report(RRReporter::WARN,"RRIlluminationEnvironmentMap::bindTexture: Not implemented.");
	RR_ASSERT(0);
}

bool RRIlluminationEnvironmentMap::save(const char* filenameMask, const char* cubeSideName[6])
{
	RRReporter::report(RRReporter::WARN,"RRIlluminationEnvironmentMap::save: Not implemented.");
	RR_ASSERT(0);
	return false;
}

} // namespace
