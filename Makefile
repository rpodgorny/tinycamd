
CFLAGS := -Wall -Werror -O2 -MMD $(CFLAGS) $(COPTS)
LDLIBS += -ljpeg -lpthread -lrt
HOSTCC ?= cc

all : tinycamd 


tinycamd : tinycamd.o options.o device.o frame.o controls.o httpd.o logging.o probe.o html.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

util/bintoc : util/bintoc.c
	$(HOSTCC) $^ -o $@

html.c : util/bintoc resources/setup.html resources/tinycamd.js resources/tinycamd.css
	util/bintoc setup_html=resources/setup.html \
		    tinycamd_js=resources/tinycamd.js \
		    tinycamd_css=resources/tinycamd.css > $@	

# to profile, LD_PRELOAD=./gprof-helper.so ./tinycamd ....
gprof-helper.so:
	$(CC) -shared -fPIC util/gprof-helper.c -o gprof-helper.so -lpthread -ldl

clean : 
	- rm -f *.[do] *~ tinycamd  *.gcov *.gcda *.gcno gmon.out html.c

install : 
	mkdir -p $(DESTDIR)/usr/bin/
	install tinycamd $(DESTDIR)/usr/bin/

include $(wildcard *.d)