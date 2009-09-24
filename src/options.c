#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>

#include "tinycamd.h"

enum io_method io_method = IO_METHOD_MMAP;
enum camera_method camera_method = CAMERA_METHOD_JPEG;
char *videodev_name = "/dev/video0";
char *bind_name = "0.0.0.0:8080";
char *url_prefix = "";
int video_width = 640;
int video_height = 480;
int verbose = 0;
int quality = 100;
int fps = 5;
int daemon_mode = 0;
int probe_only = 0;
int mono = 0;

static const char short_options [] = "p:d:hmMruvq:s:f:DU:PF:";

static const struct option
long_options [] = {
        { "device",     required_argument,      NULL,           'd' },
        { "port",       required_argument,      NULL,           'p' },
	{ "daemon",     no_argument,            NULL,           'D' },
        { "help",       no_argument,            NULL,           'h' },
        { "mmap",       no_argument,            NULL,           'm' },
        { "read",       no_argument,            NULL,           'r' },
        { "userp",      no_argument,            NULL,           'u' },
	{ "size",       required_argument,      NULL,           's' },
	{ "verbose",    no_argument,            NULL,           'v' },
	{ "quality",    required_argument,      NULL,           'q' },
	{ "fps",        required_argument,      NULL,           'f' },
	{ "url-prefix", required_argument,      NULL,           'U' },
	{ "probe",      no_argument,            NULL,           'P' },
	{ "format",     required_argument,      NULL,           'F' },
	{ "monochrome", no_argument,            NULL,           'M' },
        { 0, 0, 0, 0 }
};

static void usage(FILE *fp, int argc, char **argv)
{
    fprintf (fp,
	     "Usage: %s [options]\n\n"
	     "Options:\n"
	     "-d | --device name       Video device name [/dev/video]\n"
	     "-p | --port [addr:]port  HTTP daemon port to bind (default: 8080)\n"
	     "-D | --daemon            Detach and run as a daemon\n"
	     "-U | --url-prefix        Static prefix to URL, e.g. /camera\n"
	     "-s | --size widxhgt      Size, e.g. 640x480\n"
	     "-f | --fps num           Frames per second\n"
	     "-q | --quality num       JPEG quality, 0-100\n"
	     "-F FMT | --format FMT    Camera image format, e.g. JPEG or YUYV\n"
	     "-M | --monochrome        Grayscale only, if possible\n"
	     "-h | --help              Print this message\n"
	     "-m | --mmap              Use memory mapped buffers\n"
	     "-r | --read              Use read() calls\n"
	     "-u | --userp             Use application allocated buffers\n"
	     "-v | --verbose           Print a lot of debug messages\n"
	     "-P | --probe             Probe the camera capabilities\n"
	     "",
	     argv[0]);
}

void do_options(int argc, char **argv)
{
    for (;;) {
	int index;
	int c;
                
	c = getopt_long (argc, argv, short_options, long_options, &index);

	if (-1 == c) break;

	switch (c) {
	  case 0: /* getopt_long() flag */
	    break;
	  case 'd':
	    videodev_name = optarg;
	    break;
	  case 'U':
	    url_prefix = optarg;
	    break;
	  case 'p':
	    bind_name = optarg;
	    break;
	  case 'M':
	    mono = 1;
	    break;
	  case 's':
	    if ( sscanf( optarg, "%dx%d", &video_width, &video_height) != 2) {
		usage(stderr, argc, argv);
		exit(EXIT_FAILURE);
	    }
	  case 'q':
	    sscanf( optarg,"%d", &quality);
	    break;
	  case 'f':
	    sscanf( optarg,"%d", &fps);
	    break;
    	  case 'F':
	    if ( strcmp(optarg, "jpeg")==0) camera_method = CAMERA_METHOD_JPEG;
	    else if ( strcmp(optarg, "yuyv")==0) camera_method = CAMERA_METHOD_YUYV;
	    else {
	      fprintf(stderr,"Illegal camera format: %s\n", optarg);
	      exit(EXIT_FAILURE);
	    }
	    break;
	  case 'h':
	    usage (stdout, argc, argv);
	    exit (EXIT_SUCCESS);
	  case 'v':
	    verbose = 1;
	    break;
	  case 'P':
	    probe_only = 1;
	    break;
	  case 'D':
	    daemon_mode = 1;
	    break;
	  case 'm':
	    io_method = IO_METHOD_MMAP;
	    break;
	  case 'r':
	    io_method = IO_METHOD_READ;
	    break;
	  case 'u':
	    io_method = IO_METHOD_USERPTR;
	    break;
	  default:
	    usage (stderr, argc, argv);
	    exit (EXIT_FAILURE);
	}
    }
}
    
