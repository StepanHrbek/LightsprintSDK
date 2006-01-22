#ifndef GLSL_SHADER
#define GLSL_SHADER

#include <GL/glew.h>
#include <GL/glut.h>
#include "GLSLObject.hpp"

class GLSLShader : public GLSLObject
{
public:
  GLSLShader(const char* defines, const char* filename, GLenum shaderType = GL_FRAGMENT_SHADER_ARB);
  
  void compileIt();
private:
  char *readShader(const char *filename);
};

#endif //GLSL_SHADER
