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
