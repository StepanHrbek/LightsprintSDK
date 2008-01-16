/*
 * This is part of mgflib, http://radsite.lbl.gov/mgf/HOME.html
 * Used by Lightsprint SDK samples for testing only.
 *
 * Copyright (c) 1994 Regents of the University of California
 */

#ifndef lint
static char SCCSid[] = "@(#)fvect.c 1.3 3/18/97 LBL";
#endif

/*
 * Routines for 3-d vectors
 */

#include <stdio.h>
#include <math.h>
#include "mgfparser.h"


double
normalize(v)			/* normalize a vector, return old magnitude */
register FVECT  v;
{
	static double  len;
	
	len = DOT(v, v);
	
	if (len <= 0.0)
		return(0.0);
	
	if (len <= 1.0+FTINY && len >= 1.0-FTINY)
		len = 0.5 + 0.5*len;	/* first order approximation */
	else
		len = sqrt(len);

	v[0] /= len;
	v[1] /= len;
	v[2] /= len;

	return(len);
}


void
fcross(vres, v1, v2)		/* vres = v1 X v2 */
register FVECT  vres, v1, v2;
{
	vres[0] = v1[1]*v2[2] - v1[2]*v2[1];
	vres[1] = v1[2]*v2[0] - v1[0]*v2[2];
	vres[2] = v1[0]*v2[1] - v1[1]*v2[0];
}
