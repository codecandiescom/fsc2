#  $Id$
#
#  Copyright (C) 1999-2004 Jens Thoms Toerring
#
#  This file is part of fsc2.
#
#  Fsc2 is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2, or (at your option)
#  any later version.
#
#  Fsc2 is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with fsc2; see the file COPYING.  If not, write to
#  the Free Software Foundation, 59 Temple Place - Suite 330,
#  Boston, MA 02111-1307, USA.


#########################################################################
#                                                                       #
#  In this first section several variables can be set to configure      #
#  fsc2. Please read the comments carefully and change the variables    #
#  if necessary. Please take care, most settings are case-sensitive!    #
#                                                                       #
#  Also consider using a configuration file for your machine in the     #
#  'machines' subdirectory. You can simply copy the 'template.nowhere'  #
#  file in this directory to a file with the fully qualified host name  #
#  of your machine (that's what 'hostname -f' returns when you enter    #
#  it at the command line) and then edit this file. Then, whenever you  #
#  want to install a new version of fsc2 you only have to copy this     #
#  file instead of editing the Makefile each time. Please also note     #
#  that when such a configuration file for your machine exists its      #
#  content will override everything you set here!                       #
#                                                                       #
#########################################################################


# Please note: When in the following it is said that you have to "comment
# out a variable" this simply means that you have to put a hash character
# ('#') at the start of the line where the variable is set, effectively
# making that line invisible for the progam reading this file.

# The following variable determines under which directory the program and
# its auxiliary files will be installed (defaults to /usr/local)

prefix             := /usr/local


# Next the owner and the group of fsc2 all files that will be installed has
# to be set. I would recommended to create a special account for the program,
# with 'fsc2' being the natural choice (which is also the default). The group
# that owns the files, especially the executable fsc2 should be one that
# allows access to the device files. See the longer discussion in section B
# of the INSTALL file.

OWNER              := fsc2
GROUP              := uucp


# Now set the system wide path for files to be included by EDL files when
# after the '#INCLUDE' directive a file name enclosed in '<' and '>' is
# found. This is for the case where a system wide set of EDL files has been
# set up from which users may use include files without going to the hassle
# of always having to write out the full path. If no such repository exists
# it's save to leave this variable undefined.

# DEF_INCL_DIR       := /scratch/EDL/includes


# The next variable defines the GPIB library to use, it must be set to either
# 'LLP' for the LLP library, 'NI' for the National Instruments library or
# 'JTT' for the library written by me. Alternatively, if no GPIB support is
# needed, define GPIB_LIBRARY as 'NONE' (which is the default if not set).

GPIB_LIBRARY       := JTT


# Here the location of the GPIB configuration file is set (this usually is
# needed for the National Instruments GPIB library only, if not set
# GPIB_CONF_FILE defaults to /etc/gpib.conf).

GPIB_CONF_FILE     := /etc/gpib.conf


# Next set the file for writing logs about the activity on the GPIB bus.
# Make sure that fsc2 or its users have write access to the directory the
# log file will be created in. Don't use a partition on which you don't have
# at least a few megabytes to spare, depending on the type of experiments
# you will do the file can become rather long. But the file will not grow
# indefinitely, if you start a new experiment its previous contents gets
# overwritten. If no file is specified logs will be written to stderr.

GPIB_LOG_FILE      := /tmp/gpib.log


# The next variable, GPIB_LOG_LEVEL, sets the verbosity level for the GPIB
# logs, use either 'HIGH', 'MEDIUM', 'LOW' or 'OFF'. When set to 'HIGH' all
# GPIB function calls together with all data send to or received from devices
# are stored. When set to 'MEDIUM' only GPIB function calls are logged. When
# set to 'LOW' only errors messages will be written to the log file. If not
# set it defaults to 'LOW'.

GPIB_LOG_LEVEL     := HIGH


# Finally, if the header files for the GPIB library aren't in either
# /usr/include or /usr/local/include (e.g. if they are in /usr/include/gpib)
# set the variable GPIB_HEADER_DIR to the absolute path of the directory
# where the header file(s) for the library reside.

# GPIB_HEADER_DIR    := /usr/include/gpib


# NUM_SERIAL_PORTS defines the number of serial ports that are available on
# the computer (or how many you want to allow fsc2 to use) and defaults to 0
# if not set (in which case no serial ports can be used).

NUM_SERIAL_PORTS   := 2


# The second variable, SERIAL_LOCK_DIR, is the directory where UUCP style
# lock files for the serial ports are created. This should be '/var/lock'
# according to version 2.2 of the File-system Hierachy Standard. Either fsc2
# itself or all its users must have read and write permissions for this
# directory. Also see the longer discussion in section C of the INSTALL file.

SERIAL_LOCK_DIR    := /var/lock


# Next set the file for writing logs about the activity on the serial port.
# Make sure that fsc2 or its users have write access to the directory the
# log file will be created in. Don't use a partition on which you don't have
# at least a few megabytes to spare, depending on the type of experiments
# you will do the file can become rather long. But the file will not grow
# indefinitely, if you start a new experiment its previous contents gets
# overwritten. If no file is specified logs will be written to stderr.

SERIAL_LOG_FILE    := /tmp/serial.log


# The next variable, SERIAL_LOG_LEVEL, sets the verbosity level for the logs
# for the serial port, use either 'HIGH', 'MEDIUM', 'LOW' or 'OFF'. When set
# to 'HIGH' all serial port function calls together with all data send to or
# received from devices are stored. When set to 'MEDIUM' only serial port
# function calls are logged. When set to 'LOW' only errors messages will be
# written to the log file. If not set it defaults to 'LOW'.

SERIAL_LOG_LEVEL   := HIGH


# Define the default editor to use for editing when the "Edit" button gets
# pressed or for writing a bug report  (but a user can still override this
# by setting an environment variable named 'EDITOR')

EDITOR             := vi


# Define the default browser to use when the "Help" button gets pressed and
# the manual is to be shown, currently supported are netscape, mozilla,
# opera, konqueror, galeon, lnyx and w3m (but a user can still override this
# setting an environment variable named 'BROWSER').

BROWSER            := netscape


# Both the next two lines must be uncommented if you want support for the
# National Instruments 6601 GPCT card. If the library for the card isn't in a
# place where the linker will find it automatically (e.g. '/usr/lib' and
# '/usr/local/lib') you need to specify the full absolute path for the library
# (e.g. '/usr/xxx/yyy/libni6601.so'). You must make sure that the library for
# the card is already properly installed if you want to build fsc2 with
# support for the card.

# WITH_NI6601        := yes
# ni6601_libs        := libni6601.so


# Uncommented both the next two lines if you want support for the Meilhaus
# Electronics 6000 or 6100 DAC card. If the library for the card isn't in a
# place where the linker will find it automatically (e.g. '/usr/lib' and
# '/usr/local/lib') you need to specify the full absolute path for the
# library  (e.g. '/opt/zzz/ttt/libme6x00.so'). You must make sure that the
# library for the card is already properly installed if you want to build
# fsc2 with support for the card.

# WITH_ME6000        := yes
# me6000_libs        := libme6x00.so


# Both the next two lines must be uncommented if you want support for the
# Wasco WITIO-48 DIO card. If the library for the card isn't in a place where
# the linker will find it automatically (e.g. '/usr/lib' and '/usr/local/lib')
# you need to specify the full absolute path for the library (e.g.
# '/usr/xxx/yyy/libwitio_48.so'). You must make sure that the library for the
# card is already properly installed if you want to build fsc2 with support
# for the Wasco WITIO-48 DIO card.

# WITH_WITIO_48      := yes
# witio_48_libs      := libwitio_48.so


# All of the following three lines must be uncommented when you want support
# for the Roper Scientific Spec-10 CCD camera. Depending on where the header
# files required by the PVCAM library are installed (which is to compile the
# module for the the camera) you may have to change the 'rs_spec10_incl'
# variable to the name of the installation directory. The third variable,
# 'rs_spec10_libs', must be changed if the library isn't in a place where the
# linker will find it automatically (e.g. '/usr/lib' and '/usr/local/lib'). In
# this case it must be the full absolute path of the library (for example
# '/usr/lib/pvcam/libpvcam.so'). You must make sure that the header files and
# the library for the camera are already properly installed before you build
# fsc2 with support for the camera.

# WITH_RS_SPEC10     := yes
# rs_spec10_incl     := /usr/local/include/pvcam
# rs_spec10_libs     := libpvcam.so


# Both the next two lines must be uncommented if you want support for the
# National Instruments PCI-MIO-16E-1 card. If the library for the card isn't
# in a place where the linker will find it automatically (e.g. '/usr/lib' and
# '/usr/local/lib') you need to specify the full absolute path for the library
# (e.g. '/usr/xxx/yyy/libni_daq.so'). You must make sure that the library for
# the card is already properly installed if you want to build fsc2 with
# support for the card.

# WITH_PCI_MIO_16E_1 := yes
# pci_mio_16e_1_libs := libni_daq.so


# The next line must be uncommented if you want support for the RULBUS
# (Rijksuniversiteit Leiden BUS) and the devices connected to it. If the
# include file for the library isn't in one of the standard places (i.e.
# '/usr/include' or '/usr/local/include') you must also uncomment the line
# starting with 'rulbus_incl_path and specify the complete path to the place
# where it installed (e.g. '/usr/xxx/yyy). If the library for the card isn't
# in a place where the linker will find it automatically (e.g. '/usr/lib' and
# '/usr/local/lib') you need also to specify the full absolute path for the
# library by uncommenting the line starting with 'rulbus_lib_path and change
# the what it gets set to . You must make sure that the library for the Rulbus
# interface is already properly installed if you want to build fsc2 with
# support for it.

# WITH_RULBUS        := yes
# rulbus_incl_path   := /usr/lib/include
# rulbus_lib_path    := /usr/lib/local


# Comment out the next two lines if you don't want the ability to run a HTTP
# server that allows to monitor fsc2s current state via the Internet. The
# default port number the HTTP server will listen on can be set by the second
# variable, DEFAULT_HTTP_PORT, which must be a  number between 1024 and 65535.

WITH_HTTP_SERVER   := yes
DEFAULT_HTTP_PORT  := 8080


# Uncomment the following line if you don't want the program to send me an
# email with a report when it crashes.

# NO_MAIL            := yes


# Normally, fsc2 will use the systems 'mail' command when it needs to send
# emails. This requires that a mail transport agent (MTA) like sendmail is
# installed on the system (i.e. sendmail or a similar program is installed,
# but not necessarily be running, waiting for incoming connections).
# Otherwise, when no MTA is installed a somewhat rudimentary mail transfer
# agent built into fsc2 can be used instead. If you want to use fsc2s MTA
# (e.g. because sendmail isn't installed on your machine) uncomment the
# following line. In this case you must also uncomment the line defining
# the USE_IPv6 variable if your machine uses the new IP address scheme with
# 128 bit addresses instead of the traditional one with only 32 bit addresses.

# USE_FSC2_MTA       := yes
# USE_IPv6           := yes


# This last variable controls to whom emails with bug reports and about
# crashes will be send - if it's commented out no "Bug report" button will
# be shown. Please change this if you change the program yourself (in which
# case sending bug reports or emails about crashes to me wouldn't make too
# much sense).

MAIL_ADDRESS       := Jens.Toerring@physik.fu-berlin.de


##############################################################################
#
# Here follow settings determining the make process - which compiler and
# tools to use as well as their respective options. Better leave this alone
# if you don't have a good reason to apply changes (but you will probably
# need to do so if you don't use gcc)
#
# Don't use '-fomit-frame-pointer' in CFLAGS on Intel processors (unless
# you want to keep me from getting meaningful messages about crashes).
#
##############################################################################


# Set the optimization level

OPTIMIZATION     := -O2


# Define this in order to use mpatrol

#MPATROL          := YES


# Set where the program, the libraries etc. get installed

ifndef prefix
	prefix := /usr/local
endif

bindir           := $(prefix)/bin
libdir           := $(prefix)/lib/fsc2
auxdir           := $(libdir)/aux
docdir           := $(prefix)/share/doc/fsc2
mandir           := $(prefix)/man
infodir          := $(prefix)/info


# These variables are used internally in the make process

fdir             := $(shell pwd)
sdir             := $(fdir)/src
mdir             := $(fdir)/modules
adir             := $(fdir)/aux
udir             := $(fdir)/utils
ddir             := $(fdir)/doc
cdir             := $(fdir)/config
ldir             := $(fdir)/Fcntl_Lock
mchdir           := $(fdir)/machines


SHELL            := /bin/sh
CC               := gcc
CFLAGS           := -Wall                   \
					-Wwrite-strings         \
					-Wstrict-prototypes     \
					-Wmissing-prototypes    \
					-Wmissing-declarations  \
					-W                      \
					-Wshadow                \
					-Wpointer-arith         \
					-Waggregate-return      \
					-Wnested-externs        \
					-Wcast-align            \
				    # -Wconversion
				    # -Wsign-compare      # missing in older gcc versions
				    # -Wbad-function-cast # too many false positives
				    # -Wcast-qual         # too many false positives
				    # -Wundef             # fails for bison/flex created files
				    # -ansi               # fails on inline/asm
				    # -pedantic           # ditto


# If make gets started with 'make debug' switch off optimization and enable
# built-in memory debugging

ifeq ($(findstring debug,$(MAKECMDGOALS)),debug)
	CFLAGS += -O0
	CFLAGS += -DFSC2_MDEBUG               # for built-in malloc debugging
else
	CFLAGS += $(OPTIMIZATION)
endif


# If make gets stated with 'make release' switch off all debugging code
# (i.e. mostly assert() calls etc.). Otherwise enable debug symbols in
# code and libc memory debugging (if available)

ifneq ($(MAKECMDGOALS),release)
	CFLAGS += -ggdb
else
	CFLAGS += -DNDEBUG                    # switch off all debugging code
	ifeq ($(shell ls /lib/libc.so.6 2>/dev/null),/lib/libc.so.6)
		CFLAGS += -DLIBC_MDEBUG           # for libc memory debugging
	endif
endif


# If make gets started with 'make mdebug' enable both internal memory
# debugging options as well as libc memory debugging (if available)

ifeq ($(MAKECMDGOALS),mdebug)
	CFLAGS += -DFSC2_MDEBUG               # for built-in malloc debugging
	ifeq ($(shell ls /lib/libc.so.6 2>/dev/null),/lib/libc.so.6)
		CFLAGS += -DLIBC_MDEBUG           # for libc memory debugging
	endif
endif


INCLUDES := -I$(fdir) -I$(adir) -I/usr/X11R6/include \
			-I/usr/X11R6/include/X11 -I/usr/local/include \

LFLAGS	 := $(OPT) -shared -fpic  # needed for libraries


ifeq ($(MPATROL),YES)
	CONFFLAGS := -DMPATROL
	LIBS      := -L/usr/local/lib -lmpatrol -lelf -L/usr/X11R6/lib -lforms \
				  -lX11 -lXpm -lm -ldl
else
	CONFFLAGS :=
	LIBS      := -L/usr/X11R6/lib -lforms -lX11 -lXpm -lm -ldl
				 # -lmcheck   # for malloc debugging
				 # -lefence   # to run with efence
endif

BISON         := bison
BISONFLAGS    := -d -v -t -p
FLEX          := flex
FLEXFLAGS     := -B -P
RM            := rm
RMFLAGS       := -f
LN            := ln
LNFLAGS       := -s
INSTALL       := install
EXEC_INSTALL  := install


############################################################################
###########                                                #################
###########   Don't change anything below this line        #################
###########  unless you really know what you're doing ...  #################
###########                                                #################
############################################################################


tagsfile      := $(fdir)/TAGS


# Check that the machine has a new libc, i.e. at least version 2 (which means
# that /lib/libc.so.6 should exist), otherwise some problems with the older
# libc have to be dealt with from within the program

ifneq ($(shell ls /lib/libc.so.6 2>/dev/null),/lib/libc.so.6)
	CONFFLAGS += -DIS_STILL_LIBC1
endif


# Set up a variable with the machines FQDN - if necessary follow symbolic
# links (in cases where the machine has been renamed and the files in the
# machines subdirectory are still for the old name but a symbolic link has
# been set from the new machine name to the old one)

machine_name  := $(shell                                 \
					mn=`hostname -f 2>/dev/null`;        \
					if [    -n $$mn                      \
						 -a -L $(mchdir)/$$mn ]; then    \
						ls -l $(mchdir)/$$mn |           \
						sed 's/^.*->[ \t]\+\(.*\)/\1/';  \
					else                                 \
						echo $$mn;                       \
					fi; )

# Try to include a configuration file for machines that are listed in the
# 'machines' subdirectory. This makes configuring a new fsc2 version for a
# certain machine much simpler since only the configuration file must be
# copied instead of always having to edit this Makefile. The variable
# 'MACHINE_INCLUDE' gets set to the name of the machine specific file
# to be included (if one exists, and only then we can include it;-) in
# order to be able to automatically recompile when the file changed.

MACHINE_INCLUDE := $(shell ls $(mchdir)/$(machine_name) 2>/dev/null)
ifneq ($(MACHINE_INCLUDE),)
	include $(MACHINE_INCLUDE)
endif


# Pass lots of variables to the source files...
# For directories make sure they end with a '/'.

CONFFLAGS += -Dsrcdir=\"$(sdir:/=)/\"
CONFFLAGS += -Dbindir=\"$(bindir:/=)/\"
CONFFLAGS += -Dlibdir=\"$(libdir:/=)/\"
CONFFLAGS += -Dauxdir=\"$(auxdir:/=)/\"
CONFFLAGS += -Ddocdir=\"$(docdir:/=)/\"


# Make sure settings for GPIB bus are reasonable

ifdef GPIB_LIBRARY
	GPIB_LIBRARY := $(patsubst % ,%,$(GPIB_LIBRARY))
endif

ifdef GPIB_LOG_LEVEL
	GPIB_LOG_FILE := $(patsubst % ,%,$(GPIB_LOG_LEVEL))
endif

ifneq ($(GPIB_LIBRARY),NONE)
	ifdef GPIB_HEADER_DIR
		INCLUDES         += -I$(GPIB_HEADER_DIR)
	endif

	ifdef GPIB_CONF_FILE
		CONFFLAGS += -DGPIB_CONF_FILE=\"$(patsubst % ,%,$(GPIB_CONF_FILE))\"
	else
		CONFFLAGS += -DGPIB_CONF_FILE=\"/etc/gpib.conf\"
	endif
endif

ifdef GPIB_LOG_FILE
	CONFFLAGS += -DGPIB_LOG_FILE=\"$(patsubst % ,%,$(GPIB_LOG_FILE))\"
else
	CONFFLAGS += -DGPIB_LOG_FILE=\"/tmp/gpib.log\"
endif

ifeq ($(GPIB_LOG_LEVEL),HIGH)
	CONFFLAGS += -DGPIB_LOG_LEVEL=LL_ALL
else
	ifeq ($(GPIB_LOG_LEVEL),MEDIUM)
		CONFFLAGS += -DGPIB_LOG_LEVEL=LL_CE
	else
		ifeq ($(GPIB_LOG_LEVEL),OFF)
			CONFFLAGS += -DGPIB_LOG_LEVEL=LL_NONE
		else
			CONFFLAGS += -DGPIB_LOG_LEVEL=LL_ERR
		endif
	endif
endif

ifeq ($(GPIB_LIBRARY),LLP)
	GPIB_LIBRARY         := llp
	ifndef GPIB_HEADER_DIR
		INCLUDES         += -I/usr/local/include/gpib
	endif
	LIBS                 += -lfl
else
	ifeq ($(GPIB_LIBRARY),NI)
		GPIB_LIBRARY     := ni
	else
		ifeq ($(GPIB_LIBRARY),JTT)
			GPIB_LIBRARY := jtt
		else
			GPIB_LIBRARY := none
		endif
	endif
endif

LIBS                     += -lgpib


# Make sure settings for serial port are reasonable

ifdef NUM_SERIAL_PORTS
	CONFFLAGS += -DNUM_SERIAL_PORTS=$(patsubst % ,%,$(NUM_SERIAL_PORTS))
else
	CONFFLAGS += -DNUM_SERIAL_PORTS=0
endif

ifdef SERIAL_LOCK_DIR
	CONFFLAGS += -DSERIAL_LOCK_DIR=\"$(patsubst % ,%,$(SERIAL_LOCK_DIR))\"
endif

ifdef SERIAL_LOG_FILE
	CONFFLAGS += -DSERIAL_LOG_FILE=\"$(patsubst % ,%,$(SERIAL_LOG_FILE))\"
else
	CONFFLAGS += -DSERIAL_LOG_FILE=\"/tmp/serial.log\"
endif

ifdef SERIAL_LOG_LEVEL
	SERIAL_LOG_FILE = $(patsubst % ,%,$(SERIAL_LOG_LEVEL))
endif

ifeq ($(SERIAL_LOG_LEVEL),HIGH)
	CONFFLAGS += -DSERIAL_LOG_LEVEL=LL_ALL
else
	ifeq ($(SERIAL_LOG_LEVEL),MEDIUM)
		CONFFLAGS += -DSERIAL_LOG_LEVEL=LL_CE
	else
		ifeq ($(SERIAL_LOG_LEVEL),OFF)
			CONFFLAGS += -DSERIAL_LOG_LEVEL=LL_NONE
		else
			CONFFLAGS += -DSERIAL_LOG_LEVEL=LL_ERR
		endif
	endif
endif


# Make sure RULBUS settings are reasonable

ifdef WITH_RULBUS
	ifdef rulbus_incl_path
		INCLUDES += -I$(rulbus_incl_path)
	endif
	ifdef rulbus_lib_path
		LIBS += -L$(rulbus_lib_path)
	endif
	LIBS      += -lrulbus
	CONFFLAGS += -DWITH_RULBUS
endif


ifdef WITH_HTTP_SERVER
	ifdef DEFAULT_HTTP_PORT
		CONFFLAGS += -DWITH_HTTP_SERVER \
					  -DDEFAULT_HTTP_PORT=$(DEFAULT_HTTP_PORT)
	else
		CONFFLAGS += -DWITH_HTTP_SERVER -DDEFAULT_HTTP_PORT=8080
	endif
endif


ifdef EDITOR
	CONFFLAGS += -DEDITOR=\"$(patsubst % ,%,$(EDITOR))\"
endif


ifdef BROWSER
	CONFFLAGS += -DBROWSER=\"$(patsubst % ,%,$(BROWSER))\"
endif


ifdef DEF_INCL_DIR
	CONFFLAGS += -DDEF_INCL_DIR=\"$(patsubst % ,%,$(DEF_INCL_DIR))\"
endif


# Make sure some owner and group for the program are set

ifndef OWNER
	OWNER := fsc2
endif

ifndef GROUP
	GROUP := uucp
endif


# Define a variable with the full path of the 'addr2line' program and make
# sure addr2line gets found by 'which', otherwise don't set the variable.
# If NO_MAIL has been defined never send crash mails (which requires
# ADDR2LINE to be set).

ifndef NO_MAIL
	ADDR2LINE     := $(shell which addr2line)
	ifneq ($(word 1,$(ADDR2LINE)),which:)
		CONFFLAGS += -DADDR2LINE=\"$(ADDR2LINE)\"
	endif
endif


# IF USE_FSC2_MTA has not been defined also create a variable with the full
# name of the 'mail' program. Otherwise we need the libresolv.so library
# for the built-in mail transport agent.

ifdef MAIL_ADDRESS
	CONFFLAGS += -DMAIL_ADDRESS=\"$(patsubst % ,%,$(MAIL_ADDRESS))\"
	ifndef USE_FSC2_MTA
		MAIL_PROGRAM  := $(shell which mail)
		ifneq ($(word 1,$(MAIL_PROGRAM)),which:)
			CONFFLAGS += -DMAIL_PROGRAM=\"$(MAIL_PROGRAM)\"
		endif
	else
		LIBS      += -lresolv
		CONFFLAGS += -DUSE_FSC2_MTA
	endif
endif


export            # export all variables to sub-makes


# Switch off all implicit rules

.SUFFIXES:

.PHONY: all config src modules utils docs built        \
		install-strip install uninstall http_server    \
		version test cleanup clean pack tags MANIFEST  \
		me6x00 ni6601 ni_daq rulbus witio_48


############## End of configuration section ##############


# Default rule: after some configuration stuff make the main program and
# some required utilities, the modules (the Makefile for modules is called
# via the Makefile in src), the HTTP server, and then the documentation and
# some other, more or less useful stuff

all release debug mdebug:
	$(MAKE) config
	$(MAKE) -C $(sdir) src
	$(MAKE) -C $(sdir) modules
	$(MAKE) http_server
	$(MAKE) utils
	$(MAKE) docs

config: fsc2_config.h
	@if [ -n "$(machine_name)" ]; then                      \
		echo "Configuring for machine $(machine_name)...";  \
		if [ -r $(mchdir)/$(machine_name)-config ]; then    \
			$(SHELL) $(mchdir)/$(machine_name)-config;      \
		fi                                                  \
	fi
	$(MAKE) -C $(sdir) gpib_setup
	$(MAKE) built
	$(MAKE) tags


src:
	$(MAKE) config
	$(MAKE) -C $(sdir) src


modules:
	$(MAKE) config
	$(MAKE) -C $(sdir) modules


utils:
	$(MAKE) -C $(udir) utils


docs:
	$(MAKE) -C $(ddir) docs


me6x00:
	cd me6x00; ./INSTALL; cd $(fdir)


ni6601:
	cd ni6601; ./INSTALL; cd $(fdir)


ni_daq:
	cd ni_daq; ./INSTALL; cd $(fdir)


rulbus:
	cd rulbus; ./INSTALL; cd $(fdir)


witio_48:
	cd witio_48; ./INSTALL; cd $(fdir)


# Create a header file with configuration information from the contents of
# CONFFLAGS if either the Makefile got changed or it does not already exist

fsc2_config.h: Makefile $(MACHINE_INCLUDE)
	@if [ -e fsc2_config.h ]; then           \
		echo "Recreating fsc2_config.h...";  \
	else                                     \
		echo "Creating fsc2_config.h...";    \
	fi
	echo $(CONFFLAGS)
	@echo "/*--- Created automatically, don't change ---*/" > fsc2_config.h
	@echo -e $(patsubst -D%,\\n#define %,$(subst =, ,$(CONFFLAGS))) >> \
		  fsc2_config.h


# Create a file with as much as possible information about the files
# involved in the make process

built:
	-@cat fsc2_config.h > version
	-@echo >> version
	-@grep '$$Id\: ' Makefile $(sdir)/Makefile $(sdir)/*.[chly]             \
					 $(mdir)/Makefile $(mdir)/*.[chl]                       \
					 $(cdir)/Functions $(cdir)/Devices $(cdir)/*.conf |     \
	grep -v 'sed\|grep' |                                               \
	sed 's/^.*$$Id\: \(.*,v [0-9]\{1,\}\.[0-9]\{1,\} [0-9]\{4\}\/[0-9]\{2\}\/[0-9]\{2\} [\0-9]\{2\}:[0-9]\{2\}:[0-9]\{2\}\).*/\1/g' >> version
	-@gzip -c -9 version | uuencode - > version.ugz
	-@$(RM) $(RMFLAGS) version


# Creates everything that allows the web server to be run (on condition that
# the use of the web server isn't switched off)

http_server:
ifdef WITH_HTTP_SERVER
	@if [ ! -e $(ldir)/Makefile                                \
		  -o $(ldir)/Makefile.PL -nt $(ldir)/Makefile ]; then  \
		cd $(ldir) &&                                          \
		perl Makefile.PL;                                      \
		cd $(fdir);                                            \
	fi
	$(MAKE) -C $(ldir)
endif


# Install the program and everything else

install-strip:
	$(MAKE) EXEC_INSTALL='$(INSTALL) -s' install

install:
	$(INSTALL) -d $(libdir)
	$(INSTALL) -d $(auxdir)

	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(cdir)/Functions $(libdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(cdir)/Devices   $(libdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(adir)/*.xpm     $(auxdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(adir)/fsc2.png  $(auxdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(adir)/fsc2.gif  $(auxdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(adir)/fsc2.jpeg $(auxdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(adir)/na.png    $(auxdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(adir)/na.gif    $(auxdir)
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) $(adir)/na.jpeg   $(auxdir)

	$(MAKE) -C $(sdir) install
	$(MAKE) -C $(mdir) install
	$(MAKE) -C $(cdir) install
	$(INSTALL) -m 644 -o $(OWNER) -g $(GROUP) version.ugz $(libdir)
ifdef WITH_HTTP_SERVER
	$(MAKE) -C Fcntl_Lock install
endif
	$(MAKE) -C $(udir) install
	$(MAKE) -C scripts install
	$(MAKE) -C $(ddir) install


# How to get rid of everything that got installed - requires that the
# Makefile and the device configuration files haven't been changed
# significantly since the installation!

uninstall:
	$(RM) $(RMFLAGS) $(libdir)/version.ugz
	$(RM) $(RMFLAGS) $(libdir)/Functions
	$(RM) $(RMFLAGS) $(libdir)/Devices
	$(MAKE) -C $(cdir) uninstall
	for file in $(adir)/*.xpm $(adir)/*.jpeg $(adir)/*.png $(adir)/*.gif; do  \
		$(RM) $(RMFLAGS) $(auxdir)/`basename $$file`;                         \
	done
	$(MAKE) -C $(sdir) uninstall
	$(MAKE) -C $(mdir) uninstall
	$(MAKE) -C $(udir) uninstall
	$(MAKE) -C scripts uninstall
	$(MAKE) -C $(ddir) uninstall
	if [ -z "`ls -A $(bindir)`" ]; then   \
		rmdir $(bindir);                  \
	fi
	if [ -z "`ls -A $(auxdir)`" ]; then   \
		rmdir $(auxdir);                  \
	fi
	if [ -z "`ls -A $(libdir)`" ]; then   \
		rmdir $(libdir);                  \
	fi


# Unpack the (local) version file and write its contents to stdout

version:
	-@if [ -e version.ugz ]; then             \
		uudecode version.ugz | gunzip -c -q;  \
	fi


# 'cleanup' deletes all the files created during the compilation but not
# needed for an installation, while 'clean' also removes all executables,
# shared libraries and documentation.

cleanup:
	$(MAKE) -C $(sdir) cleanup
	$(MAKE) -C $(mdir) cleanup
	$(MAKE) -C $(ddir) cleanup
	$(MAKE) -C me6x00 cleanup
	$(MAKE) -C ni6601 cleanup
	$(MAKE) -C ni_daq cleanup
	$(MAKE) -C rulbus cleanup
	$(MAKE) -C witio_48 cleanup

clean:
	$(MAKE) -C $(sdir) clean
	$(MAKE) -C $(mdir) clean
	$(MAKE) -C $(ddir) clean
ifdef WITH_HTTP_SERVER
	if [ -e $(ldir)/Makefile ]; then   \
		$(MAKE) -C $(ldir) distclean;  \
	fi
endif
	$(MAKE) -C $(udir) clean
	$(RM) $(RMFLAGS) version.ugz $(tagsfile) makelog fsc2_config.h *~
	$(MAKE) -C me6x00 clean
	$(MAKE) -C ni6601 clean
	$(MAKE) -C ni_daq clean
	$(MAKE) -C rulbus clean
	$(MAKE) -C witio_48 clean


# Create a tags file to be used with emacs or vi

tags:
	-find . -name "*.[chly]" -print | etags -o $(tagsfile) -


# Run a set of test EDL programs

test:
	$(MAKE) -C tests


# Clean up everything for a distribution and create a zipped tarball

pack:
	@$(MAKE) clean
	cd ..; tar -c fsc2 | gzip -c -9 > fsc2.tar.gz


# Make distributions of the device modules etc.

packages:
	@for dir in Fcntl_Lock me6x00 ni6601 ni_daq rulbus witio_48; do  \
		$(MAKE) -C $$dir clean;                                      \
		tar -c $$dir | gzip -c -9 > ../$$dir.tar.gz;                 \
	done


# Creates the MANIFEST file with the list of relevant files

MANIFEST:
	@$(MAKE) clean
	@echo '# List of all (necessary) files of the package' > MANIFEST
	@echo '#' >> MANIFEST
	@echo -n '# $$Id' >> MANIFEST
	@echo '$$' >> MANIFEST
	@echo '#' >> MANIFEST

	@for dir in . $(ldir) $(adir) $(cdir) $(ddir) $(mdir) scripts $(sdir) \
		tests $(udir) me6x00 me6x00/driver me6x00/lib me6x00/utils ni6601 \
		ni6601/driver ni6601/lib ni6601/utils witio_48 witio_48/driver    \
		witio_48/lib witio_48/utils ni_daq ni_daq/driver                  \
		ni_daq/driver/daq_stc ni_daq/driver/include                       \
		ni_daq/driver/pci_e_series ni_daq/include ni_daq/lib              \
		ni_daq/misc_doc ni_daq/utils rulbus rulbus/driver rulbus/lib      \
		rulbus/util; do                                                   \
		echo "" >> MANIFEST; \
		'ls' -1F $$dir | grep -v '/$$' | sed "s|^|$$dir/|" | \
		sed "s|^$$fdir/||" | sed 's|\./||' | sed 's|\*$$||' >> MANIFEST; \
	done;
