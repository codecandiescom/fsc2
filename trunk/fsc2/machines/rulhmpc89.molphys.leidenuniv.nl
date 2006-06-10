# -*-Makefile-*-
#
# $Id$

OWNER              := fsc2
GROUP              := uucp

# If we're running a 2.4 kernel we use the old NI driver, otherwise the
# SourveForge driver

KERNEL_VERSION     := $(shell uname -r)

ifeq ($(KERNEL_VERSION),$(subst 2.6,XXX,$(KERNEL_VERSION)))
	GPIB_LIBRARY       := NI_OLD
else
	GPIB_LIBRARY       := SLG
	GPIB_HEADER_DIR    := /usr/local/include/gpib
endif

BROWSER            := mozilla
NUM_SERIAL_PORTS   := 2
SERIAL_LOCK_DIR    := /var/lock
WITH_RULBUS        := yes
rulbus_incl_path   := /usr/local/include
rulbus_lib_path    := /usr/local/lib
MAIL_ADDRESS       := fsc2@toerring.de
