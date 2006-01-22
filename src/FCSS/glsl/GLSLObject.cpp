#include <iostream>
#include "GLSLObject.hpp"
using namespace std;

// Public part :

GLSLObject::GLSLObject()
{
  if(!initialized)
    init();
}

GLSLObject::~GLSLObject()
{
  glDeleteObjectARB(handle);
}

GLhandleARB GLSLObject::getHandle()
{
  return handle;
}

void GLSLObject::getParameterf(GLenum param, GLfloat *data)
{
  glGetObjectParameterfvARB(handle, param, data);
}

void GLSLObject::getParameteri(GLenum param, GLint *data)
{
  glGetObjectParameterivARB(handle, param, data);
}

// Protected part :

const int GLSLObject::numNeededExts = 4;
const char *GLSLObject::neededExts[] = {"GL_ARB_shading_language_100",
				       "GL_ARB_shader_objects",
				       "GL_ARB_vertex_shader",
				       "GL_ARB_fragment_shader"};

bool GLSLObject::initialized = false;

void GLSLObject::extNotSupported(int i)
{
  cerr << "The extension " << neededExts[i] << " is not supported by your graphic card.\n";
  cerr << "I cannot run without this extension, please buy a new card.\n";
  exit(-1);
}

void GLSLObject::init()
{
  const unsigned char *extensions = glGetString(GL_EXTENSIONS);
  for(int i = 0; i < numNeededExts; i++)
    if(!strstr((char *)extensions, neededExts[i]))
      extNotSupported(i);
  initialized = true;
}
