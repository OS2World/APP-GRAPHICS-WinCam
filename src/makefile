#
# winc makefile -- requires GNU make
#
# Copyright (C) 1996, Paul G. Fox
# This software may not be, in whole or in part, licensed or sold for
# profit as a software product itself, nor may it be included in or
# distributed with commercial products without prior written permission. 
# This restriction does not apply to the inclusion of this source code in
# collections of published source code, even where those collections are
# sold as commercial products in their own right.  This copyright notice
# must not be removed, altered, or obscured.  pgf@foxharp.boston.ma.us
#
# $Header: E:/winc/RCS/makefile.org 1.1 1997/03/01 03:44:56 Derek Exp Derek $
#

# most of this package is a library of camera access routines.  
# the program winc itself is a simple command line interface to that
# library.
# of the headers, only wincam.h is "public" -- the rest should be
# considered library-internal.

# what get's built -- a library/header file combo, and a couple of
#  programs that use it.
LIB = libwinc.a
INCLFILE = wincam.h
PROGRAMS = winc.exe wincd.exe 
MANPAGES = winc.1 wincd.1 xwinc.1
MANFORMATTED = winc.man wincd.man xwinc.man

# where they all go on "make install", respectively
INST_BINDIR = /usr/local/bin
INST_INCDIR = /usr/local/include
INST_LIBDIR = /usr/local/lib
INST_MANDIR = /usr/local/man/man1

# build-tool setup
CC = gcc
# i had trouble with "winc -D" on freebsd without this.  odd.
# CC  = gcc --static

# PROF=-pg
CFLAGS = -MD	# auto-dependency (file.d) generation, requires GNU make below
CFLAGS += -g -O -Wall -Wstrict-prototypes -Wshadow $(PROF)
#CFLAGS += -I/usr/X11R6/include -I/usr/openwin/include
MORELIBS = -lm
XLIBS = -L/usr/X11R6/lib -L/usr/openwin/lib -lX11 -lXext
RANLIB = ranlib
INSTALL		= /usr/bin/install -c
INSTALL_PROGRAM	= ${INSTALL}
INSTALL_DATA	= ${INSTALL} -m 644

PRIVATE_HEADERS = private.h trace.h pnm.h color.h
LIBSRC = find.c view.c image.c trace.c comms.c \
	protocol.c diags.c convert.c config.c mergeenv.c pnm.c vignette.c \
	color.c locks.c starfield.c
SRC = winc.c wincd.c xwinc.c

OTHERSTUFF =  README Makefile ChangeLog $(INCLFILE) $(MANPAGES) \
		grabber.sh wincd_script xv-winc.patch COPYING 

DISTFILES = $(OTHERSTUFF) \
	$(SRC) $(LIBSRC) \
	$(PRIVATE_HEADERS) $(PUBLIC_HEADERS)


OBJ = $(SRC:%.c=%.o)
LIBOBJ = $(LIBSRC:%.c=%.o)

all: $(PROGRAMS)

winc.exe: winc.o $(LIB)
	$(CC) $(PROF) -o winc.exe winc.o $(LIB) $(MORELIBS)

wincd.exe: wincd.o $(LIB)
	$(CC) $(PROF) -o wincd.exe wincd.o $(LIB) $(MORELIBS)

#xwinc: xwinc.o $(LIB)
#	$(CC) $(PROF) -o xwinc xwinc.o $(LIB) $(MORELIBS) $(XLIBS)

clean:
	rm -f $(OBJ) $(LIBOBJ) *.d

$(LIB): $(LIBOBJ)
	-rm -f $@
	$(AR) cq $@ $^
	$(AR) s $@ $^

# distribute a compiled man page, as well as the source
winc.man: winc.1
	nroff -man winc.1 | col -b >winc.man

wincd.man: wincd.1
	nroff -man wincd.1 | col -b >wincd.man

xwinc.man: xwinc.1
	nroff -man xwinc.1 | col -b >xwinc.man

install: installdirs $(PROGRAMS) $(LIB) $(MANPAGES)
	$(INSTALL_PROGRAM) winc  $(INST_BINDIR)/winc
	$(INSTALL_PROGRAM) wincd  $(INST_BINDIR)/wincd
	$(INSTALL_PROGRAM) xwinc  $(INST_BINDIR)/xwinc
	$(INSTALL_DATA)    $(INCLFILE) $(INST_INCDIR)/$(INCLFILE)
	$(INSTALL_DATA)    $(LIB)      $(INST_LIBDIR)/$(LIB)
	$(INSTALL_DATA)    winc.1  $(INST_MANDIR)/winc.1
	$(INSTALL_DATA)    wincd.1  $(INST_MANDIR)/wincd.1
	$(INSTALL_DATA)    xwinc.1  $(INST_MANDIR)/xwinc.1

installdirs:
	-mkdir -p $(INST_BINDIR) $(INST_INCDIR) $(INST_LIBDIR) $(INST_MANDIR)

# list the files
list:
	@ls $(DISTFILES)

# list the writable files
rw:
	@ls -l $(DISTFILES) |\
		egrep '^[^l].w' | \
		sed 's;.* ;;'   # strip to last space

/tmp/wincvers: ALWAYS
	@expr "`egrep 'VERSION' private.h`" : \
		'.* \([0-9][0-9].*\)".*' >/tmp/wincvers
	@echo Current version is `cat /tmp/wincvers`

tarball:	$(DISTFILES) release_warnings /tmp/wincvers $(MANFORMATTED)
	echo winc-`cat /tmp/wincvers` > .fname
	rm -rf `cat .fname`
	mkdir `cat .fname`
	SRCDIR=. ;\
	test "$(srcdir)" = "\@srcdir\@" && SRCDIR=$(srcdir) ;\
	for file in $(DISTFILES) $(MANFORMATTED) ; do \
	  ln $$SRCDIR/$$file `cat .fname` \
	    || { echo copying $$file instead; cp -p $$file `cat .fname`; }; \
	done
	tar -czf `cat .fname`.tar.gz `cat .fname`
	@echo Created `cat .fname`.tar.gz 
	rm -rf `cat .fname` .fname

sharball:	$(DISTFILES) release_warnings /tmp/wincvers $(MANFORMATTED)
	echo winc-`cat /tmp/wincvers` > .fname
	rm -rf `cat .fname`
	mkdir `cat .fname`
	SRCDIR=. ;\
	test "$(srcdir)" = "\@srcdir\@" && SRCDIR=$(srcdir) ;\
	for file in $(DISTFILES) $(MANFORMATTED) ; do \
	  ln $$SRCDIR/$$file `cat .fname` \
	    || { echo copying $$file instead; cp -p $$file `cat .fname`; }; \
	done
	shar -spgf@foxharp.boston.ma.us -n`cat .fname` \
	    -o vileshar README $(MANFORMATTED) `ls $(DISTFILES) | \
	    sed '/^README$$/d'` ; \
	mv vileshar.01 `cat .fname`.shar ; \
	echo Created `cat .fname`.shar
	rm -rf `cat .fname` .fname

release_warnings:
	@echo
	@echo 'Do you need to update the version info in private.h?  it says:'
	@egrep 'VERSION|DATE' private.h
	@echo -n "Hit return to continue..."
	@read a

# include the auto-generated dependencies
files := $(foreach file, $(wildcard ./*.d), $(file))
ifneq ($(strip $(files)),)
    include $(files)
endif



ALWAYS:
