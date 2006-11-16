// --------------------------------------------------------------------------
// DemoEngine
// UberProgram, general Program parametrized using #defines.
// Copyright (C) Stepan Hrbek, Lightsprint, 2005-2006
// --------------------------------------------------------------------------

#ifndef UBERPROGRAM_H
#define UBERPROGRAM_H

#include <map>
#include "Program.h"


/////////////////////////////////////////////////////////////////////////////
//
// UberProgram

class DE_API UberProgram
{
public:
	UberProgram(const char* avertexShaderFileName, const char* afragmentShaderFileName);
	virtual ~UberProgram();

	Program* getProgram(const char* defines);

private:
	const char* vertexShaderFileName;
	const char* fragmentShaderFileName;
	std::map<unsigned,Program*> cache;
};

#endif //UBERPROGRAM_H
