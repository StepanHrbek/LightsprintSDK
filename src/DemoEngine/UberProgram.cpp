// --------------------------------------------------------------------------
// DemoEngine
// UberProgram, fully general Program with optional #defines.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2007
// --------------------------------------------------------------------------

#include <cstdlib>
#include "Lightsprint/DemoEngine/UberProgram.h"

namespace de
{

UberProgram::UberProgram(const char* avertexShaderFileName, const char* afragmentShaderFileName)
{
	vertexShaderFileName = _strdup(avertexShaderFileName);
	fragmentShaderFileName = _strdup(afragmentShaderFileName);
}

UberProgram::~UberProgram()
{
	// delete all programs in cache
	while(cache.begin()!=cache.end())
	{
		delete cache.begin()->second;
		cache.erase(cache.begin());
	}
	free((void*)fragmentShaderFileName);
	free((void*)vertexShaderFileName);
}

Program* UberProgram::getProgram(const char* defines)
{
	unsigned hash = 0;
	for(const char* tmp = defines;tmp && *tmp;tmp++)
	{
		hash=(hash<<5)+(hash>>27)+*tmp;
	}
	std::map<unsigned,Program*>::iterator i = cache.find(hash);
	if(i!=cache.end()) return i->second;
	Program* program = Program::create(defines,vertexShaderFileName,fragmentShaderFileName);
	cache[hash] = program;
	//cache.insert(defines,tmp);
	return program;
}

}; // namespace
