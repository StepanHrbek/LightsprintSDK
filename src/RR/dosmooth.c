// usage: dosmooth in out power wrap
// example: dosmooth jidelna.tga jidelna.smooth.tga 5 1
//
// vyhlazuje sloupce v .tga, pri wrap!=0 vyhladi i horni a dolni okraje

#include <stdlib.h>
#include <stdio.h>
#include <allegro.h>

#define byte unsigned char

#define U16 unsigned short
#define U32 unsigned

#define WR(var) fwrite(&(var),sizeof(var),1,f)
#define RD(var) fread(&(var),sizeof(var),1,f)

BITMAP *load_TGA(char *name)
{
 FILE *f=fopen(name,"rb"); BITMAP *bmp;

 U32 i; U16 w,h,j;

 RD(i); RD(i); RD(i); RD(w); RD(h); RD(j);

 bmp=create_bitmap(w,h);

 fread(bmp->dat,1,w*h,f); return bmp;
}

void save_TGA(char *name, BITMAP *bmp)
{
 FILE *f=fopen(name,"wb");

 int y; U32 i=0x00030000; U16 j; WR(i);
 i=0x18010000; WR(i);
 i=0; WR(i);
 j=bmp->w; WR(j);
 j=bmp->h; WR(j);
 j=8; WR(j);

 fwrite(bmp->dat,1,bmp->w*bmp->h,f);
}

int main(int argc, char *argv[])
{
 int pass,loop,i,x,y; BITMAP *bmp; byte h[1024]; allegro_init();

 bmp=load_TGA(argv[1]); pass=atoi(argv[3]); loop=atoi(argv[4]);

 if(bmp->h>1)
   for (x=0;x<bmp->w;x++)
     for (i=0;i<pass;i++) {

         //for (y=0;y<bmp->h-1;y++)
           //  h[y]=(bmp->line[y][x]+bmp->line[y+1][x])/2;
         //for (y=0;y<bmp->h-1;y++) bmp->line[y][x]=h[y];

         for (y=0;y<bmp->h;y++) { int b,v;

             if (y-2<0) v=bmp->line[loop?bmp->h+y-2:0][x]; else v=bmp->line[y-2][x];
             if (y-1<0) b=bmp->line[loop?bmp->h+y-1:0][x]; else b=bmp->line[y-1][x]; v+=b*2;

             b=bmp->line[y][x]; v+=b*4;

             if (y+1>=bmp->h) b=bmp->line[loop?y+1-bmp->h:bmp->h-1][x]; else b=bmp->line[y+1][x]; v+=b*2;
             if (y+2>=bmp->h) b=bmp->line[loop?y+2-bmp->h:bmp->h-1][x]; else b=bmp->line[y+2][x]; v+=b;

             v/=(1+2+4+2+1); h[y]=v;

             }

         for (y=0;y<bmp->h;y++) bmp->line[y][x]=h[y];

         }

 save_TGA(argv[2],bmp); allegro_exit(); return 0;
}

END_OF_MAIN();
