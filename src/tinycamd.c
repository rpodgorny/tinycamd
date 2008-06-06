/*
 * threaded.c -- A simple multi-threaded HTTPD application.
 */

#include <pthread.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <stdio.h>
#include <errno.h>

#include "tinycamd.h"
#include "httpd.h"

#define MAXFRAME 60

static void do_status_request( HTTPD_Request req)
{
  const char *status = "<html><head><title>Status</title></head>"
    "<body>status</body>"
    "</html>";

  HTTPD_Add_Header( req, "Content-type: text/html");
  HTTPD_Send_Body(req, status,strlen(status));
}

#if 0
static void put_image(const struct chunk *c, void *arg)
{
  HTTPD_Request req = (HTTPD_Request)arg;
  int i,s=0;

  for ( i = 0; c[i].data != 0; i++) s += c[i].length;

    FCGX_FPrintF(req->out, 
		 "Content-type: image/jpeg\r\n"
		 "Content-length: %d\r\n"
		 "\r\n", s);
    for ( i = 0; c[i].data != 0; i++) {
	FCGX_PutStr( c[i].data, c[i].length, req->out);
    }
    fprintf(stderr,"image size = %d\n",s);
}
#endif

static void put_single_image(const struct chunk *c, void *arg)
{
  HTTPD_Request req = (HTTPD_Request)arg;
  int i,s=0;
  unsigned char *buffer, *b;

  HTTPD_Add_Header(req, "Cache-Control: no-cache");
  HTTPD_Add_Header(req, "Pragma: no-cache");
  HTTPD_Add_Header(req, "Expires: Thu, 01 Dec 1994 16:00:00 GMT");
  HTTPD_Add_Header(req, "Content-type: image/jpeg");

  for ( i = 0; c[i].data != 0; i++) s += c[i].length;
  buffer = malloc( s);
  for ( i = 0, b = buffer; c[i].data != 0; i++) {
    memcpy( b, c[i].data, c[i].length);
    b += c[i].length;
  }
  HTTPD_Send_Body( req, buffer, s);
  free(buffer);

  if ( verbose) fprintf(stderr,"image size = %d\n",s);
}

#if 0
static void stream_image( HTTPD_Request req)
{
    int i;
    const char *boundary = "--myboundary\r\n";

    HTTPD_Add_Header( req, "Cache-Control: no-cache");
    HTTPD_Add_Header( req, "Pragma: no-cache");
    HTTPD_Add_Header( req, "Expires: Thu, 01 Dec 1994 16:00:00 GMT");
    HTTPD_Add_Header( req, "Content-Type: text/xml");
    HTTPD_Add_Header( req, "Content-Type: multipart/x-mixed-replace; boundary=--myboundary");

    for(i = 0; i < MAXFRAME; i++) {
      HTTPD_Send_Body_Chunk( req, boundary, sizeof(boundary)-1);
      with_next_frame( &put_image, req);
      HTTPD_Push(req);
    }
}
#endif

static void do_video_call( HTTPD_Request req, video_action action, int cid, int val)
{
    char buf[8192];

    with_device( action, buf, sizeof(buf), cid, val);
    HTTPD_Add_Header( req, "Cache-Control: no-cache");
    HTTPD_Add_Header( req, "Pragma: no-cache");
    HTTPD_Add_Header( req, "Expires: Thu, 01 Dec 1994 16:00:00 GMT");
    HTTPD_Add_Header( req, "Content-Type: text/xml");
    HTTPD_Send_Body( req, buf, strlen(buf));
}

static void handle_requests(HTTPD_Request req, const char *method, const char *url)
{
  int cid,val;

  if ( verbose) fprintf(stderr,"Request: %s %s\n", method, url);
  if ( strcmp(url,"/status")==0) {
    do_status_request(req);
#if 0
  } else if ( strcmp(url,"/image.replace")==0) {
    stream_image(req);
#endif
  } else if ( strcmp(url,"/controls")==0) {
    do_video_call( req, list_controls,0,0);
  } else if ( sscanf(url,"/set?%d=%d",&cid,&val)==2 ) {
    do_video_call( req, set_control,cid,val);
  } else if ( strcmp(url,"/")==0 ||
	      strcmp( url, "/image.jpg") == 0 ||
	      strncmp( url, "/image.jpg?", 11) == 0) {
    with_current_frame( &put_single_image, req);
  } else {
    HTTPD_Send_Status( req, 404, "Not Found");
    HTTPD_Send_Body( req, "404 - Not found", 15);
  }
}

int main(int argc, char **argv)
{
    pthread_t captureThread;
    pthread_t httpdThread;

    do_options(argc, argv);

    if ( daemon_mode) {
      if ( daemon(0,0) != 0) {
	fprintf(stderr,"Failed to become a daemon: %s\n", strerror(errno));
	exit(1);
      }
    }

    open_device();
    init_device();
    start_capturing();

    pthread_create( &captureThread, NULL, main_loop, NULL);

    httpdThread = HTTPD_Start( bind_name, handle_requests);

    for(;;) sleep(100);

    close_device();

    return 0;
}
