#include "Torus.hpp"
#include <stdlib.h>
#include <math.h>
#include <GL/glut.h>
//#include <GL/gl.h>

// Public part :

Torus::Torus(float nInRadius, float nOutRadius, int nInTess, int nOutTess)
  :inRadius(nInRadius), outRadius(nOutRadius), inTess(nInTess),
   outTess(nOutTess)
{
  vertices = new float[inTess * outTess * 3];
  normals = new float[inTess * outTess * 3];
  texCoords = new float[inTess * outTess * 2];
  indices = new unsigned int[inTess * (outTess-1) * 2];
  
  genVerticesNormals();
  genIndices();
}

Torus::~Torus()
{
  delete [] vertices;
  delete [] normals;
  delete [] texCoords;
  delete [] indices;
  
  vertices = normals = texCoords = NULL;
  indices = NULL;
}

void Torus::draw(unsigned int mode)
{
  glEnableClientState(GL_NORMAL_ARRAY);
  glEnableClientState(GL_VERTEX_ARRAY);
  
  glNormalPointer(GL_FLOAT, 0, (void *)normals);
  glVertexPointer(3, GL_FLOAT, 0, (void *)vertices);
  
  for(int i = 0; i < outTess - 1; i++)
    glDrawElements(mode, inTess*2, GL_UNSIGNED_INT, indices + i*inTess*2);
}

// Private part :

void Torus::genVerticesNormals()
{
  for(int i = 0; i < outTess; i++)
    {
      float outCos = cos((float)i/(outTess-1) * M_PI * 2);
      float outSin = sin((float)i/(outTess-1) * M_PI * 2);
      
      for(int j = 0; j < inTess; j++)
	{
	  float inCos = cos((float)j/(inTess-1) * M_PI * 2);
	  float inSin = sin((float)j/(inTess-1) * M_PI * 2);
	  
	  vertices[3 * (i * inTess + j) + 0] = outRadius*outCos
	                                       * (1.0 + inRadius*inCos);
	  vertices[3 * (i * inTess + j) + 1] = outRadius*outSin
	                                       * (1.0 + inRadius*inCos);
	  vertices[3 * (i * inTess + j) + 2] = inRadius*inSin;
	  
	  normals[3 * (i * inTess + j) + 0] = outCos * inCos;
	  normals[3 * (i * inTess + j) + 1] = outSin * inCos;
	  normals[3 * (i * inTess + j) + 2] = inSin;
	}
    }
}

void Torus::genIndices()
{
  for(int i = 0; i < outTess-1; i++)
    for(int j = 0; j < inTess; j++)
      {
	indices[(i*inTess + j) * 2 + 0] = (i*inTess + j);
	indices[(i*inTess + j) * 2 + 1] = ((i+1)*inTess + j);
      }
}

