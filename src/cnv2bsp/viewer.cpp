#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cnv2bsp.h"

int main(int argc,char **argv)
{
 int dir=0,one=0,i;
 WORLD *w;

 printf("\n -=[ BSP/KD generator v2.2 ]=- by ReDox/MovSD (C) 2000..2005\n\n");

 if (argc<2) {
    printf(" usage: cnv2bsp <*.[3ds|mgf]> <*.bsp> [dir|one|{max}] \n\n"
           " dir -> create sub-directory for each object to store lightmaps\n"
           " one -> produce one big BSP tree by taking all objects together\n"
           " example: cnv2bsp big.mgf big.bsp one\n"); return 0; }

 for (i=3;i<argc;i++) {
     if (!strcmp(argv[i],"dir")) dir=1;
     if (!strcmp(argv[i],"one")) one=1;
     }

 w=load_Scene(argv[1],argv[2],dir,one);

 return 0;
}
