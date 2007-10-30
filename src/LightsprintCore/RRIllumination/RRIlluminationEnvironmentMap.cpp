#include "Lightsprint/RRDebug.h"
#include "Lightsprint/RRIllumination.h"

namespace rr
{

/////////////////////////////////////////////////////////////////////////////
//
// Sky

class Sky : public RRIlluminationEnvironmentMap
{
public:
	Sky(const RRColorRGBF& _upper, const RRColorRGBF& _lower)
	{
		upper = _upper;
		lower = _lower;
	}
	virtual void setValues(unsigned size, const RRColorRGBF* irradiance)
	{
		if(size && irradiance)
		{
			upper = irradiance[0];
			lower = irradiance[1];
		}
	}
	virtual RRColorRGBF getValue(const RRVec3& direction) const
	{
		return (direction.y>0)?upper:lower;
	};
private:
	RRColorRGBF upper;
	RRColorRGBF lower;
};

RRIlluminationEnvironmentMap* RRIlluminationEnvironmentMap::createSky(const RRColorRGBF& upper, const RRColorRGBF& lower)
{
	return new Sky(upper,lower);
}


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
	RRReporter::report(WARN,"RRIlluminationEnvironmentMap::getValue: Not implemented.");
	RR_ASSERT(0);
	return RRColorRGBF(0);
};

void RRIlluminationEnvironmentMap::bindTexture() const
{
	RRReporter::report(WARN,"RRIlluminationEnvironmentMap::bindTexture: Not implemented.");
	RR_ASSERT(0);
}

bool RRIlluminationEnvironmentMap::save(const char* filenameMask, const char* cubeSideName[6])
{
	RRReporter::report(WARN,"RRIlluminationEnvironmentMap::save: Not implemented.");
	RR_ASSERT(0);
	return false;
}

} // namespace
