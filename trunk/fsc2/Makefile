# $Id$
#
# Copyright (C) 2001 Jens Thoms Toerring
#
# This file is part of fsc2.
#
# Fsc2 is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# Fsc2 is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with fsc2; see the file COPYING.  If not, write to
# the Free Software Foundation, 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.


# The following variables determine where the program, auxiliary files and
# the documentation will be copied to by 'make install'.

prefix         = /usr/local
bindir         = $(prefix)/bin
libdir         = $(prefix)/lib/fsc2
auxdir         = $(libdir)/aux
docdir         = $(prefix)/share/doc/fsc2
mandir         = $(prefix)/man
infodir        = $(prefix)/info


# The next variable tells the Makefile whether to compile the program for a
# high or a low resolution display (high is roughly 17" monitors and better),
# set it to either HIGH or LOW.

RESOLUTION     = HIGH


# The first variable sets the GPIB library to use, it must be set to either
# LLP, NI or JTT for the LLP library, the National Instruments library or
# the library written by me. Alternatively, if no GPIB support is needed
# define GPIB_LIBRARY as NONE.
# Then the location of the GPIB configuration file is set (this is only needed
# with the National Instruments GPIB library, if not set GPIB_CONF_FILE
# defaults to /etc/gpib.conf).
# Next set the file for writing logs about the activity on the GPIB bus.
# If not given logs will be written to stderr.
# Finally, set the verbosity level of the GPIB logs, use HIGH, MEDIUM, LOW
# or OFF. If not set it defaults to LOW, i.e. only errors will be logged.

GPIB_LIBRARY   = JTT
GPIB_CONF_FILE = /etc/gpib.conf
GPIB_LOG_FILE  = /tmp/gpib.log
GPIB_LOG_LEVEL = HIGH


# Here the owner of the program of all files that will be installed as well
# as the group the owner belongs to has to be set. It is highly recommended
# to create a special account and group for the program, with 'fsc2' being
# the natural choice. For a local install, i.e. if the program will be used
# by one person only and everything is going to be installed in his/her home
# directory this persons account and group can be used.

OWNER          = fsc2
GROUP          = fsc2


# Only leave the next line in if your machine does have a i386 type processor,
# i.e. a i386, i486, Pentium (Pro, II, III or 4) or something compatible like
# a K6, K7, Celeron, Duron, Athlon etc., otherwise comment it out or delete it.
# (Some stuff that helps me with debugging the program uses a bit of assembler,
# so it only works on processors with an Intel type architecture. Removing the
# following line doesn't influence the basic functionality, it just makes it a
# little bit harder to trace bugs...)

IS_I386        = 1


##############################################################################
# Here follow settings determining the make process - which compiler and
# tools to use as well as their respective options. Better leave this alone
# if you don't have a good reason...
##############################################################################

SHELL          = /bin/sh
CC             = gcc
CFLAGS         = -ggdb -Wall -Wwrite-strings -Wstrict-prototypes \
				 -Wmissing-declarations -W
				 # -DMDEBUG # for malloc debugging
				 # -DDEBUG
				 # -DMAX_DEBUG # for debugging of digitizer
LD             = ld
LIBS           = -L/usr/X11R6/lib -L/usr/local/lib -lforms -lX11 \
				 -lfl -lm -ldl
				 # -lmcheck   # for malloc debugging
				 # -lefence   # to run with efence
BISON          = bison
BISONFLAGS     = -d -v -t -p
FLEX           = flex
FLEXFLAGS      = -B -P
RM             = rm
RMFLAGS        = -f
LN             = ln
LNFLAGS        = -s
INSTALL        = install
EXEC_INSTALL   = install


############################################################################
###########     Don't change anything below this line      #################
########### (unless you really know what you're doing ...) #################
############################################################################


# To whom mail with bug reports and crash messages will get send

MAIL_ADDRESS   = Jens.Toerring@physik.fu-berlin.de


# Set up a variable with the machines FQDN

machine_name = $(shell hostname -f)


# Check that the machine has a new libc, i.e. at least version 2, otherwise
# some problems with the older libc have to be dealt with in the program

ifneq ($(shell ls /lib/libc.so.6),/lib/libc.so.6)
	CFLAGS += -DIS_STILL_LIBC1
endif


# Pass variables via macros to the source files
# For directories make sure they end with a `/'.

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


ifdef GPIB_LOG_FILE
	CFLAGS += -DGPIB_LOG_FILE=\"$(GPIB_LOG_FILE)\"
else
	CFLAGS += -DGPIB_LOG_FILE=\"/tmp/gpib.log\"
endif


ifeq ($(GPIB_LOG_LEVEL),HIGH)
	CFLAGS += -DGPIB_LOG_LEVEL=LL_ALL
else
	ifeq ($(GPIB_LOG_LEVEL),MEDIUM)
		CFLAGS += -DGPIB_LOG_LEVEL=LL_CE
	else
		ifeq ($(GPIB_LOG_LEVEL),OFF)
			CFLAGS += -DGPIB_LOG_LEVEL=LL_NONE
		else
			CFLAGS += -DGPIB_LOG_LEVEL=LL_ERR
		endif
	endif
endif

ifdef IS_I386
	CFLAGS += -DIS_I386
endif

ifdef MAIL_ADDRESS
	CFLAGS += -DMAIL_ADDRESS=\"$(MAIL_ADDRESS)\"
endif


# Define variables with full paths for the 'mail' and 'addr2line' programs
# and make sure the mail program and addr2line got found by 'which', otherwise
# don't set the variables

MAIL_PROGRAM   = $(shell which mail)
ADDR2LINE      = $(shell which addr2line)

ifneq ($(word 1,$(MAIL_PROGRAM)),which:)
	CFLAGS += -DMAIL_PROGRAM=\"$(MAIL_PROGRAM)\"
endif

ifneq ($(word 1,$(ADDR2LINE)),which:)
	CFLAGS += -DADDR2LINE=\"$(ADDR2LINE)\"
endif


# Here's some stuff that makes it more convenient to make and install on
# machines that I know well enough - for these machines I don't have
# to edit the Makefile all the time...

INSTALL_MACHINE = install-unknown

ifeq ($(machine_name),pion.anorg.chemie.uni-frankfurt.de)
	RESOLUTION      = HIGH
	GPIB_LIBRARY    = LLP
	INSTALL_MACHINE = install-$(machine_name)
endif

ifeq ($(machine_name),moebi.physik.fu-berlin.de)
	RESOLUTION      = LOW
	GPIB_LIBRARY    = JTT
	INSTALL_MACHINE = install-$(machine_name)
endif

ifeq ($(machine_name),crowley.physik.fu-berlin.de)
	RESOLUTION      = HIGH
	GPIB_LIBRARY    = JTT
	INSTALL_MACHINE = install-$(machine_name)
endif

ifeq ($(machine_name),door.physik.fu-berlin.de)
	RESOLUTION      = LOW
	GPIB_LIBRARY    = JTT
	INSTALL_MACHINE = install-$(machine_name)
endif

ifeq ($(machine_name),isaak.physik.fu-berlin.de)
	RESOLUTION      = HIGH
	GPIB_LIBRARY    = JTT
	INSTALL_MACHINE = install-$(machine_name)
endif

ifeq ($(machine_name),crowley.none)
	 RESOLUTION      = HIGH
	 GPIB_LIBRARY    = JTT
	 INSTALL_MACHINE = install-$(machine_name)
endif


# Finally the definition of remaining variables that are needed...

ifeq ($(RESOLUTION),HIGH)
	RESOLUTION   =  hr
	CFLAGS      += -DSIZE=HI_RES
else
	RESOLUTION   =  lr
	CFLAGS      += -DSIZE=LOW_RES
endif

ifeq ($(GPIB_LIBRARY),LLP)
	GPIB_LIBRARY = old
	LIBS += -lgpib
else
	ifeq ($(GPIB_LIBRARY),NI)
		GPIB_LIBRARY = ni
		LIBS += -lgpib
		ifdef GPIB_CONF_FILE
			CFLAGS += -DGPIB_CONF_FILE=\"$(GPIB_CONF_FILE)\"
		else
			CFLAGS += -DGPIB_CONF_FILE=\"/etc/gpib.conf\"
		endif
	else
		ifeq ($(GPIB_LIBRARY),JTT)
			GPIB_LIBRARY = jtt
			LIBS += -lgpib
		else
			GPIB_LIBRARY = none
		endif
	endif
endif


export            # export all variables to sub-makes


# Switch off all implicit rules

.SUFFIXES:
.PHONY: all install-strip install clean distclean pack \
		all-docs info-docs html-docs ps-docs pdf-docs  \
        install-pion.anorg.chemie.uni-frankfurt.de     \
		install-crowley.physik.fu-berlin.de            \
		install-moebi.physik.fu-berlin.de              \
		install-door.physik.fu-berlin.de               \
		install-isaak.physik.fu-berlin.de              \
		install-crowley.none                           \
		ZIP


# Default rule: make the main program, the primary lexer and the modules
# (the Makefile for modules is called via the Makefile in src)

all:
	$(MAKE) -C src all
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

	$(EXEC_INSTALL) -m 6755 -o $(OWNER) -g $(GROUP) src/fsc2 $(bindir)
	$(EXEC_INSTALL) -m 755  -o $(OWNER) -g $(GROUP) src/fsc2_clean $(libdir)
	$(EXEC_INSTALL) -m 644  -o $(OWNER) -g $(GROUP) modules/*.so $(libdir)
	$(EXEC_INSTALL) -m 755  -o $(OWNER) -g $(GROUP) src/fsc2_connect $(bindir)

	-$(RM) $(RMFLAGS) $(bindir)/fsc2_start $(bindir)/fsc2_test \
					  $(bindir)/fsc2_load
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_start
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_test
	$(LN) $(LNFLAGS) $(bindir)/fsc2_connect $(bindir)/fsc2_load

	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) config/Functions $(libdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) config/Devices $(libdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) aux/*.xpm $(auxdir)

	$(MAKE) -C utils install
	$(MAKE) -C scripts install
	$(MAKE) -C doc install
	if [ "$(INSTALL_MACHINE)" = "install-unknown" ]; then  \
		$(MAKE) $(INSTALL_MACHINE);                        \
	fi


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
	-$(RM) $(RMFLAGS) fsc2 fsc2_clean fsc2_connect fsc2_start fsc2_test \
					  fsc2_load *.so *~


# Clean up everything for a distribution and create a zipped tarball

pack:
	$(MAKE) distclean
	cd ..; tar -c fsc2 | gzip -c -9 > fsc2.tgz


# Here links of module libraries for often used devices for the different
# spectrometers are created. If, for example, the spectrometer always uses
# the aeg_x_band.so as the driver for the magnet power supply, by creating
# a link with the name magnet.so allows to use 'magnet' as an alternative name
# for the power supply in EDL programs. Don't forget to delete already
# existing links before creating new ones!

install-pion.anorg.chemie.uni-frankfurt.de:
	-$(RM) $(RMFLAGS) $(libdir)/dg2020.so $(libdir)/s_band.so     \
					  $(libdir)/lockin.so $(libdir)/gaussmeter.so \
					  $(libdir)/magnet.so $(libdir)/pulser.so
	$(LN) $(LNFLAGS)  $(libdir)/dg2020_f.so $(libdir)/dg2020.so
	chown $(OWNER).$(GROUP) $(libdir)/dg2020.so
	$(LN) $(LNFLAGS) $(libdir)/dg2020_f.so $(libdir)/pulser.so
	chown $(OWNER).$(GROUP) $(libdir)/pulser.so
	$(LN) $(LNFLAGS) $(libdir)/aeg_s_band.so $(libdir)/s_band.so
	chown $(OWNER).$(GROUP) $(libdir)/s_band.so
	$(LN) $(LNFLAGS) $(libdir)/aeg_s_band.so $(libdir)/magnet.so
	chown $(OWNER).$(GROUP) $(libdir)/magnet.so
	$(LN) $(LNFLAGS) $(libdir)/bh15.so $(libdir)/gaussmeter.so
	chown $(OWNER).$(GROUP) $(libdir)/gaussmeter.so
	$(LN) $(LNFLAGS) $(libdir)/sr510.so $(libdir)/lockin.so
	chown $(OWNER).$(GROUP) $(libdir)/lockin.so

install-crowley.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/gaussmeter.so $(libdir)/synthesizer.so \
					  $(libdir)/boxcar.so
	$(LN) $(LNFLAGS) $(libdir)/er035m_sas.so $(libdir)/gaussmeter.so
	chown $(OWNER).$(GROUP) $(libdir)/gaussmeter.so
	$(LN) $(LNFLAGS) $(libdir)/hp8647a.so $(libdir)/synthesizer.so
	chown $(OWNER).$(GROUP) $(libdir)/synthesizer.so
	$(LN) $(LNFLAGS) $(libdir)/egg4402so $(libdir)/boxcar.so
	chown $(OWNER).$(GROUP) $(libdir)/boxcar.so
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) hp8647a.table $(libdir)

install-moebi.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/pulser.so $(libdir)/magnet.so \
					  $(libdir)/gaussmeter.so
	$(LN) $(LNFLAGS) $(libdir)/dg2020_b.so $(libdir)/pulser.so
	chown fsc2.fsc2 $(libdir)/pulser.so
	$(LN) $(LNFLAGS) $(libdir)/aeg_x_band.so $(libdir)/magnet.so
	chown fsc2.fsc2 $(libdir)/magnet.so
	$(LN) $(LNFLAGS) $(libdir)/er035m_s.so $(libdir)/gaussmeter.so
	chown fsc2.fsc2 $(libdir)/gaussmeter.so

install-door.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/hfs9003.so $(libdir)/w_band.so \
					  $(libdir)/magnet.so
	$(LN) $(LNFLAGS) $(libdir)/hfs9000.so $(libdir)/hfs9003.so
	chown fsc2.fsc2 $(libdir)/hfs9003.so
	$(LN) $(LNFLAGS) $(libdir)/keithley228a.so $(libdir)/w_band.so
	chown fsc2.fsc2 $(libdir)/w_band.so
	$(LN) $(LNFLAGS) $(libdir)/keithley228a.so $(libdir)/magnet.so
	chown fsc2.fsc2 $(libdir)/magnet.so
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) hp8647a.table $(libdir)

install-isaak.physik.fu-berlin.de:
	-$(RM) $(RMFLAGS) $(libdir)/pulser.so $(libdir)/magnet.so \
					  $(libdir)/gaussmeter.so
	$(LN) $(LNFLAGS) $(libdir)/dg2020_b.so $(libdir)/pulser.so
	chown fsc2.fsc2 $(libdir)/pulser.so
	$(LN) $(LNFLAGS) $(libdir)/aeg_x_band.so $(libdir)/magnet.so
	chown fsc2.fsc2 $(libdir)/magnet.so
	$(LN) $(LNFLAGS) $(libdir)/er035m_s.so $(libdir)/gaussmeter.so
	chown fsc2.fsc2 $(libdir)/gaussmeter.so

install-crowley.none:
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) hp8647a.table $(libdir)


# Create zipped tarball and write it to the ZIP drive (at home or on moebi
# also mount and unmount the ZIP drive otherwise assume that it's already
# mounted)

ZIP:
	$(MAKE) pack
	if [    "$(machine_name)" = "crowley.none"                       \
		 -o "$(machine_name)" = "moebi.physik.fu-berlin.de" ]; then  \
		if ! `grep ZIP /etc/mtab > /dev/null`; then                  \
			sleep 1;                                                 \
			mount /ZIP;                                              \
		fi;                                                          \
		cp ../fsc2.tgz /ZIP;                                         \
		umount /ZIP;                                                 \
	else                                                             \
		scp ../fsc2.tgz moebi.physik.fu-berlin.de:/ZIP;              \
	fi
