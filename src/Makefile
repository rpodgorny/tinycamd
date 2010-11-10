
CFLAGS := -Wall -Werror -O2 -MMD $(CFLAGS) $(COPTS)
CFLAGS := -g -Wall -Werror -O0 -MMD $(CFLAGS) $(COPTS)
LDLIBS += -ljpeg -lpthread -lrt

all : tinycamd 


tinycamd : tinycamd.o options.o device.o frame.o controls.o httpd.o logging.o probe.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

# to profile, LD_PRELOAD=./gprof-helper.so ./tinycamd ....
gprof-helper.so:
	$(CC) -shared -fPIC util/gprof-helper.c -o gprof-helper.so -lpthread -ldl

clean : 
	- rm -f *.[do] *~ tinycamd  *.gcov *.gcda *.gcno gmon.out

install : 
	mkdir -p $(DESTDIR)/usr/bin/
	install tinycamd $(DESTDIR)/usr/bin/

include $(wildcard *.d)