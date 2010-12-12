# -*-Makefile-*-

OWNER                   := fsc2
GROUP                   := dialout

# If we're running a 2.4 kernel we use the old NI driver, otherwise the
# SourceForge driver

KERNEL_VERSION          := $(shell uname -r)

ifeq ($(KERNEL_VERSION),$(shell echo $(KERNEL_VERSION) | sed -e 's/^2\.6\.//'))
	GPIB_LIBRARY        := NI_OLD
else
	GPIB_LIBRARY        := SLG
	GPIB_HEADER_DIR     := /usr/local/include/gpib
	GPIB_CARD_NAME      .= "gpib0"
endif

BROWSER                 := mozilla
SERIAL_LOCK_DIR         := /var/lock
WITH_RULBUS             := yes
rulbus_incl_path        := /usr/local/include
rulbus_lib_path         := /usr/local/lib
FLEX_NEEDS_DECLARATIONS := yes
MAIL_ADDRESS            := fsc2@toerring.de
DESCRIPTION             := "W-Band"