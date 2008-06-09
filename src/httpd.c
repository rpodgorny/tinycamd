
#include <pthread.h>
#include <sys/types.h>
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

#include "httpd.h"
#include "logging.h"

#define MAX_HTTPD_CONNECTIONS 16
#define MAX_HTTPD_TIMEOUT 10
#define MAX_HTTPD_LISTEN_BACKLOG 24


typedef void *(*Pfunc)(void *);  // make the pthread_create calls tidier

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
    pthread_mutex_t killer;
    pthread_t thread;
    pthread_t watchdog;
    struct sockaddr_in remote_addr;
    int protocol;    // 0x10 = 1.0, 0x11 = 1.1
    int socket;
    int sentStatus;
    void (*func)(HTTPD_Request req, const char *method, const char *url);
};

//
// This the the request cleanup function. It could be called from either request()'s thread or
// the request_watchdog() thread. The req->kill mutex ensures that only one gets into the goodies.
// The other is killed and joined before the cleanup begins.
//
static void cleanup_request( HTTPD_Request req)
{
    if ( pthread_mutex_lock( &req->killer) == 0) {
	pthread_t other = ( req->watchdog != pthread_self() ? req->watchdog : req->thread);
	void *value;
	int e;

	e = pthread_cancel( other);
	if ( e == 0) pthread_join( other, &value);
	else {
	  log_f("httpd request cancel failed\n");
	}

	shutdown( req->socket,SHUT_RDWR);
	close( req->socket);
	
	sem_post( &req->httpd->counter);
	
	errno = 0;
	if ( pthread_mutex_destroy( &req->killer) && errno != 0) {
	  log_f("Failed to destroy mutex: %s\n", strerror(errno));
	}
	free(req);

	pthread_detach( pthread_self());
    } else {
	// there is a tiny possibility of this happenening, so not really an error, but
	// if it happens all the time you have a bug.
      log_f("A killer failed\n");
    }
}

//
// This is the watchdog thread. THere is one for each request thread.
// 
static void *request_watchdog( HTTPD_Request req)
{
    pthread_detach( pthread_self());
    sleep(MAX_HTTPD_TIMEOUT);
    cleanup_request(req);
    return NULL;
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
// There is one of these threads for each http connection.
// It reads the request dispatches it, and cleans up afterward.
//
static void *request( HTTPD_Request req)
{
    char line[10240];
    char url[8192];
    char method[32], protocol[32];

    if ( pthread_create( &req->watchdog, NULL, (Pfunc)request_watchdog, req) != 0) {
      log_f("Failed to create HTTPD request watchdog: %s\n", strerror(errno));
	cleanup_request(req);
	return NULL;
    }

    //
    // Get the Request
    //
    if ( !sgets(line,sizeof(line)-1, req->socket)) {
        log_f("Failed to read request line\n");
	goto Die;
    }
    if ( sscanf( line, "%31s %8191s %31s\n", method, url, protocol) < 2) {
        log_f("Illegal request: %s\n", line);
	goto Die;
    }
    if ( strcmp(protocol,"HTTP/1.1")==0) req->protocol = 0x11;
    else req->protocol = 0x10;
    
    //
    // Get the headers
    //
    for (;;) {
	if ( !sgets(line,sizeof(line)-1, req->socket)) {
	  log_f("Failed to read request line\n");
	    goto Die;
	}
	if ( line[0] == '\n' || line[0] == '\r') break;
	//log_f("Header: %s", line);
    }
    
    (req->func)(req, method, url);
    
    if ( 0) {
	int cork = 0; 
	if ( setsockopt(req->socket, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork))) {
	  log_f("Failed to un-TCP_CORK for HTTPD: %s\n", strerror(errno));
	}
    }
    
  Die:
    cleanup_request(req);

    return 0;
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

	{
	    int off = 0;
	    if (setsockopt(ns, IPPROTO_TCP, TCP_QUICKACK, &off, sizeof(off)) < 0) {
	      log_f("Failed to clear TCP_QUICKACK for HTTPD: %s\n", strerror(errno));
	    }
	}
	{
	    int cork = 1; 
	    if ( setsockopt(ns, IPPROTO_TCP, TCP_CORK, &cork, sizeof(cork))) {
	      log_f("Failed to set TCP_CORK for HTTPD: %s\n", strerror(errno));
	    }
	}

	r = calloc( sizeof(*r), 1);
	memcpy( &r->remote_addr, &addr, sizeof(r->remote_addr));
	r->httpd = httpd;
	r->socket = ns;
	r->func = httpd->func;
	pthread_mutex_init( &r->killer, NULL);

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
    if ( req->protocol >= 0x11) {
      snprintf( buf, sizeof(buf)-1, "HTTP/1.1 %3d %s\r\nConnection: close\r\n", status, text);
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

