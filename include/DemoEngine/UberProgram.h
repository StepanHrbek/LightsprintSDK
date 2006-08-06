#ifndef UBERPROGRAM_H
#define UBERPROGRAM_H

#include "Program.h"
#include <map>


/////////////////////////////////////////////////////////////////////////////
//
// UberProgram

class UberProgram
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
