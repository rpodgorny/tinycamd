#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

static void usage( FILE *fp, int argc, char **argv)
{
    fprintf (fp,
	     "Usage: %s [options]\n\n"
	     "Options:\n"
	     "-d | --device name   Video device name [/dev/video0]\n"
	     "-h | --help          Print this message\n"
	     "",
	     argv[0]);
}

static int verbose = 0;

static const char short_options [] = "d:hv";

static const struct option
long_options [] = {
    { "device",     required_argument,      NULL,           'd' },
    { "help",       no_argument,            NULL,           'h' },
    { "verbose",    no_argument,            NULL,           'v' },
    { 0, 0, 0, 0 }
};

static int xioctl( int fd, int request, void *arg)
{
    int r;
    
    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);
    
    return r;
}


static char *xml(unsigned char *s)
{
    static char buf[1024];

    sprintf(buf, "\"%s\"", s);  // do better
    return buf;
}

void ListControls(int fd)
{
    int cid;

    printf("<?xml version=\"1.0\" ?>\n");
    printf("<controls>\n");
    // do BASE up to LASTP1 then skip to PRIVATE_BASE and continue until one fails 
    for ( cid = V4L2_CID_BASE; ; cid = (++cid == V4L2_CID_LASTP1 ? V4L2_CID_PRIVATE_BASE : cid)) {
	struct v4l2_queryctrl queryctrl;
	
	memset( &queryctrl, 0, sizeof queryctrl);
	queryctrl.id = cid;
	if (0 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
	    struct v4l2_control con;
	    if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;
	    
	    memset(&con, sizeof con, 0);
	    con.id = queryctrl.id;

	    if ( xioctl( fd, VIDIOC_G_CTRL, &con)) {
		fprintf(stderr,"Failed to get %s value: %s\n", queryctrl.name, strerror(errno));
	    }
	    
	    if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
		;//printf(":menu.\n");
	    } else {
		printf("<range_control name=%s minimum=\"%d\" maximum=\"%d\" by=\"%d\" default=\"%d\" current=\"%d\" "
		       "%s%s%s%s%s%s />\n",
		       xml(queryctrl.name), 
		       queryctrl.minimum, queryctrl.maximum, queryctrl.step, queryctrl.default_value, con.value,
		       (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) ? " disabled=\"1\"" : "",
		       (queryctrl.flags & V4L2_CTRL_FLAG_GRABBED) ? " grabbed=\"1\"" : "",
		       (queryctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) ? " readonly=\"1\"" : "",
		       (queryctrl.flags & V4L2_CTRL_FLAG_UPDATE) ? " update=\"1\"" : "",
		       (queryctrl.flags & V4L2_CTRL_FLAG_INACTIVE) ? " inactive=\"1\"" : "",
		       (queryctrl.flags & V4L2_CTRL_FLAG_SLIDER) ? " slider=\"1\"" : "" );
	    }
	} else {
	    if (errno == EINVAL && cid < V4L2_CID_LASTP1 ) continue;
	    break;
	}
    }
    printf("</controls>\n");
}

void SetControl( int fd, char *cname, int val)
{
    int cid;

    printf("setting %s to %d\n", cname, val);

    // do BASE up to LASTP1 then skip to PRIVATE_BASE and continue until one fails 
    for ( cid = V4L2_CID_BASE; ; cid = (++cid == V4L2_CID_LASTP1 ? V4L2_CID_PRIVATE_BASE : cid)) {
	struct v4l2_queryctrl queryctrl;
	
	memset( &queryctrl, 0, sizeof queryctrl);
	queryctrl.id = cid;
	if (0 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
	    struct v4l2_control con;

	    if ( strcmp((char *)queryctrl.name, cname) != 0 ) continue;

	    if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;
	    
	    printf ( "control:%s:", queryctrl.name);

	    memset(&con, sizeof con, 0);
	    con.id = queryctrl.id;
	    con.value = val;

	    if ( 0 == xioctl( fd, VIDIOC_S_CTRL, &con)) {
		printf("Control set.\n");
	    } else {
		fprintf(stderr,"Failed to set %s to %d: %s\n", cname, val, strerror(errno));
	    }
	    break;
	}
    }
}


int main( int argc, char **argv)
{
    char *dev_name = "/dev/video0";
    int arg;
    int fd;

    if ( strstr( argv[0], ".cgi") >= 0) {
	printf("Content-Type: text/xml\n\n");
    }

    for (;;) {
	int c, index;
	
	c = getopt_long (argc, argv,
			 short_options, long_options,
			 &index);
	
	if (-1 == c)
	    break;
	
	switch (c) {
	case 0: /* getopt_long() flag */
	    break;
	    
	case 'd':
	    dev_name = optarg;
	    break;
	    
	case 'v':
	    verbose = 1;
	    break;
	    
	case 'h':
	    usage (stdout, argc, argv);
	    exit (EXIT_SUCCESS);
	    
	default:
	    usage (stderr, argc, argv);
	    exit (EXIT_FAILURE);
	}
    }

    fd = open( dev_name, O_RDWR);
    if ( fd < 0) {
	fprintf(stderr,"Failed to open %s: %s\n", dev_name, strerror(errno));
	exit(1);
    }

    for ( arg = optind; arg < argc; arg++) {
	char cname[1024];
	int val;

	if ( verbose) fprintf(stderr,"... processing %s\n", argv[arg]);

	if ( strcmp( argv[arg], "list:controls")==0) {
	    ListControls(fd);
	} else if ( sscanf(argv[arg], "set:control:%1000[^:]:%d", cname, &val) == 2) {
	    SetControl( fd, cname, val);
	} else {
	    fprintf(stderr,"Unrecognized command: %s\n", argv[arg]);
	    exit(1);
	}
    }

    return 0;
}
