#ifndef SHADER_H
#define SHADER_H

#include <GL/glew.h>
#include <GL/glut.h>


/////////////////////////////////////////////////////////////////////////////
//
// Shader

class Shader
{
public:
  Shader(const char* defines, const char* filename, GLenum shaderType = GL_FRAGMENT_SHADER_ARB);
  ~Shader();
  
  void compileIt();
  GLuint getHandle();
private:
  char *readShader(const char *filename);
  GLuint handle;
};

#endif
