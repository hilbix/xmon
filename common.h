/*
 * File: common.c
 *
 * Description: Standard header file
 *
 */

#ifndef COMMON_H
#define COMMON_H

#include    <ctype.h>
#include    <sys/types.h>
#include    <string.h>
#include    <stdio.h>
#include    <unistd.h>            /* for close, etc */
#include    <stdlib.h>            /* for atoi, etc */

#ifndef Bool
typedef	    short		Bool;
#endif
typedef	    caddr_t		Pointer;
typedef	    void		(*VoidCallback)();  /* ptr to void function */
typedef	    int			(*IntCallback)();   /* ptr to int function */

#define	    Global		/* Global */
#ifndef True
#define	    True		(1)
#define	    False		(0)
#endif
#define	    Streq(a,b)		(strcmp(a,b) == 0)
#define	    Strneq(a,b,n)	(strncmp(a,b,n) == 0)
#define	    abs(x)		(((x) < 0) ? (-(x)) : (x))
#define	    max(n1, n2)		(((n1) < (n2)) ? (n2) :	 (n1))
#define	    min(n1, n2)		(((n1) > (n2)) ? (n2) :	 (n1))
#define	    Tmalloc(type)	(type *)malloc(sizeof(type))
#define	    Tcalloc(n, type)	(type *)calloc(n, sizeof(type))
#define	    Tfree(ptr)		free((char *)(ptr))

#if	( __STDC__ == 1 )
#define	    P(args)		args
#else
#define	    P(args)		()
#endif

#if defined(SVR4)
# define bzero(a,b)   memset(a,0,b)
# define bcopy(a,b,c) memmove(b,a,c)
# define index(a,b)   strchr(a,b)
#endif        /* SVR4 */

#endif	/* COMMON_H */
