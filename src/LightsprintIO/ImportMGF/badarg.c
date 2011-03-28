/*
 * This is part of mgflib, http://radsite.lbl.gov/mgf/HOME.html
 * Used by Lightsprint SDK samples for testing only.
 *
 * Copyright (c) 1994 Regents of the University of California
 */

/* this hack was necessary for some platform(which one?) but it broke osx
int isascii(int c)
{
	return 1;
}*/

#ifndef lint
static char SCCSid[] = "@(#)badarg.c 1.1 6/21/94 LBL";
#endif

/*
 * Check argument list against format string.
 */

#include <ctype.h>
#include "mgfparser.h"

int
badarg(ac, av, fl)		/* check argument list */
int	ac;
register char	**av;
register char	*fl;
{
	register int  i;

	if (fl == NULL)
		fl = "";		/* no arguments? */
	for (i = 1; *fl; i++,av++,fl++) {
		if (i > ac || *av == NULL)
			return(-1);
		switch (*fl) {
		case 's':		/* string */
			if (**av == '\0' || isspace(**av))
				return(i);
			break;
		case 'i':		/* integer */
			if (!isintd(*av, " \t\r\n"))
				return(i);
			break;
		case 'f':		/* float */
			if (!isfltd(*av, " \t\r\n"))
				return(i);
			break;
		default:		/* bad call! */
			return(-1);
		}
	}
	return(0);		/* all's well */
}
