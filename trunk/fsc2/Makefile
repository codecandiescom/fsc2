# $Id$

# The following variables determine where the program files will be copied to
# by 'make install' as well as the name of the GPIB configuration and log file
# (set 'prefix' to './' for a local install)

prefix         = /usr/local
bindir         = $(prefix)/bin
libdir         = $(prefix)/lib/fsc2
auxdir         = $(libdir)/aux
docdir         = $(libdir)/doc
infodir        = /usr/local/info

GPIB_CONF      = /etc/gpib.conf
GPIB_LOG       = /tmp/gpib.log

# To whom bug reports and crash messages will get send

MAIL_ADDRESS   = Jens.Toerring@physik.fu-berlin.de


##############################################################################
# Here follow settings determining the make process - which compiler and
# tools to use as well as the respective options. Better leave them alone
# if you don't have a really good reason...
##############################################################################

SHELL          = /bin/sh
CC             = gcc
CFLAGS         = -ggdb -Wall -Wwrite-strings -Wstrict-prototypes \
				 -Wmissing-declarations -W
				 # -DMDEBUG # for malloc debugging
				 # -DDEBUG
				 # -DMAX_DEBUG # for debugging of digitizer
LD             = ld
LIBS           = -L/usr/X11R6/lib -L/usr/local/lib -lforms -lX11 -lgpib \
				 -lfl -lm -ldl
				 # -lmcheck   # for malloc debugging
				 # -lefence   # to run with efence
BISON          = bison
BISONFLAGS     = -d -v -t -p
FLEX           = flex
FLEXFLAGS      = -B -P
RM             = -rm
RMFLAGS        = -f
INSTALL        = install             # command for installing non-executables
EXEC_INSTALL   = install             # command for installing executables


############################################################################
###########     Don't change anything below this line      #################
########### (unless you really know what you're doing ...) #################
############################################################################

# Define variables with full paths for the 'mail' and 'addr2line' programs

MAIL_PROGRAM   = $(shell which mail)
ADDR2LINE      = $(shell which addr2line)

# Set up a make variable with the machines FQDN

machine_name   = $(shell hostname -f)

# Last time I checked the pion still had the old libc...

ifeq ($(machine_name),pion.anorg.chemie.uni-frankfurt.de)
	CFLAGS    += -DIS_STILL_LIBC1
endif

# Pass GPIB_LOG and GPIB_CONF and other stuff via macros to the source files
# For directories make sure they end with a `/'.

ifdef GPIB_LOG
    CFLAGS += -DGPIB_LOG=\"$(GPIB_LOG)\"
else
	CFLAGS += -DGPIB_LOG=\"/tmp/gpib.log\"
endif

ifdef GPIB_CONF
    CFLAGS += -DGPIB_CONF=\"$(GPIB_CONF)\"
else
    CFLAGS += -DGPIB_CONF=\"/etc/gpib.conf\"
endif

ifdef bindir
	CFLAGS += -Dbindir=\"$(bindir:/=)/\"
else
	CFLAGS += -Dbindir=\"$(shell pwd)/\"
endif

ifdef libdir
	CFLAGS += -Dlibdir=\"$(libdir:/=)/\"
else
	CFLAGS += -Dlibdir=\"$(shell pwd)/\"
endif

ifdef auxdir
	CFLAGS += -Dauxdir=\"$(auxdir:/=)/\" -I../aux
else
	CFLAGS += -Dauxdir=\"$(shell pwd)/aux/\" -I../aux
endif

ifdef docdir
	CFLAGS += -Ddocdir=\"$(docdir:/=)/\"
else
	CFLAGS += -Ddocdir=\"$(shell pwd)/doc/\"
endif

ifdef MAIL_ADDRESS
	CFLAGS += -DMAIL_ADDRESS=\"$(MAIL_ADDRESS)\"
endif

# Make sure the mailprogram and addr2line got found by which,
# otherwise don't set the variables

ifneq ($(word 1,$(MAIL_PROGRAM)),which:)
	CFLAGS += -DMAIL_PROGRAM=\"$(MAIL_PROGRAM)\"
endif

ifneq ($(word 1,$(ADDR2LINE)),which:)
	CFLAGS += -DADDR2LINE=\"$(ADDR2LINE)\"
endif


export            # export all variables to sub-makes


# Switch off all implicit rules

.SUFFIXES:
.PHONY: all install-strip install clean distclean pack \
        install-pion.anorg.chemie.uni-frankfurt.de     \
		install-crowley.physik.fu-berlin.de            \
		install-moebi.physik.fu-berlin.de              \
		install-door.physik.fu-berlin.de               \
		install-crowley.none


# default rule: make the main program, the primary lexer and the modules
# (the Makefile for modules is called via the Makefile in src)

all:
	$(MAKE) -C src all
	cp src/fsc2 .
	cp src/fsc2_clean .
	cp src/fsc2_connect .
	$(RM) $(RMFLAGS) fsc2_start fsc2_test fsc2_load
	ln -s fsc2_connect fsc2_start
	ln -s fsc2_connect fsc2_test
	ln -s fsc2_connect fsc2_load
	cp modules/*.so .
	$(MAKE) -C utils all
	$(MAKE) -C doc all


# Install the program in bindir and all the other stuff in libdir, auxdir
# and docdir. Finally create aliases for devices for the current machine.

install-strip:
	$(MAKE) EXEC_INSTALL='$(INSTALL) -s' install

install:
	$(INSTALL) -d $(bindir)
	$(INSTALL) -d $(libdir)
	$(INSTALL) -d $(auxdir)
	$(INSTALL) -d $(docdir)

	$(EXEC_INSTALL) -m 6755 -o fsc2 -g fsc2 fsc2 $(bindir)
	$(EXEC_INSTALL) -m 755 -o fsc2 -g fsc2 fsc2_clean $(libdir)
	$(EXEC_INSTALL) -m 644 -o fsc2 -g fsc2 *.so $(libdir)
	$(EXEC_INSTALL) -m 755 -o fsc2 -g fsc2 fsc2_connect $(bindir)

	$(RM) $(RMFLAGS) $(bindir)/fsc2_start $(bindir)/fsc2_test \
					 $(bindir)/fsc2_load
	ln -s $(bindir)/fsc2_connect $(bindir)/fsc2_start
	ln -s $(bindir)/fsc2_connect $(bindir)/fsc2_test
	ln -s $(bindir)/fsc2_connect $(bindir)/fsc2_load

	$(INSTALL) -m 644 -o fsc2 -g fsc2 Functions $(libdir)
	$(INSTALL) -m 644 -o fsc2 -g fsc2 Devices $(libdir)
	$(INSTALL) -m 644 -o fsc2 -g fsc2 aux/*.xpm $(auxdir)

	$(MAKE) -C utils install
	if `grep install-$(machine_name) ./Makefile > /dev/null`; then \
		$(MAKE) install-$(machine_name); \
	fi
	$(MAKE) -C scripts install
	$(MAKE) -C doc install

install-pion.anorg.chemie.uni-frankfurt.de:
	$(RM) $(RMFLAGS)  $(libdir)/dg2020.so $(libdir)/s_band.so \
					  $(libdir)/lockin.so $(libdir)/gaussmeter.so \
					  $(libdir)/magnet.so $(libdir)/pulser.so
	ln -s $(libdir)/dg2020_f.so $(libdir)/dg2020.so
	chown fsc2.fsc2 $(libdir)/dg2020.so
	ln -s $(libdir)/dg2020_f.so $(libdir)/pulser.so
	chown fsc2.fsc2 $(libdir)/pulser.so
	ln -s $(libdir)/aeg_s_band.so $(libdir)/s_band.so
	chown fsc2.fsc2 $(libdir)/s_band.so
	ln -s $(libdir)/aeg_s_band.so $(libdir)/magnet.so
	chown fsc2.fsc2 $(libdir)/magnet.so
	ln -s $(libdir)/bh15.so $(libdir)/gaussmeter.so
	chown fsc2.fsc2 $(libdir)/gaussmeter.so
	ln -s $(libdir)/sr510.so $(libdir)/lockin.so
	chown fsc2.fsc2 $(libdir)/lockin.so

install-crowley.physik.fu-berlin.de:
	$(INSTALL) -m 644 -o fsc2 -g fsc2 hp8647a.table $(libdir)
	$(RM) $(RMFLAGS) $(libdir)/gaussmeter.so $(libdir)/synthesizer.so \
					 $(libdir)/boxcar.so
	ln -s $(libdir)/er035m_sas.so $(libdir)/gaussmeter.so
	chown fsc2.fsc2 $(libdir)/gaussmeter.so
	ln -s $(libdir)/hp8647a.so $(libdir)/synthesizer.so
	chown fsc2.fsc2 $(libdir)/synthesizer.so
	ln -s $(libdir)/egg4402so $(libdir)/boxcar.so
	chown fsc2.fsc2 $(libdir)/boxcar.so

install-moebi.physik.fu-berlin.de:
	$(RM) $(RMFLAGS) $(libdir)/pulser.so $(libdir)/magnet.so \
					 $(libdir)/gaussmeter.so
	ln -s $(libdir)/dg2020_b.so $(libdir)/pulser.so
	chown fsc2.fsc2 $(libdir)/pulser.so
	ln -s $(libdir)/aeg_x_band.so $(libdir)/magnet.so
	chown fsc2.fsc2 $(libdir)/magnet.so
	ln -s $(libdir)/er035m_s.so $(libdir)/gaussmeter.so
	chown fsc2.fsc2 $(libdir)/gaussmeter.so

install-door.physik.fu-berlin.de:
	$(INSTALL) -m 644 -o fsc2 -g fsc2 hp8647a.table $(libdir)
	$(RM) $(RMFLAGS) $(libdir)/hfs9003.so $(libdir)/w_band.so
	ln -s $(libdir)/hfs9000.so $(libdir)/hfs9003.so
	ln -s $(libdir)/keithley228a.so $(libdir)/w_band.so

install-crowley.none:
	$(INSTALL) -m 644 -o fsc2 -g fsc2 hp8647a.table $(libdir)


# make all types of documentation

all-docs:
	$(MAKE) -C doc all

# make info documentation

info-docs:
	$(MAKE) -C doc info

# make html documentation

html-docs:
	$(MAKE) -C doc html

# make Postscript documentation

ps-docs:
	$(MAKE) -C doc ps

# make PDF documentation

pdf-docs:
	$(MAKE) -C doc pdf


# `clean' deletes all the files remaining from the compilation while
# `distclean' also removes all executables and shared libraries

clean:
	$(MAKE) -C src clean
	$(MAKE) -C modules clean
	$(MAKE) -C doc clean

distclean:
	$(MAKE) -C doc distclean
	$(MAKE) -C utils distclean
	$(MAKE) clean
	$(RM) $(RMFLAGS) fsc2 fsc2_clean fsc2_connect fsc2_start fsc2_test \
					 fsc2_load *.so *~

# Clean up everything for a distribution and create a zipped tarball

pack:
	$(MAKE) distclean
	cd ..; tar -c fsc2 | gzip -c -9 > fsc2.tgz

# Create zipped tarball and write to ZIP drive (at home with mount and
# unmount of ZIP drive otherwise assume that it's already mounted)

ZIP:
	$(MAKE) pack
	if [ `hostname -f` = "crowley.none" ]; then \
		if ! `grep ZIP /etc/mtab > /dev/null`; then \
			sleep 1; \
			mount /ZIP; \
		fi; \
		cp ../fsc2.tgz /ZIP; \
		umount /ZIP; \
	else \
		scp ../fsc2.tgz moebi:/ZIP; \
	fi
