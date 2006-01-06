//
// high level mgf loader by Stepan Hrbek, dee@mail.cz
//

#ifndef _RR2GL_H
#define _RR2GL_H

#include "RRVision.h"

void     rr2gl_load(rrVision::RRObjectImporter* objectImporter);
void     rr2gl_draw_onlyz();
void     rr2gl_draw_colored();
unsigned rr2gl_draw_indexed(); // returns number of triangles

