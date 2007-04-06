
CFLAGS = -Wall -Werror -g -MMD $(CLFAGS)

all : tinycamd tinycamctl tinycamctl.cgi


tinycamd : tinycamd.o capture.o
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

tinycamctl : tinycamctl.o 
	$(LINK.c) $^ $(LOADLIBES) $(LDLIBS) -o $@

tinycamctl.cgi : tinycamctl
	ln $^ $@

clean : 
	- rm -f *.[do] *~ tinycamd tinycamctl tinycamctl.cgi

install : 
	mkdir -p $(PKG_INSTALL_DIR)/usr/bin/
	$(INSTALL) tinycamd $(PKG_INSTALL_DIR)/usr/bin/
	$(INSTALL) tinycamctl $(PKG_INSTALL_DIR)/usr/bin/

include $(wildcard *.d)