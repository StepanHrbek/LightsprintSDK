#ifndef _LDMGF_H
#define _LDMGF_H

#include "mgfparser.h"

// polygons won't share vertices if you say...
extern bool shared_vertices;

// load mgf, return codes are defined in mgfparser.h (success=MG_OK)
int ldmgf(char *filename,
          void *add_vertex(FLOAT *p,FLOAT *n),
          void *add_surface(C_MATERIAL *material),
          void  add_polygon(unsigned vertices,void **vertex,void *surface),
	  void  begin_object(char *name),
	  void  end_object(void));

// get a named material from previously loaded mgf
C_MATERIAL *c_getmaterial(char *);

// convert mgf color to rgb
void xy2rgb(double cx,double cy,double intensity,float cout[3]);
void mgf2rgb(C_COLOR *cin,double intensity,float cout[3]);

#endif
