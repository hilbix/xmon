/*
 *  File: widgeti.c
 *
 *  Description: Routines to build various widget combinations
 */
#include    <stdio.h>
#if defined(SVR4) || defined(SYSV)
#include    <string.h>
#else
#include    <strings.h>
#endif
#include    <X11/cursorfont.h>
#include    <X11/Intrinsic.h>
#include    <X11/StringDefs.h>
#include    <X11/Shell.h>
#include    <X11/Xaw/Form.h>
#include    <X11/Xaw/Command.h>
#include    <X11/Xaw/Label.h>
#include    <X11/Xaw/Box.h>
#include    <X11/Xaw/List.h>
#include    <X11/Xaw/Toggle.h>
#include    <X11/Xaw/Viewport.h>

#include    "common.h"
#include    "widgeti.h"

/* function prototypes: */
/* widgeti.c: */

/* end function prototypes */

DeclareArgs

static	char		    toggle_translation_table[] =
    "<EnterWindow>:		highlight(Always)	\n\
     <LeaveWindow>:		unhighlight()		\n\
     <Btn1Down>,<Btn1Up>:	set() notify()";

static XtTranslations	    toggle_trans;

Global Widget
init_widgeti(name, app_class, argcp, argv)
    char	*name;
    char	*app_class;
    int		*argcp;
    char	**argv;
{
    return XtInitialize(NULL, app_class, NULL, 0, argcp, argv);
}

Global Widget
create_form(parent, fromH, fromV, borderWidth)
Widget			    parent;
Widget			    fromH;
Widget			    fromV;
int			    borderWidth;
{
    Widget		    widget;

    numargs = 0;
    AddArg(XtNfromHoriz, fromH);
    AddArg(XtNfromVert, fromV);
    AddArg(XtNborderWidth, borderWidth);
    AddArg(XtNdefaultDistance, 2);
    widget = XtCreateManagedWidget
    (
	"form", formWidgetClass, parent, arglist, numargs
    );
    return widget;
}

Global Widget
create_label(parent, fromH, fromV, width, borderWidth, string)
Widget			    parent;
Widget			    fromH;
Widget			    fromV;
int			    width;
int			    borderWidth;
char			    *string;
{
    Widget		    widget;

    numargs = 0;
    AddArg(XtNfromHoriz, fromH);
    AddArg(XtNfromVert, fromV);
    AddArg(XtNlabel, string);
    AddArg(XtNborderWidth, borderWidth);
    if (width != 0)
    {
	AddArg(XtNwidth, width);
    }
    widget = XtCreateManagedWidget
    (
	"label", labelWidgetClass, parent, arglist, numargs
    );
    return widget;
}

Global Widget
create_command_button(parent, fromH, fromV, string, callback, data)
Widget			    parent;
Widget			    fromH;
Widget			    fromV;
char			    *string;
VoidCallback		    callback;
Pointer			    data;
{
    Widget		    widget;

    numargs = 0;
    AddArg(XtNfromHoriz, fromH);
    AddArg(XtNfromVert, fromV);
    AddArg(XtNlabel, string);
    widget = XtCreateManagedWidget
    (
	"command", commandWidgetClass, parent, arglist, numargs
    );
    XtAddCallback(widget, XtNcallback, callback, data);
    return widget;
}

Global Widget
create_one_of_many_toggle(label, parent, fromH, fromV, strings, callback)
char			    *label;
Widget			    parent;
Widget			    fromH;
Widget			    fromV;
char			    *strings;
VoidCallback		    callback;
{
    Widget		    form;
    Widget		    widget;
    Widget		    radio = NULL;
    char		    *ptr;
    char		    *var_strings;

    /* Take a copy of strings since we need to modify it. */
    var_strings = (char *)malloc(strlen(strings) + 1);
    strcpy(var_strings, strings);

    numargs = 0;
    AddArg(XtNborderWidth, 0);
    AddArg(XtNfromHoriz, fromH);
    AddArg(XtNfromVert, fromV);
    AddArg(XtNdefaultDistance, 0);
    form = XtCreateManagedWidget
    (
	"toggle_form", formWidgetClass, parent, arglist, numargs
    );

    numargs = 0;
    AddArg(XtNborderWidth, 0);
    AddArg(XtNlabel, label);
    widget = XtCreateManagedWidget
    (
	"label", labelWidgetClass, form, arglist, numargs
    );

    toggle_trans = XtParseTranslationTable(toggle_translation_table);

    do
    {
	ptr = index(var_strings, '\n');
	if (ptr != NULL)
	    *ptr = '\0';
	if ((int)strlen(var_strings) > 0)
	{
	    numargs = 0;
	    AddArg(XtNfromHoriz, widget);
	    AddArg(XtNtranslations, toggle_trans);
	    AddArg(XtNlabel, var_strings);
	    AddArg(XtNradioData, var_strings);
	    if (radio != NULL)
		AddArg(XtNradioGroup, radio);
	    widget = XtCreateManagedWidget
	    (
		(radio == NULL) ? "radio" : "",
		toggleWidgetClass, form, arglist, numargs
	    );
	    XtAddCallback(widget, XtNcallback, callback, var_strings);
	    if (radio == NULL)
		radio = widget;
	}
	if (ptr != NULL)
	    var_strings = ptr + 1;
    }
	while (ptr != NULL);
    return form;
}

Global void
set_toggle_state(widget, data)
Widget			    widget;
char			    *data;
{
    Widget	child;
    WidgetList	list;
    Cardinal	num;
    int		i;
    char	*radio_data;

    child = XtNameToWidget(widget, "radio");
    if (child == (Widget)NULL)
	return;
    XawToggleUnsetCurrent(child);

    numargs = 0;
    AddArg(XtNchildren, &list);
    AddArg(XtNnumChildren, &num);
    XtGetValues(widget, arglist, numargs);

    for (i = 0; i < (int)num; i++)
    {	
	if (XtIsSubclass(child = list[i], toggleWidgetClass))
	{
	    numargs = 0;
	    AddArg(XtNradioData, &radio_data);
	    XtGetValues(child, arglist, numargs);
	    if (!strcmp(data, radio_data))
	    {
		numargs = 0;
		AddArg(XtNstate, True);
		XtSetValues(child, arglist, numargs);
		break;
	    }
	}
    }
}

Global char *
get_toggle_state(widget)
Widget			    widget;
{
    Widget		    child;
    char		    *rtn;

    child = XtNameToWidget(widget, "radio");
    if (child != (Widget)NULL)
	rtn = (char *)XawToggleGetCurrent(child);
    else
	rtn = (char *)NULL;
    return rtn;
}

Global Widget
create_viewlist
(
    parent, fromH, fromV, list, width, height, borderWidth, callback, data
)
Widget			    parent;
Widget			    fromH;
Widget			    fromV;
char			    **list;
int			    width;
int			    height;
int			    borderWidth;
VoidCallback		    callback;
Pointer			    data;
{
    Widget		    view_list;
    Widget		    list_area;

    numargs = 0;
    AddArg(XtNfromHoriz, fromH);
    AddArg(XtNfromVert, fromV);
    AddArg(XtNborderWidth, borderWidth);
    AddArg(XtNwidth, width);
    AddArg(XtNheight, height);
    AddArg(XtNallowVert, True);
    AddArg(XtNforceBars, True);
    view_list = XtCreateManagedWidget
    (
	"view_list", viewportWidgetClass, parent, arglist, numargs
    );

    numargs = 0;
    AddArg(XtNlist, list);
    AddArg(XtNdefaultColumns, 1);
    AddArg(XtNforceColumns, True);
    AddArg(XtNverticalList, True);
    list_area = XtCreateManagedWidget
    (
	"list_area", listWidgetClass, view_list, arglist, numargs
    );
    XtAddCallback(list_area, XtNcallback, callback, data);

    return view_list;
}

Global void
set_viewlist(widget, list, nitems, longest)
    Widget		    widget;
    char		    **list;
    int			    nitems;
    int			    longest;
{
    Widget	child;
    WidgetList	wlist;
    Cardinal	num;
    int		i;

    numargs = 0;
    AddArg(XtNchildren, &wlist);
    AddArg(XtNnumChildren, &num);
    XtGetValues(widget, arglist, numargs);
    for (i = 0; i < (int)num; i++)
	if (XtIsSubclass(child = wlist[i], listWidgetClass))
	    XawListChange(child, list, nitems, longest, False);
}
