#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "spline.h"

#ifdef _MSC_VER
#pragma warning(disable:4244) // there are lots of float-double conversions
#endif

#define EPSILON 1.0E-06
#define M_PI		3.14159265358979323846
#define M_PI_2		1.57079632679489661923

static KEY *keys;
static int MAX;
static int LAST;
static int LOOP=0;
static int REPEAT=0;

//**************************** Quaternions **********************************

static inline void qcopy(QUAT *s, QUAT *d)
{ d->w=s->w; d->x=s->x; d->y=s->y; d->z=s->z; }

// Scale quaternion by value.
static inline void qscale(QUAT q, float s, QUAT *dest)
{ dest->w=q.w*s; dest->x=q.x*s; dest->y=q.y*s; dest->z=q.z*s; }

static inline float qmod(QUAT q) // Returns modul of quaternion
{ float d=sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w); return d!=0 ? d : 1; }

static inline void qunit(QUAT q, QUAT *dest) // Normilize quaternion.
{ float s=1.0/qmod(q); qscale(q,s,dest); }

static inline float qdot(QUAT q1, QUAT q2)  // Returns dot product of q1*q2
{ return (q1.x*q2.x+q1.y*q2.y+q1.z*q2.z+q1.w*q2.w)/(qmod(q1)*qmod(q2)); }

// Returns dot product of normilized quternions q1*q2
static inline float qdotunit(QUAT q1, QUAT q2)
{ return q1.x*q2.x+q1.y*q2.y+q1.z*q2.z+q1.w*q2.w; }

// Calculates quaternion product dest = q1 * q2.
static inline void qmul(QUAT q1, QUAT q2, QUAT *dest)
{
 dest->w=q1.w*q2.w-q1.x*q2.x-q1.y*q2.y-q1.z*q2.z;
 dest->x=q1.w*q2.x+q1.x*q2.w+q1.y*q2.z-q1.z*q2.y;
 dest->y=q1.w*q2.y+q1.y*q2.w+q1.z*q2.x-q1.x*q2.z;
 dest->z=q1.w*q2.z+q1.z*q2.w+q1.x*q2.y-q1.y*q2.x;
}

// Multiplicative inverse of q.
static inline void qinv(QUAT q, QUAT *dest)
{
 float d=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w; d = d!=0 ? 1.0/d : 1;
 dest->w=q.w*d; dest->x=-q.x*d; dest->y=-q.y*d; dest->z=-q.z*d;
}

static void qnegate(QUAT *q) // Negates q;
{ float d=1.0/qmod(*q); q->w*=d; q->x*=-d; q->y*=-d; q->z*=-d; }

// Calculate quaternion`s exponent.
static inline void qexp(QUAT q, QUAT *dest)
{
 float d1,d=sqrt(q.x*q.x+q.y*q.y+q.z*q.z); d1 = d>0 ? sin(d)/d : 1;
 dest->w=cos(d); dest->x=q.x*d1; dest->y=q.y*d1; dest->z=q.z*d1;
}

// Calculate quaternion`s logarithm.
static inline void qlog(QUAT q, QUAT *dest)
{
 float d=sqrt(q.x*q.x+q.y*q.y+q.z*q.z); d = q.w!=0 ? atan(d/q.w) : M_PI_2;
 dest->w=0; dest->x=q.x*d; dest->y=q.y*d; dest->z=q.z*d;
}

// Calculate logarithm of the relative rotation from p to q
static void qlndif(QUAT p, QUAT q, QUAT *dest)
{
 QUAT inv,dif; float d,d1,s; qinv(p,&inv); qmul(inv,q,&dif);
 d=sqrt(dif.x*dif.x+dif.y*dif.y+dif.z*dif.z);
 s=p.x*q.x+p.y*q.y+p.z*q.z+p.w*q.w;
 d1 = s!=0 ? atan(d/s) : M_PI_2; if (d!=0) d1/=d; dest->w=0;
 dest->x=dif.x*d1; dest->y=dif.y*d1; dest->z=dif.z*d1;
}

// Converts quaternion to inverse matrix.
extern void spline_Quat2invMatrix(QUAT q, spline_MATRIX *M)
{
 float d,s,xs,ys,zs,wx,wy,wz,xx,xy,xz,yy,yz,zz;

 d=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w; s = d==0 ? 1.0 : 2.0/d;

 xs=q.x*s;   ys=q.y*s;  zs=q.z*s;
 wx=q.w*xs;  wy=q.w*ys; wz=q.w*zs;
 xx=q.x*xs;  xy=q.x*ys; xz=q.x*zs;
 yy=q.y*ys;  yz=q.y*zs; zz=q.z*zs;

 M->xx=1.0-(yy+zz); M->xy=xy+wz; M->xz=xz-wy;
 M->yx=xy-wz; M->yy=1.0-(xx+zz); M->yz=yz+wx;
 M->zx=xz+wy; M->zy=yz-wx; M->zz=1.0-(xx+yy);
}

static void qslerp(QUAT a, QUAT b, QUAT *dest, float time, float spin)
{
 double k1,k2,angle,angleSpin,sin_a,cos_a=qdotunit(a,b); int flipk2;
 if (cos_a<0) { cos_a=-cos_a; flipk2=-1; } else flipk2=1;

 if ((1.0-cos_a)<EPSILON) { k1=1.0-time; k2=time; } else {
    angle=acos(cos_a); sin_a=sin(angle); angleSpin=angle+spin*M_PI;
    k1=sin(angle-time*angleSpin)/sin_a; k2=sin(time*angleSpin)/sin_a;
    } k2*=flipk2;

 dest->x=k1*a.x+k2*b.x;
 dest->y=k1*a.y+k2*b.y;
 dest->z=k1*a.z+k2*b.z;
 dest->w=k1*a.w+k2*b.w;
}

static void qslerplong(QUAT a, QUAT b, QUAT *dest, float time, float spin)
{
 double k1,k2,angle,angleSpin,sin_a,cos_a=qdotunit(a,b);

 if (1.0-fabs(cos_a)<EPSILON) { k1=1.0-time; k2=time; } else {
    angle=acos(cos_a); sin_a=sin(angle); angleSpin=angle+spin*M_PI;
    k1=sin(angle-time*angleSpin)/sin_a; k2=sin(time*angleSpin)/sin_a;
    }

 dest->x=k1*a.x+k2*b.x;
 dest->y=k1*a.y+k2*b.y;
 dest->z=k1*a.z+k2*b.z;
 dest->w=k1*a.w+k2*b.w;
}

//**************************** Splines **********************************

static float Ease(float t, float a, float b)
{
 float k,s=a+b; if (s==0) return t;
 if (s>1.0) { a=a/s; b=b/s; b=b/s; } k=1.0/(2.0-a-b);
 if (t<a) return ((k/a)*t*t); else { if (t<1.0-b) {
    return (k*(2*t-a)); } else { t=1.0-t; return (1.0-(k/b)*t*t); } }
}

static void CompDeriv(KEY *prevkey, KEY *key, KEY *nextkey)
{
 float dsA,dsB,ddA,ddB,dsAdjust,ddAdjust,v1,v2,pf,f,nf;

 pf=prevkey->frame; f= key->frame; nf=nextkey->frame;
 if (pf>f) { f += keys[LAST].frame; nf += keys[LAST].frame; }
    else if (f > nf) { nf += keys[LAST].frame; }

 dsA=(1.0-key->tens)*(1.0-key->cont)*(1.0+key->bias);
 dsB=(1.0-key->tens)*(1.0+key->cont)*(1.0-key->bias);
 dsAdjust=(f-pf)/(nf-pf);

 ddA=(1.0-key->tens)*(1.0+key->cont)*(1.0+key->bias);
 ddB=(1.0-key->tens)*(1.0-key->cont)*(1.0-key->bias);
 ddAdjust=(nf-f)/(nf-pf);

 v1=key->pos.x-prevkey->pos.x; v2=nextkey->pos.x-key->pos.x;
 key->ds.x=dsAdjust*(dsA*v1+dsB*v2); key->dd.x=ddAdjust*(ddA*v1+ddB*v2);

 v1=key->pos.y-prevkey->pos.y; v2=nextkey->pos.y-key->pos.y;
 key->ds.y=dsAdjust*(dsA*v1+dsB*v2); key->dd.y=ddAdjust*(ddA*v1+ddB*v2);

 v1=key->pos.z-prevkey->pos.z; v2=nextkey->pos.z-key->pos.z;
 key->ds.z=dsAdjust*(dsA*v1+dsB*v2); key->dd.z=ddAdjust*(ddA*v1+ddB*v2);
}

static void CompDerivFirst(KEY *key, KEY *keyn, KEY *keynn)
{
 float f20,f10,v20,v10;
 f20=keynn->frame-key->frame; f10=keyn->frame-key->frame;

 v20=keynn->pos.x-key->pos.x; v10=keyn->pos.x-key->pos.x;
 key->dd.x=(1-key->tens)*(v20*(0.25-f10/(2*f20))+(v10-v20/2)*3/2+v20/2);

 v20=keynn->pos.y-key->pos.y; v10=keyn->pos.y-key->pos.y;
 key->dd.y=(1-key->tens)*(v20*(0.25-f10/(2*f20))+(v10-v20/2)*3/2+v20/2);

 v20=keynn->pos.z-key->pos.z; v10=keyn->pos.z-key->pos.z;
 key->dd.z=(1-key->tens)*(v20*(0.25-f10/(2*f20))+(v10-v20/2)*3/2+v20/2);
}

static void CompDerivLast(KEY *keypp, KEY *keyp, KEY *key)
{
 float f20,f10,v20,v10;
 f20=key->frame-keypp->frame; f10=key->frame-keyp->frame;

 v20=key->pos.x-keypp->pos.x; v10=key->pos.x-keyp->pos.x;
 key->ds.x=(1-key->tens)*(v20*(0.25-f10/(2*f20))+(v10-v20/2)*3/2+v20/2);

 v20=key->pos.y-keypp->pos.y; v10=key->pos.y-keyp->pos.y;
 key->ds.y=(1-key->tens)*(v20*(0.25-f10/(2*f20))+(v10-v20/2)*3/2+v20/2);

 v20=key->pos.z-keypp->pos.z; v10=key->pos.z-keyp->pos.z;
 key->ds.z=(1-key->tens)*(v20*(0.25-f10/(2*f20))+(v10-v20/2)*3/2+v20/2);
}

static void CompDerivFirst2(KEY *key, KEY *keyn)
{
 float v;
 v=keyn->pos.x-key->pos.x; key->dd.x=v*(1-key->tens);
 v=keyn->pos.y-key->pos.y; key->dd.y=v*(1-key->tens);
 v=keyn->pos.z-key->pos.z; key->dd.z=v*(1-key->tens);
}

static void CompDerivLast2(KEY *keyp, KEY *key)
{
 float v;
 v=key->pos.x-keyp->pos.x; key->ds.x=v*(1-key->tens);
 v=key->pos.y-keyp->pos.y; key->ds.y=v*(1-key->tens);
 v=key->pos.z-keyp->pos.z; key->ds.z=v*(1-key->tens);
}

static int CompAB(KEY *prev, KEY *cur, KEY *next)
{
 QUAT qprev,qnext,q,qp,qm,qa,qb,qae,qbe;
 float tm,cm,cp,bm,bp,tmcm,tmcp,ksm,ksp,kdm,kdp,c,dt,fp,fn;

 if (prev) {
    if (fabs(cur->aa.w-prev->aa.w)>2*M_PI-EPSILON) {
       q=cur->aa; q.w=0; qlog(q,&qm);
       } else { qprev=prev->pos;
       if (qdotunit(qprev,cur->pos)<0) qnegate(&qprev);
       qlndif(qprev, cur->pos, &qm);
       }
    }

 if (next) {
    if (fabs(next->aa.w-cur->aa.w) > 2*M_PI-EPSILON) {
       q=next->aa; q.w=0; qlog(q, &qp);
       } else { qnext=next->pos;
       if (qdotunit(qnext,cur->pos) < 0) qnegate(&qnext);
       qlndif(cur->pos, qnext, &qp);
       }
    }

 if (!prev) qm=qp; if (!next) qp=qm; fp=fn=1.0; cm=1.0-cur->cont;

 if (prev && next) { dt=0.5*(float)(next->frame-prev->frame);
    fp=((float)(cur->frame-prev->frame))/dt;
    fn=((float)(next->frame-cur->frame))/dt;
    c=fabs(cur->cont); fp=fp+c-c*fp; fn=fn+c-c*fn;
    }

 tm=0.5*(1.0-cur->tens); cp=2.0-cm; bm=1.0-cur->bias; bp=2.0-bm;
 tmcm=tm*cm; tmcp=tm*cp; ksm =1.0-tmcm*bp*fp; ksp =-tmcp*bm*fp;
 kdm =tmcp*bp*fn; kdp =tmcm*bm*fn-1.0;

 qa.x=.5*(kdm*qm.x+kdp*qp.x); qb.x=.5*(ksm*qm.x+ksp*qp.x);
 qa.y=.5*(kdm*qm.y+kdp*qp.y); qb.y=.5*(ksm*qm.y+ksp*qp.y);
 qa.z=.5*(kdm*qm.z+kdp*qp.z); qb.z=.5*(ksm*qm.z+ksp*qp.z);
 qa.w=.5*(kdm*qm.w+kdp*qp.w); qb.w=.5*(ksm*qm.w+ksp*qp.w);

 qexp(qa, &qae); qexp(qb, &qbe);

 qmul(cur->pos,qae,&cur->ds);
 qmul(cur->pos,qbe,&cur->dd);

 return 0;
}

static void SetTrack(TRACK *track)
{
 keys=track->keys; MAX=track->num;
 LAST=MAX-1; LOOP=track->loop;
 REPEAT=track->repeat;
}

extern void spline_Init(TRACK *track, int quat)
{
 int n; QUAT q; SetTrack(track);
 if (MAX<2) return;
 if (quat) {
    if (MAX>2) {
       for (n=1;n<MAX;n++) {
           qmul(keys[n-1].pos,keys[n].pos,&q); qcopy(&q,&keys[n].pos);
           qmul(keys[n-1].aa,keys[n].aa,&q); qcopy(&q,&keys[n].aa);
           }
       for (n=1;n<LAST;n++) CompAB(&keys[n-1],&keys[n],&keys[n+1]);
       }
    CompAB(NULL,&keys[0],&keys[1]); CompAB(&keys[0],&keys[LAST],NULL);
    } else {
    if (MAX>2) {
       for (n=1;n<LAST;n++) CompDeriv(&keys[n-1],&keys[n],&keys[n+1]);
       if (LOOP) {
          CompDeriv(&keys[LAST-1],&keys[0],&keys[1]);
          CompDeriv(&keys[LAST-1],&keys[LAST],&keys[1]);
          } else {
          CompDerivFirst(&keys[0],&keys[1],&keys[2]);
          CompDerivLast(&keys[LAST-2],&keys[LAST-1],&keys[LAST]);
          }
       } else {
       CompDerivFirst2(&keys[0],&keys[1]);
       CompDerivLast2(&keys[0],&keys[1]);
       }
    }
}

extern void spline_Interpolate(TRACK *track, float time,
                               float *x, float *y, float *z,
                               spline_MATRIX *m)
{
 int n; QUAT p,q,q1; float t,t2,t3,spin,angle,frames,h[4];
 SetTrack(track);

 if (MAX<2) {
    if (m) spline_Quat2invMatrix(keys[0].pos,m);
       else { *x=keys[0].pos.x; *y=keys[0].pos.y; *z=keys[0].pos.z; }
    return;
    }

 if (LOOP || REPEAT)
    time=keys[0].frame+fmod(time,keys[LAST].frame-keys[0].frame);

 if (time<keys[0].frame) {
    *x=keys[0].pos.x; *y=keys[0].pos.y; *z=keys[0].pos.z; return;
    } else if (time > keys[LAST].frame) {
    *x=keys[LAST].pos.x; *y=keys[LAST].pos.y; *z=keys[LAST].pos.z;
    return;
    }

 n=0; while ((n<LAST)&&(keys[n+1].frame<time)) n++;

 frames=keys[n+1].frame-keys[n].frame; t=(time-keys[n].frame)/frames;
 t=Ease(t,keys[n].ef,keys[n+1].et);

 if (!m) {

    t2=t*t; t3=t2*t;

    h[0]=2*t3-3*t2+1; h[1]=-2*t3+3*t2; h[2]=t3-2*t2+t; h[3]=t3-t2;

    *x=(h[0]*keys[n].pos.x)+(h[1]*keys[n+1].pos.x)+
       (h[2]*keys[n].dd.x)+(h[3]*keys[n+1].ds.x);
    *y=(h[0]*keys[n].pos.y)+(h[1]*keys[n+1].pos.y)+
       (h[2]*keys[n].dd.y)+(h[3]*keys[n+1].ds.y);
    *z=(h[0]*keys[n].pos.z)+(h[1]*keys[n+1].pos.z)+
       (h[2]*keys[n].dd.z)+(h[3]*keys[n+1].ds.z);

    } else {

    angle=(keys[n+1].aa.w-keys[n].aa.w);

    spin=angle>0 ? floor(angle/(2*M_PI)) : ceil(angle/(2*M_PI));

    angle-=(2*M_PI)*spin;

    if (fabs(angle)>M_PI) {
       qslerplong(keys[n].pos,keys[n+1].pos,&p,t,spin);
       qslerplong(keys[n].dd,keys[n+1].ds,&q,t,spin);
       t=(((1.0-t)*2.0)*t); qslerplong(p,q,&q1,t,0);
       } else {
       qslerp(keys[n].pos,keys[n+1].pos,&p,t,spin);
       qslerp(keys[n].dd,keys[n+1].ds,&q,t,spin);
       t=(((1.0-t)*2.0)*t);
       qslerp(p,q,&q1,t,0);
       }

    spline_Quat2invMatrix(q1,m);
    }
}
