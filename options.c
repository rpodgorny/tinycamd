#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#include "tinycamd.h"

enum io_method io_method = IO_METHOD_MMAP;
char *videodev_name = "/dev/video0";
int video_width = 640;
int video_height = 480;
int verbose = 0;
int quality = 100;
int fps = 5;

static const char short_options [] = "d:hmruvq:s:f:";

static const struct option
long_options [] = {
        { "device",     required_argument,      NULL,           'd' },
        { "help",       no_argument,            NULL,           'h' },
        { "mmap",       no_argument,            NULL,           'm' },
        { "read",       no_argument,            NULL,           'r' },
        { "userp",      no_argument,            NULL,           'u' },
	{ "size",       required_argument,      NULL,           's' },
	{ "verbose",    no_argument,            NULL,           'v' },
	{ "quality",    required_argument,      NULL,           'q' },
	{ "fps",        required_argument,      NULL,           'f' },
        { 0, 0, 0, 0 }
};

static void usage(FILE *fp, int argc, char **argv)
{
    fprintf (fp,
	     "Usage: %s [options]\n\n"
	     "Options:\n"
	     "-d | --device name   Video device name [/dev/video]\n"
	     "-s | --size widxhgt  Size, e.g. 640x480\n"
	     "-f | --fps num       Frames per second\n"
	     "-q | --quality num   JPEG quality, 0-100\n"
	     "-h | --help          Print this message\n"
	     "-m | --mmap          Use memory mapped buffers\n"
	     "-r | --read          Use read() calls\n"
	     "-u | --userp         Use application allocated buffers\n"
	     "-v | --verbose       Print a lot of debug messages\n"
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
	  case 'h':
	    usage (stdout, argc, argv);
	    exit (EXIT_SUCCESS);
	  case 'v':
	    verbose = 1;
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
    
