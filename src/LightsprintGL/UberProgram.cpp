// --------------------------------------------------------------------------
// UberProgram, fully general Program with optional #defines.
// Copyright (C) 2005-2012 Stepan Hrbek, Lightsprint. All rights reserved.
// --------------------------------------------------------------------------

#include <cstdlib>
#include <string.h> // _strdup
#include <boost/unordered_map.hpp>
#include "Lightsprint/GL/UberProgram.h"

namespace rr_gl
{

/////////////////////////////////////////////////////////////////////////////
//
// UberProgramImpl
//
// intentionally separated implementation to keep STL outside interface

class UberProgramImpl : public UberProgram
{
public:
	UberProgramImpl(const char* _vertexShaderFileName, const char* _fragmentShaderFileName)
	{
		vertexShaderFileName = _strdup(_vertexShaderFileName);
		fragmentShaderFileName = _strdup(_fragmentShaderFileName);
	}
	virtual ~UberProgramImpl()
	{
		// delete all programs in cache
		for (Cache::const_iterator i=cache.begin();i!=cache.end();i++)
			delete (*i).second;
		free((void*)fragmentShaderFileName);
		free((void*)vertexShaderFileName);
	}

	virtual Program* getProgram(const char* defines)
	{
		unsigned hash = 0;
		for (const char* tmp = defines;tmp && *tmp;tmp++)
		{
			hash=(hash<<5)+(hash>>27)+*tmp;
		}
		Cache::iterator i = cache.find(hash);
		if (i!=cache.end()) return i->second;
		Program* program = Program::create(defines,vertexShaderFileName,fragmentShaderFileName);
		cache[hash] = program;
		return program;
	}

private:
	const char* vertexShaderFileName;
	const char* fragmentShaderFileName;
	typedef boost::unordered_map<unsigned,Program*> Cache;
	Cache cache;
};

/////////////////////////////////////////////////////////////////////////////
//
// UberProgram

UberProgram* UberProgram::create(const char* vertexShaderFileName, const char* fragmentShaderFileName)
{
	return new UberProgramImpl(vertexShaderFileName,fragmentShaderFileName);
}

}; // namespace
