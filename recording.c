/*
 * Project: XMON - An X protocol monitor
 *
 * File: recording.c
 *
 * Description: Routines to record and playback events to an X client
 *
 * This is the beginning of a client-side X event record/playback
 * program.  If you finish it let me know.
 *    - gregm@iname.com
 *
 * Modified by Marc Vertes 18-jul-96 to record and replay delays
 * between events - mvertes@thomson.starway.net.au
 */

/*
  Wed, 17 Jul 1996 13:44:15 +1000

  Please find hereafter my small contribution to your xmon program:
  I added the ability to record and replay delays between X events in
  xmond.
  The result is that the replay is now more realistic :-). Sometime, the
  replay is still waiting for ever (I didn't change the algorithm to send 
  events).

  My solution relies on usleep() system call, which allows to suspend the
  process during a time specified in micro-seconds. If this system call is
  not available (old UNIX versions), a public domain implementation has
  been made available in the Unix-FAQ.  (Now included in xmon release - gregm)

  A last thing, may be you are aware that another implementation of
  recording/replay of Xevents exists, also based on xscope from James
  Peterson. It is called xscript and can be found on ftp.x.org/contrib.
  The strongest limitation of this program is that it can deals with only
  one client at a time, which makes it unusable for my needs. But a good
  point is the better reliability of replay, and the readable ASCII
  representation of X events. Maybe it can inspire you...

  Regards
	    Marc Vertes.
*/

#ifdef RECORD_EVENTS

#include <sys/time.h>

#include "common.h"

#include "xmond.h"
#include "linkl.h"
#include "commands.h"

/* function prototypes: */
/* recording.c: */
//extern void usleep P((unsigned int useconds ));

/* end function prototypes */

typedef struct
{
    int			wait_for_ImageText8;
    int			wait_for_PolyText8;
    unsigned int        wait_for_delay;
    unsigned char	data[32];
}
    SaveEvent;

char *event_name[] =
{
    "",		/* error packet */
    "",		/* reply packet */
    "KP",	/* KeyPress */
    "KR",	/* KeyRelease */
    "BP",	/* ButtonPress */
    "BR",	/* ButtonRelease */
    "MN",	/* MotionNotify */
    "EN",	/* EnterNotify */
    "LN",	/* LeaveNotify */
    "FI",	/* FocusIn */
    "FO",	/* FocusOut */
};

static int			ImageText8_count = 0;
static int			PolyText8_count = 0;
static LinkList			*event_list = NULL;

#define IsMouseEvent(Event)						    \
    (									    \
	Event == 4 /*ButtonPress*/ || Event == 5 /*ButtonRelease*/	    \
	||								    \
	Event == 6 /*MotionNotify*/ || Event == 7 /*EnterNotify*/	    \
	||								    \
	Event == 8 /*LeaveNotify*/					    \
    )

#define IsKeyboardEvent(Event)						    \
    (									    \
	Event == 2 /*KeyPress*/ || Event == 3 /*KeyRelease*/		    \
	||								    \
	Event == 9 /*FocusIn*/ || Event == 10 /*FocusOut*/		    \
    )

extern int			EventVerbose;

/* Time stamping in milliseconds - Marc Vertes 18-juil-96 */
static struct timeval tv;
static unsigned long mtimestamp = 0L;

/* Store the number of milli-seconds since the last call in o */
#define mstamp(t, o) 			\
	gettimeofday(&tv, 0); 		\
	t = tv.tv_sec * 1000; 		\
	t += tv.tv_usec / 1000; 	\
	if (mtimestamp == 0L) o = 0; 	\
	else o = t - mtimestamp; 	\
	mtimestamp = t
/*--------------------------------------------------------*/

Global void
RecordEvent(server, buf, n)
    Server		    *server;
    unsigned char	    *buf;
    long		    n;
{
    short		    Event = buf[0] & 0x7f;
    int			    i;
    unsigned long	    ms_ts, ms_delay;

    if (IsKeyboardEvent(Event) || IsMouseEvent(Event))
    {
        mstamp(ms_ts, ms_delay);
	fprintf
	(
	    stdout, "%s %s %d %d %lu ", PLAY_EVENT_STR, event_name[buf[0]],
	    ImageText8_count, PolyText8_count , ms_delay
	);
	for(i = 0; i < 32; i++)
	{
	    putchar('A' + ((buf[i] >> 4) & 0xf));
	    putchar('A' + (buf[i] & 0xf));

	}
	putchar('\n');
	fflush(stdout);
    }
}

#ifdef USE_WRITE_NOT_PRINTF
Global void
RecordEvent(server, buf, n)
    Server		    *server;
    unsigned char	    *buf;
    long		    n;
{
    short		    Event = buf[0] & 0x7f;
    int			    i;
    int			    fd = fileno(stdout);
    int			    BytesToWrite;
    int			    num_written;
    unsigned char	    outbuf[1000]; /* enough for one line of output */
    unsigned char	    *ptr;
    unsigned long           ms_ts, ms_delay;

    if (IsKeyboardEvent(Event) || IsMouseEvent(Event))
    {
        mstamp(ms_ts, ms_delay);
	sprintf
	(
	    outbuf, "%s %s %d %d %u ", PLAY_EVENT_STR, event_name[buf[0]],
	    ImageText8_count, PolyText8_count , ms_delay
	);
	ptr = outbuf + strlen(outbuf);
	for(i = 0; i < 32; i++)
	{
	    *ptr++ = 'A' + ((buf[i] >> 4) & 0xf);
	    *ptr++ = 'A' + (buf[i] & 0xf);
	}
	*ptr++ = '\n';
	*ptr++ = '\0';
	/* need non-buffering output, so can't use fprint */
	BytesToWrite = strlen(outbuf);
	ptr = outbuf;
	fflush(stdout);
	do
	{
	    num_written = write(fd, ptr, BytesToWrite);
	    if (num_written > 0)
	    {
		BytesToWrite -= num_written;
		ptr += num_written;
	    }
	}
	    while (BytesToWrite > 0 && num_written > 0);
    }
}
#endif

Global void
ReplayEvent(client_list, buf)
    unsigned char	    *buf;
    LinkList		    *client_list;
{
    int			    i;
    Client		    *client;
    Server		    *server;
    Buffer		    *outbuffer;
    unsigned char	    outbuf[32];
    unsigned char	    *ptr = buf;
    int			    wait_for_ImageText8;
    int			    wait_for_PolyText8;
    long                    wait_for_delay;
    SaveEvent		    *save_event;

    client = (Client *)TopOfList(client_list);
    server = (Server *)TopOfList(&client->server_list);
    outbuffer = &client->fdd->outBuffer;

    /* skip spaces */
    while (*ptr == ' ')
	ptr++;
    /* skip event name */
    while (*ptr != ' ')
	ptr++;
    /* skip spaces */
    while (*ptr == ' ')
	ptr++;
    /* read number of ImageText8 requests to wait for */
    wait_for_ImageText8 = atoi(ptr);
    /* skip ImageText8 count */
    while (*ptr != ' ')
	ptr++;
    /* skip spaces */
    while (*ptr == ' ')
	ptr++;
    /* read number of PolyText8 requests to wait for */
    wait_for_PolyText8 = atoi(ptr);
    /* skip PolyText8 count */
    while (*ptr != ' ')
	ptr++;
    /* skip spaces */
    while (*ptr == ' ')
	ptr++;
    /* read number of milliseconds to wait for */
    wait_for_delay = atoi(ptr);
    /* skip delay count */
    while (*ptr != ' ')
	ptr++;
    /* skip spaces */
    while (*ptr == ' ')
	ptr++;

    for(i = 0; i < 32; i++)
	outbuf[i] = (((ptr[2*i] - 'A') << 4) & 0xf0) | (ptr[2*i + 1] - 'A');

    if 
    (
	(wait_for_ImageText8 <= ImageText8_count)
	&&
	(wait_for_PolyText8 <= PolyText8_count)
    )
    {
	ShortToBuf(server->SequenceNumber, &outbuf[2]);
	DecodeEvent(server, outbuf, 32, True);
	SaveBytes(outbuffer, outbuf, 32);
    }
    else
    {
	save_event = Tmalloc(SaveEvent);
	save_event->wait_for_ImageText8 = wait_for_ImageText8;
	save_event->wait_for_PolyText8 = wait_for_PolyText8;
	save_event->wait_for_delay = (unsigned int) wait_for_delay;
	bcopy(outbuf, save_event->data, 32);
	if (event_list == NULL)
	{
	    event_list = Tmalloc(LinkList);
	    initList(event_list);
	}
	appendToList(event_list, (Pointer) save_event);
    }
}

Global void
RecordRequest(Request, client, buf, n)
    short		    Request;
    Client		    *client;
    unsigned char	    *buf;
    long		    n;
{
    Server		    *server;
    Buffer		    *outbuffer;
    unsigned char	    *outbuf;
    SaveEvent		    *save_event;

    if (Request == 76) /* ImageText8 */
	ImageText8_count++;
    if (Request == 74) /* PolyText8 */
	PolyText8_count++;

    if (event_list == NULL)
	return;

    server = (Server *)TopOfList(&client->server_list);
    outbuffer = &client->fdd->outBuffer;

    event_list->current = event_list->top;
    while(event_list->current != (LinkLeaf *)event_list)
    {
	save_event = (SaveEvent *)CurrentContentsOfList(event_list);
	if 
	(
	    (save_event->wait_for_ImageText8 <= ImageText8_count)
	    &&
	    (save_event->wait_for_PolyText8 <= PolyText8_count)
	)
	{
            usleep(1000 * save_event->wait_for_delay);
	    outbuf = save_event->data;
	    ShortToBuf(server->SequenceNumber, &outbuf[2]);
	    DecodeEvent(server, outbuf, 32, True);
	    SaveBytes(outbuffer, outbuf, 32);
	    freeCurrent(event_list);
	}
	else
	    break;
    }
}
#endif /* RECORD_EVENTS */
