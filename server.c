/*
 * Project: XMON - An X protocol monitor
 *
 * File: server.c
 *
 * Description: Code to decode and print X11 protocol

 * These are the possible values for ByteProcessing:
 *   For clients:
 *	StartSetUpMessage
 *	FinishSetUpMessage
 *	StartRequest
 *	FinishRequest
 *   For servers:
 *	StartSetUpReply
 *	FinishSetUpReply
 *	ServerPacket
 *	FinishReply
 *
 */

#include <sys/time.h>	       /* for struct timeval * */
#include <errno.h>	       /* for EINTR, EADDRINUSE, ... */

#include "xmond.h"

/* function prototypes: */
/* server.c: */
static long StartSetUpMessage P((Pointer private_data , unsigned char *buf , long n ));
static long FinishSetUpMessage P((Pointer private_data , unsigned char *buf , long n ));
static long StartRequest P((Pointer private_data , unsigned char *buf , long n ));
static long FinishRequest P((Pointer private_data , unsigned char *buf , long n ));
static long StartSetUpReply P((Pointer private_data , unsigned char *buf , long n ));
static long FinishSetUpReply P((Pointer private_data , unsigned char *buf , long n ));
static long ErrorPacket P((Server *server , unsigned char *buf , long n ));
static long EventPacket P((Server *server , unsigned char *buf , long n ));
static long ReplyPacket P((Server *server , unsigned char *buf , long n ));
static long ServerPacket P((Pointer private_data , unsigned char *buf , long n ));
static long FinishReply P((Pointer private_data , unsigned char *buf , long n ));

/* end function prototypes */

extern int		    CurrentVerbose;
extern int		    ErrorVerbose;
extern Bool		    littleEndian;
#ifdef RECORD_EVENTS
extern Bool		    RecordingEvents;
#endif

static long ZeroTime1 = -1;
static long ZeroTime2 = -1;
static struct timeval	tp;

/*
 * print the time since we started in hundredths (1/100) of seconds
 */

Global void
PrintTime()
{
    static long lastsec = 0;
    long    sec /* seconds */ ;
    long    hsec /* hundredths of a second */ ;

    gettimeofday(&tp, (struct timezone *)NULL);
    if (ZeroTime1 == -1 || (tp.tv_sec - lastsec) >= 1000)
	{
	    ZeroTime1 = tp.tv_sec;
	    ZeroTime2 = tp.tv_usec / 10000;
	}

    lastsec = tp.tv_sec;
    sec = tp.tv_sec - ZeroTime1;
    hsec = tp.tv_usec / 10000 - ZeroTime2;
    if (hsec < 0)
	{
	    hsec += 100;
	    sec -= 1;
	}
    fprintf(stdout, "%2ld.%02ld: ", sec, hsec);
}

/*
 * pad:
 *
 * we will need to be able to interpret the values stored in the requests
 * as various built-in types.  The following routines support the types
 * built into X11
 */
Global int
pad (n)
	long	n;
{
    /* round up to next multiple of 4 */
    return((n + 3) & ~0x3);
}

Global unsigned long
ILong (buf)
	unsigned char	buf[];
{
    if (littleEndian)
	return((((((buf[3] << 8) | buf[2]) << 8) | buf[1]) << 8) | buf[0]);
    return((((((buf[0] << 8) | buf[1]) << 8) | buf[2]) << 8) | buf[3]);
}

Global unsigned short
IShort (buf)
unsigned char	buf[];
{
    if (littleEndian)
	return (buf[1] << 8) | buf[0];
    return((buf[0] << 8) | buf[1]);
}

Global unsigned short
IByte (buf)
unsigned char	buf[];
{
    return(buf[0]);
}

Global Bool
IBool(buf)
	unsigned char	buf[];
{
    if (buf[0] != 0)
	return(True);
    else
	return(False);
}

Global void
LongToBuf(value, buf)
    long		    value;
    unsigned char	    *buf;
{
    if (!littleEndian)
    {
	buf[0] = value >> 24;
	buf[1] = value >> 16;
	buf[2] = value >> 8;
	buf[3] = value;
    }
    else
    {
	buf[3] = value >> 24;
	buf[2] = value >> 16;
	buf[1] = value >> 8;
	buf[0] = value;
    }
}

Global void
ShortToBuf(value, buf)
    unsigned short	    value;
    unsigned char	    *buf;
{
    if (!littleEndian)
    {
	buf[0] = value >> 8;
	buf[1] = value;
    }
    else
    {
	buf[1] = value >> 8;
	buf[0] = value;
    }
}

/*
 * Byte Processing Routines.  Each routine MUST set num_Needed
 * and ByteProcessing.	It probably needs to do some computation first.
 */

Global void
StartClientConnection(client)
    Client *client;
{
    /* when a new connection is started, we have no saved bytes */
    client->fdd->inBuffer.data = NULL;
    client->fdd->inBuffer.BlockSize = 0;
    client->fdd->inBuffer.num_Saved = 0;
    client->fdd->outBuffer.data = NULL;
    client->fdd->outBuffer.BlockSize = 0;
    client->fdd->outBuffer.num_Saved = 0;

    /* each new connection gets a request sequence number */
    client->SequenceNumber = 0;

    /* we need 12 bytes to start a SetUp message */
    client->fdd->ByteProcessing = StartSetUpMessage;
    client->fdd->inBuffer.num_Needed = 12;
}

Global void
StartServerConnection(server)
    Server *server;
{
    /* when a new connection is started, we have no saved bytes */
    server->fdd->inBuffer.data = NULL;
    server->fdd->inBuffer.BlockSize = 0;
    server->fdd->inBuffer.num_Saved = 0;
    server->fdd->outBuffer.data = NULL;
    server->fdd->outBuffer.BlockSize = 0;
    server->fdd->outBuffer.num_Saved = 0;

    /* when a new connection is started, we have no reply Queue */
    initList(&server->reply_list);

    /* zero the SN of the last event or reply received from the server */
    server->SequenceNumber = 0;

    /* we need 8 bytes to start a SetUp reply */
    server->fdd->ByteProcessing = StartSetUpReply;
    server->fdd->inBuffer.num_Needed = 8;
}

/*
 * StartSetUpMessage:
 *
 * we need the first 12 bytes to be able to determine if, and how many,
 * additional bytes we need for name and data authorization.  However, we
 * can't process the first 12 bytes until we get all of them, so
 * return zero bytes used, and increase the number of bytes needed
 */
static long
StartSetUpMessage (private_data, buf, n)
    Pointer		    private_data;
    unsigned char	    *buf;
    long		    n;
{
    Client		    *client = (Client *) private_data;
    short		    namelength;
    short		    datalength;

    littleEndian = client->fdd->littleEndian = (buf[0] == 'l');
    ((Server *)(TopOfList(&client->server_list)))->fdd->littleEndian
	= littleEndian;

    namelength = IShort(&buf[6]);
    datalength = IShort(&buf[8]);
    client->fdd->ByteProcessing = FinishSetUpMessage;
    client->fdd->inBuffer.num_Needed =
	n + pad((long)namelength) + pad((long)datalength);
    return(0);
}

static long
FinishSetUpMessage (private_data, buf, n)
    Pointer		    private_data;
    unsigned char	    *buf;
    long		    n;
{
    Client		    *client = (Client *) private_data;

    CurrentVerbose = ErrorVerbose;
    PrintSetUpMessage(buf);

    /* after a set-up message, we expect a string of requests */
    client->fdd->ByteProcessing = StartRequest;
    client->fdd->inBuffer.num_Needed = 4;
    return(n);
}


static long
StartRequest (private_data, buf, n)
    Pointer		    private_data;
    unsigned char	    *buf;
    long		    n;
{
    unsigned short	    requestlength = IShort(&buf[2]);
    Client		    *client = (Client *) private_data;

    client->fdd->ByteProcessing = FinishRequest;
    client->fdd->inBuffer.num_Needed = 4 * requestlength;
    return(0);
}


static long
FinishRequest (private_data, buf, n)
    Pointer		    private_data;
    unsigned char	    *buf;
    long		    n;
{
    Client		    *client = (Client *) private_data;

    DecodeRequest(client, buf, n);
    client->fdd->ByteProcessing = StartRequest;
    client->fdd->inBuffer.num_Needed = 4;
    return(n);
}

static long
StartSetUpReply (private_data, buf, n)
    Pointer		    private_data;
    unsigned char	    *buf;
    long		    n;
{
    unsigned short	    replylength = IShort(&buf[6]);
    Server		    *server = (Server *) private_data;

    server->fdd->ByteProcessing = FinishSetUpReply;
    server->fdd->inBuffer.num_Needed = n + 4 * replylength;
    return(0);
}

static long
FinishSetUpReply (private_data, buf, n)
    Pointer		    private_data;
    unsigned char	    *buf;
    long		    n;
{
    Server		    *server = (Server *) private_data;

    CurrentVerbose = ErrorVerbose;
    PrintSetUpReply(buf);
    server->fdd->ByteProcessing = ServerPacket;
    server->fdd->inBuffer.num_Needed = 32;
    return(n);
}

static long
ServerPacket (private_data, buf, n)
    Pointer		    private_data;
    unsigned char	    *buf;
    long		    n;
{
    short		    PacketType = IByte(&buf[0]);
    Server		    *server = (Server *) private_data;

    if (PacketType == 0)
	return(ErrorPacket(server, buf, n));
    else if (PacketType == 1)
	return(ReplyPacket(server, buf, n));
    else
	return(EventPacket(server, buf, n));
}

static long
ErrorPacket (server, buf, n)
    Server		    *server;
    unsigned char	    *buf;
    long		    n;
{
    DecodeError(server, buf, n);
    server->fdd->ByteProcessing = ServerPacket;
    server->fdd->inBuffer.num_Needed = 32;
    return(n);
}


static long
EventPacket (server, buf, n)
    Server		    *server;
    unsigned char	    *buf;
    long		    n;
{
#ifdef RECORD_EVENTS
    if (RecordingEvents)
        RecordEvent(server, buf, n);
#endif
    DecodeEvent(server, buf, n, True);
    server->fdd->ByteProcessing = ServerPacket;
    server->fdd->inBuffer.num_Needed = 32;
    return(n);
}

/* stores the request code for the reply currently being processed */
static short ReplyRequestCode;

/*
    Replies may need more bytes, so we compute how many more
    bytes are needed and ask for them, not using any of the bytes
    we were given (return(0) to say that no bytes were used).
    If the replylength is zero (we don't need any more bytes), the
    number of bytes needed will be the same as what we have, and
    so the top-level loop will call the next routine immediately
    with the same buffer of bytes that we were given.
*/
static long
ReplyPacket (server, buf, n)
    Server		    *server;
    unsigned char	    *buf;
    long		    n;
{
    long		    replylength;
    short		    SequenceNumber = IShort(&buf[2]);

    ReplyRequestCode = CheckReplyTable(server, SequenceNumber, True);
    server->fdd->ByteProcessing = FinishReply;
    replylength = ILong(&buf[4]);
    server->fdd->inBuffer.num_Needed = n + 4 * replylength;
    return(0);
}

static long
FinishReply (private_data, buf, n)
    Pointer		    private_data;
    unsigned char	    *buf;
    long		    n;
{
    Server		    *server = (Server *) private_data;

    DecodeReply(server, buf, n, ReplyRequestCode);
    server->fdd->ByteProcessing = ServerPacket;
    server->fdd->inBuffer.num_Needed = 32;
    return(n);
}
