// --------------------------------------------------------------------------
// DemoEngine
// Shader, OpenGL 2.0 object.
// Copyright (C) Lightsprint, Stepan Hrbek, 2005-2006
// --------------------------------------------------------------------------

#ifndef SHADER_H
#define SHADER_H

#include "DemoEngine.h"
#include <GL/glew.h>


/////////////////////////////////////////////////////////////////////////////
//
// Shader

class RR_API Shader
{
public:
  Shader(const char* defines, const char* filename, GLenum shaderType = GL_FRAGMENT_SHADER);
  ~Shader();
  
  void compileIt();
  GLuint getHandle();
private:
  char *readShader(const char *filename);
  GLuint handle;
};

#endif
