#ifndef _LDMGF_H
#define _LDMGF_H

#include "mgfparser.h"
#include "Lightsprint/RRObject.h"

// polygons won't share vertices if you say...
extern bool shared_vertices;

// load mgf, return codes are defined in mgfparser.h (success=MG_OK)
int ldmgf(const char *filename,
          void *add_vertex(FLOAT *p,FLOAT *n),
          void *add_surface(C_MATERIAL *material),
          void  add_polygon(unsigned vertices,void **vertex,void *surface),
          void  begin_object(char *name),
          void  end_object(void));

// convert mgf color to rgb
void mgf2rgb(C_COLOR *cin,FLOAT intensity,rr::RRColor& cout);

#endif
