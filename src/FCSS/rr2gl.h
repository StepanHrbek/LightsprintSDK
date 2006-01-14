//
// high level mgf loader by Stepan Hrbek, dee@mail.cz
//

#ifndef _RR2GL_H
#define _RR2GL_H

#include "RRVision.h"

void     rr2gl_compile(rrVision::RRObjectImporter* objectImporter, rrVision::RRScene* radiositySolver);
void     rr2gl_draw_onlyz();
unsigned rr2gl_draw_indexed(); // returns number of triangles
void     rr2gl_draw_colored(bool direct);

