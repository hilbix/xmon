/*
 * Project: XMON - An X protocol monitor
 *
 * File: decode11.c
 *
 * Description: Decoding and switching routines for the X11 protocol
 *
 * There are 4 types of things in X11: requests, replies, errors, and
 * events.
 *
 * Each of them has a format defined by a small integer that defines the
 * type of the thing.
 *
 * Requests have an opcode in the first byte.
 *
 * Events have a code in the first byte.
 *
 * Errors have a code in the second byte (the first byte is 0)
 *
 * Replies have a sequence number in bytes 2 and 3.  The sequence number
 * should be used to identify the request that was sent, and from that
 * request we can determine the type of the reply.
 *
 */

#include "xmond.h"

#define MAX_REQUEST	    127
#define MAX_EXT_REQUEST	    255
#define MAX_EVENT	    34
#define MAX_ERROR	    17

/*
 * We need to keep the sequence number for a request to match it with an
 * expected reply.  The sequence number is associated only with the
 * particular connection that we have. We would expect these replies to be
 * handled as a FIFO queue.
*/

typedef struct
{
    long    SequenceNumber;
    short   Request;
}
    QueueEntry;

/* function prototypes: */
/* decode11.c: */
static void SequencedReplyExpected P((Client *client , short RequestType ));
static void ReplyExpected P((Client *client , short Request ));

/* end function prototypes */

extern Bool		    ignore_bytes;

extern int		    RequestVerbose;
extern int		    EventVerbose;
extern int		    ReplyVerbose;
extern int		    ErrorVerbose;
extern int		    CurrentVerbose;

extern Bool		    VerboseRequest[];
extern Bool		    VerboseEvent[];
extern Bool		    BlockRequest[];
extern Bool		    BlockEvent[];
extern Bool		    MonitoringRequests;
extern Bool		    MonitoringEvents;
extern Bool		    BlockingRequests;
extern Bool		    BlockingEvents;
extern int		    SelectedRequestVerbose;
extern int		    SelectedEventVerbose;

static int		    Lastfd;
static long		    LastSequenceNumber;
static short		    LastReplyType;
static int		    RequestCount[MAX_EXT_REQUEST + 1];
static int		    EventCount[MAX_EVENT + 1];
static int		    ErrorCount[MAX_ERROR + 1];
static Bool		    CountRequests;
static Bool		    CountEvents;
static Bool		    CountErrors;


Global void
InitRequestCount()
{
    ClearRequestCount();
    CountRequests = True;
}

Global void
ClearRequestCount()
{
    int i;

    for (i = 0; i <= MAX_EXT_REQUEST; i++)
	RequestCount[i] = 0;
}

Global void
StartRequestCount()
{
    CountRequests = True;
}

Global void
EndRequestCount()
{
    CountRequests = False;
}

Global void
PrintRequestCounts()
{
    int i;
    int tmpInt;
    Bool found;

    found = False;
    for (i = MAX_REQUEST + 1; i <= MAX_EXT_REQUEST; i++)
	if (RequestCount[i] != 0)
	{
	    if (!found)
		fprintf(stdout, "extended requests received:\n");
	    found = True;
	    fprintf(stdout, "%3d  %d\n", i, RequestCount[i]);
	}
    if (found)
	fprintf(stdout, "\n");
    found = False;
    for (i = 1; i <= MAX_REQUEST; i++)
	if ((i <= 119 || i == 127) && RequestCount[i] != 0)
	{
	    if (!found)
		fprintf(stdout, "requests received:\ncode  count  name \n");
	    found = True;
	    fprintf(stdout, "%3d   %3d    ", i, RequestCount[i]);
	    tmpInt = ILong((unsigned char *)&i);
	    PrintENUMERATED((unsigned char *)&tmpInt,
	        sizeof(i), TD[REQUEST].ValueList);
	    fprintf(stdout, "\n");
	}
    if (!found)
	fprintf(stdout, "no requests received\n");
    fflush(stdout);
}

Global void
PrintZeroRequestCounts()
{
    int i;
    int tmpInt;

    fprintf(stdout, "requests never received:\ncode  name\n");
    for (i = 1; i <= MAX_REQUEST; i++)
	if ((i <= 119 || i == 127) && RequestCount[i] == 0)
	{
	    fprintf(stdout, "%3d   ", i);
	    tmpInt = ILong((unsigned char *)&i);
	    PrintENUMERATED((unsigned char *)&tmpInt,
	        sizeof(i), TD[REQUEST].ValueList);
	    fprintf(stdout, "\n");
	}
    fflush(stdout);
}

Global void
InitEventCount()
{
    ClearEventCount();
    CountEvents = True;
}

Global void
ClearEventCount()
{
    int i;

    for (i = 0; i <= MAX_EVENT; i++)
	EventCount[i] = 0;
}

Global void
StartEventCount()
{
    CountEvents = True;
}

Global void
EndEventCount()
{
    CountEvents = False;
}

Global void
PrintEventCounts()
{
    int i;
    int tmpInt;
    Bool found;

    found = False;
    for (i = 2; i <= MAX_EVENT; i++)
	if (EventCount[i] != 0)
	{
	    if (!found)
		fprintf(stdout, "events received:\ncode  count name:\n");
	    found = True;
	    fprintf(stdout, "%3d   %3d    ", i, EventCount[i]);
	    tmpInt = ILong((unsigned char *)&i);
	    PrintENUMERATED((unsigned char *)&tmpInt,
	        sizeof(i), TD[EVENT].ValueList);
	    fprintf(stdout, "\n");
	}
    if (!found)
	fprintf(stdout, "no events received\n");
    fflush(stdout);
}

Global void
PrintZeroEventCounts()
{
    int i;
    int tmpInt;

    fprintf(stdout, "events never received:\ncode  name\n");
    for (i = 2; i <= MAX_EVENT; i++)
	if (EventCount[i] == 0)
	{
	    fprintf(stdout, "%3d   ", i);
	    tmpInt = ILong((unsigned char *)&i);
	    PrintENUMERATED((unsigned char *)&tmpInt,
	        sizeof(i), TD[EVENT].ValueList);
	    fprintf(stdout, "\n");
	}
    fflush(stdout);
}

Global void
InitErrorCount()
{
    ClearErrorCount();
    CountErrors = True;
}

Global void
ClearErrorCount()
{
    int i;

    for (i = 1; i <= MAX_ERROR; i++)
	ErrorCount[i] = 0;
}

Global void
StartErrorCount()
{
    CountErrors = True;
}

Global void
EndErrorCount()
{
    CountErrors = False;
}

Global void
PrintErrorCounts()
{
    int i;
    int tmpInt;
    Bool found;

    found = False;
    for (i = 1; i <= MAX_ERROR; i++)
	if (ErrorCount[i] != 0)
	{
	    if (!found)
		fprintf(stdout, "errors received:\ncode  count  name \n");
	    found = True;
	    fprintf(stdout, "%3d   %3d    ", i, ErrorCount[i]);
	    tmpInt = ILong((unsigned char *)&i);
	    PrintENUMERATED((unsigned char *)&tmpInt,
	        sizeof(i), TD[ERROR].ValueList);
	    fprintf(stdout, "\n");
	}
    if (!found)
	fprintf(stdout, "no errors received\n");
    fflush(stdout);
}

Global void
PrintZeroErrorCounts()
{
    int i;
    int tmpInt;

    fprintf(stdout, "errors never received:\ncode  name\n");
    for (i = 1; i <= MAX_ERROR; i++)
	if (ErrorCount[i] == 0)
	{
	    fprintf(stdout, "%3d   ", i);
	    tmpInt = ILong((unsigned char *)&i);
	    PrintENUMERATED((unsigned char *)&tmpInt,
	        sizeof(i), TD[ERROR].ValueList);
	    fprintf(stdout, "\n");
	}
    fflush(stdout);
}

Global void
DecodeRequest(client, buf, n)
    Client		    *client;
    unsigned char	    *buf;
    long		    n;
{
    int			    fd = client->fdd->fd;
    short		    Request = IByte (&buf[0]);
    Bool		    extended_request = False;
    unsigned short	    tmpSN;

#ifdef RECORD_EVENTS
    RecordRequest(Request, client, buf, n);
#endif

    SetIndentLevel(PRINTCLIENT);

    if
    (
	BlockingRequests
	&&
	1 <= Request && Request <= 127
	&&
	BlockRequest[Request]
    )
	ignore_bytes = True;

    if (!ignore_bytes) /* We don't send this request, so don't increment SN */
	client->SequenceNumber++;

    /* SBf is passed to PrintCARD16 to print the sequence number. 
     * However, PrintCARD16 will apply an endian-ness correction, so,
     * apply the same endian-ness correction here, so that we either
     * correct twice or not at all.
     */
    tmpSN = (unsigned short) (client->SequenceNumber & 0xffff);
    tmpSN = IShort((unsigned char *) &tmpSN);
    bcopy ((char *)&tmpSN, (char *)SBf, sizeof(short));

    if
    (
	MonitoringRequests
	&&
	1 <= Request && Request <= 127
	&&
	VerboseRequest[Request]
    )
	CurrentVerbose = SelectedRequestVerbose;
    else
	CurrentVerbose = RequestVerbose;

    if (Request <= 0 || 127 < Request)
	extended_request = True;
    if (CurrentVerbose > 3 && !(extended_request && ErrorVerbose > 3))
	DumpItem("Request", fd, buf, n);
    if (extended_request)
    {
	if (ErrorVerbose > 3)
	    DumpItem("Extended request", fd, buf, n);
	if (ErrorVerbose > 0)
	{
	    fprintf
	    (
		stdout, "Extended request: opcode %d, sequence number %04lx\n",
		Request, client->SequenceNumber
	    );
	    fflush(stdout);
	}
    }
    if (CountRequests)
	RequestCount[Request]++;
    if (extended_request)
        return;
    switch (Request)
    {
    case 1:
	CreateWindow(buf);
	break;
    case 2:
	ChangeWindowAttributes(buf);
	break;
    case 3:
	GetWindowAttributes(buf);
	ReplyExpected(client, Request);
	break;
    case 4:
	DestroyWindow(buf);
	break;
    case 5:
	DestroySubwindows(buf);
	break;
    case 6:
	ChangeSaveSet(buf);
	break;
    case 7:
	ReparentWindow(buf);
	break;
    case 8:
	MapWindow(buf);
	break;
    case 9:
	MapSubwindows(buf);
	break;
    case 10:
	UnmapWindow(buf);
	break;
    case 11:
	UnmapSubwindows(buf);
	break;
    case 12:
	ConfigureWindow(buf);
	break;
    case 13:
	CirculateWindow(buf);
	break;
    case 14:
	GetGeometry(buf);
	ReplyExpected(client, Request);
	break;
    case 15:
	QueryTree(buf);
	ReplyExpected(client, Request);
	break;
    case 16:
	InternAtom(buf);
	ReplyExpected(client, Request);
	break;
    case 17:
	GetAtomName(buf);
	ReplyExpected(client, Request);
	break;
    case 18:
	ChangeProperty(buf);
	break;
    case 19:
	DeleteProperty(buf);
	break;
    case 20:
	GetProperty(buf);
	ReplyExpected(client, Request);
	break;
    case 21:
	ListProperties(buf);
	ReplyExpected(client, Request);
	break;
    case 22:
	SetSelectionOwner(buf);
	break;
    case 23:
	GetSelectionOwner(buf);
	ReplyExpected(client, Request);
	break;
    case 24:
	ConvertSelection(buf);
	break;
    case 25:
	SendEvent(buf);
	break;
    case 26:
	GrabPointer(buf);
	ReplyExpected(client, Request);
	break;
    case 27:
	UngrabPointer(buf);
	break;
    case 28:
	GrabButton(buf);
	break;
    case 29:
	UngrabButton(buf);
	break;
    case 30:
	ChangeActivePointerGrab(buf);
	break;
    case 31:
	GrabKeyboard(buf);
	ReplyExpected(client, Request);
	break;
    case 32:
	UngrabKeyboard(buf);
	break;
    case 33:
	GrabKey(buf);
	break;
    case 34:
	UngrabKey(buf);
	break;
    case 35:
	AllowEvents(buf);
	break;
    case 36:
	GrabServer(buf);
	break;
    case 37:
	UngrabServer(buf);
	break;
    case 38:
	QueryPointer(buf);
	ReplyExpected(client, Request);
	break;
    case 39:
	GetMotionEvents(buf);
	ReplyExpected(client, Request);
	break;
    case 40:
	TranslateCoordinates(buf);
	ReplyExpected(client, Request);
	break;
    case 41:
	WarpPointer(buf);
	break;
    case 42:
	SetInputFocus(buf);
	break;
    case 43:
	GetInputFocus(buf);
	ReplyExpected(client, Request);
	break;
    case 44:
	QueryKeymap(buf);
	ReplyExpected(client, Request);
	break;
    case 45:
	OpenFont(buf);
	break;
    case 46:
	CloseFont(buf);
	break;
    case 47:
	QueryFont(buf);
	ReplyExpected(client, Request);
	break;
    case 48:
	QueryTextExtents(buf);
	ReplyExpected(client, Request);
	break;
    case 49:
	ListFonts(buf);
	ReplyExpected(client, Request);
	break;
    case 50:
	ListFontsWithInfo(buf);
	ReplyExpected(client, Request);
	break;
    case 51:
	SetFontPath(buf);
	break;
    case 52:
	GetFontPath(buf);
	ReplyExpected(client, Request);
	break;
    case 53:
	CreatePixmap(buf);
	break;
    case 54:
	FreePixmap(buf);
	break;
    case 55:
	CreateGC(buf);
	break;
    case 56:
	ChangeGC(buf);
	break;
    case 57:
	CopyGC(buf);
	break;
    case 58:
	SetDashes(buf);
	break;
    case 59:
	SetClipRectangles(buf);
	break;
    case 60:
	FreeGC(buf);
	break;
    case 61:
	ClearArea(buf);
	break;
    case 62:
	CopyArea(buf);
	break;
    case 63:
	CopyPlane(buf);
	break;
    case 64:
	PolyPoint(buf);
	break;
    case 65:
	PolyLine(buf);
	break;
    case 66:
	PolySegment(buf);
	break;
    case 67:
	PolyRectangle(buf);
	break;
    case 68:
	PolyArc(buf);
	break;
    case 69:
	FillPoly(buf);
	break;
    case 70:
	PolyFillRectangle(buf);
	break;
    case 71:
	PolyFillArc(buf);
	break;
    case 72:
	PutImage(buf);
	break;
    case 73:
	GetImage(buf);
	ReplyExpected(client, Request);
	break;
    case 74:
	PolyText8(buf);
	break;
    case 75:
	PolyText16(buf);
	break;
    case 76:
	ImageText8(buf);
	break;
    case 77:
	ImageText16(buf);
	break;
    case 78:
	CreateColormap(buf);
	break;
    case 79:
	FreeColormap(buf);
	break;
    case 80:
	CopyColormapAndFree(buf);
	break;
    case 81:
	InstallColormap(buf);
	break;
    case 82:
	UninstallColormap(buf);
	break;
    case 83:
	ListInstalledColormaps(buf);
	ReplyExpected(client, Request);
	break;
    case 84:
	AllocColor(buf);
	ReplyExpected(client, Request);
	break;
    case 85:
	AllocNamedColor(buf);
	ReplyExpected(client, Request);
	break;
    case 86:
	AllocColorCells(buf);
	ReplyExpected(client, Request);
	break;
    case 87:
	AllocColorPlanes(buf);
	ReplyExpected(client, Request);
	break;
    case 88:
	FreeColors(buf);
	break;
    case 89:
	StoreColors(buf);
	break;
    case 90:
	StoreNamedColor(buf);
	break;
    case 91:
	QueryColors(buf);
	ReplyExpected(client, Request);
	break;
    case 92:
	LookupColor(buf);
	ReplyExpected(client, Request);
	break;
    case 93:
	CreateCursor(buf);
	break;
    case 94:
	CreateGlyphCursor(buf);
	break;
    case 95:
	FreeCursor(buf);
	break;
    case 96:
	RecolorCursor(buf);
	break;
    case 97:
	QueryBestSize(buf);
	ReplyExpected(client, Request);
	break;
    case 98:
	QueryExtension(buf);
	ReplyExpected(client, Request);
	break;
    case 99:
	ListExtensions(buf);
	ReplyExpected(client, Request);
	break;
    case 100:
	ChangeKeyboardMapping(buf);
	break;
    case 101:
	GetKeyboardMapping(buf);
	ReplyExpected(client, Request);
	break;
    case 102:
	ChangeKeyboardControl(buf);
	break;
    case 103:
	GetKeyboardControl(buf);
	ReplyExpected(client, Request);
	break;
    case 104:
	Bell(buf);
	break;
    case 105:
	ChangePointerControl(buf);
	break;
    case 106:
	GetPointerControl(buf);
	ReplyExpected(client, Request);
	break;
    case 107:
	SetScreenSaver(buf);
	break;
    case 108:
	GetScreenSaver(buf);
	ReplyExpected(client, Request);
	break;
    case 109:
	ChangeHosts(buf);
	break;
    case 110:
	ListHosts(buf);
	ReplyExpected(client, Request);
	break;
    case 111:
	SetAccessControl(buf);
	break;
    case 112:
	SetCloseDownMode(buf);
	break;
    case 113:
	KillClient(buf);
	break;
    case 114:
	RotateProperties(buf);
	break;
    case 115:
	ForceScreenSaver(buf);
	break;
    case 116:
	SetPointerMapping(buf);
	ReplyExpected(client, Request);
	break;
    case 117:
	GetPointerMapping(buf);
	ReplyExpected(client, Request);
	break;
    case 118:
	SetModifierMapping(buf);
	ReplyExpected(client, Request);
	break;
    case 119:
	GetModifierMapping(buf);
	ReplyExpected(client, Request);
	break;
    case 127:
	NoOperation(buf);
	break;
    default:
	fprintf(stdout, "####### DecodeRequest: Impossible! %d\n", Request);
	break;
    }
    fflush(stdout);
}

Global void
DecodeReply(server, buf, n, Request)
    Server		    *server;
    unsigned char	    *buf;
    long		    n;
    short		    Request;
{
    int			    fd = server->fdd->fd;
    Bool		    extended_reply = False;

    SetIndentLevel(PRINTSERVER);
    server->SequenceNumber = IShort(buf + 2) & 0xffff;

    if (Request == 0)
    {
	if (ErrorVerbose > 3)
	    DumpItem("Unexpected reply", fd, buf, n);
	return;
    }

    if
    (
	MonitoringRequests
	&&
	1 <= Request && Request <= 127
	&&
	VerboseRequest[Request]
    )
	CurrentVerbose = SelectedRequestVerbose;
    else
	CurrentVerbose = ReplyVerbose;

    if (Request <= 0 || 127 < Request)
	extended_reply = True;
    if (CurrentVerbose > 3 && !(extended_reply && ErrorVerbose > 3))
	DumpItem("Reply", fd, buf, n);
    if (extended_reply)
    {
	if (ErrorVerbose > 3)
	    DumpItem("Extended reply", fd, buf, n);
	if (ErrorVerbose > 0)
	{
	    fprintf
	    (
		stdout, "Extended reply: opcode %d, sequence number %04lx\n",
		Request, server->SequenceNumber
	    );
	    fflush(stdout);
	}
	return;
    }

    if (CurrentVerbose <= 0)
	return;

    RBf[0] = Request /* for the PrintField in the Reply procedure */ ;
    switch (Request)
    {
    case 3:
	GetWindowAttributesReply(buf);
	break;
    case 14:
	GetGeometryReply(buf);
	break;
    case 15:
	QueryTreeReply(buf);
	break;
    case 16:
	InternAtomReply(buf);
	break;
    case 17:
	GetAtomNameReply(buf);
	break;
    case 20:
	GetPropertyReply(buf);
	break;
    case 21:
	ListPropertiesReply(buf);
	break;
    case 23:
	GetSelectionOwnerReply(buf);
	break;
    case 26:
	GrabPointerReply(buf);
	break;
    case 31:
	GrabKeyboardReply(buf);
	break;
    case 38:
	QueryPointerReply(buf);
	break;
    case 39:
	GetMotionEventsReply(buf);
	break;
    case 40:
	TranslateCoordinatesReply(buf);
	break;
    case 43:
	GetInputFocusReply(buf);
	break;
    case 44:
	QueryKeymapReply(buf);
	break;
    case 47:
	QueryFontReply(buf);
	break;
    case 48:
	QueryTextExtentsReply(buf);
	break;
    case 49:
	ListFontsReply(buf);
	break;
    case 50:
	ListFontsWithInfoReply(buf);
	break;
    case 52:
	GetFontPathReply(buf);
	break;
    case 73:
	GetImageReply(buf);
	break;
    case 83:
	ListInstalledColormapsReply(buf);
	break;
    case 84:
	AllocColorReply(buf);
	break;
    case 85:
	AllocNamedColorReply(buf);
	break;
    case 86:
	AllocColorCellsReply(buf);
	break;
    case 87:
	AllocColorPlanesReply(buf);
	break;
    case 91:
	QueryColorsReply(buf);
	break;
    case 92:
	LookupColorReply(buf);
	break;
    case 97:
	QueryBestSizeReply(buf);
	break;
    case 98:
	QueryExtensionReply(buf);
	break;
    case 99:
	ListExtensionsReply(buf);
	break;
    case 101:
	GetKeyboardMappingReply(buf);
	break;
    case 103:
	GetKeyboardControlReply(buf);
	break;
    case 106:
	GetPointerControlReply(buf);
	break;
    case 108:
	GetScreenSaverReply(buf);
	break;
    case 110:
	ListHostsReply(buf);
	break;
    case 116:
	SetPointerMappingReply(buf);
	break;
    case 117:
	GetPointerMappingReply(buf);
	break;
    case 118:
	SetModifierMappingReply(buf);
	break;
    case 119:
	GetModifierMappingReply(buf);
	break;
    default:
	fprintf(stdout, "####### Unimplemented reply opcode %d\n",Request);
	break;
    }
    fflush(stdout);
}

Global void
DecodeError(server, buf, n)
    Server		    *server;
    unsigned char	    *buf;
    long		    n;
{
    int			    fd = server->fdd->fd;
    short		    Error = IByte (&buf[1]);
    Bool		    extended_error = False;

    CurrentVerbose = ErrorVerbose;
    SetIndentLevel(PRINTSERVER);
    server->SequenceNumber = IShort(buf + 2) & 0xffff;
    (void)CheckReplyTable (server, (short)IShort(&buf[2]), False);
    if (Error < 1 || Error > MAX_ERROR)
	extended_error = True;
    if (CurrentVerbose > 3 && !(extended_error && ErrorVerbose > 3))
	DumpItem("Error", fd, buf, n);
    if (extended_error)
    {
	if (ErrorVerbose > 3)
	    DumpItem("Extended error", fd, buf, n);
	if (ErrorVerbose > 0)
	{
	    fprintf
	    (
		stdout, "Extended error: opcode %d, sequence number %04lx\n",
		Error, server->SequenceNumber
	    );
	    fflush(stdout);
	}
	return;
    }
    if (CountErrors)
	ErrorCount[Error]++;
    if (CurrentVerbose <= 0)
	return;
    switch (Error)
    {
    case 1:
	RequestError(buf);
	break;
    case 2:
	ValueError(buf);
	break;
    case 3:
	WindowError(buf);
	break;
    case 4:
	PixmapError(buf);
	break;
    case 5:
	AtomError(buf);
	break;
    case 6:
	CursorError(buf);
	break;
    case 7:
	FontError(buf);
	break;
    case 8:
	MatchError(buf);
	break;
    case 9:
	DrawableError(buf);
	break;
    case 10:
	AccessError(buf);
	break;
    case 11:
	AllocError(buf);
	break;
    case 12:
	ColormapError(buf);
	break;
    case 13:
	GContextError(buf);
	break;
    case 14:
	IDChoiceError(buf);
	break;
    case 15:
	NameError(buf);
	break;
    case 16:
	LengthError(buf);
	break;
    case 17:
	ImplementationError(buf);
	break;
    default:
	fprintf(stdout, "####### DecodeError: Impossible! %d\n", Error);
	break;
    }
    fflush(stdout);
}

Global void
DecodeEvent(server, buf, n, real_event)
    Server		    *server;
    unsigned char	    *buf;
    long		    n;
    Bool		    real_event;
{
    short		    Event = IByte (&buf[0]) & 0x7f;
    int			    fd = server->fdd->fd;
    Bool		    extended_event = False;

    if (real_event)
    {
	SetIndentLevel(PRINTSERVER);

	if (Event != 11 /* KeymapNotify */)
	    server->SequenceNumber = IShort(buf + 2) & 0xffff;

	if (CountEvents)
	    if (0 <= Event && Event <= MAX_EVENT)
		EventCount[Event]++;

	if
	(
	    BlockingEvents
	    &&
	    2 <= Event && Event <= 34
	    &&
	    BlockEvent[Event]
	)
	    ignore_bytes = True;

	if
	(
	    MonitoringEvents
	    &&
	    2 <= Event && Event <= 34
	    &&
	    VerboseEvent[Event]
	)
	    CurrentVerbose = SelectedEventVerbose;
	else
	    CurrentVerbose = EventVerbose;


	if (Event < 2 || Event > 34)
	    extended_event = True;
	if (CurrentVerbose > 3 && !(extended_event && ErrorVerbose > 3))
	    DumpItem("Event", fd, buf, n);
	if (extended_event)
	{
	    if (ErrorVerbose > 3)
		DumpItem("Extended event", fd, buf, n);
	    if (ErrorVerbose > 0)
	    {
		fprintf
		(
		    stdout, "Extended event: opcode %d, sequence number %04lx\n",
		    Event, server->SequenceNumber
		);
		fflush(stdout);
	    }
	    return;
	}

	if (CurrentVerbose <= 0)
	    return;
    }
    switch (Event)
    {
    case 2:
	KeyPressEvent(buf);
	break;
    case 3:
	KeyReleaseEvent(buf);
	break;
    case 4:
	ButtonPressEvent(buf);
	break;
    case 5:
	ButtonReleaseEvent(buf);
	break;
    case 6:
	MotionNotifyEvent(buf);
	break;
    case 7:
	EnterNotifyEvent(buf);
	break;
    case 8:
	LeaveNotifyEvent(buf);
	break;
    case 9:
	FocusInEvent(buf);
	break;
    case 10:
	FocusOutEvent(buf);
	break;
    case 11:
	KeymapNotifyEvent(buf);
	break;
    case 12:
	ExposeEvent(buf);
	break;
    case 13:
	GraphicsExposureEvent(buf);
	break;
    case 14:
	NoExposureEvent(buf);
	break;
    case 15:
	VisibilityNotifyEvent(buf);
	break;
    case 16:
	CreateNotifyEvent(buf);
	break;
    case 17:
	DestroyNotifyEvent(buf);
	break;
    case 18:
	UnmapNotifyEvent(buf);
	break;
    case 19:
	MapNotifyEvent(buf);
	break;
    case 20:
	MapRequestEvent(buf);
	break;
    case 21:
	ReparentNotifyEvent(buf);
	break;
    case 22:
	ConfigureNotifyEvent(buf);
	break;
    case 23:
	ConfigureRequestEvent(buf);
	break;
    case 24:
	GravityNotifyEvent(buf);
	break;
    case 25:
	ResizeRequestEvent(buf);
	break;
    case 26:
	CirculateNotifyEvent(buf);
	break;
    case 27:
	CirculateRequestEvent(buf);
	break;
    case 28:
	PropertyNotifyEvent(buf);
	break;
    case 29:
	SelectionClearEvent(buf);
	break;
    case 30:
	SelectionRequestEvent(buf);
	break;
    case 31:
	SelectionNotifyEvent(buf);
	break;
    case 32:
	ColormapNotifyEvent(buf);
	break;
    case 33:
	ClientMessageEvent(buf);
	break;
    case 34:
	MappingNotifyEvent(buf);
	break;
    default:
	fprintf(stdout, "####### Unimplemented Event opcode %d\n", Event);
	break;
    }
    fflush(stdout);
}

#ifdef NOT_YET
/*
 * KeepLastReplyExpected:
 *
 * another reply is expected for the same reply as we just had.	 This is
 * only used with ListFontsWithInfo
 */
Global void
KeepLastReplyExpected()
{
    SequencedReplyExpected(Lastfd, LastSequenceNumber, LastReplyType);
}
#endif

/*
 * SequencedReplyExpected:
 *
 * A reply is expected to the type of request given for the fd associated
 * with this one
 */
static void
SequencedReplyExpected(client, RequestType)
    Client		    *client;
    short		    RequestType;
{
    Server		    *server;
    long		    SequenceNumber = client->SequenceNumber;
    QueueEntry		    *p;

    if (ignore_bytes) /* We don't send request, so we won't get a reply */
	return;
    /* create a new queue entry */
    p = Tmalloc(QueueEntry);
    p->SequenceNumber = SequenceNumber;
    p->Request = RequestType;

    server = (Server *)(TopOfList(&client->server_list));
    appendToList(&server->reply_list, (Pointer)p);
}

/*
 * CheckReplyTable:
 *
 * search for the type of request that is associated with a reply to the
 * given sequence number for this fd
 */

Global short
CheckReplyTable (server, SequenceNumber, checkZero)
    Server		    *server;
    short		    SequenceNumber;
    Bool		    checkZero;
{
    int			    fd = server->fdd->fd;
    QueueEntry		    *p;

    ForAllInList(&server->reply_list)
    {
	p = (QueueEntry *)CurrentContentsOfList(&server->reply_list);
	if (SequenceNumber == ((short)(0xffff & p->SequenceNumber)))
	{
	    /* save the Request type */
	    Lastfd = fd;
	    LastSequenceNumber = p->SequenceNumber;
	    LastReplyType = p->Request;
	    /* pull the queue entry out of the queue for this fd */
	    freeCurrent(&server->reply_list);
	    return(LastReplyType);
	}
    }

    /* not expecting a reply for that sequence number */
    if (checkZero)
    {
	fprintf(stdout, "Unexpected reply, sequence number: %04x",SequenceNumber);
	if (ListIsEmpty(&server->reply_list))
	    fprintf(stdout, ". No expected replies.\n");
	else
	{
	    fprintf(stdout, ". Expected replies are:\n");
	    ForAllInList(&server->reply_list)
	    {
		p = (QueueEntry *)CurrentContentsOfList(&server->reply_list);
		fprintf
		(
		    stdout, "Reply on fd %d for sequence number %04lx is type %d\n",
		    fd, p->SequenceNumber, p->Request
		);
	    }
	    fprintf(stdout, "End of expected replies\n");
	}
	fflush(stdout);
    }
    return(0);
}


/*
 * ReplyExpected:
 *
 * A reply is expected to the type of request given for the sequence
 * number associated with this fd
 */
static void
ReplyExpected(client, Request)
    Client		    *client;
    short		    Request;
{
    SequencedReplyExpected(client, Request);
}
