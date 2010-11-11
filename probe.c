#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <pthread.h>

#include <linux/videodev2.h>
#include "tinycamd.h"
#include "uvc_compat.h"

struct buffer {
        void *                  start;
        size_t                  length;
};

static void errno_exit(const char *s)
{
    fprintf (stderr, "%s error %d, %s\n", s, errno, strerror (errno));
    exit (EXIT_FAILURE);
}

static int xioctl(int fd, int request, void *arg)
{
    int r;
    
    do r = ioctl (fd, request, arg);
    while (-1 == r && EINTR == errno);
    
    return r;
}

/*
** Probe the device and print a bunch of info.
** This does not lock the device!!!! Don't do it while anything else is running.
*/
void do_probe (int videodev)
{
    unsigned int min;

    printf("Probing...\n");

    /*
    ** Is it a video device?
    */
    {
	struct v4l2_capability cap;

	if (-1 == xioctl (videodev, VIDIOC_QUERYCAP, &cap)) {
	    if (EINVAL == errno) {
		printf("Not a v4l2 device.\n");
		return;
	    } else {
		errno_exit ("VIDIOC_QUERYCAP");
	    }
	}

	printf("%-12s: %s\n", "driver", cap.driver);
	printf("%-12s: %s\n", "card", cap.card);
	printf("%-12s: %s\n", "bus_info", cap.bus_info);
	printf("%-12s: %u.%u.%u\n", "version", (cap.version>>16)&0xff, (cap.version>>8)&0xff, cap.version&0xff);
	printf("%-12s: %s\n", "capture?", (cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ? "yes" : "no");
	printf("%-12s: %s\n", "tuner?", (cap.capabilities & V4L2_CAP_TUNER) ? "yes" : "no");
	printf("%-12s: %s\n", "read()?", (cap.capabilities & V4L2_CAP_READWRITE) ? "yes" : "no");
	printf("%-12s: %s\n", "asyncio?", (cap.capabilities & V4L2_CAP_ASYNCIO) ? "yes" : "no");
	printf("%-12s: %s\n", "streaming?", (cap.capabilities & V4L2_CAP_STREAMING) ? "yes" : "no");
    }

    /*
    ** Enumerate the Image Capture Formats
    */
    {
	struct v4l2_fmtdesc fmtDesc = {
	    .index = 0,
	    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	};

	for (;;fmtDesc.index++) {
	    if (-1 == xioctl (videodev, VIDIOC_ENUM_FMT, &fmtDesc)) {
		if (EINVAL == errno) break;
		errno_exit ("VIDIOC_ENUM_FMT");
	    }
	    printf("%12s: (%d) = \"%s\" \"%c%c%c%c\"%s\n", "fmtDesc", fmtDesc.index,
		   fmtDesc.description, 
		   (fmtDesc.pixelformat >> 0)&0xff,
		   (fmtDesc.pixelformat >> 8)&0xff,
		   (fmtDesc.pixelformat >> 16)&0xff,
		   (fmtDesc.pixelformat >> 24)&0xff,
		   (fmtDesc.flags & V4L2_FMT_FLAG_COMPRESSED) ? "(compressed)": "");

	    {
		struct v4l2_frmsizeenum frm = {
		    .index = 0,
		    .pixel_format = fmtDesc.pixelformat,
		};

		for ( ;;frm.index++) {
		    int wid = 0, hgt = 0;
		    if (-1 == xioctl (videodev, VIDIOC_ENUM_FRAMESIZES, &frm)) {
			if (EINVAL == errno) break;
			errno_exit ("VIDIOC_ENUM_FRAMESIZES");
		    }
		    switch(frm.type) {
		      case V4L2_FRMSIZE_TYPE_DISCRETE:
			printf("%12s: frame(%d) %dx%d: fps ", "", frm.index,
			       frm.discrete.width,
			       frm.discrete.height);
			wid = frm.discrete.width;
			hgt = frm.discrete.height;
			break;
		      case V4L2_FRMSIZE_TYPE_CONTINUOUS:
			printf("%12s: frame(%d) %dx%d to %dx%d: fps ", "", frm.index,
			       frm.stepwise.min_width,
			       frm.stepwise.min_height,
			       frm.stepwise.max_width,
			       frm.stepwise.max_height);
			wid = frm.stepwise.max_width;
			hgt = frm.stepwise.max_height;
			break;
		      case V4L2_FRMSIZE_TYPE_STEPWISE:
			printf("%12s: frame(%d) %dx%d to %dx%d by %dx%d: fps ", "", frm.index,
			       frm.stepwise.min_width,
			       frm.stepwise.min_height,
			       frm.stepwise.max_width,
			       frm.stepwise.max_height,
			       frm.stepwise.step_width,
			       frm.stepwise.step_height );
			wid = frm.stepwise.max_width;
			hgt = frm.stepwise.max_height;
			break;
		      default:
			printf("unknown format type\n");
		    }
		    
		    if ( wid && hgt) {
			struct v4l2_frmivalenum ival = {
			    .index = 0,
			    .pixel_format = frm.pixel_format,
			    .width = wid,
			    .height = hgt,
			};
			for (;;ival.index++) {
			    if (-1 == xioctl (videodev, VIDIOC_ENUM_FRAMEINTERVALS, &ival)) {
				if (EINVAL == errno) break;
				errno_exit ("VIDIOC_ENUM_FRAMEINTERVALS");
			    }
			    switch(ival.type) {
			      case V4L2_FRMIVAL_TYPE_DISCRETE:
				printf("%d/%d ", ival.discrete.numerator, ival.discrete.denominator);
				break;
			      default:
				printf("non-discrete ");
				break;
			    }
			}
		    }
		    printf("\n");
		}
	    }
	}
    }
    /*
    ** The the format
    */
    {
	struct v4l2_format fmt = {
	    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	    .fmt.pix.width = video_width,
	    .fmt.pix.height = video_height,
	    // .fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV,
	    .fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG,
	    .fmt.pix.field = V4L2_FIELD_INTERLACED,
	};
	struct v4l2_jpegcompression comp = {
	    .quality = quality,
	};
	struct v4l2_streamparm strm = {
	    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
	};

	if (-1 == xioctl (videodev, VIDIOC_S_FMT, &fmt)) errno_exit ("VIDIOC_S_FMT");

	if (-1 == xioctl( videodev, VIDIOC_G_JPEGCOMP, &comp)) {
	    if ( errno != EINVAL) errno_exit("VIDIOC_G_JPEGCOMP");
	    fprintf(stderr,"driver does not support VIDIOC_G_JPEGCOMP\n");
	    comp.quality = quality;
	} else {
	    comp.quality = quality;
	    if (-1 == xioctl( videodev, VIDIOC_S_JPEGCOMP, &comp)) errno_exit("VIDIOC_S_JPEGCOMP");
	    if (-1 == xioctl( videodev, VIDIOC_G_JPEGCOMP, &comp)) errno_exit("VIDIOC_G_JPEGCOMP");
	    fprintf(stderr,"jpegcomp quality came out at %d\n", comp.quality);
	}

	if (-1 == xioctl( videodev, VIDIOC_G_PARM, &strm)) errno_exit("VIDIOC_G_PARM");
	strm.parm.capture.timeperframe.numerator = 1;
	if ( strm.parm.capture.capability & V4L2_CAP_TIMEPERFRAME) {
	    fprintf(stderr,"fps=%d\n", fps);
	    strm.parm.capture.timeperframe.denominator = fps;
	    if (-1 == xioctl( videodev, VIDIOC_S_PARM, &strm)) errno_exit("VIDIOC_S_PARM");
	    fprintf(stderr,"fps came out %d/%d\n", 
		    strm.parm.capture.timeperframe.numerator,
		    strm.parm.capture.timeperframe.denominator);
	}
	/* Note VIDIOC_S_FMT may change width and height. */
	
	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
	    fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
	    fmt.fmt.pix.sizeimage = min;
	
    }
}
