PRODUCT = fbshow
VERSION = 2.0

SHELL = /bin/sh
top_srcdir = .
srcdir = .

.SUFFIXES:
.SUFFIXES: .c .o

CC = gcc
DEFINES = -DHAVE_CONFIG_H
CFLAGS = -I. -Wall -g -O2 $(DEFINES) `pkg-config --cflags MagickWand freetype2`
LDFLAGS = 

LIBS =  -lm `pkg-config --libs MagickWand freetype2`
INSTALL = /usr/bin/install -c

prefix = /usr/local
exec_prefix = ${prefix}
bindir = ${exec_prefix}/bin
mandir = ${datarootdir}/man
datarootdir = ${prefix}/share

DISTFILES =

TARGET=fbshow
SOURCES=fbshow.c
OBJS=fbshow.o
LIB_OBJS=
DISTSRC=aclocal.m4 config.h.in configure configure.ac fbshow.c install-sh Makefile.in mkinstalldirs README
DISTBIN=$(TARGET) README

all: $(TARGET)

install: all
	$(top_srcdir)/mkinstalldirs $(bindir)
	$(INSTALL) $(TARGET) $(bindir)/$(TARGET)
##	$(top_srcdir)/mkinstalldirs $(mandir)/man1
##	$(INSTALL) $(MAN) $(mandir)/man1/$(MAN)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LIBS)

$(OBJS): $(SOURCES)

clean:
	/bin/rm -f $(TARGET) *.o core $(PRODUCT)-$(VERSION)-*.tar.gz*

distclean: clean config-clean

config-clean: confclean-recursive

confclean-recursive: cfg-clean

cfg-clean:
	/bin/rm -f Makefile config.h config.status config.cache config.log

mostlyclean: clean

maintainer-clean: clean

package: all
	tar czf $(PRODUCT)-$(VERSION)-bin.tar.gz $(DISTBIN)
	md5sum $(PRODUCT)-$(VERSION)-bin.tar.gz > $(PRODUCT)-$(VERSION)-bin.tar.gz.md5
	tar czf $(PRODUCT)-$(VERSION)-src.tar.gz $(DISTSRC)
	md5sum $(PRODUCT)-$(VERSION)-src.tar.gz > $(PRODUCT)-$(VERSION)-src.tar.gz.md5

# automatic re-running of configure if the configure.ac file has changed
${srcdir}/configure: configure.ac 
	cd ${srcdir} && autoconf

# autoheader might not change config.h.in, so touch a stamp file
${srcdir}/config.h.in: stamp-h.in
${srcdir}/stamp-h.in: configure.ac 
		cd ${srcdir} && autoheader
		echo timestamp > ${srcdir}/stamp-h.in

config.h: stamp-h
stamp-h: config.h.in config.status
	./config.status
Makefile: Makefile.in config.status
	./config.status
config.status: configure
	./config.status --recheck



