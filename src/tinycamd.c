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
#include <jpeglib.h>
#include <pwd.h>

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

  HTTPD_Add_Header(req, "Cache-Control: no-cache");
  HTTPD_Add_Header(req, "Pragma: no-cache");
  HTTPD_Add_Header(req, "Expires: Thu, 01 Dec 1994 16:00:00 GMT");
  HTTPD_Add_Header(req, "Content-type: image/jpeg");

  switch(camera_method) {
    case CAMERA_METHOD_MJPEG:
    case CAMERA_METHOD_JPEG:
	{
	    unsigned char *buffer, *b;
	    for ( i = 0; c[i].data != 0; i++) s += c[i].length;
	    buffer = malloc( s);
	    for ( i = 0, b = buffer; c[i].data != 0; i++) {
		memcpy( b, c[i].data, c[i].length);
		b += c[i].length;
	    }
	    HTTPD_Send_Body( req, buffer, s);
	    free(buffer);
	}
      break;
    case CAMERA_METHOD_YUYV:
	{
	    unsigned char *jpegBuffer;
	    unsigned int jpegLeft = 1024*1024;
	    unsigned int jpegSize = 0;
	    struct jpeg_compress_struct cinfo = { .dest = 0};
	    struct jpeg_destination_mgr dmgr;
	    struct jpeg_error_mgr err;

	    jpegBuffer = malloc(jpegLeft);
	    if ( !jpegBuffer) fatal_f("Failed to allocate JPEG encoding buffer.\n");

	    void init_destination(j_compress_ptr cinfo) {
		struct jpeg_destination_mgr *d = cinfo->dest;
		d->next_output_byte = jpegBuffer;
		d->free_in_buffer = jpegLeft;
	    }
	    int empty_output_buffer(j_compress_ptr cinfo) {
		//struct jpeg_destination_mgr *d = cinfo->dest;
		log_f("eob\n");
		return  TRUE;
	    }
	    void term_destination(j_compress_ptr cinfo) {
		struct jpeg_destination_mgr *d = cinfo->dest;
		log_f("termdest\n");
		jpegSize = d->next_output_byte - jpegBuffer;
	    }
	    dmgr.init_destination = init_destination;
	    dmgr.empty_output_buffer = empty_output_buffer;
	    dmgr.term_destination = term_destination;

	    cinfo.err = jpeg_std_error(&err);
	    jpeg_create_compress(&cinfo);
	    cinfo.image_width = video_width;
	    cinfo.image_height = video_height;
	    if ( mono) {
		cinfo.input_components = 1;
		cinfo.in_color_space = JCS_GRAYSCALE;
	    } else {
		cinfo.input_components = 3;
		cinfo.in_color_space = JCS_YCbCr;
	    }
	    jpeg_set_defaults(&cinfo);
	    jpeg_set_quality(&cinfo, quality, TRUE);
	    cinfo.dest = &dmgr;

	    jpeg_start_compress( &cinfo, TRUE);
	    {
		const unsigned char *b = c[0].data;
		int row = 0;
		int col = 0;
		JSAMPLE pix[video_width*3];
		JSAMPROW rows[] = { pix};
		JSAMPARRAY scanlines = rows;

		for ( row = 0; row < video_height; row++) {
		    JSAMPLE *p = pix;
		    for ( col = 0; col < video_width; col+=2) {
			*p++ = b[0];
			if ( !mono) {
			    *p++ = b[1];
			    *p++ = b[3];
			}
			*p++ = b[2];
			if ( !mono) {
			    *p++ = b[1];
			    *p++ = b[3];
			}
			b += 4;
		    }
		    jpeg_write_scanlines( &cinfo, scanlines, 1);
		}
	    }
	    jpeg_finish_compress( &cinfo);

	    HTTPD_Send_Body( req, jpegBuffer, jpegSize);
	    free(jpegBuffer);
	}
      break;
  }

  log_f("image size = %d\n",s);
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

static void handle_requests(HTTPD_Request req, const char *method, const char *rawUrl)
{
  int cid,val;
  const char *url = rawUrl;

  if ( strncmp( rawUrl, url_prefix, strlen(url_prefix))) {
      url = "***BADURL-NOPREFIX***";
  } else {
      url = rawUrl + strlen(url_prefix);
  }

  log_f("Request: %s %s => %s\n", method, rawUrl, url);
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

static void *sleeper(void *arg)
{
  while(1) sleep(1000);
  return 0;
}

int main(int argc, char **argv)
{
    pthread_t captureThread;
    pthread_t httpdThread;

    do_options(argc, argv);

    if ( daemon_mode) {
      if ( daemon(0,0) == -1) {
	fatal_f("Failed to become a daemon: %s\n", strerror(errno));
      }
    }

    if ( pid_file) {
	FILE *pf = fopen( pid_file,"w");
	if ( !pf) fatal_f("Failed to open pid file %s: %s\n", pid_file, strerror(errno));
	fprintf(pf, "%d\n", getpid());
	if ( fclose(pf)==EOF) fatal_f("Failed to close pid file %s: %s\n", pid_file, strerror(errno));
    }

    open_device();

    if ( probe_only) {
	probe_device();
	return 0;
    }

    init_device();
    start_capturing();

    pthread_create( &captureThread, NULL, main_loop, NULL);

    /*
    ** I am so sorry. But glibc dynamically loads libgcc_s.so.1 to handle pthread_cancel, so
    ** I need to get that in before chrooting.
    */
    {
      pthread_t crappyHack;
      void *whatever;

      if ( pthread_create( &crappyHack, 0, sleeper, 0)) fatal_f("Failed to start test thread.\n");
      if ( pthread_cancel( crappyHack)) fatal_f("Failed to cancel test thread.\n");
      if ( pthread_join( crappyHack, &whatever)) fatal_f("Failed to join test thread.\n");
    }

    /*
    ** Slink into our ghetto and lower our privileges in preparation for handling queries.
    */
    {
        uid_t uid = getuid();
	
        /* lookup uid before we chroot... or it is gone. */
        if ( setuid_to) {
            struct passwd *pw = getpwnam(setuid_to);
            if ( !pw) fatal_f("Failed to lookup user `%s' for setuid: %s\n", setuid_to, strerror(errno));
            uid = pw->pw_uid;
        }

        if ( chroot_to) {
            if ( chroot( chroot_to)) fatal_f("Failed to chroot to `%s': %s\n", chroot_to, strerror(errno));
        }

        if ( setuid_to) {
            if ( setreuid(uid, uid)) fatal_f("Failed to setuid to `%s': %s\n", setuid_to, strerror(errno));
        }
    }

    httpdThread = HTTPD_Start( bind_name, handle_requests);

    for(;;) sleep(100);

    close_device();

    return 0;
}
