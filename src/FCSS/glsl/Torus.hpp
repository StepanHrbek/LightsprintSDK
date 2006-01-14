#ifndef TORUS_H
#define TORUS_H

#include <GL/glut.h>
//#include <GL/gl.h>
#define M_PI 3.14159f

class Torus
{
public:
  Torus(float inRadius, float outRadius, int inTess = 50, int outTess = 50);
  ~Torus();
  
  void draw(unsigned int mode = GL_TRIANGLE_STRIP);
private:
  float inRadius, outRadius;
  int inTess, outTess;
  float *vertices, *normals, *texCoords;
  unsigned int *indices;

  void genVerticesNormals();
  void genIndices();
  void genTorus(float inRadius, float outRadius, int inTess, int outTess);
};

#endif //TORUS_H
