#ifndef _MISC_H
#define _MISC_H

#include <stdlib.h>


// misc

#define DBGLINE
//#define DBGLINE printf("- %s %i\n",__FILE__, __LINE__);

#ifndef MAX
 #define MAX(a,b) ((a)>(b)?(a):(b))
#endif
#ifndef MIN
 #define MIN(a,b) ((a)<(b)?(a):(b))
#endif

extern bool allegro_inited;


// time

#ifdef DJGPP
 // high precision DJGPP timer
 #include <time.h>
 #define TIME    uclock_t         
 #define GETTIME uclock()
 #define PER_SEC UCLOCKS_PER_SEC
#else
 // ANSI timer
 #include <time.h>
 #define TIME    clock_t            
 #define GETTIME clock()
 #define PER_SEC CLOCKS_PER_SEC
#endif


// keyboard

#ifdef DJGPP
 // standardni fce v djgpp
 #include <conio.h>
 #include <pc.h>
 #define kb_init()
 #define kb_hit() kbhit()
 #define kb_get() getch()
 #define kb_put(ch) 0
#else
 // jiny jinde, viz misc.cpp
 extern void kb_init();
 extern bool kb_hit();
 extern int  kb_get();
 extern void kb_put(int key);
#endif


// mouse

extern void mouse_init();
extern void mouse_show();
extern void mouse_hide();
extern bool mouse_hit();
extern bool mouse_get(int &x,int &y,int &z,int &b);


// memory

#ifdef DJGPP
 void _MEM_INIT();
 unsigned _MEM_ALLOCATED();
 #define MEM_INIT      _MEM_INIT()
 #define MEM_ALLOCATED _MEM_ALLOCATED()
#else
 #define MEM_INIT      
 #define MEM_ALLOCATED 0
#endif

void* realloc(void* p,size_t oldsize,size_t newsize);


#endif
