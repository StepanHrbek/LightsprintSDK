#include "misc.h"
#include <stdio.h>

/*
 Platformne zavisle veci apod.

 Pokud jsou k dispozici funkce kb_hit() a kb_get() rikajici zda
 byla stisknuta a jaka klavesa, program muze na stisk reagovat hned,
 ne az po dokonceni snimku.
 Otazka je jak tyto funkce implementovat.
 Defaultne je podporuji pouze v DJGPP.
 V unixu lze zapnout jejich implementaci pres curses (ale ma to svoje rizika).
 Je-li prilinkovano Allegro, lze volat jeho funkce.
 Je ale nutne si to vyzadat odkomentovanim nasledujiciho #define
*/
//#define KB_VIA_CURSES
//#define KB_VIA_ALLEGRO


// kvuli volitelnemu pouziti allegra na vic mistech se vsichni musi nejak
//  dohodnout jestli uz ho zinicializovali
bool allegro_inited=false;



// keyboard

#ifdef DJGPP
 // standradni fce v djgpp
#else
#ifdef KB_VIA_CURSES //__unix__
 // curses v unixu... vedlejsi efekt jejich pouziti je ze zneskodni stdout
 #include <curses.h>
 #include <stdlib.h>
 static int __ncurses_key=ERR;
 static void exitfunc()
 {
   endwin();
 }
 void kb_init()
 {
   initscr();
   cbreak();
   noecho();
   nodelay(stdscr,TRUE);
   atexit(exitfunc);
 }
 bool kb_hit()
 {
   return __ncurses_key!=ERR ? true : ((__ncurses_key=getch())!=ERR );
 }
 int kb_get()
 {
   while(!kb_hit());
   int i = __ncurses_key;
   __ncurses_key = ERR;
   return i;
 }
 void kb_put(int key)
 {
   __ncurses_key=key;
 };
#else
#ifdef KB_VIA_ALLEGRO
 // standardni fce v allegru
 #include <allegro.h>
 #ifdef RASTERGL
   #include <GL/glut.h>
 #else
   #include "subglut.h"
 #endif
 void kb_init()
 {
   if(!allegro_inited) {
     allegro_init();
     allegro_inited=true;
     }
   install_keyboard();
 }
 bool kb_hit()
 {
   return keypressed();
 }
 int kb_get()
 {
   #if (ALLEGRO_VERSION<4)
     return readkey();
   #else
     // prevede s nicim nekompatibilni allegro4 kody na standardni
     static int last=0;
     if(!last) {last=readkey();}//printf("%i %i\n",last>>8,last&0xff);}
     int result=last&0xff;
     if(!result) last>>=8;else last=0;
     switch(result)
     {
       case 82:return GLUT_KEY_LEFT;
       case 83:return GLUT_KEY_RIGHT;
       case 84:return GLUT_KEY_UP;
       case 85:return GLUT_KEY_DOWN;
      /*
       case 47:return GLUT_KEY_F1;
       case 48:return GLUT_KEY_F2;
       case 49:return GLUT_KEY_F3;
       case 50:return GLUT_KEY_F4;
       case 51:return GLUT_KEY_F5;
       case 52:return GLUT_KEY_F6;
       case 53:return GLUT_KEY_F7;
       case 54:return GLUT_KEY_F8;
       case 55:return GLUT_KEY_F9;
       case 56:return GLUT_KEY_F10;
       case 57:return GLUT_KEY_F11;
       case 58:return GLUT_KEY_F12;
      */
     }
     return result;
   #endif
 }
 void kb_put(int key)
 {
   simulate_keypress(key);
 }
#else
 // nic, na klavesy bude reagovat jednou za cas (0.5sec), ne okamzite
 int buf=-1;
 void kb_init() {};
 bool kb_hit() {return false;};
 int kb_get() {if(buf!=-1) {int result=buf;buf=-1;return result;} return getchar();};
 void kb_put(int key) {buf=key;};
#endif
#endif
#endif


// mouse

#ifdef RASTERGL
 void mouse_init() {}   
 void mouse_show() {}
 void mouse_hide() {}
 bool mouse_hit() {return false;}
#else
 #include <allegro.h>
 int lastx,lasty,lastz,lastb;
 void mouse_init() {   
   if(!allegro_inited) {
     allegro_init();
     allegro_inited=true;
   }
   install_mouse();
 }
 void mouse_show() {show_mouse(screen);}
 void mouse_hide() {show_mouse(NULL);}
 bool mouse_hit() {if(lastb) return true; lastb=mouse_b;if(!lastb) return false;lastx=mouse_x;lasty=mouse_y;lastz=mouse_z;return true;}
 bool mouse_get(int &x,int &y,int &z,int &b) {if(!lastb) return false;x=lastx;y=lasty;z=lastz;b=lastb;lastb=0;return true;}
#endif


// memory

#ifdef DJGPP
 #include <dpmi.h>
 unsigned memavail;
 void _MEM_INIT()
 {
        _go32_dpmi_meminfo info;
        _go32_dpmi_get_free_memory_information(&info);
        memavail=info.available_memory;
 }
 unsigned _MEM_ALLOCATED()
 {
        _go32_dpmi_meminfo info;
        _go32_dpmi_get_free_memory_information(&info);
        return memavail-info.available_memory;
 }
#endif

#define SIMULATE_REALLOC

#include <memory.h>

void* realloc(void* p,size_t oldsize,size_t newsize)
{
#ifdef SIMULATE_REALLOC
 //if(newsize>500000) return realloc(p,newsize);
 // this simulated realloc prevents real but buggy realloc from crashing rr (seen in DJGPP)
 // it is also faster (seen in MinGW)
 void *q=malloc(newsize);
 if(p)
 {
   memcpy(q,p,MIN(oldsize,newsize));
   free(p);
 }
 return q;
#else
 return realloc(p,newsize);
#endif
}

