#ifndef _RENDER_H
#define _RENDER_H

#include "world.h"
#include "Lightsprint/RRVision.h" // rrscene

#define C_INDICES 256

// d=draw params
extern float   d_gamma;
extern float   d_bright;
extern bool    d_fast;            // max speed, low quality
extern bool    d_gouraud;
extern bool    d_gouraud3;
extern char    d_needle;          // 0=pink 1=retusovat jehly (pomale kresleni)
extern char    d_meshing;         // 0=source faces, 1=reflector meshing, 2=receiver meshing
extern char    d_engine;          // output via rrvision interface, no direct access
extern float   d_details;
extern bool    d_pointers;        // jako barvu pouzit pointer na subtriangle
extern void   *d_factors2;        // zobrazi faktory do nodu
extern char    d_forceSides;      // 0 podle mgf, 1=vse zobrazi 1sided, 2=vse zobrazi 2sided
extern int     d_only_o;          // draw only object o, -1=all
extern bool    d_saving_tga;      // flag ktery je nastaven behem ukladani tga

extern int     p_ffPlay;          // vsechny snimky se loadujou 1=z 000, 2=z tga

extern const char *__hidematerial1;
extern const char *__hidematerial2;
extern const char *__hidematerial3;
extern const char *__exportmaterial;
extern const char *__dontexportmaterial;
extern char  __infolevel;

void render_init();
void render_world(WORLD *w, rr::RRStaticSolver* scene, int camera_id, bool mirrorFrame);

#endif
