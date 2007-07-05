#include "Lightsprint/RRDynamicSolver.h"

namespace rr
{

struct TexelResult
{
	RRColorRGBAF irradiance; // alpha = 0|1
	RRColorRGBAF bentNormal; // alpha = 0|1
	TexelResult() : irradiance(0), bentNormal(0) {}
};

} // namespace
