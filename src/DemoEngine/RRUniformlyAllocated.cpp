#include "Lightsprint/DemoEngine/DemoEngine.h"

#include <stdlib.h> // malloc, free


namespace de
{


/////////////////////////////////////////////////////////////////////////////
//
// RRUniformlyAllocated

void* RRUniformlyAllocated::operator new(std::size_t n)
{
	return malloc(n);
};

void* RRUniformlyAllocated::operator new[](std::size_t n)
{
	return malloc(n);
};

void RRUniformlyAllocated::operator delete(void* p, std::size_t n)
{
	if(p) free(p);
};

void RRUniformlyAllocated::operator delete[](void* p, std::size_t n)
{
	if(p) free(p);
};


} //namespace
