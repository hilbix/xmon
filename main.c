/*
 * Project: XMON - An X protocol monitor
 *
 * File: main.c
 *
 * Description: Contains main() for xmond
 *
 */

#include <sys/types.h>	       /* needed by sys/socket.h and netinet/in.h */
#include <sys/uio.h>	       /* for struct iovec, used by socket.h */
#include <sys/socket.h>	       /* for AF_INET, SOCK_STREAM, ... */
#ifdef AIXV3
#include <sys/select.h>        /* for fd_set, ... */
#endif
#include <sys/ioctl.h>	       /* for FIONCLEX, FIONBIO, ... */
#if defined(SVR4) && defined(sun)
# include <sys/filio.h>	       /* for FIONCLEX, etc on Solaris */
#endif
#ifdef SYSV
#ifdef AIXV3
#include <fcntl.h>
#else
#include <sys/fcntl.h>
#endif
#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif
#endif
#include <netinet/in.h>	       /* struct sockaddr_in */
#include <arpa/inet.h>	       /* inet_addr() */
#include <netdb.h>	       /* struct servent * and struct hostent * */
#include <errno.h>	       /* for EINTR, EADDRINUSE, ... */
#include <signal.h>
#ifdef SYSV
#include <fcntl.h>
#endif

#include "xmond.h"
#include "select_args.h"
#include "commands.h"

#define BACKLOG			5
#define XBasePort		6000
#define NUM_REQUESTS		/* 128 */ 256
#define NUM_EVENTS		/* 40 */ 256

#define Strleneq(a,b) Strneq(a, b, strlen(b))

/* function prototypes: */
/* main.c: */
static void ScanArgs P((int argc , char **argv ));
static void CloseConnection P((Client *client ));
static void SetUpConnectionSocket P((int iport ));
static void SetSignalHandling P((void ));
static void MainLoop P((void ));
static Bool SendBuffer P((int fd , Buffer *buffer ));
static short GetServerport P((void ));
static short GetScopePort P((void ));
static void Usage P((void ));
static void ReadStdin P((Pointer private_data ));
static void do_command P((char *ptr ));
static void NewConnection P((Pointer private_data ));
static void DataFromClient P((Pointer private_data ));
static void DataFromServer P((Pointer private_data ));
static Bool ReadAndProcessData P((Pointer private_data , FDDescriptor *read_fdd , FDDescriptor *write_fdd , Bool is_server ));
static void RemoveSavedBytes P((Buffer *buffer , int n ));
static int ConnectToClient P((int ConnectionSocket ));
static int ConnectToServer P((char *hostName ));
static char *OfficialName P((char *name ));
static void SignalINT P((int n ));
static void SignalTERM P((int n ));

#if (mskcnt>4)
static Bool ANYSET P((long *src));
#endif

extern char *getenv();
extern int gethostname P((char *name, int namelen ));

/* end function prototypes */

Global Bool    ignore_bytes;
Global int     RequestVerbose = 0;	/* verbose level for requests */
Global int     EventVerbose = 0;	/* verbose level for events */
Global int     ReplyVerbose = 0;	/* verbose level for replies */
Global int     ErrorVerbose = 1;	/* verbose level for error */
Global Bool    VerboseRequest[NUM_REQUESTS];/* requests that are very verbose */
Global Bool    VerboseEvent[NUM_EVENTS];    /* events that are very verbose */
Global Bool    BlockRequest[NUM_REQUESTS];  /* do not transmit these requests */
Global Bool    BlockEvent[NUM_EVENTS];	    /* do not transmit these events */
Global Bool    MonitoringRequests = True;   /* monitor selected requests? */
Global Bool    MonitoringEvents = True;	    /* monitor selected events? */
Global Bool    BlockingRequests = True;	    /* block selected requests? */
Global Bool    BlockingEvents = True;	    /* block selected events? */
Global int     SelectedRequestVerbose = 3;  /* selected requests verboseness */
Global int     SelectedEventVerbose = 3;    /* selected errors verboseness */
Global char    ServerHostName[255];
Global char    *LocalHostName;
Global int     debuglevel = 0;
Global LinkList client_list;	    /* list of Client */
Global FDDescriptor *FDD;
Global long	ReadDescriptors[mskcnt];
Global long	WriteDescriptors[mskcnt];
Global short	HighestFD;
Global Bool	littleEndian;
#ifdef RECORD_EVENTS
Global Bool	RecordingEvents = False;     /* send event packets to stdout? */
#endif
Global Bool	RawBytesOnly = False;        /* pass on and print raw bytes? */
Global Bool	PrintRawBytes = True;
Global Bool	AsciiOutput = False;

static long	    ClientNumber = 0;
static int		ServerPort = 0;
static int		ListenForClientsPort = 1;
static Bool	ShowPacketSize = False;	    /* show size of net packets? */

Global int
main(argc, argv)
	int	argc;
	char  **argv;
{
    int i;

    ScanArgs(argc, argv);
    InitializeFD();
    InitializeX11();
    InitRequestCount();
    InitEventCount();
    InitErrorCount();
    (void)UsingFD(fileno(stdin), ReadStdin, (Pointer)(int)fileno(stdin));
    SetUpConnectionSocket(GetScopePort());
    SetSignalHandling();
    for (i = 0; i < NUM_REQUESTS; i++)
	BlockRequest[i] = VerboseRequest[i] = False;
    for (i = 0; i < NUM_EVENTS; i++)
	BlockEvent[i] = VerboseEvent[i] = False;
    initList(&client_list);
    MainLoop();
    return 0;
}

static void
CloseConnection(client)
    Client		    *client;
{
    Server		    *server;

    server = (Server *)(TopOfList(&client->server_list));
    debug
    (
	1,
	(
	    stderr, "CloseConnection: client = %d, server = %d\n",
	    client->fdd->fd, server->fdd->fd
	)
    );
    if (client->fdd->outBuffer.BlockSize > 0)
	Tfree(client->fdd->outBuffer.data);
    if (client->fdd->inBuffer.BlockSize > 0)
	Tfree(client->fdd->inBuffer.data);
    NotUsingFD(client->fdd->fd);

    if (server->fdd->outBuffer.BlockSize > 0)
	Tfree(server->fdd->outBuffer.data);
    if (server->fdd->inBuffer.BlockSize > 0)
	Tfree(server->fdd->inBuffer.data);
    NotUsingFD(server->fdd->fd);

    freeList(&client->server_list);
    freeMatchingLeaf(&client_list, (Pointer)client);
}

static void
ScanArgs(argc, argv)
	int	argc;
	char  **argv;
{
    int i;

    ServerHostName[0] = '\0';
    /* Scan argument list */
    for (i = 1; i < argc; i++)
    {
	if (Streq(argv[i], "-server"))
	{
	    /*  Generally of the form server_name:display_number.
	     *  These all mean port 0 on server blah:
	     *	"blah",
	     *	"blah:",
	     *	"blah:0"
	     *  This means port 0 on local host:
	     *      ":0".
	     *  Positive port values are added to the base X server port 6000.
	     *  Negative port values mean absolute port numbers and are
	     *  used unchanged.
	     */
	    if (++i < argc && argv[i] != NULL && argv[i][0] != '\0')
	    {
		char *index = strchr(argv[i], ':');
		if (index != NULL)
		{
		    ServerPort = atoi(index + 1);
		    *index = '\0';
		}
		if (index != argv[i])
		    strcpy(ServerHostName, OfficialName(argv[i]));
	    }
	    else
		    Usage();
	    debug(1,(stderr, "ServerHostName=%s\n", ServerHostName));
	    debug(1,(stderr, "ServerPort=%d\n", ServerPort));
	}
	else if (Streq(argv[i], "-port"))
	{
	    if (++i < argc)
		    ListenForClientsPort = atoi(argv[i]);
	    else
		    Usage();
	    debug(1,(stderr, "ListenForClientsPort=%d\n",ListenForClientsPort));
	}
	else if (Streq(argv[i], "-debug"))
	{
	    /*
		debug levels:
		    2 - trace each procedure entry
		    4 - I/O, connections
		    8 - Scope internals
		    16 - Message protocol
		    32 - 64 - malloc
		    128 - 256 - really low level
	    */
	    if (++i < argc)
		    debuglevel = atoi(argv[i]);
	    else
		    Usage();
	    if (debuglevel == 0)
		debuglevel = 255;
	    debuglevel |= 1;
	    debug(1,(stderr, "debuglevel = %d\n", debuglevel));
	}
	else if (Streq(argv[i], "-verbose"))
	{
	    if (++i < argc)
		RequestVerbose = EventVerbose =
		    ReplyVerbose = ErrorVerbose = atoi(argv[i]);
	    else
		Usage();
	}
#ifdef RECORD_EVENTS
	else if (Streq(argv[i], "-record"))
	{
	    RecordingEvents = True;
	    RequestVerbose = EventVerbose = ReplyVerbose = ErrorVerbose = 0;
	}
	else if (Streq(argv[i], "-play"))
	    RequestVerbose = EventVerbose = ReplyVerbose = ErrorVerbose = 0;
#endif
	else if (Streq(argv[i], "-ascii"))
	    AsciiOutput = True;
	else if (Streq(argv[i], "-raw"))
	    RawBytesOnly = True;
	else if (Streq(argv[i], "-packet_size"))
	    ShowPacketSize = True;
	else if (Streq(argv[i], "-noprintraw"))
	    PrintRawBytes = False;
	else if (Streq(argv[i], "-raw_size"))
	{
	    ShowPacketSize = True;
	    RawBytesOnly = True;
	    PrintRawBytes = False;
	}
	else
	    Usage();
    }

    LocalHostName = (char *)malloc(255);
    (void) gethostname(LocalHostName, 255);
    if (ServerHostName[0] == '\0')
    {
	char *display_env_name;

	if ((display_env_name = getenv("DISPLAY")) == NULL)
	    (void) gethostname(ServerHostName, sizeof (ServerHostName));
	else
	{
	    char *index;

	    strcpy(ServerHostName, display_env_name);
	    index = strchr(ServerHostName, ':');
	    if (index != NULL)
	    {
		*index = '\0';
		ServerPort = atoi(index + 1);
	    }
	    if (index == ServerHostName)
		(void) gethostname(ServerHostName, sizeof (ServerHostName));
	}
    }
    if (Streq(ServerHostName,LocalHostName) &&
      ListenForClientsPort == ServerPort)
	{
	    fprintf
	    (
		stderr, "Can't have xmond on same port as server (%d)\n",
		ListenForClientsPort
	    );
	    Usage();
	}
}

/*
 * SetUpConnectionSocket:
 *
 * Create a socket for a service to listen for clients
 */
static void
SetUpConnectionSocket(iport)
int			iport;
{
#ifndef SVR4
    int			  ON = 1;	  /* used in ioctl */
#endif
    int			   ConnectionSocket;
    struct sockaddr_in	  sin;
    int yes = 1;

    enterprocedure("SetUpConnectionSocket");

    /* create the connection socket and set its parameters of use */
    ConnectionSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (ConnectionSocket < 0)
	{
	    perror("socket");
	    exit(-1);
	}
    (void)setsockopt(ConnectionSocket, SOL_SOCKET, SO_REUSEADDR,
        (char*)&yes, sizeof(yes));
#ifdef SO_USELOOPBACK
    (void)setsockopt(ConnectionSocket, SOL_SOCKET, SO_USELOOPBACK,
        (char*)&yes, sizeof(yes));
#endif
#ifdef SO_DONTLINGER
    (void)setsockopt(ConnectionSocket, SOL_SOCKET, SO_DONTLINGER,
	(char*)&yes, sizeof(yes));
#endif

    /* define the name and port to be used with the connection socket */
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;

#ifdef OLD_CODE
    /* the address of the socket is composed of two parts: the host machine and
	the port number.  We need the host machine address for the current host
    */
    {
	/* define the host part of the address */
	struct hostent *hp;

	hp = gethostbyname(LocalHostName);
	if (hp == NULL)
	    panic("No address for our host"); /* and exit */
	bcopy((char *)hp->h_addr, (char*)&sin.sin_addr, hp->h_length);
    }
#endif
	/* new code -- INADDR_ANY should be better than using the name of the
	    host machine.  The host machine may have several different network
	    addresses.	INADDR_ANY should work with all of them at once. */
    sin.sin_addr.s_addr = INADDR_ANY;

    sin.sin_port = htons (iport);

    /* bind the name and port number to the connection socket */
    if (bind(ConnectionSocket, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
	    perror("bind");
	    exit(-1);
	}

    debug(4,(stderr, "Socket is FD %d for %s,%d\n",
		    ConnectionSocket, LocalHostName, iport));

    /* now activate the named connection socket to get messages */
    if (listen(ConnectionSocket, BACKLOG) < 0)
	{
	    perror("listen");
	    exit(-1);
	};

    /* a few more parameter settings */
#ifdef SYSV
    fcntl(ConnectionSocket, F_SETFD, FD_CLOEXEC);
#else
    ioctl(ConnectionSocket, FIOCLEX, 0);
#endif
#ifndef SVR4
    ioctl(ConnectionSocket, FIONBIO, &ON);
#endif

    debug(4,(stderr, "Listening on FD %d\n", ConnectionSocket));
    (void)UsingFD(ConnectionSocket, NewConnection, (Pointer)ConnectionSocket);
}

/*
 * Signal Handling support
 */
static void
SetSignalHandling()
{
    enterprocedure("SetSignalHandling");
    signal(SIGINT, SignalINT);
    signal(SIGTERM, SignalTERM);
}

/*
 * MainLoop:
 *
 * Wait for input from any source and Process.
 */
static void
MainLoop()
{
    long		    rfds[mskcnt];
    long		    wfds[mskcnt];
    long		    xfds[mskcnt];
    short		    nfds;
    short		    fd;
    Client		    *client;
    Server		    *server;

    while (True)
    {
	COPYBITS(ReadDescriptors, rfds);
	COPYBITS(WriteDescriptors, wfds);
	ORBITS(xfds, ReadDescriptors, WriteDescriptors);
	nfds = select
	(
	    HighestFD + 1, (fd_set *)rfds, (fd_set *)wfds, (fd_set *)xfds, NULL
	);
	if (nfds <= 0)
	{
	    switch (errno)
	    {
	    case EINTR:
		/* do nothing; just try again next time */
		break;
	    case EBADF:
		/* one of the file descriptors is bad (which one?) */
		fprintf(stderr, "MainLoop: select gets EBADF\n");
		/* was: NotUsingFD(HighestFD); */
		break;
	    default:
		/* serious error */
		panic("Select returns error");
	    }
	}
	else
	{
	    for (fd = 0; ANYSET(rfds) && fd <= HighestFD; fd++)
	    {
		if (GETBIT(rfds, fd))
		{
		    BITCLEAR(rfds, fd);
		    if (FDD[fd].InputHandler != (VoidCallback)NULL)
			/* InputHandler will be NULL if select returns
			 * both the server and client in the same call
			 * and the first one has EOF.  This will cause
			 * both fds to be closed.
			 */
			(FDD[fd].InputHandler)(FDD[fd].private_data);
		    else
			fprintf(stderr, "Null InputHandler for fd %d\n", fd);
		}
	    }
	    for (fd = 0; ANYSET(wfds) && fd <= HighestFD; fd++)
	    {
		if (GETBIT(wfds, fd))
		{
		    BITCLEAR(wfds, fd);
		    if (!SendBuffer(fd, &FDD[fd].outBuffer))
			panic("help: which connection do we shut down (TODO?)");
		}
	    }
	    for (fd = 0; ANYSET(xfds) && fd <= HighestFD; fd++)
	    {
		if (GETBIT(xfds, fd))
		{
		    BITCLEAR(xfds, fd);
		    if (fd != fileno(stdin))
			fprintf(stderr, "error in select: fd is %d\n", fd);
		}
	    }
	}
	client_list.current = client_list.top;
	while (client_list.current != (LinkLeaf *)(&client_list))
	{
	    client = (Client *)CurrentContentsOfList(&client_list);
	    server = (Server *)(TopOfList(&client->server_list));
	    if
	    (
		SendBuffer(client->fdd->fd, &client->fdd->outBuffer)
		&&
		SendBuffer(server->fdd->fd, &server->fdd->outBuffer)
	    )
		client_list.current = client_list.current->next;
	    else
		CloseConnection(client);
	}
    }
}

static Bool
SendBuffer(fd, buffer)
    int			    fd;
    Buffer		    *buffer;
{
    char		    *buf = buffer->data;
    int			    BytesToWrite = buffer->num_Saved;
    int			    num_written;
    int			    total_num_written = 0;
    Bool		    ok = True;

    BITCLEAR(WriteDescriptors, fd);
    if (BytesToWrite > 0)
    {
	do
	{
	    num_written = write (fd, buf, BytesToWrite);
	    if (num_written > 0)
	    {
		BytesToWrite -= num_written;
		buf += num_written;
		total_num_written += num_written;
	    }
	}
	while (BytesToWrite > 0 && num_written > 0);

	if (BytesToWrite > 0)
	{
	    if (errno == EWOULDBLOCK || errno == EINTR)
	    {
		debug(4,(stderr, "write is blocked: buffering output\n"));
		RemoveSavedBytes(buffer, total_num_written);
		BITSET(WriteDescriptors, fd);
	    }
	    else
	    {
		perror("Error on write to Client/Server");
		ok = False;
	    }
	}
	else
	    buffer->num_Saved = 0;
    }
    return ok;
}

static short
GetServerport ()
{
    short     port;

    enterprocedure("GetServerport");

    if (ServerPort < 0)
	port = -ServerPort;
    else
	port = XBasePort + ServerPort;
    debug(4,(stderr, "Server service is on port %d\n", port));
    return(port);
}

static short
GetScopePort ()
{
    short     port;

    enterprocedure("GetScopePort");

    if (ListenForClientsPort < 0)
	port = -ListenForClientsPort;
    else
	port = XBasePort + ListenForClientsPort;
    debug(4,(stderr, "xmond service is on port %d\n", port));
    return(port);
}

static void
Usage()
{
    fprintf(stderr, "Usage: xmond [options]\n");
    fprintf(stderr, "    -server <server_name:port>\n");
    fprintf(stderr, "    -port <listen_port>\n");
    fprintf(stderr, "    -debug <debug_level>\n");
    fprintf(stderr, "    -verbose <all_monitor_level>\n");
#ifdef RECORD_EVENTS
    fprintf(stderr, "    -record\n");
    fprintf(stderr, "    -play\n");
#endif
    fprintf(stderr, "    -raw\n");
    fprintf(stderr, "    -packet_size\n");
    fprintf(stderr, "    -noprintraw\n");
    fprintf(stderr, "    -raw_size\n");
    exit(1);
}

static void
ReadStdin(private_data)
    Pointer		private_data;
{
    static char		save_buf[4096];
    static int	    	saved_bytes = 0;
    int			fd = (int)private_data;
    char		buf[4096];
    char		*ptr;
    char		*end_ptr;
    int			n;
    int			endn;

    enterprocedure("ReadStdin");
    n = read(fd, buf, 2048);
    if (n <= 0)
    {
	if (n < 0)
	    fprintf(stderr, "Error reading stdin\n");
	NotUsingFD(fd);
	n = 0;
    }
    if (n + saved_bytes == 0)
	return;
    if (saved_bytes > 0)
    {
	bcopy(buf, buf + saved_bytes, n);
	bcopy(save_buf, buf, saved_bytes);
	n += saved_bytes;
	saved_bytes = 0;
    }
    ptr = buf;
    while(n > 0)
    {
	while (n > 0 && (*ptr == ' ' || *ptr == '\t'))
	{
	    ptr++;
	    n--;
	}
	if (n <= 0)
	    break;
	end_ptr = ptr;
	endn = n;
	while(endn > 0 && *end_ptr != '\n')
	{
	    end_ptr++;
	    endn--;
	}
	if (endn <= 0)
	{
	    bcopy(ptr, save_buf, n);
	    saved_bytes = n;
	    break;
	}
	*end_ptr = '\0';
	do_command(ptr);
	ptr = end_ptr + 1;
	n = endn - 1;
    }
}

static void
do_command(ptr)
    char		*ptr;
{
	if (Strleneq(ptr, REQUEST_VERBOSE_STR))
	    RequestVerbose = atoi(ptr + strlen(REQUEST_VERBOSE_STR));
	else if (Strleneq(ptr, EVENT_VERBOSE_STR))
	    EventVerbose = atoi(ptr + strlen(EVENT_VERBOSE_STR));
	else if (Strleneq(ptr, REPLY_VERBOSE_STR))
	    ReplyVerbose = atoi(ptr + strlen(REPLY_VERBOSE_STR));
	else if (Strleneq(ptr, ERROR_VERBOSE_STR))
	    ErrorVerbose = atoi(ptr + strlen(ERROR_VERBOSE_STR));
	else if (Strleneq(ptr, MONITOR_REQUEST_ON_STR))
	    VerboseRequest[atoi(ptr + strlen(MONITOR_REQUEST_ON_STR))] = True;
	else if (Strleneq(ptr, MONITOR_REQUEST_OFF_STR))
	    VerboseRequest[atoi(ptr + strlen(MONITOR_REQUEST_OFF_STR))] =False;
	else if (Strleneq(ptr, MONITOR_EVENT_ON_STR))
	    VerboseEvent[atoi(ptr + strlen(MONITOR_EVENT_ON_STR))] = True;
	else if (Strleneq(ptr, MONITOR_EVENT_OFF_STR))
	    VerboseEvent[atoi(ptr + strlen(MONITOR_EVENT_OFF_STR))] = False;
	else if (Strleneq(ptr, REQUEST_MONITORING_ON_STR))
	    MonitoringRequests = True;
	else if (Strleneq(ptr, REQUEST_MONITORING_OFF_STR))
	    MonitoringRequests = False;
	else if (Strleneq(ptr, EVENT_MONITORING_ON_STR))
	    MonitoringEvents = True;
	else if (Strleneq(ptr, EVENT_MONITORING_OFF_STR))
	    MonitoringEvents = False;
	else if (Strleneq(ptr, REQUEST_BLOCKING_ON_STR))
	    BlockingRequests = True;
	else if (Strleneq(ptr, REQUEST_BLOCKING_OFF_STR))
	    BlockingRequests = False;
	else if (Strleneq(ptr, EVENT_BLOCKING_ON_STR))
	    BlockingEvents = True;
	else if (Strleneq(ptr, EVENT_BLOCKING_OFF_STR))
	    BlockingEvents = False;
	else if (Strleneq(ptr, SHOW_PACKET_SIZE_ON_STR))
	    ShowPacketSize = True;
	else if (Strleneq(ptr, SHOW_PACKET_SIZE_OFF_STR))
	    ShowPacketSize = False;
#ifdef RECORD_EVENTS
	else if (Strleneq(ptr, RECORD_EVENTS_ON_STR))
	    RecordingEvents = True;
	else if (Strleneq(ptr, RECORD_EVENTS_OFF_STR))
	    RecordingEvents = False;
	else if (Strleneq(ptr, PLAY_EVENT_STR))
	    ReplayEvent(&client_list, ptr + strlen(PLAY_EVENT_STR));
#endif
	else if (Strleneq(ptr, SELECTED_REQUEST_VERBOSE_STR))
	    SelectedRequestVerbose =
		atoi(ptr +strlen(SELECTED_REQUEST_VERBOSE_STR));
	else if (Strleneq(ptr, SELECTED_EVENT_VERBOSE_STR))
	    SelectedEventVerbose =
		atoi(ptr + strlen(SELECTED_EVENT_VERBOSE_STR));
	else if (Strleneq(ptr, BLOCK_REQUEST_ON_STR))
	    BlockRequest[atoi(ptr + strlen(BLOCK_REQUEST_ON_STR))] = True;
	else if (Strleneq(ptr, BLOCK_REQUEST_OFF_STR))
	    BlockRequest[atoi(ptr + strlen(BLOCK_REQUEST_OFF_STR))] = False;
	else if (Strleneq(ptr, BLOCK_EVENT_ON_STR))
	    BlockEvent[atoi(ptr + strlen(BLOCK_EVENT_ON_STR))] = True;
	else if (Strleneq(ptr, BLOCK_EVENT_OFF_STR))
	    BlockEvent[atoi(ptr + strlen(BLOCK_EVENT_OFF_STR))] = False;
	else if (Strleneq(ptr, START_REQUEST_COUNT_STR))
	    StartRequestCount();
	else if (Strleneq(ptr, STOP_REQUEST_COUNT_STR))
	    EndRequestCount();
	else if (Strleneq(ptr, CLEAR_REQUEST_COUNT_STR))
	    ClearRequestCount();
	else if (Strleneq(ptr, PRINT_REQUEST_COUNT_STR))
	    PrintRequestCounts();
	else if (Strleneq(ptr, PRINT_ZERO_REQUEST_COUNT_STR))
	    PrintZeroRequestCounts();
	else if (Strleneq(ptr, START_EVENT_COUNT_STR))
	    StartEventCount();
	else if (Strleneq(ptr, STOP_EVENT_COUNT_STR))
	    EndEventCount();
	else if (Strleneq(ptr, CLEAR_EVENT_COUNT_STR))
	    ClearEventCount();
	else if (Strleneq(ptr, PRINT_EVENT_COUNT_STR))
	    PrintEventCounts();
	else if (Strleneq(ptr, PRINT_ZERO_EVENT_COUNT_STR))
	    PrintZeroEventCounts();
	else if (Strleneq(ptr, START_ERROR_COUNT_STR))
	    StartErrorCount();
	else if (Strleneq(ptr, STOP_ERROR_COUNT_STR))
	    EndErrorCount();
	else if (Strleneq(ptr, CLEAR_ERROR_COUNT_STR))
	    ClearErrorCount();
	else if (Strleneq(ptr, PRINT_ERROR_COUNT_STR))
	    PrintErrorCounts();
	else if (Strleneq(ptr, PRINT_ZERO_ERROR_COUNT_STR))
	    PrintZeroErrorCounts();
	else if (Strleneq(ptr, QUIT_STR))
	    exit(0);
	else if (Strleneq(ptr, HELP_STR))
	{
	    fprintf(stdout, "%s <verbose_level>\n", REQUEST_VERBOSE_STR);
	    fprintf(stdout, "%s <verbose_level>\n", EVENT_VERBOSE_STR);
	    fprintf(stdout, "%s <verbose_level>\n", REPLY_VERBOSE_STR);
	    fprintf(stdout, "%s <verbose_level>\n", ERROR_VERBOSE_STR);
	    fprintf(stdout, "%s <verbose_request_number>\n",
						MONITOR_REQUEST_ON_STR);
	    fprintf(stdout, "%s <verbose_request_number>\n",
						MONITOR_REQUEST_OFF_STR);
	    fprintf(stdout, "%s <verbose_event_number>\n",
						MONITOR_EVENT_ON_STR);
	    fprintf(stdout, "%s <verbose_event_number>\n",
						MONITOR_EVENT_OFF_STR);
	    fprintf(stdout, "%s <selected_request_verboseness>\n",
						SELECTED_REQUEST_VERBOSE_STR);
	    fprintf(stdout, "%s <selected_event_verboseness>\n",
						SELECTED_EVENT_VERBOSE_STR);
	    fprintf(stdout,"%s <block_request_number>\n",BLOCK_REQUEST_ON_STR);
	   fprintf(stdout,"%s <block_request_number>\n",BLOCK_REQUEST_OFF_STR);
	    fprintf(stdout, "%s <block_event_number>\n", BLOCK_EVENT_ON_STR);
	    fprintf(stdout, "%s <block_event_number>\n", BLOCK_EVENT_OFF_STR);

	    fprintf(stdout, "%s\n", START_REQUEST_COUNT_STR);
	    fprintf(stdout, "%s\n", STOP_REQUEST_COUNT_STR);
	    fprintf(stdout, "%s\n", CLEAR_REQUEST_COUNT_STR);
	    fprintf(stdout, "%s\n", PRINT_REQUEST_COUNT_STR);
	    fprintf(stdout, "%s\n", PRINT_ZERO_REQUEST_COUNT_STR);
	    fprintf(stdout, "%s\n", START_EVENT_COUNT_STR);
	    fprintf(stdout, "%s\n", STOP_EVENT_COUNT_STR);
	    fprintf(stdout, "%s\n", CLEAR_EVENT_COUNT_STR);
	    fprintf(stdout, "%s\n", PRINT_EVENT_COUNT_STR);
	    fprintf(stdout, "%s\n", PRINT_ZERO_EVENT_COUNT_STR);
	    fprintf(stdout, "%s\n", START_ERROR_COUNT_STR);
	    fprintf(stdout, "%s\n", STOP_ERROR_COUNT_STR);
	    fprintf(stdout, "%s\n", CLEAR_ERROR_COUNT_STR);
	    fprintf(stdout, "%s\n", PRINT_ERROR_COUNT_STR);
	    fprintf(stdout, "%s\n", PRINT_ZERO_ERROR_COUNT_STR);
	    fprintf(stdout, "%s\n", QUIT_STR);
	    fprintf(stdout, "%s\n", HELP_STR);
	    fflush(stdout);
	}
	else
	{
	    fprintf
	    (
		stdout, "illegal command: %s\n\"help\" to get help\n", ptr
	    );
	    fflush(stdout);
	}
}

/*
 * NewConnection:
 *
 * Create New Connection to a client program and to Server.
 */
static void
NewConnection(private_data)
    Pointer		private_data;
{
    int			XPort = (int)private_data;
    Client		*client;
    Server		*server;
    int			fd;

    client = Tmalloc(Client);
    appendToList(&client_list, (Pointer)client);
    fd = ConnectToClient(XPort);
    client->fdd = UsingFD(fd, DataFromClient, (Pointer)client);
    client->fdd->fd = fd;
    StartClientConnection(client);

    initList(&client->server_list);
    server = Tmalloc(Server);
    appendToList(&client->server_list, (Pointer)server);
    fd = ConnectToServer(ServerHostName);
    server->fdd = UsingFD(fd, DataFromServer, (Pointer)server);
    server->fdd->fd = fd;
    server->client = client;
    StartServerConnection(server);

    ClientNumber += 1;
    client->ClientNumber = ClientNumber;
    if (ErrorVerbose > 1)
    {
	fprintf(stdout, "Opening client connection: %ld\n", ClientNumber);
	fflush(stdout);
    }
}

static void
DataFromClient(private_data)
    Pointer		    private_data;
{
    Client		    *client = (Client *)private_data;
    Server		    *server;

    server = (Server *)(TopOfList(&client->server_list));
    if (!ReadAndProcessData(private_data, client->fdd, server->fdd, False))
	CloseConnection(client);
}

static void
DataFromServer(private_data)
    Pointer		    private_data;
{
    Server		    *server = (Server *)private_data;
    Client		    *client = server->client;

    if (!ReadAndProcessData(private_data, server->fdd, client->fdd, True))
	CloseConnection(client);
}

/*
 * ReadAndProcessData:
 *
 * Read as much as we can and then loop as long as we have enough
 * bytes to do anything.
 *
 * In each cycle check if we have enough bytes (saved or in the newly read
 * buffer) to do something.  If so, we want the bytes to be grouped
 * together into one contiguous block of bytes.	 We have three cases:
 *
 * (1) num_Saved == 0; so all needed bytes are in the read buffer.
 *
 * (2) num_Saved >= num_Needed; in this case all needed
 * bytes are in the save buffer and we will not need to copy any extra
 * bytes from the read buffer into the save buffer.
 *
 * (3) 0 < num_Saved < num_Needed; so some bytes are in
 * the save buffer and others are in the read buffer.  In this case we
 * need to copy some of the bytes from the read buffer to the save buffer
 * to get as many bytes as we need, then use these bytes.  First determine
 * the number of bytes we need to transfer; then transfer them and remove
 * them from the read buffer.  (There may be additional requests in the
 * read buffer - we'll deal with them next cycle.)
 *
 * At this stage, we have a pointer to a contiguous block of
 * num_Needed bytes that we should process.  The type of
 * processing depends upon the state we are in, given in the
 * ByteProcessing field of the FDDescriptor structure pointed to by
 * read_fdd.  The processing routine returns the number of bytes that it
 * actually used.
 *
 * The number of bytes that were actually used is normally (but not
 * always) the number of bytes needed.	Discard the bytes that were
 * actually used, not the bytes that were needed.  The number of used
 * bytes must be less than or equal to the number of needed bytes.  If
 * there were no saved bytes, then the bytes that were used must have been
 * in the read buffer so just modify the buffer pointer.  Otherwise call
 * RemoveSavedBytes.
 *
 * After leaving the loop, if there are still some bytes left in the read
 * buffer append the newly read bytes to the save buffer.
 *
 * Return False if we reached end-of-file on read.
 */

static Bool
ReadAndProcessData(private_data, read_fdd, write_fdd, is_server)
    Pointer		    private_data;
    FDDescriptor	    *read_fdd;
    FDDescriptor	    *write_fdd;
    Bool		    is_server;
{
    Buffer		    *inbuffer = &read_fdd->inBuffer;
    Buffer		    *outbuffer = &write_fdd->outBuffer;
    int			    fd = read_fdd->fd;
    char		    read_buf_block[16384];
    char		    *read_buf = read_buf_block;
    int			    num_read;
    char		    *process_buf;
    int			    NumberofUsedBytes;
    int			    BytesToSave;

    num_read = read(fd, read_buf, 16384);
    if (ShowPacketSize)
    {
	if (is_server)
	    fprintf(stdout, "\t\t\t\t\t");
	else
	    fprintf(stdout, "Client %d --> ", fd);
	fprintf(stdout, "%4d byte%s", num_read, (num_read == 1) ? "" : "s");
	if (is_server)
	    fprintf(stdout, "<-- Server %d\n", fd);
	else
	    fprintf(stdout, "\n");
	fflush(stdout);
    }
    if (num_read < 0)
    {
	perror("read error");
	if (errno == EWOULDBLOCK)
	    panic("ReadAndProcessData: read would block (not implemented)");
	if (errno == EINTR)
	    panic("ReadAndProcessData: read interrupted (not implemented)");
    }
    if (num_read == 0)
    {
	if (ErrorVerbose > 1)
	{
	    if (inbuffer->num_Saved == 0)
		fprintf(stdout, "EOF. No unsent bytes in incoming buffer.\n");
	    else
	    {
		fprintf(stdout, "EOF. Unsent bytes in incoming buffer:\n");
		DumpItem
		(
		    "Server bytes", fd, inbuffer->data,
		    inbuffer->num_Saved
		);
	    }
	    fflush(stdout);
	}
	return False;
    }

    if (RawBytesOnly)
    {
	if (PrintRawBytes)
	    DumpItem(is_server ? "Server" : "Client", fd, read_buf, num_read);
	SaveBytes(outbuffer, read_buf, num_read);
	return True;
    }

    littleEndian = read_fdd->littleEndian;
    while (inbuffer->num_Saved + num_read >= inbuffer->num_Needed)
    {
	if (inbuffer->num_Saved == 0)
	    process_buf = read_buf;
	else
	{
	    if (inbuffer->num_Saved < inbuffer->num_Needed)
	    {
		BytesToSave = inbuffer->num_Needed - inbuffer->num_Saved;
		SaveBytes(inbuffer, read_buf, BytesToSave);
		read_buf += BytesToSave;
		num_read -= BytesToSave;
	    }
	    process_buf = inbuffer->data;
	}
	ignore_bytes = False;
	NumberofUsedBytes = (*read_fdd->ByteProcessing)
	(
	    private_data, process_buf, inbuffer->num_Needed
	);
	if (NumberofUsedBytes > 0)
	{
	    if (!ignore_bytes)
		SaveBytes(outbuffer, process_buf, NumberofUsedBytes);
	    if (inbuffer->num_Saved > 0)
		RemoveSavedBytes(inbuffer, NumberofUsedBytes);
	    else
	    {
		read_buf += NumberofUsedBytes;
		num_read -= NumberofUsedBytes;
	    }
	}
    }
    if (num_read > 0)
	SaveBytes(inbuffer, read_buf, num_read);
    return True;
}

/*
 * We will need to save bytes until we get a complete request to
 * interpret.  The following procedures provide this ability.
 */

Global void
SaveBytes(buffer, buf, n)
    Buffer		    *buffer;
    char		    *buf;
    long		    n;
{
    long		    new_size;
    char		    *new_buf;

    /* check if there is enough space to hold the bytes we want */
    if (buffer->num_Saved + n > buffer->BlockSize)
	{
	    /* not enough room so far; malloc more space and copy */
	    new_size = (buffer->num_Saved + n + 1);
	    new_buf = (char *)malloc(new_size);
	    bcopy(buffer->data, new_buf, buffer->BlockSize);
	    if (buffer->BlockSize > 0)
		Tfree(buffer->data);
	    buffer->data = new_buf;
	    buffer->BlockSize = new_size;
	}

    /* now copy the new bytes onto the end of the old bytes */
    bcopy(buf, (buffer->data + buffer->num_Saved), n);
    buffer->num_Saved += n;
}

static void
RemoveSavedBytes(buffer, n)
    Buffer		    *buffer;
    int			    n;
{
    /* check if all bytes are being removed -- easiest case */
    if (buffer->num_Saved <= n)
	buffer->num_Saved = 0;
    else if (n != 0)
    {
	/* not all bytes are being removed -- shift the remaining ones down  */
	register char  *p = buffer->data;
	register char  *q = buffer->data + n;
	register int   i = buffer->num_Saved - n;
	while (i-- > 0)
	    *p++ = *q++;
	buffer->num_Saved -= n;
    }
}

static int
ConnectToClient(ConnectionSocket)
	int ConnectionSocket;
{
#ifndef SVR4
    int			  ON = 1;	  /* used in ioctl */
#endif
    int ClientFD;
    struct sockaddr_in	from;

    /* test 
    struct sockaddr name;
    int namelen = sizeof(name);
    int i;
    */

    int	   len = sizeof (from);

    ClientFD = accept(ConnectionSocket, (struct sockaddr *)&from, &len);

    /* test
    getpeername(ClientFD, &name, &namelen);
    printf ("len is %d\n", namelen);
    for (i = 0; i < namelen; i++)
	printf("%02x ", ((unsigned char *)&name)[i]);
    printf("\n");
    exit(0);
    */

    debug(4,(stderr, "Connect To Client: FD %d\n", ClientFD));
    if (ClientFD < 0 && errno == EWOULDBLOCK)
	{
	    debug(4,(stderr, "Almost blocked accepting FD %d\n", ClientFD));
	    panic("Can't connect to Client");
	}
    if (ClientFD < 0)
	{
	    debug(4,(stderr, "ConnectToClient: error %d\n", errno));
	    panic("Can't connect to Client");
	}

#ifdef SYSV
    fcntl(ClientFD, F_SETFD, FD_CLOEXEC);
#else
    ioctl(ClientFD, FIOCLEX, 0);
#endif
#ifndef SVR4
    ioctl(ClientFD, FIONBIO, &ON);
#endif
    return(ClientFD);
}

static int
ConnectToServer(hostName)
    char *hostName;
{
    int ServerFD;
    struct sockaddr_in	sin;
    struct hostent *hp;
    unsigned long hostinetaddr;
    int yes = 1;

    enterprocedure("ConnectToServer");

    /* establish a socket to the name server for this host */
    bzero((char *)&sin, sizeof(sin));
    ServerFD = socket(AF_INET, SOCK_STREAM, 0);
    if (ServerFD < 0)
	{
	    perror("socket() to Server failed");
	    debug(1,(stderr, "socket failed\n"));
	    panic("Can't open connection to Server");
	}
    (void) setsockopt(ServerFD, SOL_SOCKET, SO_REUSEADDR,
	(char*)&yes, sizeof(yes));
#ifdef SO_USELOOPBACK
    (void) setsockopt(ServerFD, SOL_SOCKET, SO_USELOOPBACK,
	(char*)&yes, sizeof(yes));
#endif
#ifdef SO_DONTLINGER
    (void) setsockopt(ServerFD, SOL_SOCKET, SO_DONTLINGER,
	(char*)&yes, sizeof(yes));
#endif

    debug(4,(stderr, "try to connect on %s\n", hostName));

    if (isdigit(hostName[0]))
    {
	hostinetaddr = inet_addr(hostName);
	if (hostinetaddr == (unsigned long)-1)
	{
	    fprintf(stdout, "ConnectToServer: hostinetaddr is -1\n");
	    fflush(stdout);
	}
    }
    else
	hostinetaddr = -1;
    if (hostinetaddr == (unsigned long)-1)
    {
	if ((hp = gethostbyname(hostName)) == 0)
	{
	    perror("gethostbyname failed");
	    debug(1,(stderr, "gethostbyname failed for %s\n", hostName));
	    panic("Can't open connection to Server");
	}
	if (hp->h_addrtype != AF_INET)
	{
	    perror("gethostbyname failed (not INET)");
	    debug(1,(stderr, "gethostbyname failed for %s\n", hostName));
	    panic("Can't open connection to Server");
	}
	sin.sin_family = hp->h_addrtype;
	bcopy((char *)hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
    }
    else
    {
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = hostinetaddr;
    }

    sin.sin_port = htons (GetServerport());

    /* ******************************************************** */
    /* try to connect to Server */

    if (connect(ServerFD, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
	    debug(4,(stderr, "connect returns errno of %d\n", errno));
	    if (errno != 0)
		perror("connect");
	    switch (errno)
		{
		case ECONNREFUSED:
		    /* experience says this is because there is no Server
			to connect to */
		    close(ServerFD);
		    debug(1,(stderr, "No Server\n"));
		    panic("Can't open connection to Server (ECONNREFUSED)");
		default:
		    close(ServerFD);
		    fprintf(stderr, "errno = %d: ", errno);
		    panic("Can't open connection to Server");
		}
	}

    debug(4,(stderr, "Connect To Server: FD %d\n", ServerFD));
    return(ServerFD);
}

static char*
OfficialName(name)
char *name;
{
    struct hostent *HostEntry;

    HostEntry = gethostbyname(name);
    if (HostEntry == NULL)
	return(name);
    else
	return(HostEntry->h_name);
}

static void
SignalINT(n)
	int n;
{
    debug(1,(stderr, "==> SIGINT received\n"));
    (void) fflush(stdout);
    exit(1);
}

static void
SignalTERM(n)
	int n;
{
    debug(1,(stderr, "==> SIGTERM received\n"));
    (void) fflush(stdout);
    exit(1);
}

Global void
enterprocedure(s)
	char   *s;
{
    debug(2,(stderr, "-> %s\n", s));
}

Global void
panic(s)
	char   *s;
{
    fprintf(stderr, "%s\n", s);
    exit(1);
}

#if (mskcnt>4)
static Bool
ANYSET(src)
long *src;
{
    int cri;

    for (cri = 0; cri < mskcnt; cri++)
	if (src[cri])
	    return 1;
    return 0;
}
#endif
