#include "Lightsprint/RRIllumination.h"

#include "Lightsprint/RRDebug.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Sky

class Sky : public RRIlluminationEnvironmentMap
{
public:
	Sky(RRColorRGBF atop)
	{
		top = atop;
	}
	virtual void setValues(unsigned size, RRColorRGBF* irradiance)
	{
		RRReporter::report(RRReporter::WARN,"RRIlluminationEnvironmentMap::setValues: Not implemented in RRIlluminationEnvironmentMap::createSky.");
		RR_ASSERT(0);
	}
	virtual RRColorRGBF getValue(const RRVec3& direction) const
	{
		return (direction.y>0)?top:RRColorRGBF(0);
	};
private:
	RRColorRGBF top;
};

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMap

RRColorRGBF RRIlluminationEnvironmentMap::getValue(const RRVec3& direction) const
{
	RRReporter::report(RRReporter::WARN,"RRIlluminationEnvironmentMap::getValue: Not implemented.");
	RR_ASSERT(0);
	return RRColorRGBF(0);
};

void RRIlluminationEnvironmentMap::bindTexture()
{
	RRReporter::report(RRReporter::WARN,"RRIlluminationEnvironmentMap::bindTexture: Not implemented.");
	RR_ASSERT(0);
}

RRIlluminationEnvironmentMap* RRIlluminationEnvironmentMap::createSky(const RRColorRGBF& top)
{
	return new Sky(top);
}

bool RRIlluminationEnvironmentMap::save(const char* filenameMask, const char* cubeSideName[6])
{
	RRReporter::report(RRReporter::WARN,"RRIlluminationEnvironmentMap::save: Not implemented.");
	RR_ASSERT(0);
	return false;
}

} // namespace
