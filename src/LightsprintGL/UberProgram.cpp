// --------------------------------------------------------------------------
// Copyright (C) 1999-2021 Stepan Hrbek
// This file is part of Lightsprint SDK, you can use and/or redistribute it
// only under terms of Lightsprint SDK license agreement. A copy of the agreement
// is available by contacting Lightsprint at http://lightsprint.com
//
// UberProgram, GLSL program parametrized with optional #defines.
// --------------------------------------------------------------------------

#include <cstdlib>
#include <unordered_map>
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
	UberProgramImpl(const rr::RRString& _vertexShaderFileName, const rr::RRString& _fragmentShaderFileName)
	{
		vertexShaderFileName = _vertexShaderFileName;
		fragmentShaderFileName = _fragmentShaderFileName;
	}
	virtual ~UberProgramImpl()
	{
		// delete all programs in cache
		for (Cache::const_iterator i=cache.begin();i!=cache.end();i++)
			delete (*i).second;
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
	rr::RRString vertexShaderFileName;
	rr::RRString fragmentShaderFileName;
	typedef std::unordered_map<unsigned,Program*> Cache;
	Cache cache;
};

/////////////////////////////////////////////////////////////////////////////
//
// UberProgram

UberProgram* UberProgram::create(const rr::RRString& vertexShaderFileName, const rr::RRString& fragmentShaderFileName)
{
	return new UberProgramImpl(vertexShaderFileName,fragmentShaderFileName);
}

}; // namespace
