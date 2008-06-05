#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>
#include "tinycamd.h"
#include "uvc_compat.h"

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

    cid = 0;
    for (;;) {
	struct v4l2_queryctrl queryctrl = {
	  .id = ( cid | V4L2_CTRL_FLAG_NEXT_CTRL),
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

	cid = queryctrl.id;

	if ( rc==0 ) {
	    struct v4l2_control con = {
		.id = queryctrl.id,
	    };

	    fprintf(stderr,"ctrl: %d(%s) type:%d\n", cid, queryctrl.name,queryctrl.type);
	    if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;

	    if ( xioctl( fd, VIDIOC_G_CTRL, &con)) {
		fprintf(stderr,"Failed to get %s value: %s\n", queryctrl.name, strerror(errno));
	    }
	    
	    switch( queryctrl.type) {
	    case V4L2_CTRL_TYPE_MENU:
	      used += snprintf( buf+used, size-used, 
				"<menu_control name=%s />\n",
				xml(queryctrl.name));
	      break;
	    case V4L2_CTRL_TYPE_BOOLEAN:
	      used += snprintf( buf+used, size-used, 
				"<boolean_control name=%s default=\"%d\" current=\"%d\" cid=\"%d\" />\n",
				xml(queryctrl.name),
				queryctrl.default_value, con.value, con.id);
	      break;
	    case V4L2_CTRL_TYPE_INTEGER:
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
	      break;
	    default:
	      fprintf(stderr,"Unhandled control type for %s, type=%d\n",
		      queryctrl.name, queryctrl.type);
	      break;
	    }
	} else {
	    fprintf(stderr,"control errno:%d(%s) cid:%d(%d,%d)\n", errno, strerror(errno),cid, V4L2_CID_LASTP1, V4L2_CID_CAMERA_CLASS_BASE);
	    break;
	}
    }


    used += snprintf( buf+used, size-used, "</controls>\n");
    if ( verbose) fprintf(stderr,"%d %s\n", used, buf);
    return used;
}

