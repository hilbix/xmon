/*
 * Project: XMON - An X protocol monitor
 *
 * File: fd.c
 *
 * Description: Support routines for file descriptors
 *
 * All of this code is to support the handling of file descriptors.
 * The idea is to keep a table of the FDs that are in use and why.  For
 * each FD that is open for input, we keep the name of a procedure to call
 * if input arrives for that FD.  When an FD is created (by an open, pipe,
 * socket, ...) declare that by calling UsingFD.  When it is no longer in
 * use (close ...), call NotUsingFD.
 */

#include <errno.h>	       /* for EINTR, EADDRINUSE, ... */

#if defined(SVR4) || defined(hpux)
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "xmond.h"
#include "select_args.h"

/* function prototypes: */
/* fd.c: */

/* end function prototypes */

extern FDDescriptor		*FDD;
extern long			ReadDescriptors[];
extern long			WriteDescriptors[];
extern short			HighestFD;

Global void
InitializeFD()
{
    register short  i;
    short   MaxFD /* maximum number of FD's possible */ ;
#if defined(SVR4) || defined(hpux)
    struct rlimit rlp;
#endif

    enterprocedure("InitializeFD");
    /* get the number of file descriptors the system will let us use */
#if defined(SVR4) || defined(hpux)
    if (getrlimit(RLIMIT_NOFILE, &rlp) < 0)
    {
	perror("getrlimit(RLIMIT_NOFILE)");
	exit(1);
    }
    MaxFD = rlp.rlim_cur;
#else
    MaxFD = getdtablesize();
#endif

    /* allocate space for a File Descriptor Table */
    FDD = (FDDescriptor *)malloc((long)(MaxFD * sizeof(FDDescriptor)));

    for (i = 0; i < MaxFD; i++)
	FDD[i].InputHandler = (VoidCallback)NULL;

    CLEARBITS(ReadDescriptors);
    CLEARBITS(WriteDescriptors);
    HighestFD = 0;
}

Global FDDescriptor*
UsingFD(fd, Handler, private_data)
    int			    fd;
    VoidCallback	    Handler;
    Pointer		    private_data;
{
    FDD[fd].InputHandler = Handler;
    FDD[fd].private_data = private_data;
    if (Handler == NULL)
	BITCLEAR(ReadDescriptors, fd);
    else
	BITSET(ReadDescriptors, fd);

    if (fd > HighestFD)
	HighestFD = fd;
    return &FDD[fd];
}

Global void
NotUsingFD(fd)
	int fd;
{
    close(fd);
    FDD[fd].InputHandler = (VoidCallback)NULL;
    BITCLEAR(ReadDescriptors, fd);

    while (FDD[HighestFD].InputHandler == (VoidCallback)NULL && HighestFD > 0)
	HighestFD -= 1;
}
