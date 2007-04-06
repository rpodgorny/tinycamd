#ifndef CAPTURE_IS_IN
#define CAPTURE_IS_IN

typedef enum {
	IO_METHOD_READ,
	IO_METHOD_MMAP,
	IO_METHOD_USERPTR,
} io_method;

void mainloop (void);
void stop_capturing (void);
void start_capturing (void);
void uninit_device (void);
void init_read (unsigned int buffer_size);
void init_mmap (void);
void init_userp	(unsigned int buffer_size);
void init_device (void);
void close_device (void);
void open_device ( const char *dev_arg, io_method io_arg);

#endif
