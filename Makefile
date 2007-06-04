
CFLAGS = -Wall -Werror -g -MMD $(CLFAGS)
LDLIBS += -lfcgi -lpthread -lm
all : tinycamd 


tinycamd : tinycamd.o options.o device.o frame.o controls.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

clean : 
	- rm -f *.[do] *~ tinycamd 

install : 
	mkdir -p $(DESTDIR)/usr/bin/
	install tinycamd $(DESTDIR)/usr/bin/

include $(wildcard *.d)