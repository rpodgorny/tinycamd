#include <fcgi_config.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>
#include "tinycamd.h"

static int xioctl(int fd, int request, void *arg)
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

int set_control( int fd, char *buf, int size, int cid, int val)
{
    struct v4l2_control con = {
	.id = cid,
    };
    
    if ( xioctl( fd, VIDIOC_G_CTRL, &con)) {
	fprintf(stderr,"set_control failed to check value: %s\n", strerror(errno));
	return snprintf( buf, size, "failed to get check value: %s\n", strerror(errno));
    }

    con.value = val;
    if ( xioctl( fd, VIDIOC_S_CTRL, &con)) {
	fprintf(stderr,"set_control failed to set value: %s\n", strerror(errno));
	return snprintf( buf, size, "failed to get set value: %s\n", strerror(errno));
    }
    return snprintf(buf, size, "OK");
}

int list_controls( int fd, char *buf, int size, int cidArg, int valArg)
{
    int cid;
    int used = 0;

    if ( verbose) fprintf(stderr, "buf=%08x, size=%d\n", (unsigned int)buf, size);
    used += snprintf( buf+used, size-used, "<?xml version=\"1.0\" ?>\n");
    used += snprintf( buf+used, size-used, "<controls>\n");
    // do BASE up to LASTP1 then skip to PRIVATE_BASE and continue until one fails 
    for ( cid = V4L2_CID_BASE; ; cid = (++cid == V4L2_CID_LASTP1 ? V4L2_CID_PRIVATE_BASE : cid)) {
	struct v4l2_queryctrl queryctrl = {
	    .id = cid,
	};
	int rc, try;

	for ( try = 0; try < 10; try++) {
	    rc = xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl);
	    if ( rc != 0 && errno == EIO) {
		fprintf(stderr,"Repolling for control %d\n", cid);
		continue;
	    }
	    break;
	}
		
	if ( rc==0 ) {
	    struct v4l2_control con = {
		.id = queryctrl.id,
	    };
	    
	    if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;

	    if ( xioctl( fd, VIDIOC_G_CTRL, &con)) {
		fprintf(stderr,"Failed to get %s value: %s\n", queryctrl.name, strerror(errno));
	    }
	    
	    if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
		;//FCGX_PrintF(":menu.\n");
	    } else {
		used += snprintf( buf+used, size-used, "<range_control name=%s minimum=\"%d\" maximum=\"%d\" by=\"%d\" default=\"%d\" current=\"%d\" cid=\"%d\""
				  "%s%s%s%s%s%s />\n",
				  xml(queryctrl.name), 
				  queryctrl.minimum, queryctrl.maximum, queryctrl.step, queryctrl.default_value, con.value, con.id,
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
    used += snprintf( buf+used, size-used, "</controls>\n");
    if ( verbose) fprintf(stderr,"%d %s\n", used, buf);
    return used;
}

