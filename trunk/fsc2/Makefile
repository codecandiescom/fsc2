# $Id$

# The following variables determine where the program files will be copied to
# by 'make install' as well as the name of the GPIB configuration and log file
# together with its ownerships

prefix         = /usr/local
bindir         = $(prefix)/bin
libdir         = $(prefix)/lib/fsc2
auxdir         = $(libdir)/aux
docdir         = $(libdir)/doc

#prefix         = $(shell pwd)
#bindir         = $(prefix)
#libdir         = $(prefix)
#auxdir         = $(prefix)/aux
#docdir         = $(prefix)/doc

GPIB_CONF      = /etc/gpib.conf
GPIB_LOG       = /tmp/gpib.log

# here one defines if the program is for a high or a low resolution screen
# (set SIZE to either `HI_RES' or `LOW_RES')

SIZE           = LOW_RES


# The name of the awk tool is passed to the programs via a macro

AWK            = /usr/bin/awk


##############################################################################
# Here follow settings determining the make process - which compiler and
# tools to use as well as the respective options. Better leave them alone
# if you don't have really good reasons...
##############################################################################

SHELL          = /bin/sh
CC             = gcc
CFLAGS         = -ggdb -Wall -Wwrite-strings -Wstrict-prototypes \
				 -Wmissing-prototypes -Wmissing-declarations -W \
				 # -DDEBUG \
				 # -DMDEBUG # for malloc debugging
				 # -DMAX_DEBUG # for debugging of digitizer

LIBS           = -L/usr/X11R6/lib -lforms -lX11 -lgpib -lfl -lm -ldl \
				 #  -lmcheck   # for malloc debugging
LDFLAGS        = -Wl,-E
BISON          = bison
BISONFLAGS     = -d -v -t -p
FLEX           = flex
FLEXFLAGS      = -B -P
RM             = -rm
RMFLAGS        = -f


############################################################################
###########     Don't change anything below this line      #################
########### (unless you really know what you're doing ...) #################
############################################################################


# Pass GPIB_LOG, GPIB_CONF and AWK via macros to the source files
# Don't forget to escape all double quotes (`"') with backslashes.
# For directories we make sure they end with a `/'.

ifdef GPIB_LOG
    CFLAGS += -DGPIB_LOG=\"$(GPIB_LOG)\"
endif

ifdef GPIB_CONF
    CFLAGS += -DGPIB_CONF=\"$(GPIB_CONF)\"
endif

ifdef AWK
    CFLAGS += -DAWK=\"$(AWK)\"
endif

CFLAGS += $(patsubst %,-I%,$(subst :, ,$(VPATH)))

ifdef libdir
	CFLAGS += -Dlibdir=\"$(libdir:/=)/\"
endif

ifdef auxdir
	CFLAGS += -Dauxdir=\"$(auxdir:/=)/\" -I../aux
endif

ifdef docdir
	CFLAGS += -Ddocdir=\"$(docdir:/=)/\"
endif

export            # export all variables to sub-makes


# Switch off all implicit rules and set suffices of file to be made by make

.SUFFIXES:
.PHONY: all clean distclean pack


# default rule: make the main program, the primary lexer and the modules
# (the Makefile for modules is called via the Makefile in src)

all:
	$(MAKE) -C src all
	cp src/fsc2 .
	cp src/fsc2_clean .
	cp modules/*.so .
	$(MAKE) -C doc all


# Install the program in $bindir and all the other stuff in $libdir, $auxdir
# and $docdir. Finally (for some sites) create aliases for devices.

install:
	install -d $(bindir)
	install -m 6755 -o fsc2 -g fsc2 -s fsc2 $(bindir)
	install -d $(libdir)
	install -d $(auxdir)
	install -d $(docdir)
	install -m 755 -o fsc2 -g fsc2 -s fsc2_clean $(libdir)
	install -m 644 -o fsc2 -g fsc2 Functions $(libdir)
	install -m 644 -o fsc2 -g fsc2 Devices $(libdir)
	install -m 644 -o fsc2 -g fsc2  *.so $(libdir)
	install -m 644 -o fsc2 -g fsc2 hp8647a.table $(libdir)
	install -m 644 -o fsc2 -g fsc2 aux/*.xpm $(auxdir)
	install -m 444 -o fsc2 -g fsc2 doc/fsc2.html $(docdir)
	install -m 444 -o fsc2 -g fsc2 doc/fsc2_*.html $(docdir)
	install -m 444 -o fsc2 -g fsc2 doc/fsc2.ps $(docdir)
	install -m 444 -o fsc2 -g fsc2 doc/fsc2.pdf $(docdir)
	if [ `hostname --fqdn` = "pion.anorg.chemie.uni-frankfurt.de" ]; then \
        rm -rf $(libdir)/dg2020.so $(libdir)/s_band.so \
               $(libdir)/lockin.so $(libdir)/gaussmeter.so \
               $(libdir)/magnet.so $(libdir)/pulser.so \
			   $(libdir)/tds520c.so; \
		ln -s $(libdir)/dg2020_f.so $(libdir)/dg2020.so; \
		chown fsc2.fsc2 $(libdir)/dg2020.so; \
		ln -s $(libdir)/aeg_s_band.so $(libdir)/s_band.so; \
		chown fsc2.fsc2 $(libdir)/s_band.so; \
		ln -s $(libdir)/bh15.so $(libdir)/gaussmeter.so; \
		chown fsc2.fsc2 $(libdir)/gaussmeter.so; \
		ln -s $(libdir)/aeg_s_band.so $(libdir)/magnet.so; \
		chown fsc2.fsc2 $(libdir)/magnet.so; \
		ln -s $(libdir)/sr510.so $(libdir)/lockin.so; \
		chown fsc2.fsc2 $(libdir)/lockin.so; \
		ln -s $(libdir)/dg2020_f.so $(libdir)/pulser.so; \
		chown fsc2.fsc2 $(libdir)/pulser.so; \
		ln -s $(libdir)/tds520a.so $(libdir)/tds520c.so; \
		chown fsc2.fsc2 $(libdir)/tds520c.so; \
	fi
	if [ `hostname --fqdn` = "moebi.fu-berlin.de" ]; then \
		ln -s $(libdir)/dg2020_b.so $(libdir)/pulser.so; \
		chown fsc2.fsc2 $(libdir)/pulser.so; \
		ln -s $(libdir)/aeg_x_band.so $(libdir)/magnet.so; \
		chown fsc2.fsc2 $(libdir)/magnet.so; \
		ln -s $(libdir)/er035m_s.so $(libdir)/gaussmeter.so; \
		chown fsc2.fsc2 $(libdir)/gaussmeter.so; \
	fi


# make a tags file, needs still some changes to really work

tags:
	etags -T ${c_sources} *.flex *.y ${c_sources:.c=.h} $(headers)  \
		  ${modules} ${dg2020_f} dg2020_f.h ${dg2020_b} dg2020_b.h \
		  ${tds754a} tds754a.h ${tds744a} tds744a.h ${tds520a} tds520a.h \
		  ${tds520} tds520.h


# make all documentation

all-docs:
	$(MAKE) -C doc all

# make just the info documentation

info-docs:
	$(MAKE) -C doc info

# make just the html documentation

html-docs:
	$(MAKE) -C doc html

# make just the Postscript documentation

ps-docs:
	$(MAKE) -C doc ps

pdf-docs:
	$(MAKE) -C doc pdf

# `clean' deletes just all the files remaining from the compilation while
# `distclean' also removes all executables and shared libraries

clean:
	$(MAKE) -C src clean
	$(MAKE) -C modules clean
	$(MAKE) -C doc clean

distclean:
	$(MAKE) -C doc distclean
	$(MAKE) clean
	$(RM) $(RMFLAGS) fsc2 fsc2_clean *.so *~

pack:
	$(MAKE) distclean
	cd ..; tar -c fsc2a | gzip -c -9 > fsc2a.tgz

ZIP:
	if ! `grep ZIP /etc/mtab > /dev/null`; then \
	    sleep 1; \
		mount /ZIP; \
	fi
	cp ../fsc2a.tgz /ZIP
	umount /ZIP

# don't automatically delete intermediate files created by bison and flex

.PRECIOUS: %.c
