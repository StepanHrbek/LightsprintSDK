#include <stdlib.h>
#include "UberProgram.h"

using namespace std;

UberProgram::UberProgram(const char* avertexShaderFileName, const char* afragmentShaderFileName)
{
	vertexShaderFileName = avertexShaderFileName;
	fragmentShaderFileName = afragmentShaderFileName;
}

UberProgram::~UberProgram()
{
}

Program* UberProgram::getVariant(const char* defines)
{
	unsigned hash = 0;
	for(const char* tmp = defines;tmp && *tmp;tmp++)
	{
		hash=(hash<<5)+(hash>>27)+*tmp;
	}
	map<unsigned,Program*>::iterator i = cache.find(hash);
	if(i!=cache.end()) return i->second;
	Program* tmp = new Program(defines,vertexShaderFileName,fragmentShaderFileName);
	if(!tmp->isReady())
	{
		delete tmp;
		tmp=NULL;
	}
	cache[hash] = tmp;
	//cache.insert(defines,tmp);
	return tmp;
}

