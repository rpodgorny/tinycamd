
#include <pthread.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <semaphore.h>
#include <signal.h>
#include <time.h>

#include "httpd.h"
#include "logging.h"

#define MAX_HTTPD_CONNECTIONS 16
#define MAX_HTTPD_TIMEOUT 10
#define MAX_HTTPD_LISTEN_BACKLOG 24


typedef void *(*Pfunc)(void *);  // make the pthread_create calls tidier

static const int zero = 0;
static const int one = 1;

struct httpd {
    pthread_t thread;
    sem_t counter;
    struct addrinfo *bindAddr;
    const char *bindName;
    void (*func)(HTTPD_Request req, const char *method, const char *url);
    int sock;
};

struct http_request {
    struct httpd *httpd;
    pthread_t thread;
    timer_t watchdog;
    struct sockaddr_in remote_addr;
    int protocol;    // 0x10 = 1.0, 0x11 = 1.1
    int socket;
    int sentStatus;
    void (*func)(HTTPD_Request req, const char *method, const char *url);
};

const int noKeepAlive = 0;


static int set_deadline( HTTPD_Request req, unsigned int seconds) {
    struct itimerspec its = { .it_value = { .tv_sec = seconds } };
    if ( timer_settime( req->watchdog, 0, &its, 0) == -1) {
	log_f("Failed timer_settime in set_deadline: %s\n", strerror(errno));
	return 0;
    }
    return 1;
}
//
// Fired by the watchdog to kill an idle or hung thread.
// This runs in a different thread from the request.
//
static void expire_request( union sigval arg) 
{
    HTTPD_Request req = (HTTPD_Request)(arg.sival_ptr);

    //
    // DANGER!!! There is a race here. We could have fired and gotten to here
    //           at the same time request could have exitted and req is released
    //           and req->thread is dead.
    // FIX THIS!!!
    //
    if ( pthread_cancel( req->thread) != 0) {
	log_f("Watchdog failed canceling request thread: %s\n", strerror(errno));
    }
}
//
// Wherein we start the watchdog thread. This is insanely verbose because
// pthreads condition variables are ridiculous, but we have to make sure
// the watchdog is good and going before we come back, or we can handle
// the request and get to the shutdown code before the watchdog has started
// and that hangs.
//
static int start_watchdog( HTTPD_Request req)
{
    struct sigevent ev = { .sigev_notify = SIGEV_THREAD,
			   .sigev_notify_function = expire_request,
			   .sigev_value = { .sival_ptr = (void *)req },
    };

    if ( timer_create( CLOCK_MONOTONIC, &ev, &req->watchdog) != 0) {
	log_f("Failed to create watchdog for request thread: %s\n", strerror(errno));
	return 0;
    }
    return set_deadline( req, MAX_HTTPD_TIMEOUT);
}
static void stop_watchdog( HTTPD_Request req)
{
    if ( timer_delete( req->watchdog) != 0) {
	log_f("Failed to stop watchdog: %s\n", strerror(errno));
    }
}

//
// This the the request cleanup function. It could be called from either request()'s thread or
// the request_watchdog() thread. The req->kill mutex ensures that only one gets into the goodies.
// The other is killed and joined before the cleanup begins.
//
static void cleanup_request( void *arg)
{
    HTTPD_Request req = (HTTPD_Request)arg;

    stop_watchdog(req);

    log_f("Shutting down sockets\n");
    
    shutdown( req->socket,SHUT_RDWR);
    close( req->socket);
	
    sem_post( &req->httpd->counter);
}



//
// Sort of an fgets() for a socket. I had originally used fdopen() and fgets, but
// there is some evil interraction when you pthread_cancel() a thread that is inside
// an fgets. You can't then fclose() the FILE * from the other thread.
// This is simple, but a lot of system calls. Maybe I should make a 1500 byte buffer.
//
static char *sgets(char *sbuf, int size, int sock)
{
    int togo = size;
    char *s = sbuf;

    errno = 0;
    while( togo >1) {
	int e;
	do {
	    e = recv( sock, s, 1, 0);
	    if ( e == -1 && errno == EINTR) continue;
	} while(0);
	
	if ( e == 0) {
	    if ( s == sbuf) return 0;  // eof and no characters, return a 0
	    break;
	}
	if ( e == -1) return 0;
        s++;
	togo--;
	if ( s[-1] == '\n') break;
    }
    *s++ = 0;
    return sbuf;
}


//
// The main request loop, one per connection.
// This is wrapped in cleanup code by request().
//
static void *request_loop( HTTPD_Request req)
{
    char line[10240];
    char url[8192];
    char method[32], protocol[32];

    if ( !start_watchdog(req)) return NULL;

    do {
	//
	// Reset in case we are on a keep alive connection
	//
	req->sentStatus = 0;

	//
	// Get the Request
	//
	if ( !sgets(line,sizeof(line)-1, req->socket)) {
	    log_f("Failed to read request line: %s\n", strerror(errno));
	    return NULL;
	}
	if ( sscanf( line, "%31s %8191s %31s\n", method, url, protocol) < 2) {
	    log_f("Illegal request: %s\n", line);
	    return NULL;
	}
	if ( strcmp(protocol,"HTTP/1.1")==0) req->protocol = 0x11;
	else req->protocol = 0x10;
    
	//
	// Get the headers
	//
	for (;;) {
	    if ( !sgets(line,sizeof(line)-1, req->socket)) {
		log_f("Failed to read request line\n");
		return NULL;
	    }
	    if ( line[0] == '\n' || line[0] == '\r') break;
	    //log_f("Header: %s", line);
	}
    
	(req->func)(req, method, url);
    
	if ( 1) {
	    if ( setsockopt(req->socket, IPPROTO_TCP, TCP_CORK, &zero, sizeof(zero))) {
		log_f("Failed to un-TCP_CORK for HTTPD: %s\n", strerror(errno));
	    }
	    if ( setsockopt(req->socket, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one))) {
		log_f("Failed to TCP_NODELAY for HTTPD: %s\n", strerror(errno));
	    }
	    if ( setsockopt(req->socket, IPPROTO_TCP, TCP_CORK, &one, sizeof(one))) {
		log_f("Failed to TCP_CORK for HTTPD: %s\n", strerror(errno));
	    }
	}

	set_deadline( req, MAX_HTTPD_TIMEOUT);
    } while ( req->protocol == 0x11 && !noKeepAlive);

    log_f("Ending request thread.\n");

    return NULL;
}

//
// There is one of these threads for each http connection.
// It reads the request dispatches it, and cleans up afterward.
//
static void *request( HTTPD_Request req)
{
    void *res;

    pthread_detach( pthread_self());

    pthread_cleanup_push( cleanup_request, (void *)req);
    res = request_loop(req);
    pthread_cleanup_pop( 1);
    
    free(req);

    return NULL;
}

//
// There is one of these threads per daemon. It listens to the port, accepts connections,
// and starts 'request' threads to handle them.
//
// It really should keep track of how many requests are going and not make too many.
//
static void *listener( struct httpd *httpd)
{
  log_f("Starting listener on %s...\n", httpd->bindName);

    if ( sem_init( &httpd->counter, 0, MAX_HTTPD_CONNECTIONS) == -1) {
      log_f("Failed to initialize HTTPD semaphore: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }

    httpd->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (httpd->sock == -1) {
      log_f("Failed to create socket for HTTPD: %s\n", strerror(errno));
	exit(EXIT_FAILURE);
    }

    // This tells us to bind even if there are sockets laying around in the TIME_WAIT state.
    // It is what lets us stop the server and restart immediately without hanging around 30 seconds.
    {
	int on = 1;
	if (setsockopt(httpd->sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
	  log_f("Failed to set SO_REUSEADDR for HTTPD: %s\n", strerror(errno));
	}
    }

    if (bind(httpd->sock, httpd->bindAddr->ai_addr, httpd->bindAddr->ai_addrlen) == -1) {
      log_f("Failed to bind to %s for HTTPD: %s\n", httpd->bindName, strerror(errno));
	exit(EXIT_FAILURE);
    }

    if (listen(httpd->sock,MAX_HTTPD_LISTEN_BACKLOG) == -1) {
      log_f("Failed to listen to port %s for HTTPD: %s\n", httpd->bindName, strerror(errno));
	exit(EXIT_FAILURE);
    }

    for (;;) {
	struct sockaddr_in addr = { .sin_family = AF_INET };
	socklen_t addrlen = sizeof(addr);
	struct http_request *r;
	int ns;

	//
	// Reserve a thread from the counter
	//
	do {
	    int s = sem_wait( &httpd->counter);
	    if ( s == -1 && errno == EINTR) continue;
	    if ( s == -1) {
	      log_f("Failed in HTTPD sem_wait: %s\n", strerror(errno));
		exit(EXIT_FAILURE);
	    }
	} while(0);

	do {
	    ns = accept( httpd->sock, (struct sockaddr *)&addr, &addrlen);
	    if ( ns == -1 && errno == EINTR) continue;
	} while(0);

	if ( ns == -1) {
	  log_f("Failed while accepting connections on %s for HTTPD: %s\n", httpd->bindName, strerror(errno));
	    exit(EXIT_FAILURE);
	}

	if ( 0) {
	    int v;
	    sem_getvalue( &httpd->counter, &v);
	    log_f("httpd threads left = %d\n", v);
	}

	#if 0
	if (setsockopt(ns, IPPROTO_TCP, TCP_QUICKACK, &zero, sizeof(zero)) < 0) {
	    log_f("Failed to clear TCP_QUICKACK for HTTPD: %s\n", strerror(errno));
	}
	#endif
	if ( setsockopt(ns, IPPROTO_TCP, TCP_CORK, &one, sizeof(one))) {
	    log_f("Failed to set TCP_CORK for HTTPD: %s\n", strerror(errno));
	}

	r = calloc( sizeof(*r), 1);
	memcpy( &r->remote_addr, &addr, sizeof(r->remote_addr));
	r->httpd = httpd;
	r->socket = ns;
	r->func = httpd->func;

	if ( pthread_create( &r->thread, NULL, (Pfunc)request, r) != 0) {
	  log_f("Failed to create thread for request on HTTPD %s: %s", httpd->bindName, strerror(errno));
	    close(ns);
	    free(r);
	}
    }

    return 0;
}

pthread_t HTTPD_Start( const char *bindPort, void (*func)(HTTPD_Request req, const char *method, const char *url) )
{
    char node[256]="",serv[256]="";
    struct httpd *h = calloc(sizeof(struct httpd),1);
    h->func = func;
    h->bindName = bindPort;
    
    if ( strchr( bindPort, ':')) sscanf( bindPort, "%255[^:]:%255s",node,serv); 
    else sscanf( bindPort, "%255s", serv);

    {
	int r;
	struct addrinfo hints = { .ai_family = AF_INET,
				  .ai_socktype = SOCK_STREAM,
				  .ai_flags = AI_PASSIVE, };
	if ( (r = getaddrinfo( node[0]?node:NULL, serv, &hints, &h->bindAddr)) ) {
	  log_f("HTTPD_Start getaddrinfo failed (%s:%s): %s\n", node,serv,gai_strerror(r));
	    exit(1);
	}
    }

    if ( pthread_create( &h->thread, NULL, (Pfunc)listener, h)) {
      log_f("Failed to start HTTPD thread: %s", strerror(errno));
	exit(1);
    }
    return h->thread;
}

static int Send_Buffer( HTTPD_Request req, const void *buf, int len)
{
    int togo = len;
    const void *b = buf;

    while( togo > 0) {
	int c = send( req->socket, b, togo, MSG_NOSIGNAL);
	if ( c == -1 && errno == EINTR) continue;
	if ( c == -1) {
	  log_f("Error sending on HTTPD: %s\n", strerror(errno));
	    return 0;
	}
	togo -= c;
	b += c;
    }
    return 1;
}

void HTTPD_Send_Status(HTTPD_Request req, int status, const char *text)
{
    char buf[1024];

    if ( req->sentStatus) return;
    if ( req->protocol >= 0x11 ) {
	if ( noKeepAlive) {
	    snprintf( buf, sizeof(buf)-1, "HTTP/1.1 %3d %s\r\nConnection: close\r\n", status, text);
	} else {
	    snprintf( buf, sizeof(buf)-1, "HTTP/1.1 %3d %s\r\n", status, text);
	}
    } else {
	snprintf( buf, sizeof(buf)-1, "HTTP/1.0 %3d \r\n", status);
    }


    Send_Buffer( req, buf, strlen(buf));
    req->sentStatus = 1;
}

void HTTPD_Add_Header(HTTPD_Request req, const char *h)
{
  if ( !req->sentStatus) HTTPD_Send_Status(req,200,"OK");

    Send_Buffer(req, h, strlen(h));
    Send_Buffer(req, "\r\n", 2);
}

void HTTPD_Send_Body(HTTPD_Request req, const void *data, int length)
{
    char buf[1024];
    
    if ( !req->sentStatus) HTTPD_Send_Status(req,200,"OK");

    snprintf( buf, sizeof(buf)-1, "Content-length: %d\r\n\r\n", length);
    Send_Buffer( req, buf, strlen(buf));

    Send_Buffer(req, data, length);
}

