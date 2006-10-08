#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "raster.h"

#define STEP 16
#define EDGES 10
#define FIX 65536.0
#define INF 16384
#define MAXMEM 1024

#define _C_(a,b) ((*raster_Camera)[a][b])
#define _M_(a,b) ((*raster_Matrix)[a][b])
#define ROUND(A) (int)((float)(A))
#define INT(A) ((A)>>16)

#define OUT_512(U,V) case 512:    *out=raster_Light[raster_Texture[((U>>16)&0x1ff)+((V>>7)&0xfffffe00)]]; break;
#define OUT_256(U,V) case 256:    *out=raster_Light[raster_Texture[((U>>16)&0xff)+((V>>8) &0xffffff00)]]; break;
#define OUT_128(U,V) case 128:    *out=raster_Light[raster_Texture[((U>>16)&0x7f)+((V>>9) &0xffffff80)]]; break;
#define OUT_64(U,V)  case  64:    *out=raster_Light[raster_Texture[((U>>16)&0x3f)+((V>>10)&0xffffffc0)]]; break;
#define OUT_32(U,V)  case  32:    *out=raster_Light[raster_Texture[((U>>16)&0x1f)+((V>>11)&0xffffffe0)]]; break;
#define OUT_16(U,V)  case  16:    *out=raster_Light[raster_Texture[((U>>16)&0xf) +((V>>12)&0xfffffff0)]]; break;

#define INIT start_y = red->y; i = red_num;                 l_x = led->x; l_dxdy = led->dxdy; l_num = led->num;                 r_x = red->x; r_dxdy = red->dxdy; r_num = red->num;

#define SPAN l=INT(l_x);c=INT(r_x); if (l>c) {s=l;l=c;c=s;} c-=l;l++; if (c>0)

#define NEXT if (--l_num==0) { led++; l_x = led->x;                         l_dxdy = led->dxdy; l_num = led->num;                       } else l_x += l_dxdy;                                    if (--r_num==0) { i--; red++; r_x = red->x;                    r_dxdy = red->dxdy; r_num = red->num;                       } else r_x += r_dxdy;

#define XY x[1] = x[1] - x[0]; y[1] = y[0] - y[1];                      x[2] = x[0] - x[2]; y[2] = y[2] - y[0];                      c = x[1]*y[2] - y[1]*x[2]; if (c==0) return 0;               x[1]/=c; y[1]/=c; x[2]/=c; y[2]/=c;

#define DU u[1]-=u[0]; u[2]-=u[0];                            dudx = u[1]*y[2] + u[2]*y[1];                      dudy = u[1]*x[2] + u[2]*x[1];                      orgu = u[0] - x[0]*dudx - y[0]*dudy;               dudx16 = dudx*16;

#define DV v[1]-=v[0]; v[2]-=v[0];                            dvdx = v[1]*y[2] + v[2]*y[1];                      dvdy = v[1]*x[2] + v[2]*x[1];                      orgv = v[0] - x[0]*dvdx - y[0]*dvdy;               dvdx16 = dvdx*16;

#define DZ z[1]-=z[0]; z[2]-=z[0];                            dzdx = z[1]*y[2] + z[2]*y[1];                      dzdy = z[1]*x[2] + z[2]*x[1];                      orgz = z[0] - x[0]*dzdx - y[0]*dzdy;               dzdx16 = dzdx*16;

typedef struct {
        float a,b,c,d;
        float ta,tb,tc,td;
        } raster_PLANE;

typedef struct { int x; int y; int num; int dxdy; } raster_EDGE;

int *raster_Output;
static int *raster_Light;
static float *raster_Zbuffer;
static raster_MATRIX *raster_Camera, *raster_Matrix;
static int raster_XRES,raster_YRES,raster_WIDTH;
static int led_num,red_num;
static raster_EDGE led_buf[EDGES],red_buf[EDGES],*led,*red;
static raster_PLANE pp[4],*p0=pp,*p1=pp+1,*p2=pp+2,*p3=pp+3;
static U8 raster_Memory[MAXMEM],*raster_MemNext,
          *raster_MemEnd=raster_Memory+MAXMEM;

static float dudx, dudy, orgu, dudx16,
             dzdx, dzdy, orgz, dzdx16;

//********************** Clipping & Transformation ********************

static U8 *raster_Alloc(int size)
{
 U8 *tmp=raster_MemNext;
 if (tmp+size>=raster_MemEnd) abort();
 raster_MemNext+=size;
 return tmp;
}

static void raster_TransformVertex(raster_POINT *p)
{
 float x=p->x,y=p->y,z=p->z;
 p->tx = x*_C_(0,0) + y*_C_(1,0) + z*_C_(2,0) + _C_(3,0);
 p->ty = x*_C_(0,1) + y*_C_(1,1) + z*_C_(2,1) + _C_(3,1);
 p->tz = x*_C_(0,2) + y*_C_(1,2) + z*_C_(2,2) + _C_(3,2);
 p->sx = raster_XRES/2 - p->tx/p->tz;
 p->sy = raster_YRES/2 - p->ty/p->tz;
}

static void raster_TransformPlane(raster_PLANE *p)
{
 p->a = p->ta*_M_(0,0) + p->tb*_M_(1,0) + p->tc*_M_(2,0);
 p->b = p->ta*_M_(0,1) + p->tb*_M_(1,1) + p->tc*_M_(2,1);
 p->c = p->ta*_M_(0,2) + p->tb*_M_(1,2) + p->tc*_M_(2,2);
 p->d = -(p->a*_M_(3,0) + p->b*_M_(3,1) + p->c*_M_(3,2));
}

static void raster_SetPlane(raster_PLANE *p, float a1, float a2, float a3,
                                             float b1, float b2, float b3,
                                             float c1, float c2, float c3)
{
 float u1,v1,u2,v2,u3,v3,i,j,k,s;

 u1 = b1-a1; u2 = b2-a2; u3 = b3-a3;
 v1 = a1-c1; v2 = a2-c2; v3 = a3-c3;
 i = v2*u3-u2*v3; j = v3*u1-u3*v1; k = v1*u2-u1*v2;
 s = sqrt(i*i+j*j+k*k); if (s==0) s=1; i/=s; j/=s; k/=s;
 s = i*a1 + j*a2 + k*a3; p->ta = i; p->tb = j; p->tc = k; p->td = -s;
}

extern void raster_Init(int xres, int yres)
{
 raster_XRES=xres; raster_YRES=yres;
 raster_Zbuffer=(float *)malloc(sizeof(float)*raster_XRES*raster_YRES);
}

extern void raster_Clear()
{
 memset(raster_Zbuffer,0,sizeof(float)*raster_XRES*raster_YRES);
}

extern void raster_SetFOV(float xfov, float yfov)
{
 float xr,yb,xl,yt;

 xr = (raster_XRES-1.1-raster_XRES/2.0) / xfov;
 yb = (raster_YRES-1.1-raster_YRES/2.0) / yfov;
 xl = (0.1-raster_XRES/2.0) / xfov;
 yt = (0.1-raster_YRES/2.0) / yfov;

 raster_SetPlane(p0,0,0,0,xr,0,1,xr,1,1);
 raster_SetPlane(p1,0,0,0,xl,1,1,xl,0,1);
 raster_SetPlane(p2,0,0,0,0,yb,1,1,yb,1);
 raster_SetPlane(p3,0,0,0,1,yt,1,0,yt,1);
}

extern void raster_SetMatrix(raster_MATRIX *cam, raster_MATRIX *inv)
{
 raster_Camera=cam;
 raster_Matrix=inv;
 raster_TransformPlane(p0);
 raster_TransformPlane(p1);
 raster_TransformPlane(p2);
 raster_TransformPlane(p3);
}

static int raster_LocatePoint(raster_POINT *p)
{
 unsigned int orr=0;

 p->dist[0] = p->x*p0->a + p->y*p0->b + p->z*p0->c + p0->d;
 p->dist[1] = p->x*p1->a + p->y*p1->b + p->z*p1->c + p1->d;
 p->dist[2] = p->x*p2->a + p->y*p2->b + p->z*p2->c + p2->d;
 p->dist[3] = p->x*p3->a + p->y*p3->b + p->z*p3->c + p3->d;

 if (p->dist[0]<0) orr|=1;
 if (p->dist[1]<0) orr|=2;
 if (p->dist[2]<0) orr|=4;
 if (p->dist[3]<0) orr|=8;

 return orr;
}

static raster_VERTEX *raster_ClipEdge(raster_VERTEX *a,
                                      raster_VERTEX *b, float t)
{
 raster_VERTEX *v=(raster_VERTEX *)raster_Alloc(sizeof(raster_VERTEX));
 raster_POINT *p=(raster_POINT *)raster_Alloc(sizeof(raster_POINT));
 memset(p,0,sizeof(raster_POINT));

 p->x = a->point->x + (b->point->x - a->point->x)*t;
 p->y = a->point->y + (b->point->y - a->point->y)*t;
 p->z = a->point->z + (b->point->z - a->point->z)*t;
 p->u = a->point->u + (b->point->u - a->point->u)*t;
 p->v = a->point->v + (b->point->v - a->point->v)*t;

 raster_LocatePoint(p);

 v->point = p;
 v->next = NULL;

 return v;
}

static raster_POLYGON *raster_ClipPolygon(raster_POLYGON *poly, int id)
{
 raster_VERTEX *c=poly,*n,*p=NULL,**v=&p; float t; int num=0;

 while (c) {

       n=c->next; if (!n) n=poly;

       if (c->point->dist[id]<0) {

          if (n->point->dist[id]>=0) {
             t = c->point->dist[id] /
                (c->point->dist[id] - n->point->dist[id]);
            *v = raster_ClipEdge(c,n,t); v = &((*v)->next); num++;
             }

          } else {

         *v =(raster_VERTEX *)raster_Alloc(sizeof(raster_VERTEX));
          memcpy(*v,c,sizeof(raster_VERTEX));
          v = &((*v)->next); *v=NULL; num++;

          if (n->point->dist[id]<0) {
             t = c->point->dist[id] /
                (c->point->dist[id] - n->point->dist[id]);
            *v = raster_ClipEdge(c,n,t); v = &((*v)->next); num++;
             }
          }

       c=c->next;

       }

 return (num<3) ? NULL : p;
}

static raster_POLYGON *raster_Visibility(raster_POLYGON *p)
{
 raster_VERTEX *v=p; unsigned int andd=0xFFFFFFFF,orr=0,a;

 raster_MemNext=raster_Memory;

 while (v) { a=raster_LocatePoint(v->point); andd&=a; orr|=a; v=v->next; }

 #define CLIP(A,B) if (orr&A) if (!(p=raster_ClipPolygon(p,B))) return NULL;
 if (andd) return NULL; CLIP(1,0); CLIP(2,1); CLIP(4,2); CLIP(8,3);

 v=p; while (v) { raster_TransformVertex(v->point); v=v->next; }

 return p;
}

//********************** Scan-line conversion *************************

static int raster_AddEdge(raster_VERTEX *c, raster_VERTEX *n,
                          raster_EDGE *ed)
{
 float x1,y1,x2,y2,dxdy,first_y,last_y; int num;

 x1 = c->point->sx;
 y1 = c->point->sy;
 x2 = n->point->sx;
 y2 = n->point->sy;

 first_y = ceil(y1);
 last_y = ceil(y2);

 num = ROUND(last_y - first_y);

 if (num<=0) return 0;

 dxdy = (x2-x1) / (y2-y1);
 x1+= (first_y-y1) * dxdy;
 x1*=FIX; dxdy*=FIX;

 ed->dxdy = ROUND(dxdy);
 ed->x = ROUND(x1);
 ed->y = ROUND(first_y);
 ed->num = num;

 return 1;
}

static int raster_CreateEdges(raster_POLYGON *p)
{
 int start_y,end_y,led_start=EDGES-1,red_end=0;
 float y1,min_y=INF,max_y=-INF;
 raster_VERTEX *min=NULL,*max=NULL,*c,*n;

 led_num = 0;
 red_num = 0;

 c = p;
 while (c) {
       y1 = c->point->sy;
       if (y1 < min_y) { min_y = y1; min = c; }
       if (y1 > max_y) { max_y = y1; max = c; }
       c=c->next;
       }

 start_y = ROUND(ceil(min_y));
 end_y = ROUND(ceil(max_y)-1);
 if (start_y>end_y) return 0;

 c = min;
 while (c!=max) {
       n = c->next; if (!n) n = p;
       if (raster_AddEdge(c,n,&red_buf[red_end]))
          { red_end++; red_num++; }
       c = n;
       }

 c = max;
 while (c!=min) {
       n = c->next; if (!n) n = p;
       if (raster_AddEdge(n,c,&led_buf[led_start]))
          { led_start--; led_num++; }
       c = n;
       }

 led = led_buf+led_start+1;
 red = red_buf;

 return 1;
}

//************************ Linear interpolation ***********************

static int raster_GouraudInit(raster_POLYGON *p)
{
 float x[3],y[3],z[3],u[3],c; int j;

 for (j=0;j<3;j++) {
     z[j] = -1 / p->point->tz;
     u[j] = p->point->u * z[j] * 255;
     x[j] = p->point->sx;
     y[j] = p->point->sy;
     p = p->next;
     }

 XY DU DZ return 1;
}

static int raster_FlatInit(raster_POLYGON *p)
{
 float x[3],y[3],z[3],c; int j;

 for (j=0;j<3;j++) {
     z[j] = -1 / p->point->tz;
     x[j] = p->point->sx;
     y[j] = p->point->sy;
     p = p->next;
     }

 XY DZ return 1;
}

//************************ Span rasterization ***************************

static inline void raster_ZGouraudSpan(float u, float z, int len,
                                       int *out, float *zbuf)
{
 int u1,u2,du; float zi,z1; int *s;

 z1=FIX/z; zi=z; u1=ROUND(u*z1);

 while (len>=STEP) { u+=dudx16; z+=dzdx16;
       z1=FIX/z; u2=ROUND(u*z1); du=(u2-u1)/STEP; s=out+STEP;
       while (out<s) {
             if (zi<*zbuf) { *out=raster_Light[INT(u1)]; *zbuf=zi; }
             u1+=du; zi+=dzdx; out++; zbuf++;
             }
       len-=STEP; u1=u2;
       }

 if (len>0) { u+=dudx*len; z+=dzdx*len; z1=FIX/z;
    u2=ROUND(u*z1);du=(u2-u1)/len; s=out+len;
    while (out<s) {
          if (zi<*zbuf) { *out=raster_Light[INT(u1)]; *zbuf=zi; }
          u1+=du; zi+=dzdx; out++; zbuf++;
          }
    }
}

//********************** Polygon rasterization ************************

extern int raster_ZGouraud(raster_POLYGON *p, unsigned *col)
{
 int start_y,i,l_x,r_x,l_dxdy,r_dxdy,l_num,r_num,l,c,s;
 float u,z; int *out; float *zbuf; raster_Light=(int *)col;

 out=raster_Output;
 zbuf=raster_Zbuffer;
 raster_WIDTH=raster_XRES;

 if (!(p=raster_Visibility(p))) return 0;
 if (!raster_GouraudInit(p)) return 0;
 if (!raster_CreateEdges(p)) return 0;

 INIT

 out+=start_y*raster_WIDTH;
 zbuf+=start_y*raster_WIDTH;

 u = orgu + start_y*dudy;
 z = orgz + start_y*dzdy;

 while (i>0) {
       SPAN raster_ZGouraudSpan(u+l*dudx,z+l*dzdx,c,out+l,zbuf+l);
       u+=dudy; z+=dzdy; out+=raster_WIDTH; zbuf+=raster_WIDTH; NEXT
       }

 return 1;
}

extern int raster_ZFlat(raster_POLYGON *p, unsigned *col, float brightness)
{
 int start_y,i,l_x,r_x,l_dxdy,r_dxdy,l_num,r_num,l,c,s;
 float z; int *out=raster_Output; float *zbuf=raster_Zbuffer;
 int cl=col[ROUND(brightness*255)]; raster_WIDTH=raster_XRES;

 if (!(p=raster_Visibility(p))) return 0;
 if (!raster_FlatInit(p)) return 0;
 if (!raster_CreateEdges(p)) return 0;

 INIT

 out+=start_y*raster_WIDTH;
 zbuf+=start_y*raster_WIDTH;

 z = orgz + start_y*dzdy;

 while (i>0) {
       SPAN { int *o=out+l,*s=out+l+c; float *zb=zbuf+l,zi=z+dzdx*l;
              while (o<s) { if (zi<*zb) { *o=cl; *zb=zi; }
                    zi+=dzdx; o++; zb++;
                    }
            }
       z+=dzdy; out+=raster_WIDTH; zbuf+=raster_WIDTH; NEXT
       }

 return 1;
}
