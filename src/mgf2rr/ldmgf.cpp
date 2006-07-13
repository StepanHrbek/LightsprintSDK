#include <assert.h>
#include <stdio.h>
#include "ldmgf.h"

bool shared_vertices=true;

void *(*_add_vertex)(FLOAT *p,FLOAT *n);
void *(*_add_surface)(C_MATERIAL *material);
void  (*_add_polygon)(unsigned vertices,void **vertex,void *surface);
void  (*_begin_object)(char *name);
void  (*_end_object)(void);

#define xf_xid (xf_context?xf_context->xid:0)

int my_hface(int ac,char **av)
{
        if(ac<4) return(MG_EARGC);
        void **vertex=new void *[ac-1];
        for (int i = 1; i < ac; i++) {
                C_VERTEX *vp=c_getvert(av[i]);
                if(!vp) return(MG_EUNDEF);
                if(!shared_vertices || (vp->clock!=-1-111*xf_xid && _add_vertex))
                {
                        FVECT vert;
			FVECT norm;
                        xf_xfmpoint(vert, vp->p);
                        xf_xfmvect(norm, vp->n);
                        normalize(norm);
                        vp->client_data=(char *)_add_vertex(vert,norm);
//printf("  ~vp=%i, xf_context=%i, xid=%i\n",(int)vp,(int)xf_context,(int)xf_xid);
                        vp->clock=-1-111*xf_xid;
                }
                vertex[i-1]=vp->client_data;
        }
        assert(c_cmaterial);
//static C_MATERIAL *oldmat=NULL;
//if(c_cmaterial!=oldmat) { oldmat=c_cmaterial; printf("zmena c_cmaterial! (c_cmaterial=%i, clock=%i)\n",(int)c_cmaterial,(int)c_cmaterial->clock);}
        if(c_cmaterial->clock!=-1)
        {
                c_cmaterial->clock=-1;
                c_cmaterial->client_data=_add_surface?(char *)_add_surface(c_cmaterial):NULL;
        }
        if(_add_polygon) _add_polygon(ac-1,vertex,c_cmaterial->client_data);
        delete[] vertex;
        return(MG_OK);
}

int my_hobject(int ac,char **av)
{
        if(ac==2) if(_begin_object) _begin_object(av[1]);
        if(ac==1) if(_end_object) _end_object();
	return obj_handler(ac,av);
}

int ldmgf(char *filename,
          void *add_vertex(FLOAT *p,FLOAT *n),
          void *add_surface(C_MATERIAL *material),
          void  add_polygon(unsigned vertices,void **vertex,void *surface),
	  void  begin_object(char *name),
	  void  end_object(void))
{
        static bool loaded=false;
        if(loaded) mg_clear();
        loaded=true;
	mg_ehand[MG_E_COLOR]    = c_hcolor;	/* they get color */
	mg_ehand[MG_E_CMIX]     = c_hcolor;	/* they mix colors */
	mg_ehand[MG_E_CSPEC]    = c_hcolor;	/* they get spectra */
	mg_ehand[MG_E_CXY]      = c_hcolor;	/* they get chromaticities */
	mg_ehand[MG_E_CCT]      = c_hcolor;	/* they get color temp's */
	mg_ehand[MG_E_ED]       = c_hmaterial;	/* they get emission */
	mg_ehand[MG_E_FACE]     = my_hface;	/* we do faces */
	mg_ehand[MG_E_MATERIAL] = c_hmaterial;	/* they get materials */
	mg_ehand[MG_E_NORMAL]   = c_hvertex;	/* they get normals */
	mg_ehand[MG_E_OBJECT]   = my_hobject;	/* we track object names */
	mg_ehand[MG_E_POINT]    = c_hvertex;	/* they get points */
	mg_ehand[MG_E_RD]       = c_hmaterial;	/* they get diffuse refl. */
	mg_ehand[MG_E_RS]       = c_hmaterial;	/* they get specular refl. */
	mg_ehand[MG_E_SIDES]    = c_hmaterial;	/* they get # sides */
	mg_ehand[MG_E_TD]       = c_hmaterial;	/* they get diffuse trans. */
	mg_ehand[MG_E_TS]       = c_hmaterial;	/* they get specular trans. */
	mg_ehand[MG_E_VERTEX]   = c_hvertex;	/* they get vertices */
	mg_ehand[MG_E_POINT]    = c_hvertex;	
	mg_ehand[MG_E_NORMAL]   = c_hvertex;	
	mg_ehand[MG_E_XF]       = xf_handler;	/* they track transforms */
        mg_init();
        _add_vertex=add_vertex;
        _add_surface=add_surface;
        _add_polygon=add_polygon;
	_begin_object=begin_object;
	_end_object=end_object;
        int result=mg_load(filename);
        return result;
}


			/* Change the following to suit your standard */
#define  CIE_x_r		0.640		/* nominal CRT primaries */
#define  CIE_y_r		0.330
#define  CIE_x_g		0.290
#define  CIE_y_g		0.600
#define  CIE_x_b		0.150
#define  CIE_y_b		0.060
#define  CIE_x_w		0.3333		/* use true white */
#define  CIE_y_w		0.3333

#define CIE_C_rD	( (1./CIE_y_w) *    				( CIE_x_w*(CIE_y_g - CIE_y_b) -    				  CIE_y_w*(CIE_x_g - CIE_x_b) +    				  CIE_x_g*CIE_y_b - CIE_x_b*CIE_y_g	) )
#define CIE_C_gD	( (1./CIE_y_w) *    				( CIE_x_w*(CIE_y_b - CIE_y_r) -    				  CIE_y_w*(CIE_x_b - CIE_x_r) -    				  CIE_x_r*CIE_y_b + CIE_x_b*CIE_y_r	) )
#define CIE_C_bD	( (1./CIE_y_w) *    				( CIE_x_w*(CIE_y_r - CIE_y_g) -    				  CIE_y_w*(CIE_x_r - CIE_x_g) +    				  CIE_x_r*CIE_y_g - CIE_x_g*CIE_y_r	) )


float	xyz2rgbmat[3][3] = {		/* XYZ to RGB conversion matrix */
	{(CIE_y_g - CIE_y_b - CIE_x_b*CIE_y_g + CIE_y_b*CIE_x_g)/CIE_C_rD,
	 (CIE_x_b - CIE_x_g - CIE_x_b*CIE_y_g + CIE_x_g*CIE_y_b)/CIE_C_rD,
	 (CIE_x_g*CIE_y_b - CIE_x_b*CIE_y_g)/CIE_C_rD},
	{(CIE_y_b - CIE_y_r - CIE_y_b*CIE_x_r + CIE_y_r*CIE_x_b)/CIE_C_gD,
	 (CIE_x_r - CIE_x_b - CIE_x_r*CIE_y_b + CIE_x_b*CIE_y_r)/CIE_C_gD,
	 (CIE_x_b*CIE_y_r - CIE_x_r*CIE_y_b)/CIE_C_gD},
	{(CIE_y_r - CIE_y_g - CIE_y_r*CIE_x_g + CIE_y_g*CIE_x_r)/CIE_C_bD,
	 (CIE_x_g - CIE_x_r - CIE_x_g*CIE_y_r + CIE_x_r*CIE_y_g)/CIE_C_bD,
	 (CIE_x_r*CIE_y_g - CIE_x_g*CIE_y_r)/CIE_C_bD}
};


void xy2rgb(double cx,double cy,double intensity,float cout[3])
/* convert MGF color to RGB */
/* input MGF chrominance */
/* input luminance or reflectance */
/* output RGB color */
{
	static double	cie[3];
					/* get CIE XYZ representation */
	cie[0] = intensity*cx/cy;
	cie[1] = intensity;
	cie[2] = intensity*(1./cy - 1.) - cie[0];
					/* convert to RGB */
	cout[0] = (float)( xyz2rgbmat[0][0]*cie[0] + xyz2rgbmat[0][1]*cie[1]
			+ xyz2rgbmat[0][2]*cie[2] );
	if(cout[0] < 0.) cout[0] = 0.;
	cout[1] = (float)( xyz2rgbmat[1][0]*cie[0] + xyz2rgbmat[1][1]*cie[1]
			+ xyz2rgbmat[1][2]*cie[2] );
	if(cout[1] < 0.) cout[1] = 0.;
	cout[2] = (float)( xyz2rgbmat[2][0]*cie[0] + xyz2rgbmat[2][1]*cie[1]
			+ xyz2rgbmat[2][2]*cie[2] );
	if(cout[2] < 0.) cout[2] = 0.;
}

void mgf2rgb(C_COLOR *cin,double intensity,float cout[3])
{
	c_ccvt(cin, C_CSXY);
        xy2rgb(cin->cx,cin->cy,intensity,cout);
}
