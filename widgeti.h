/*
 *  File: widgeti.h
 *
 *  Description: Prototypes and defines for widgeti
 */

#ifndef WIDGET_H
#define WIDGET_H

#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/List.h>

#include    "common.h"

#define MaxNumXArgs 20

#define AddArg(arg, val)						    \
	XtSetArg(arglist[numargs], arg, (XtArgVal)val), numargs ++

#define DeclareArgs							    \
static Arg			arglist[MaxNumXArgs];			    \
static Cardinal			numargs;

/* function prototypes: */
/* widgeti.c: */
Global Widget init_widgeti P((char *name , char *app_class , int *argcp ,
    char **argv ));
Global Widget create_form P((Widget parent , Widget fromH , Widget fromV ,
    int borderWidth ));
Global Widget create_label P((Widget parent , Widget fromH , Widget fromV ,
    int width , int borderWidth , char *string ));
Global Widget create_command_button P((Widget parent , Widget fromH ,
    Widget fromV , char *string , VoidCallback callback , Pointer data ));
Global Widget create_one_of_many_toggle P((char *label , Widget parent ,
    Widget fromH , Widget fromV , char *strings , VoidCallback callback ));
Global void set_toggle_state P((Widget widget , char *data ));
Global char *get_toggle_state P((Widget widget ));
Global Widget create_viewlist P((Widget parent , Widget fromH ,
    Widget fromV , char **list , int width , int height ,
    int borderWidth , VoidCallback callback , Pointer data ));
Global void set_viewlist P((Widget widget , char **list , int nitems ,
    int longest ));

/* end function prototypes */

#endif	/* WIDGET_H */

