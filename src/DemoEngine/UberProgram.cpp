#include <stdlib.h>
#include "DemoEngine/UberProgram.h"

UberProgram::UberProgram(const char* avertexShaderFileName, const char* afragmentShaderFileName)
{
	vertexShaderFileName = avertexShaderFileName;
	fragmentShaderFileName = afragmentShaderFileName;
}

UberProgram::~UberProgram()
{
	// delete all programs in cache
	while(cache.begin()!=cache.end())
	{
		delete cache.begin()->second;
		cache.erase(cache.begin());
	}
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
	Program* program = new Program(defines,vertexShaderFileName,fragmentShaderFileName);
	if(!program->isLinked())
	{
		delete program;
		program = NULL;
	}
	cache[hash] = program;
	//cache.insert(defines,tmp);
	return program;
}

