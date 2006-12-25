/* Copyright (c) 1995 Regents of the University of California */

#ifndef lint
static char SCCSid[] = "@(#)mgfilt.c 1.5 11/22/95 LBL";
#endif

/*
 * Filter MGF stream, removing entities that won't be understood
 */

#include <stdio.h>
#include "mgfparser.h"

				/* Number of entities for major versions */
short	nentlist[MG_VMAJOR] = MG_NELIST;


int
put_entity(ac, av)		/* general output routine */
register int	ac;
register char	**av;
{
	while (ac-- > 0) {
		fputs(*av++, stdout);
		putchar(ac ? ' ' : '\n');
	}
	return(MG_OK);
}


main(argc, argv)	/* first argument is understood entities, comma-sep. */
int	argc;
char	*argv[];
{
	char	*cp1, *cp2;
	int	i, en;

	if (argc < 2) {
		fprintf(stderr,
			"Usage: %s { version | entity,list } [file ..]\n",
				argv[0]);
		exit(1);
	}
	if (isint(argv[1])) {
		i = atoi(argv[1]);
		if (i < 1 | i > MG_VMAJOR) {
			fprintf(stderr, "%s: bad version number: %d\n",
					argv[0], i);
			exit(1);
		}
		for (en = nentlist[i-1]; en--; )
			mg_ehand[en] = put_entity;
		mg_ehand[MG_E_INCLUDE] = NULL;		/* expand include's */
	} else
		for (cp1 = cp2 = argv[1]; *cp1; cp1 = cp2) {
			while (*cp2) {
				if (*cp2 == ',') {
					*cp2++ = '\0';
					break;
				}
				cp2++;
			}
			en = mg_entity(cp1);
			if (en < 0) {
				fprintf(stderr, "%s: %s: no such entity\n",
						argv[0], cp1);
				exit(1);
			}
			mg_ehand[en] = put_entity;
		}
	mg_init();
	if (argc < 3) {
		if (mg_load((char *)NULL) != MG_OK)
			exit(1);
		if (mg_nunknown)
			printf("%s %s: %u unknown entities on input\n",
					mg_ename[MG_E_COMMENT],
					argv[0], mg_nunknown);
		exit(0);
	}
	for (i = 2; i < argc; i++) {
		if (mg_load(argv[i]) != MG_OK)
			exit(1);
		if (mg_nunknown) {
			printf("%s %s: %u unknown entities\n",
					mg_ename[MG_E_COMMENT],
					argv[i], mg_nunknown);
			mg_nunknown = 0;
		}
	}
	exit(0);
}
