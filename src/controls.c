#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>

#include <linux/videodev2.h>
#include "tinycamd.h"
#include "uvc_compat.h"
#include "uvcvideo.h"

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
        log_f("set_control failed to check value: %s\n", strerror(errno));
	//return snprintf( buf, size, "failed to get check value: %s\n", strerror(errno));
    }

    con.value = val;
    if ( xioctl( fd, VIDIOC_S_CTRL, &con)) {
        log_f("set_control failed to set value: %s\n", strerror(errno));
	return snprintf( buf, size, "failed to set value: %s\n", strerror(errno));
    }
    return snprintf(buf, size, "OK");
}

int list_controls( int fd, char *buf, int size, int cidArg, int valArg)
{
    int cid, mindex;
    int used = 0;

    log_f("buf=%08x, size=%d\n", (unsigned int)buf, size);
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
  	        log_f("Repolling for control %d\n", cid);
		continue;
	    }
	    break;
	}

	cid = queryctrl.id;

	if ( rc==0 ) {
	    struct v4l2_control con = {
		.id = queryctrl.id,
	    };

	    log_f("ctrl: %d(%s) type:%d\n", cid, queryctrl.name,queryctrl.type);
	    if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;

	    if ( xioctl( fd, VIDIOC_G_CTRL, &con)) {
	        log_f("Failed to get %s value: %s\n", queryctrl.name, strerror(errno));
	    }
	    
	    switch( queryctrl.type) {
	    case V4L2_CTRL_TYPE_MENU:
	      used += snprintf( buf+used, size-used, 
				"<menu_control name=%s minimum=\"%d\" maximum=\"%d\" default=\"%d\" current=\"%d\" cid=\"%d\">\n", 
				xml(queryctrl.name),
				queryctrl.minimum, queryctrl.maximum, queryctrl.default_value, con.value, con.id);
	      for ( mindex = 0; mindex <= queryctrl.maximum; mindex++) {
		  struct v4l2_querymenu menu = {
		      .id = con.id,
		      .index = mindex
		  };

		  if ( xioctl( fd, VIDIOC_QUERYMENU, &menu)) {
		      log_f("Failed to query control %s menu index %d: %s\n", queryctrl.name, mindex, strerror(errno));
		  }
		  used += snprintf( buf+used, size-used, "  <menu_item index=\"%d\" name=%s />\n", mindex, xml(menu.name));
	      }
	      used += snprintf( buf+used, size-used, "</menu_control>\n");
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
	      log_f("Unhandled control type for %s, type=%d\n",
		      queryctrl.name, queryctrl.type);
	      break;
	    }
	} else {
	    log_f("control errno:%d(%s) cid:%d(%d,%d)\n", errno, strerror(errno),cid, V4L2_CID_LASTP1, V4L2_CID_CAMERA_CLASS_BASE);
	    break;
	}
    }


    used += snprintf( buf+used, size-used, "</controls>\n");
    log_f("%d %s\n", used, buf);
    return used;
}

void add_logitech_controls(int fd)
{
#define UVC_GUID_LOGITECH_MOTOR_CONTROL {0x82, 0x06, 0x61, 0x63, 0x70, 0x50, 0xab, 0x49, 0xb8, 0xcc, 0xb3, 0x85, 0x5e, 0x8d, 0x22, 0x56}
#define XU_MOTORCONTROL_PANTILT_RELATIVE 1
#define XU_MOTORCONTROL_PANTILT_RESET 2
#define XU_MOTORCONTROL_FOCUS 3
    const unsigned char motor[16] = UVC_GUID_LOGITECH_MOTOR_CONTROL;

    void add_uvc( const unsigned char *entity, int selector, int index, int size) {
	int err;
	struct uvc_xu_control_info ci = {
	    .selector = selector,
	    .index = index,
	    .size = size,
	    .flags = UVC_CONTROL_SET_CUR|UVC_CONTROL_GET_MIN|UVC_CONTROL_GET_MAX|UVC_CONTROL_GET_DEF
	};
	memcpy( ci.entity, entity, sizeof ci.entity);

	errno=0;
	if ((err=ioctl(fd, UVCIOC_CTRL_ADD, &ci)) < 0) {
	    if (errno!=EEXIST) {
		log_f("uvcioc ctrl add error (selector=%d): errno=%d (retval=%d)\n",selector,errno,err);
		return;
	    } else {
		return; // control exists
	    }
	}
	if ( verbose) fprintf(stderr,"added control %d.%d(%d)\n", selector, index, size);
    };

    void add_v4l2( int id, const char *name, const unsigned char *entity, int selector, int size, int offset, int v4l2type, int uvcType) {
	int err;
	struct uvc_xu_control_mapping cm = {
	    .id = id,
	    .selector = selector,
	    .size=size,
	    .offset=offset,
	    .v4l2_type=v4l2type,
	    .data_type=uvcType
	};
	memcpy( cm.entity, entity, sizeof cm.entity);
	strncpy( (char *)cm.name, name, sizeof(cm.name)-1);

	errno=0;
	if ((err=ioctl(fd, UVCIOC_CTRL_MAP, &cm)) < 0) {
	    if (errno!=EEXIST) {
		log_f("uvcioc ctrl map error for id=%d: errno=%d (retval=%d)\n",id,errno,err);
		return;
	    } else {
		return; // mapping exists
	    }
	}
	if ( verbose) fprintf(stderr,"added v4l2 control %d(%s)\n", id, name);
    };

    add_uvc( motor, XU_MOTORCONTROL_PANTILT_RELATIVE, 0, 4);
    add_v4l2( V4L2_CID_PAN_RELATIVE, "Pan", motor, XU_MOTORCONTROL_PANTILT_RELATIVE, 16, 0, V4L2_CTRL_TYPE_INTEGER, UVC_CTRL_DATA_TYPE_SIGNED);
    add_v4l2( V4L2_CID_TILT_RELATIVE, "Tilt", motor, XU_MOTORCONTROL_PANTILT_RELATIVE, 16, 16, V4L2_CTRL_TYPE_INTEGER, UVC_CTRL_DATA_TYPE_SIGNED);

    add_uvc( motor, XU_MOTORCONTROL_PANTILT_RESET, 0, 1);
    add_v4l2( V4L2_CID_PAN_RESET, "PanTilt Reset", motor, XU_MOTORCONTROL_PANTILT_RESET, 8, 0, V4L2_CTRL_TYPE_INTEGER, UVC_CTRL_DATA_TYPE_UNSIGNED);
}

