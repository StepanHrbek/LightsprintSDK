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
	Sky(const RRVec3& _upper, const RRVec3& _lower)
	{
		upper = _upper;
		lower = _lower;
	}
	virtual void setValues(unsigned size, const RRVec3* irradiance)
	{
		if(size && irradiance)
		{
			upper = irradiance[0];
			lower = irradiance[1];
		}
	}
	virtual RRVec3 getValue(const RRVec3& direction) const
	{
		return (direction.y>0)?upper:lower;
	};
private:
	RRVec3 upper;
	RRVec3 lower;
};

RRIlluminationEnvironmentMap* RRIlluminationEnvironmentMap::createSky(const RRVec3& upper, const RRVec3& lower)
{
	return new Sky(upper,lower);
}


/////////////////////////////////////////////////////////////////////////////
//
// UniformEnvironment

class UniformEnvironment : public RRIlluminationEnvironmentMap
{
public:
	UniformEnvironment::UniformEnvironment(const RRVec3& irradiance)
	{
		color = irradiance;
	}
	virtual void setValues(unsigned size, const RRVec3* irradiance)
	{
		if(size && irradiance)
			color = irradiance[0];
	}
	virtual RRVec3 getValue(const RRVec3& direction) const
	{
		return color;
	};
private:
	RRVec3 color;
};

RRIlluminationEnvironmentMap* RRIlluminationEnvironmentMap::createUniform(const RRVec3 irradiance)
{
	return new UniformEnvironment(irradiance);
}

/////////////////////////////////////////////////////////////////////////////
//
// RRIlluminationEnvironmentMap

RRVec3 RRIlluminationEnvironmentMap::getValue(const RRVec3& direction) const
{
	RRReporter::report(WARN,"RRIlluminationEnvironmentMap::getValue: Not implemented.");
	RR_ASSERT(0);
	return RRVec3(0);
};

void RRIlluminationEnvironmentMap::bindTexture() const
{
	LIMITED_TIMES(1,RRReporter::report(WARN,"RRIlluminationEnvironmentMap::bindTexture: Not implemented."));
	//RR_ASSERT(0);
}

bool RRIlluminationEnvironmentMap::save(const char* filenameMask, const char* cubeSideName[6])
{
	RRReporter::report(WARN,"RRIlluminationEnvironmentMap::save: Not implemented.");
	RR_ASSERT(0);
	return false;
}

} // namespace
