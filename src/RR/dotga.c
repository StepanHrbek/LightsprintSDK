#include <stdlib.h>
#include <stdio.h>
#include <allegro.h>

#define byte unsigned char

#define U16 unsigned short
#define U32 unsigned

#define RD(var) fread(&(var),sizeof(var),1,f)

BITMAP *load_TGA(char *name)
{
 FILE *f=fopen(name,"rb"); BITMAP *bmp;

 U32 i; U16 w,h,j;

 RD(i); RD(i); RD(i); RD(w); RD(h); RD(j);

 bmp=create_bitmap(w,h);

 fread(bmp->dat,1,w*h,f); return bmp;
}

int main(int argc, char *argv[])
{
 char name[256]; int *dat,i; BITMAP *r,*g,*b,*bmp;

 byte *rd,*gd,*bd; allegro_init();

 #define LOAD(A) strcpy(name,argv[1]); strcat(name,"."#A".tga"); A=load_TGA(name)

 LOAD(r); LOAD(g); LOAD(b);

 set_color_depth(32);

 bmp=create_bitmap(r->w,r->h);

 dat=bmp->dat; rd=r->dat; gd=g->dat; bd=b->dat;

 for (i=0;i<bmp->w*bmp->h;i++) {
     int ri=*rd++,gi=*gd++,bi=*bd++;
     *dat++=(bi<<16)+(gi<<8)+ri;
     }

 strcpy(name,argv[1]); strcat(name,".tga");

 save_tga(name,bmp,NULL); allegro_exit(); return 0;
}

END_OF_MAIN();
