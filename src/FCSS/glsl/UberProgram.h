#ifndef UBERPROGRAM_H
#define UBERPROGRAM_H

#include "Program.h"
#include <map>

class UberProgram
{
public:
	UberProgram(const char* avertexShaderFileName, const char* afragmentShaderFileName);
	virtual ~UberProgram();

	Program* getVariant(const char* defines);

private:
	const char* vertexShaderFileName;
	const char* fragmentShaderFileName;
	std::map<unsigned,Program*> cache;
};

#endif //UBERPROGRAM_H
