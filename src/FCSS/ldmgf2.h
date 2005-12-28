//
// high level mgf loader by Stepan Hrbek, dee@mail.cz
//

#ifndef _LDMGF2_H
#define _LDMGF2_H

bool     mgf_load(char *scenename);
void     mgf_compile();
void     mgf_draw_onlyz();
void     mgf_draw_colored();
unsigned mgf_draw_indexed(); // returns number of triangles

