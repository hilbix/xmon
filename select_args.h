/*
 * Project: XMON - An X protocol monitor
 *
 * File: select_args.h
 *
 * Description: macros to handle the arguments to select().
 */

#ifndef SELECT_H
#define SELECT_H

#include <sys/param.h>

#define MAXSOCKS (NOFILE - 1)
#define mskcnt ((MAXSOCKS + 31) / 32)	/* size of bit array */

#if (mskcnt==1)
#define BITMASK(i) (1 << (i))
#define MASKIDX(i) 0
#endif
#if (mskcnt>1)
#define BITMASK(i) (1 << ((i) & 31))
#define MASKIDX(i) ((i) >> 5)
#endif

#define MASKWORD(buf, i) buf[MASKIDX(i)]
#define BITSET(buf, i) MASKWORD(buf, i) |= BITMASK(i)
#define BITCLEAR(buf, i) MASKWORD(buf, i) &= ~BITMASK(i)
#define GETBIT(buf, i) (MASKWORD(buf, i) & BITMASK(i))

#if (mskcnt==1)
#define COPYBITS(src, dst) dst[0] = src[0]
#define CLEARBITS(buf) buf[0] = 0
#define MASKANDSETBITS(dst, b1, b2) dst[0] = (b1[0] & b2[0])
#define ORBITS(dst, b1, b2) dst[0] = (b1[0] | b2[0])
#define UNSETBITS(dst, b1) (dst[0] &= ~b1[0])
#define ANYSET(src) (src[0])
#endif
#if (mskcnt==2)
#define COPYBITS(src, dst) dst[0] = src[0]; dst[1] = src[1]
#define CLEARBITS(buf) buf[0] = 0; buf[1] = 0
#define MASKANDSETBITS(dst, b1, b2)  \
	      dst[0] = (b1[0] & b2[0]);\
	      dst[1] = (b1[1] & b2[1])
#define ORBITS(dst, b1, b2)  \
	      dst[0] = (b1[0] | b2[0]);\
	      dst[1] = (b1[1] | b2[1])
#define UNSETBITS(dst, b1) \
		      dst[0] &= ~b1[0]; \
		      dst[1] &= ~b1[1]
#define ANYSET(src) (src[0] || src[1])
#endif
#if (mskcnt==3)
#define COPYBITS(src, dst) dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];
#define CLEARBITS(buf) buf[0] = 0; buf[1] = 0; buf[2] = 0
#define MASKANDSETBITS(dst, b1, b2)  \
	      dst[0] = (b1[0] & b2[0]);\
	      dst[1] = (b1[1] & b2[1]);\
	      dst[2] = (b1[2] & b2[2])
#define ORBITS(dst, b1, b2)  \
	      dst[0] = (b1[0] | b2[0]);\
	      dst[1] = (b1[1] | b2[1]);\
	      dst[2] = (b1[2] | b2[2])
#define UNSETBITS(dst, b1) \
		      dst[0] &= ~b1[0]; \
		      dst[1] &= ~b1[1]; \
		      dst[2] &= ~b1[2]
#define ANYSET(src) (src[0] || src[1] || src[2])
#endif
#if (mskcnt==4)
#define COPYBITS(src, dst) dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2];\
	      dst[3] = src[3]
#define CLEARBITS(buf) buf[0] = 0; buf[1] = 0; buf[2] = 0; buf[3] = 0
#define MASKANDSETBITS(dst, b1, b2)  \
		      dst[0] = (b1[0] & b2[0]);\
		      dst[1] = (b1[1] & b2[1]);\
		      dst[2] = (b1[2] & b2[2]);\
		      dst[3] = (b1[3] & b2[3])
#define ORBITS(dst, b1, b2)  \
		      dst[0] = (b1[0] | b2[0]);\
		      dst[1] = (b1[1] | b2[1]);\
		      dst[2] = (b1[2] | b2[2]);\
		      dst[3] = (b1[3] | b2[3])
#define UNSETBITS(dst, b1) \
		      dst[0] &= ~b1[0]; \
		      dst[1] &= ~b1[1]; \
		      dst[2] &= ~b1[2]; \
		      dst[3] &= ~b1[3]
#define ANYSET(src) (src[0] || src[1] || src[2] || src[3])
#endif

#if (mskcnt>4)
#define COPYBITS(src, dst) bcopy((caddr_t) src, (caddr_t) dst,\
		 mskcnt*sizeof(long))
#define CLEARBITS(buf) bzero((caddr_t) buf, mskcnt*sizeof(long))
#define MASKANDSETBITS(dst, b1, b2)  \
	      { int cri;	    \
	    for (cri=0; cri<mskcnt; cri++)  \
		  dst[cri] = (b1[cri] & b2[cri]); }
#define ORBITS(dst, b1, b2)  \
	      { int cri;	    \
	      for (cri=0; cri<mskcnt; cri++)	\
		  dst[cri] = (b1[cri] | b2[cri]); }
#define UNSETBITS(dst, b1) \
	      { int cri;	    \
	      for (cri=0; cri<mskcnt; cri++)	\
		  dst[cri] &= ~b1[cri];	 }
/* ANYSET is a function defined in main.c */
#endif

#endif /* SELECT_H */
