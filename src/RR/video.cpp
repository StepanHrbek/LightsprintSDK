#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <allegro.h>
#include "video.h"
#include "misc.h"

PALETTE video_Pal;
BITMAP *video_Bmp,*video_256;
float video_XFOV, video_YFOV;
unsigned video_XRES, video_YRES, video_Bits;
bool video_inited=false;

extern int *video_Init(int xres, int yres)
{
 assert(!video_inited);
 if(video_inited) return NULL;

 int i,b,c=GFX_AUTODETECT_WINDOWED;

 if(!allegro_inited) {
   allegro_init();
   allegro_inited=true;
   }

 video_XRES=xres;
 video_YRES=yres;

 b=32;set_color_depth(b);
 if (set_gfx_mode(c,xres,yres,0,0)) {
    b=24;set_color_depth(b);
    if (set_gfx_mode(c,xres,yres,0,0)) {
       b=16;set_color_depth(b);
       if (set_gfx_mode(c,xres,yres,0,0)) {
          b=15;set_color_depth(b);
          if (set_gfx_mode(c,xres,yres,0,0)) {
             b=8;set_color_depth(b);
             if (set_gfx_mode(c,xres,yres,0,0)) return NULL;
             }
          }
       }
    }

 if (set_display_switch_mode(SWITCH_BACKGROUND) != 0)
    printf("Warning: Program may stop when switched to background.");

 clear(screen);

 video_Bmp=create_bitmap_ex(32,xres,yres);

 clear(video_Bmp);

 if (b==8) {

    video_256=create_bitmap_ex(8,xres,yres);

    clear(video_256);

    for (i=0;i<256;i++) {
        video_Pal[i].r=i/4;
        video_Pal[i].g=i/4;
        video_Pal[i].b=i/4;
        }

    set_palette(video_Pal);

    }

 video_Bits=b;
 video_XFOV=xres;
 video_YFOV=-yres*4.0/3.0;

 video_inited=true;

 return (int *)video_Bmp->dat;
}
     
extern void video_Done()
{
 if(!video_inited) return;
 video_inited=false;

 destroy_bitmap(video_Bmp);
 if (video_Bits==8) destroy_bitmap(video_256);
 allegro_exit();
}

extern void video_Clear(float rgb[3])
{
 if(!video_inited) return;
 clear(video_Bmp);
}

extern void video_WriteBuf(char *text)
{
 if(!video_inited)
   printf("%s\n",text);
 else
   textout_ex(video_Bmp,font,text,0,0,0x7F7F7F,0);
}

extern void video_WriteScreen(char *text)
{
 if(!video_inited)
   printf("%s\n",text);
 else
   textout_ex(screen,font,text,0,0,0x7F7F7F,0);
}

extern void video_Grab(char *name)
{
 save_tga(name,video_Bmp,video_Pal);
}

extern void video_Blit()
{
 if(!video_inited) return;
 if (video_Bits==8) {
    byte *o=(byte *)video_256->dat,
         *t=(byte *)video_Bmp->dat,
         *s=o+video_XRES*video_YRES;
    while (o<s) { *o++=(307*t[0]+600*t[1]+117*t[2])>>10; t+=4; }
    blit(video_256,screen,0,0,0,0,video_XRES,video_YRES);
    } else blit(video_Bmp,screen,0,0,0,0,video_XRES,video_YRES);
}

void video_PutPixel(unsigned x,unsigned y,unsigned color)
{
 if(!video_inited) return;
 assert(x>=0 && x<video_XRES);
 assert(y>=0 && y<video_YRES);
 _putpixel32(video_Bmp,x,y,color);
}

unsigned video_GetPixel(unsigned x,unsigned y)
{
 if(!video_inited) return 0;
 assert(x>=0 && x<video_XRES);
 assert(y>=0 && y<video_YRES);
 return _getpixel32(video_Bmp,x,y);
}
