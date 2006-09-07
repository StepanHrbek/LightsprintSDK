#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "swraster.h"

#define STEP     16
#define EDGES    10
#define FIX      65536.0
#define INF      16384
#define MAXMEM   1024
#define ROUND(A) (int)((float)(A))
#define INT(A)   ((A)>>16)
#define INIT     start_y = red->y; i = red_num; l_x = led->x; l_dxdy = led->dxdy; l_num = led->num; r_x = red->x; r_dxdy = red->dxdy; r_num = red->num;
#define SPAN     l=INT(l_x);c=INT(r_x); if (l>c) {s=l;l=c;c=s;} c-=l;l++; if (c>0)
#define NEXT     if (--l_num==0) { led++; l_x = led->x; l_dxdy = led->dxdy; l_num = led->num; } else l_x += l_dxdy; if (--r_num==0) { i--; red++; r_x = red->x; r_dxdy = red->dxdy; r_num = red->num; } else r_x += r_dxdy;
#define XY       x[1] = x[1] - x[0]; y[1] = y[0] - y[1]; x[2] = x[0] - x[2]; y[2] = y[2] - y[0]; c = x[1]*y[2] - y[1]*x[2]; if (c==0) return 0; x[1]/=c; y[1]/=c; x[2]/=c; y[2]/=c;
#define DU       u[1]-=u[0]; u[2]-=u[0]; dudx = u[1]*y[2] + u[2]*y[1]; dudy = u[1]*x[2] + u[2]*x[1]; orgu = u[0] - x[0]*dudx - y[0]*dudy; dudx16 = dudx*16;

struct raster_EDGE
{
	int x,y,num,dxdy; 
};

static int led_num,red_num;
static raster_EDGE led_buf[EDGES],red_buf[EDGES],*led,*red;
static float dudx, dudy, orgu, dudx16;


//********************** Scan-line conversion *************************

static int raster_AddEdge(raster_VERTEX *c, raster_VERTEX *n, raster_EDGE *ed)
{
	float x1,y1,x2,y2,dxdy,first_y,last_y; int num;

	x1 = c->sx;
	y1 = c->sy;
	x2 = n->sx;
	y2 = n->sy;

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
	raster_POLYGON *min=NULL,*max=NULL,*c,*n;

	led_num = 0;
	red_num = 0;

	c = p;
	while (c) 
	{
		y1 = c->point->sy;
		if (y1 < min_y) { min_y = y1; min = c; }
		if (y1 > max_y) { max_y = y1; max = c; }
		c=c->next;
	}

	start_y = ROUND(ceil(min_y));
	end_y = ROUND(ceil(max_y)-1);
	if (start_y>end_y) return 0;

	c = min;
	while (c!=max) 
	{
		n = c->next; if (!n) n = p;
		if (raster_AddEdge(c->point,n->point,&red_buf[red_end]))
		{ red_end++; red_num++; }
		c = n;
	}

	c = max;
	while (c!=min) 
	{
		n = c->next; if (!n) n = p;
		if (raster_AddEdge(n->point,c->point,&led_buf[led_start]))
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
	float x[3],y[3],u[3],c;

	for (int j=0;j<3;j++) 
	{
		u[j] = p->point->u * 255;
		x[j] = p->point->sx;
		y[j] = p->point->sy;
		p = p->next;
	}

	XY DU return 1;
}

static int raster_FlatInit(raster_POLYGON *p)
{
	float x[3],y[3],c;

	for (int j=0;j<3;j++) 
	{
		x[j] = p->point->sx;
		y[j] = p->point->sy;
		p = p->next;
	}

	XY return 1;
}

//************************ Span rasterization ***************************

static inline void raster_LGouraudSpan(float u, int len, raster_COLOR *out)
{
	raster_COLOR *s; int u1=ROUND(u*FIX),u2,du;

	while (len>=STEP) 
	{
		u+=dudx16;
		u2=ROUND(u*FIX); du=(u2-u1)/STEP; s=out+STEP;
		while (out<s) { *out++=INT(u1); u1+=du; }
		len-=STEP; u1=u2;
	}

	if (len>0) 
	{
		u+=dudx*len;
		u2=ROUND(u*FIX);du=(u2-u1)/len; s=out+len;
		while (out<s) { *out++=INT(u1); u1+=du; }
	}
}

//********************** Polygon rasterization ************************

extern int raster_LGouraud(raster_POLYGON *p, raster_COLOR *lightmap, int width)
{
	int start_y,i,l_x,r_x,l_dxdy,r_dxdy,l_num,r_num,l,c,s;
	float u; raster_COLOR *out=lightmap;

	if (!raster_GouraudInit(p)) return 0;
	if (!raster_CreateEdges(p)) return 0;

	INIT out+=start_y*width;

	u = orgu + start_y*dudy;

	while (i>0) 
	{
		SPAN raster_LGouraudSpan(u+l*dudx,c,out+l);
		u+=dudy; out+=width; NEXT
	}

	return 1;
}

extern int raster_LFlat(raster_POLYGON *p, raster_COLOR *lightmap, int width, raster_COLOR color)
{
	int start_y,i,l_x,r_x,l_dxdy,r_dxdy,l_num,r_num,l,c,s;
	raster_COLOR *out=lightmap;

	if (!raster_CreateEdges(p)) return 0; INIT out+=start_y*width;

	while (i>0) { SPAN memset(out+l,color,c); out+=width; NEXT }

	return 1;
}
