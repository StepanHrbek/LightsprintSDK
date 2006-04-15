#ifndef GLSL_PROGRAM
#define GLSL_PROGRAM

#include <map>
#include <GL/glew.h>
#include <GL/glut.h>
#include "GLSLObject.hpp"
class GLSLShader;

class GLSLProgram : public GLSLObject
{
public:
	GLSLProgram();
	GLSLProgram(const char* defines, const char* shader, unsigned int shaderType=GL_VERTEX_SHADER_ARB);
	GLSLProgram(const char* defines, const char* vertexShader, const char* fragmentShader);
	~GLSLProgram();

	void attach(GLSLShader &shader);
	void attach(GLSLShader *shader);
	void linkIt();
	bool isReady(); // true after successful link
	void useIt(); // must be ready before use

	void sendUniform(const char *name, float x);
	void sendUniform(const char *name, float x, float y);
	void sendUniform(const char *name, float x, float y, float z);
	void sendUniform(const char *name, float x, float y, float z, float w);
	void sendUniform(const char *name, int count, const GLint* x);
	void sendUniform(const char *name, int x);
	void sendUniform(const char *name, int x, int y);
	void sendUniform(const char *name, int x, int y, int z);
	void sendUniform(const char *name, int x, int y, int z, int w);
	void sendUniform(const char *name, float *m, bool transp=false, int size=4);

private:
	int getLoc(const char *name);

	GLSLShader *vertex, *fragment;
	bool ready;
};

class GLSLProgramSet
{
public:
	GLSLProgramSet(const char* avertexShaderFileName, const char* afragmentShaderFileName);
	virtual ~GLSLProgramSet();

	GLSLProgram* getVariant(const char* defines);

private:
	const char* vertexShaderFileName;
	const char* fragmentShaderFileName;
	std::map<unsigned,GLSLProgram*> cache;
};

#endif //GLSL_PROGRAM
