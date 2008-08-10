#ifndef TINYCAMD_IS_IN
#define TINYCAMD_IS_IN

enum io_method {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
};

extern enum io_method io_method;
extern char *videodev_name;
extern char *bind_name;
extern char *url_prefix;
extern int verbose;
extern int daemon_mode;

void do_options(int argc, char **argv);


extern int videodev;
extern int video_width;
extern int video_height;
extern int quality;
extern int fps;

struct chunk {
    const void *data;
    unsigned int length;
};
typedef void (*frame_sender) (const struct chunk *, void *);
typedef int (*video_action)( int fd, char *buf, int used, int cid, int val);

void open_device();
void init_device();
void start_capturing();
void *main_loop(void *args);
void stop_capturing();
void close_device();
int with_device( video_action func, char *buf, int size, int cid, int val);

#ifdef __LINUX_VIDEODEV2_H
void new_frame( void *data, unsigned int length, struct v4l2_buffer *buf);
#endif
void with_current_frame( frame_sender func, void *arg);
void with_next_frame( frame_sender func, void *arg);

int list_controls( int fd, char *buf, int used, int cid, int val);
int set_control( int fd, char *buf, int used, int cid, int val);

#include "logging.h"

#endif
