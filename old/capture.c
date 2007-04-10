/*
 * This originated as the V4L2 example capture program and was modified a bit to suit tinycapd
 *
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <asm/types.h>          /* for videodev2.h */

#include <linux/videodev2.h>

#include "capture.h"
#include "v4l2_compat.h"

#define CLEAR(x) memset (&(x), 0, sizeof (x))

int verbose = 1;

struct buffer {
        void *                  start;
        size_t                  length;
};

static char *           dev_name        = NULL;
static io_method	io		= IO_METHOD_MMAP;
static int              fd              = -1;
struct buffer *         buffers         = NULL;
static unsigned int     n_buffers       = 0;

static void
errno_exit                      (const char *           s)
{
        fprintf (stderr, "%s error %d, %s\n",
                 s, errno, strerror (errno));

        exit (EXIT_FAILURE);
}

static int
xioctl                          (int                    fd,
                                 int                    request,
                                 void *                 arg)
{
        int r;

        do r = ioctl (fd, request, arg);
        while (-1 == r && EINTR == errno);

        return r;
}

static void xwrite( int fd, const void *p, unsigned int len)
{
  int l;

  do {
    l = write( fd, p, len);
  } while( l == -1 && errno == EINTR);

  if ( l < 0) fprintf(stderr,"Failed in xwrite: %s\n", strerror(errno));
}

/*
** MPJEG files are typically, though not always, missing their DHT. If they are
** missing then this is almost certainly what they need. I'd feel a lot better
** if there was a standard written down for this.
*/
static const unsigned char fixed_dht[] = {
  0xff,0xc4,0x01,0xa2,0x00,0x00,0x01,0x05,0x01,0x01,0x01,0x01,
  0x01,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x02,
  0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x01,0x00,0x03,
  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x00,0x00,0x00,
  0x00,0x00,0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,
  0x0a,0x0b,0x10,0x00,0x02,0x01,0x03,0x03,0x02,0x04,0x03,0x05,
  0x05,0x04,0x04,0x00,0x00,0x01,0x7d,0x01,0x02,0x03,0x00,0x04,
  0x11,0x05,0x12,0x21,0x31,0x41,0x06,0x13,0x51,0x61,0x07,0x22,
  0x71,0x14,0x32,0x81,0x91,0xa1,0x08,0x23,0x42,0xb1,0xc1,0x15,
  0x52,0xd1,0xf0,0x24,0x33,0x62,0x72,0x82,0x09,0x0a,0x16,0x17,
  0x18,0x19,0x1a,0x25,0x26,0x27,0x28,0x29,0x2a,0x34,0x35,0x36,
  0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,
  0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,
  0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,
  0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,0x95,
  0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,0xa8,
  0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,0xc2,
  0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,0xd5,
  0xd6,0xd7,0xd8,0xd9,0xda,0xe1,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
  0xe8,0xe9,0xea,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,
  0xfa,0x11,0x00,0x02,0x01,0x02,0x04,0x04,0x03,0x04,0x07,0x05,
  0x04,0x04,0x00,0x01,0x02,0x77,0x00,0x01,0x02,0x03,0x11,0x04,
  0x05,0x21,0x31,0x06,0x12,0x41,0x51,0x07,0x61,0x71,0x13,0x22,
  0x32,0x81,0x08,0x14,0x42,0x91,0xa1,0xb1,0xc1,0x09,0x23,0x33,
  0x52,0xf0,0x15,0x62,0x72,0xd1,0x0a,0x16,0x24,0x34,0xe1,0x25,
  0xf1,0x17,0x18,0x19,0x1a,0x26,0x27,0x28,0x29,0x2a,0x35,0x36,
  0x37,0x38,0x39,0x3a,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4a,
  0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5a,0x63,0x64,0x65,0x66,
  0x67,0x68,0x69,0x6a,0x73,0x74,0x75,0x76,0x77,0x78,0x79,0x7a,
  0x82,0x83,0x84,0x85,0x86,0x87,0x88,0x89,0x8a,0x92,0x93,0x94,
  0x95,0x96,0x97,0x98,0x99,0x9a,0xa2,0xa3,0xa4,0xa5,0xa6,0xa7,
  0xa8,0xa9,0xaa,0xb2,0xb3,0xb4,0xb5,0xb6,0xb7,0xb8,0xb9,0xba,
  0xc2,0xc3,0xc4,0xc5,0xc6,0xc7,0xc8,0xc9,0xca,0xd2,0xd3,0xd4,
  0xd5,0xd6,0xd7,0xd8,0xd9,0xda,0xe2,0xe3,0xe4,0xe5,0xe6,0xe7,
  0xe8,0xe9,0xea,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9,0xfa
};

static void
process_image                   (const unsigned char *           p, unsigned int len)
{
  int of = open( "image.jpg,", O_WRONLY | O_CREAT, 0666);
  int i;

  for ( i = 0; i < len-1; i++) {
    if ( p[i] == 0xff && p[i+1]== 0xc4) {  // Has its own DHT table
      xwrite( of, p, len);
      break;
    }
    if ( p[i] == 0xff && p[i+1] == 0xda) {  // Needs a DHT
      xwrite( of, p, i);
      xwrite( of, fixed_dht, sizeof fixed_dht);
      xwrite( of, p+i, len-i);
      break;
    }
  }
  // we really should have done our write already

  close(of);
  rename( "image.jpg,", "image.jpg");
}

static int
read_frame			(void)
{
        struct v4l2_buffer buf;
	unsigned int i, len;

	switch (io) {
	case IO_METHOD_READ:
    		if (-1 == (len = read (fd, buffers[0].start, buffers[0].length))) {
            		switch (errno) {
            		case EAGAIN:
                    		return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit ("read");
			}
		}

    		process_image (buffers[0].start, len);

		break;

	case IO_METHOD_MMAP:
		CLEAR (buf);

            	buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            	buf.memory = V4L2_MEMORY_MMAP;

    		if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
            		switch (errno) {
            		case EAGAIN:
                    		return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit ("VIDIOC_DQBUF");
			}
		}

                assert (buf.index < n_buffers);

	        process_image (buffers[buf.index].start, buf.bytesused);

		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");

		break;

	case IO_METHOD_USERPTR:
		CLEAR (buf);

    		buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    		buf.memory = V4L2_MEMORY_USERPTR;

		if (-1 == xioctl (fd, VIDIOC_DQBUF, &buf)) {
			switch (errno) {
			case EAGAIN:
				return 0;

			case EIO:
				/* Could ignore EIO, see spec. */

				/* fall through */

			default:
				errno_exit ("VIDIOC_DQBUF");
			}
		}

		for (i = 0; i < n_buffers; ++i)
			if (buf.m.userptr == (unsigned long) buffers[i].start
			    && buf.length == buffers[i].length)
				break;

		assert (i < n_buffers);

    		process_image ((void *) buf.m.userptr, buf.bytesused);

		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
			errno_exit ("VIDIOC_QBUF");

		break;
	}

	return 1;
}

void mainloop (void)
{
	unsigned int count;

        count = 100;

        while (1 || count-- > 0) {
                for (;;) {
                        fd_set fds;
                        struct timeval tv;
                        int r;

                        FD_ZERO (&fds);
                        FD_SET (fd, &fds);

                        /* Timeout. */
                        tv.tv_sec = 2;
                        tv.tv_usec = 0;

                        r = select (fd + 1, &fds, NULL, NULL, &tv);

                        if (-1 == r) {
                                if (EINTR == errno)
                                        continue;

                                errno_exit ("select");
                        }

                        if (0 == r) {
                                fprintf (stderr, "select timeout\n");
                                exit (EXIT_FAILURE);
                        }

			if (read_frame ())
                    		break;
	
			/* EAGAIN - continue select loop. */
                }
        }
}

void stop_capturing (void)
{
        enum v4l2_buf_type type;

	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (fd, VIDIOC_STREAMOFF, &type))
			errno_exit ("VIDIOC_STREAMOFF");

		break;
	}
}

void start_capturing (void)
{
        unsigned int i;
        enum v4l2_buf_type type;

	switch (io) {
	case IO_METHOD_READ:
		/* Nothing to do. */
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i) {
            		struct v4l2_buffer buf;

        		CLEAR (buf);

        		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        		buf.memory      = V4L2_MEMORY_MMAP;
        		buf.index       = i;

        		if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    		errno_exit ("VIDIOC_QBUF");
		}
		
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
			errno_exit ("VIDIOC_STREAMON");

		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i) {
            		struct v4l2_buffer buf;

        		CLEAR (buf);

        		buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        		buf.memory      = V4L2_MEMORY_USERPTR;
			buf.index       = i;
			buf.m.userptr	= (unsigned long) buffers[i].start;
			buf.length      = buffers[i].length;

			if (-1 == xioctl (fd, VIDIOC_QBUF, &buf))
                    		errno_exit ("VIDIOC_QBUF");
		}

		type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

		if (-1 == xioctl (fd, VIDIOC_STREAMON, &type))
			errno_exit ("VIDIOC_STREAMON");

		break;
	}
}

void uninit_device (void)
{
        unsigned int i;

	switch (io) {
	case IO_METHOD_READ:
		free (buffers[0].start);
		break;

	case IO_METHOD_MMAP:
		for (i = 0; i < n_buffers; ++i)
			if (-1 == munmap (buffers[i].start, buffers[i].length))
				errno_exit ("munmap");
		break;

	case IO_METHOD_USERPTR:
		for (i = 0; i < n_buffers; ++i)
			free (buffers[i].start);
		break;
	}

	free (buffers);
}

void init_read (unsigned int buffer_size)
{
        buffers = calloc (1, sizeof (*buffers));

        if (!buffers) {
                fprintf (stderr, "Out of memory\n");
                exit (EXIT_FAILURE);
        }

	buffers[0].length = buffer_size;
	buffers[0].start = malloc (buffer_size);

	if (!buffers[0].start) {
    		fprintf (stderr, "Out of memory\n");
            	exit (EXIT_FAILURE);
	}
}

void init_mmap (void)
{
	struct v4l2_requestbuffers req;

        CLEAR (req);

        req.count               = 4;
        req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory              = V4L2_MEMORY_MMAP;

	if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf (stderr, "%s does not support "
                                 "memory mapping\n", dev_name);
                        exit (EXIT_FAILURE);
                } else {
                        errno_exit ("VIDIOC_REQBUFS");
                }
        }

        if (req.count < 2) {
                fprintf (stderr, "Insufficient buffer memory on %s\n",
                         dev_name);
                exit (EXIT_FAILURE);
        }

        buffers = calloc (req.count, sizeof (*buffers));

        if (!buffers) {
                fprintf (stderr, "Out of memory\n");
                exit (EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
                struct v4l2_buffer buf;

                CLEAR (buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers;

                if (-1 == xioctl (fd, VIDIOC_QUERYBUF, &buf))
                        errno_exit ("VIDIOC_QUERYBUF");

                buffers[n_buffers].length = buf.length;
                buffers[n_buffers].start =
                        mmap (NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.offset);

                if (MAP_FAILED == buffers[n_buffers].start)
                        errno_exit ("mmap");
        }
}

void init_userp	(unsigned int buffer_size)
{
	struct v4l2_requestbuffers req;

        CLEAR (req);

        req.count               = 4;
        req.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory              = V4L2_MEMORY_USERPTR;

        if (-1 == xioctl (fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf (stderr, "%s does not support "
                                 "user pointer i/o\n", dev_name);
                        exit (EXIT_FAILURE);
                } else {
                        errno_exit ("VIDIOC_REQBUFS");
                }
        }

        buffers = calloc (4, sizeof (*buffers));

        if (!buffers) {
                fprintf (stderr, "Out of memory\n");
                exit (EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
                buffers[n_buffers].length = buffer_size;
                buffers[n_buffers].start = malloc (buffer_size);

                if (!buffers[n_buffers].start) {
    			fprintf (stderr, "Out of memory\n");
            		exit (EXIT_FAILURE);
		}
        }
}

void init_device (void)
{
        struct v4l2_capability cap;
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;
        struct v4l2_format fmt;
	unsigned int min;

        if (-1 == xioctl (fd, VIDIOC_QUERYCAP, &cap)) {
                if (EINVAL == errno) {
                        fprintf (stderr, "%s is no V4L2 device\n",
                                 dev_name);
                        exit (EXIT_FAILURE);
                } else {
                        errno_exit ("VIDIOC_QUERYCAP");
                }
        }
	if ( verbose) {
	  int cid,fmt;

	  fprintf(stderr,"video device: driver=\"%s\" card=\"%s\" bus_info=\"%s\" version=%u.%u.%d\n",
		  cap.driver, cap.card, cap.bus_info, 
		  (cap.version >> 16)&0xff, (cap.version>>8)&0xff, cap.version&0xff);
	  fprintf(stderr,"video device: supports%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		  ( cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ? " VIDEO_CAPTURE" : "",
		  ( cap.capabilities & V4L2_CAP_VIDEO_OUTPUT) ? " VIDEO_OUTPUT" : "",
		  ( cap.capabilities & V4L2_CAP_VIDEO_OVERLAY) ? " VIDEO_OVERLAY" : "",
		  ( cap.capabilities & V4L2_CAP_VBI_CAPTURE) ? " VBI_CAPTURE" : "",
		  ( cap.capabilities & V4L2_CAP_VBI_OUTPUT) ? " VBI_OUTPUT" : "",
		  ( cap.capabilities & V4L2_CAP_SLICED_VBI_CAPTURE) ? " SLICED_VBI_CAPTURE" : "",
		  ( cap.capabilities & V4L2_CAP_SLICED_VBI_OUTPUT) ? " SLICED_VBI_OUTPUT" : "",
		  ( cap.capabilities & V4L2_CAP_TUNER) ? " TUNER" : "",
		  ( cap.capabilities & V4L2_CAP_AUDIO) ? " AUDIO" : "",
		  ( cap.capabilities & V4L2_CAP_RADIO) ? " RADIO" : "",
		  ( cap.capabilities & V4L2_CAP_READWRITE) ? " READWRITE" : "",
		  ( cap.capabilities & V4L2_CAP_ASYNCIO) ? " ASYNCIO" : "",
		  ( cap.capabilities & V4L2_CAP_STREAMING) ? " STREAMING" : "");

	  // do BASE up to LASTP1 then skip to PRIVATE_BASE and continue until one fails 
	  for ( cid = V4L2_CID_BASE; ; cid = (++cid == V4L2_CID_LASTP1 ? V4L2_CID_PRIVATE_BASE : cid)) {
	    struct v4l2_queryctrl queryctrl;

	    memset( &queryctrl, 0, sizeof queryctrl);
	    queryctrl.id = cid;
	    if (0 == xioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
	      if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) continue;

	      fprintf (stderr, "Control %s", queryctrl.name);

	      if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
		fprintf(stderr,", menu.\n");
	      } else {
		fprintf(stderr,", %d..%d by %d, default=%d, %s%s%s%s%s%s\n",
			queryctrl.minimum, queryctrl.maximum, queryctrl.step, queryctrl.default_value,
			(queryctrl.flags & V4L2_CTRL_FLAG_DISABLED) ? " disabled" : "",
			(queryctrl.flags & V4L2_CTRL_FLAG_GRABBED) ? " grabbed" : "",
			(queryctrl.flags & V4L2_CTRL_FLAG_READ_ONLY) ? " readonly" : "",
			(queryctrl.flags & V4L2_CTRL_FLAG_UPDATE) ? " update" : "",
			(queryctrl.flags & V4L2_CTRL_FLAG_INACTIVE) ? " inactive" : "",
			(queryctrl.flags & V4L2_CTRL_FLAG_SLIDER) ? " slider" : "" );
	      }
	    } else {
	      if (errno == EINVAL && cid < V4L2_CID_LASTP1 ) continue;
	      break;

	      errno_exit ("VIDIOC_QUERYCTRL");
	    }

	  }

	  for ( fmt = 0; ; fmt++) {
	    struct v4l2_fmtdesc fdesc;
	    int siz;

	    memset( &fdesc, 0, sizeof fdesc);
	    fdesc.index = fmt;
	    fdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
	    if ( xioctl(fd, VIDIOC_ENUM_FMT, &fdesc) >= 0) {
	      fprintf(stderr,"  fmt %d(%c%c%c%c) = %s\n", fdesc.index, 
		      (fdesc.pixelformat & 0xff), (fdesc.pixelformat >> 8)&0xff, 
		      (fdesc.pixelformat >> 16)&0xff, (fdesc.pixelformat >> 24)&0xff,
		      fdesc.description);
	    } else {
	      break;
	    }

	    for ( siz = 0; ; siz++) {
	      struct v4l2_frmsizeenum sdesc;
	      int rat;

	      memset(&sdesc, 0, sizeof sdesc);
	      sdesc.index = siz;
	      sdesc.pixel_format = fdesc.pixelformat; // grr
	      if ( (xioctl(fd, VIDIOC_ENUM_FRAMESIZES, &sdesc)) >= 0) {
		switch( sdesc.type) {
		case V4L2_FRMSIZE_TYPE_DISCRETE:
		  fprintf(stderr,"    %dx%d\n", sdesc.discrete.width, sdesc.discrete.height);
		  break;
		case V4L2_FRMSIZE_TYPE_STEPWISE:
		case V4L2_FRMSIZE_TYPE_CONTINUOUS:
		  fprintf(stderr,"    %dx%d to %dx%d by %d,%d\n",
			  sdesc.stepwise.min_width, sdesc.stepwise.min_height,
			  sdesc.stepwise.max_width, sdesc.stepwise.max_height,
			  sdesc.stepwise.step_width, sdesc.stepwise.step_height);
		  break;
		default:
		  fprintf(stderr,"    unknown frame size type: %d\n", sdesc.type);
		  break;
		}
		for( rat = 0; ; rat++) {
		  struct v4l2_frmivalenum idesc;

		  memset(&idesc,0,sizeof idesc);
		  idesc.index = rat;
		  idesc.pixel_format = sdesc.pixel_format;
		  idesc.width = sdesc.discrete.width;  // danger: this is the min sizes on stepwise and continuous
		  idesc.height = sdesc.discrete.height;
		  if ( xioctl(fd,VIDIOC_ENUM_FRAMEINTERVALS,&idesc) >= 0) {
		    switch( idesc.type) {
		    case V4L2_FRMIVAL_TYPE_DISCRETE:
		      fprintf(stderr,"      %d/%d frames/sec\n", idesc.discrete.denominator, idesc.discrete.numerator);
		      break;
		    case V4L2_FRMIVAL_TYPE_STEPWISE:
		    case V4L2_FRMIVAL_TYPE_CONTINUOUS:
		      fprintf(stderr,"      %d/%d to %d/%d frames/sec by %d/%d\n", 
			      idesc.stepwise.min.denominator, idesc.stepwise.min.numerator,
			      idesc.stepwise.max.denominator, idesc.stepwise.max.numerator,
			      idesc.stepwise.step.denominator, idesc.stepwise.step.numerator);
		      break;
		    }
		  } else {
		    break;
		  }
		}

	      } else {
		break;
	      }
	    }
	  }
	}

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                fprintf (stderr, "%s is no video capture device\n",
                         dev_name);
                exit (EXIT_FAILURE);
        }

	switch (io) {
	case IO_METHOD_READ:
		if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
			fprintf (stderr, "%s does not support read i/o\n",
				 dev_name);
			exit (EXIT_FAILURE);
		}

		break;

	case IO_METHOD_MMAP:
	case IO_METHOD_USERPTR:
		if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
			fprintf (stderr, "%s does not support streaming i/o\n",
				 dev_name);
			exit (EXIT_FAILURE);
		}

		break;
	}


        /* Select video input, video standard and tune here. */


	CLEAR (cropcap);

        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (0 == xioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
                crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                crop.c = cropcap.defrect; /* reset to default */

                if (-1 == xioctl (fd, VIDIOC_S_CROP, &crop)) {
                        switch (errno) {
                        case EINVAL:
                                /* Cropping not supported. */
                                break;
                        default:
                                /* Errors ignored. */
                                break;
                        }
                }
        } else {	
                /* Errors ignored. */
        }


        CLEAR (fmt);

        fmt.type                = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width       = 640; 
        fmt.fmt.pix.height      = 480;
	//        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
        fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;

        if (-1 == xioctl (fd, VIDIOC_S_FMT, &fmt))
                errno_exit ("VIDIOC_S_FMT");

        /* Note VIDIOC_S_FMT may change width and height. */

	/* Buggy driver paranoia. */
	min = fmt.fmt.pix.width * 2;
	if (fmt.fmt.pix.bytesperline < min)
		fmt.fmt.pix.bytesperline = min;
	min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
	if (fmt.fmt.pix.sizeimage < min)
		fmt.fmt.pix.sizeimage = min;

	switch (io) {
	case IO_METHOD_READ:
		init_read (fmt.fmt.pix.sizeimage);
		break;

	case IO_METHOD_MMAP:
		init_mmap ();
		break;

	case IO_METHOD_USERPTR:
		init_userp (fmt.fmt.pix.sizeimage);
		break;
	}
}

void close_device (void)
{
        if (-1 == close (fd))
	        errno_exit ("close");

        fd = -1;
}

void open_device ( const char *dev_arg, io_method io_arg)
{
        struct stat st; 

	if ( dev_arg) dev_name = (char *)dev_arg;
	io = io_arg;

        if (-1 == stat (dev_name, &st)) {
                fprintf (stderr, "Cannot identify '%s': %d, %s\n",
                         dev_name, errno, strerror (errno));
                exit (EXIT_FAILURE);
        }

        if (!S_ISCHR (st.st_mode)) {
                fprintf (stderr, "%s is no device\n", dev_name);
                exit (EXIT_FAILURE);
        }

        fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == fd) {
                fprintf (stderr, "Cannot open '%s': %d, %s\n",
                         dev_name, errno, strerror (errno));
                exit (EXIT_FAILURE);
        }
}

