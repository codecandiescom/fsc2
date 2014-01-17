# -*-Makefile-*-

OWNER                   := fsc2
GROUP                   := dialout

GPIB_LIBRARY            := SLG
GPIB_HEADER_DIR         := /usr/local/include/gpib
GPIB_CARD_NAME          := "gpib0"
GPIB_LOG_LEVEL          := HIGH

SERIAL_LOCK_DIR         := /var/lock

WITH_RULBUS             := yes
rulbus_incl_path        := /usr/local/include
rulbus_lib_path         := /usr/local/lib

FLEX_NEEDS_DECLARATIONS := yes
BROWSER                 := firefox
MAIL_ADDRESS            := fsc2@toerring.de
DESCRIPTION             := "W-band"
FLEX_NEEDS_BISON_DECLARATIONS := yes
