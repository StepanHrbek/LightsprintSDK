/*
 * This is part of mgflib, http://radsite.lbl.gov/mgf/HOME.html
 * Used by Lightsprint SDK samples for testing only.
 *
 * Copyright (c) 1994 Regents of the University of California
 */

/* SCCSid "@(#)lookup.h 1.9 9/4/98 LBL" */

/*
 * Header file for general associative table lookup routines
 */

typedef struct {
	char	*key;		/* key name */
	long	hval;		/* key hash value (for efficiency) */
	char	*data;		/* pointer to client data */
} LUENT;

#ifdef NOPROTO
typedef struct {
	unsigned long	(*hashf)();	/* key hash function */
	int	(*keycmp)();	/* key comparison function */
	void	(*freek)();	/* free a key */
	void	(*freed)();	/* free the data */
	int	tsiz;		/* current table size */
	LUENT	*tabl;		/* table, if allocated */
	int	ndel;		/* number of deleted entries */
} LUTAB;
#else
typedef struct {
	unsigned long	(*hashf)(char *);	/* key hash function */
	int	(*keycmp)(const char *, const char *);	/* key comparison function */
	void	(*freek)(char *);	/* free a key */
	void	(*freed)(char *);	/* free the data */
	int	tsiz;		/* current table size */
	LUENT	*tabl;		/* table, if allocated */
	int	ndel;		/* number of deleted entries */
} LUTAB;
#endif

#define LU_SINIT(fk,fd)		{lu_shash,strcmp,(void (*)())(fk),  				(void (*)())(fd),0,NULL,0}

/*
 * The lu_init routine is called to initialize a table.  The number of
 * elements passed is not a limiting factor, as a table can grow to
 * any size permitted by memory.  However, access will be more efficient
 * if this number strikes a reasonable balance between default memory use
 * and the expected (minimum) table size.  The value returned is the
 * actual allocated table size (or zero if there was insufficient memory).
 *
 * The hashf, keycmp, freek and freed member functions must be assigned
 * separately.  If the hash value is sufficient to guarantee equality
 * between keys, then the keycmp pointer may be NULL.  Otherwise, it
 * should return 0 if the two passed keys match.  If it is not necessary
 * (or possible) to free the key and/or data values, then the freek and/or
 * freed member functions may be NULL.
 *
 * It isn't fully necessary to call lu_init to initialize the LUTAB structure.
 * If tsiz is 0, then the first call to lu_find will allocate a minimal table.
 * The LU_SINIT macro provides a convenient static declaration for character
 * string keys.
 *
 * The lu_find routine returns the entry corresponding to the given
 * key.  If the entry does not exist, the corresponding key field will
 * be NULL.  If the entry has been previously deleted but not yet freed,
 * then only the data field will be NULL.  It is the caller's
 * responsibility to (allocate and) assign the key and data fields when
 * creating a new entry.  The only case where lu_find returns NULL is when
 * the system has run out of memory.
 *
 * The lu_delete routine frees an entry's data (if any) by calling
 * the freed member function, but does not free the key field.  This
 * will be freed later during (or instead of) table reallocation.
 * It is therefore an error to reuse or do anything with the key
 * field after calling lu_delete.
 *
 * The lu_done routine calls the given free function once for each
 * assigned table entry (i.e. each entry with an assigned key value).
 * The user must define these routines to free the key and the data
 * in the LU_TAB structure.  The final action of lu_done is to free the
 * allocated table itself.
 */

#ifdef NOPROTO
extern int	lu_init();
extern LUENT	*lu_find();
extern void	lu_delete();
extern void	lu_done();
extern unsigned long	lu_shash();
#else
extern int	lu_init(LUTAB *, int);
extern LUENT	*lu_find(LUTAB *, char *);
extern void	lu_delete(LUTAB *, char *);
extern void	lu_done(LUTAB *);
extern unsigned long	lu_shash(char *);
#endif
