#ifndef GLSL_OBJECT
#define GLSL_OBJECT

#include <GL/glut.h>
//#include <GL/gl.h>
#include <GL/glprocs.h>

#if (defined(WIN32) || defined(_WIN32) || defined(__WIN32) || defined(__WIN32__))
#include <windows.h>
#endif //WIN32, ...

class GLSLObject
{
public:
  GLSLObject();
  ~GLSLObject();

  GLhandleARB getHandle();
  void getParameterf(GLenum param, GLfloat *data);
  void getParameteri(GLenum param, GLint *data);
protected:
  static const int numNeededExts;
  static const char *neededExts[]; 
  static bool initialized;
  static void init();
  static void extNotSupported(int i);
  
  GLhandleARB handle;
};


#endif //GLSL_OBJECT
