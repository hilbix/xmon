/* list of commands sent from xmonui to xmond */

#define SELECTED_REQUEST_VERBOSE_STR	    "selected_request_verbose"
#define SELECTED_EVENT_VERBOSE_STR	    "selected_event_verbose"
#define REQUEST_VERBOSE_STR		    "request_verbose"
#define EVENT_VERBOSE_STR		    "event_verbose"
#define REPLY_VERBOSE_STR		    "reply_verbose"
#define ERROR_VERBOSE_STR		    "error_verbose"
#define MONITOR_REQUEST_BASE_STR	    "monitor_request_"
#define MONITOR_REQUEST_ON_STR		    "monitor_request_on"
#define MONITOR_REQUEST_OFF_STR		    "monitor_request_off"
#define MONITOR_EVENT_BASE_STR		    "monitor_event_"
#define MONITOR_EVENT_ON_STR		    "monitor_event_on"
#define MONITOR_EVENT_OFF_STR		    "monitor_event_off"
#define BLOCK_REQUEST_BASE_STR		    "block_request_"
#define BLOCK_REQUEST_ON_STR		    "block_request_on"
#define BLOCK_REQUEST_OFF_STR		    "block_request_off"
#define BLOCK_EVENT_BASE_STR		    "block_event_"
#define BLOCK_EVENT_ON_STR		    "block_event_on"
#define BLOCK_EVENT_OFF_STR		    "block_event_off"
#define START_REQUEST_COUNT_STR		    "startrequestcount"
#define STOP_REQUEST_COUNT_STR		    "stoprequestcount"
#define CLEAR_REQUEST_COUNT_STR		    "clearrequestcount"
#define PRINT_REQUEST_COUNT_STR		    "printrequestcount"
#define PRINT_ZERO_REQUEST_COUNT_STR	    "printzerorequestcount"
#define START_EVENT_COUNT_STR		    "starteventcount"
#define STOP_EVENT_COUNT_STR		    "stopeventcount"
#define CLEAR_EVENT_COUNT_STR		    "cleareventcount"
#define PRINT_EVENT_COUNT_STR		    "printeventcount"
#define PRINT_ZERO_EVENT_COUNT_STR	    "printzeroeventcount"
#define START_ERROR_COUNT_STR		    "starterrorcount"
#define STOP_ERROR_COUNT_STR		    "stoperrorcount"
#define CLEAR_ERROR_COUNT_STR		    "clearerrorcount"
#define PRINT_ERROR_COUNT_STR		    "printerrorcount"
#define PRINT_ZERO_ERROR_COUNT_STR	    "printzeroerrorcount"
#define REQUEST_MONITORING_ON_STR	    "request_monitoring_on"
#define REQUEST_MONITORING_OFF_STR	    "request_monitoring_off"
#define EVENT_MONITORING_ON_STR		    "event_monitoring_on"
#define EVENT_MONITORING_OFF_STR	    "event_monitoring_off"
#define REQUEST_BLOCKING_BASE_STR	    "request_blocking_"
#define REQUEST_BLOCKING_ON_STR		    "request_blocking_on"
#define REQUEST_BLOCKING_OFF_STR	    "request_blocking_off"
#define EVENT_BLOCKING_BASE_STR		    "event_blocking_"
#define EVENT_BLOCKING_ON_STR		    "event_blocking_on"
#define EVENT_BLOCKING_OFF_STR		    "event_blocking_off"
#define SHOW_PACKET_SIZE_BASE_STR	    "show_packet_size_"
#define SHOW_PACKET_SIZE_ON_STR		    "show_packet_size_on"
#define SHOW_PACKET_SIZE_OFF_STR	    "show_packet_size_off"
#ifdef RECORD_EVENTS
#define RECORD_EVENTS_ON_STR		    "record_events_on"
#define RECORD_EVENTS_OFF_STR		    "record_events_off"
#define PLAY_EVENT_STR			    "replay"
#endif
#define QUIT_STR			    "quit"
#define HELP_STR			    "help"