#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRIllumination.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Sky


/////////////////////////////////////////////////////////////////////////////
//
// UniformEnvironment

class UniformEnvironment : public RRIlluminationEnvironmentMap
{
public:
	UniformEnvironment::UniformEnvironment(const RRColorRGBF& irradiance)
	{
		color = irradiance;
	}
	virtual void setValues(unsigned size, const RRColorRGBF* irradiance)
	{
		if(size && irradiance)
			color = irradiance[0];
	}
	virtual RRColorRGBF getValue(const RRVec3& direction) const
	{
		return color;
	};
private:
	RRColorRGBF color;
};

RRIlluminationEnvironmentMap* RRIlluminationEnvironmentMap::createUniform(const RRColorRGBF irradiance)
{
	return new UniformEnvironment(RRColorRGBF(irradiance));
}

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
