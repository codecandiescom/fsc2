# $Id$

# The following variables determine where the program files will be copied to
# by 'make install' as well as the name of the GPIB configuration and log file
# together with its ownerships

#prefix         = /usr/local
#bindir         = $(prefix)/bin
#libdir         = $(prefix)/lib/fsc2
#auxdir         = $(libdir)/aux

prefix         = $(shell pwd)
bindir         = $(prefix)
libdir         = $(prefix)
auxdir         = $(libdir)/aux

GPIB_CONF      = /etc/gpib.conf
GPIB_LOG       = /tmp/gpib.log

# here you set if you want the program for a high or a low resolution screen
# (set SIZE to either `HI_RES' or `LOW_RES')

SIZE           = HI_RES


# The name of the awk tool is passed to the programs via a macro

AWK            = /usr/bin/awk


##############################################################################
# Here follow settings determining the make process - which compiler and
# tools to use as well as the respective options. Better leave them alone
# if you don't have really good reasons...
##############################################################################

SHELL          = /bin/sh
CC             = gcc
CFLAGS        += -ggdb -Wall -Wwrite-strings \
				 -Wstrict-prototypes -Wmissing-prototypes \
				 -Wmissing-declarations -W \
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

VPATH          = .:src:modules

c_sources      = fsc2.c pulser.c util.c exceptions.c variables.c \
				 func.c T.c phases.c exp.c devices.c gpib.c run.c comm.c \
				 chld_func.c graphics.c accept.c graph_handler_1d.c \
				 graph_handler_2d.c loader.c bugs.c print.c

ifeq ($(SIZE),HI_RES)
	c_sources += fsc2_rsc_hr.c
	CFLAGS    += -DSIZE=1
else
	c_sources += fsc2_rsc_lr.c
	CFLAGS    += -DSIZE=0
endif

bison_sources  = $(notdir $(wildcard src/*_parser.y))
flex_sources   = $(notdir $(wildcard src/*_lexer.flex))
sources        = $(bison_sources:.y=.c) $(flex_sources:.flex=.c) \
				 ${c_sources}
objects        = $(sources:.c=.o)
headers        = ${c_sources:.c=.h} global.h
modules        = User_Functions.c sr510.c sr810.c s_band.c aeg_x_band.c \
				 er035m.c er035m_s.c bh15.c hp8647a.c
dg2020_f       = dg2020_f.c dg2020_gen_f.c dg2020_pulse_f.c dg2020_init_f.c \
				 dg2020_run_f.c dg2020_util_f.c dg2020_gpib_f.c
dg2020_b       = dg2020_b.c dg2020_gen_b.c dg2020_pulse_b.c dg2020_util_b.c \
				 dg2020_init_b.c dg2020_run_b.c dg2020_gpib_b.c
tds754a        = tds754a.c tds754a_gpib.c tds754a_util.c
tds520a        = tds520a.c tds520a_gpib.c tds520a_util.c


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

CFLAGS += $(patsubst %,-I%,$(subst :, ,$(VPATH))) -Iaux

ifdef libdir
	CFLAGS += -Dlibdir=\"$(libdir:/=)/\"
endif

ifdef auxdir
	CFLAGS += -Dauxdir=\"$(auxdir:/=)/\"
endif


# Switch off all implicit rules and set suffices of file to be made by make

.SUFFIXES:
.SUFFIXES: .c .o .h .so .lo
.PHONY: all clean distclean pack


# default rule: make the main program, the modules and the primary lexer

all: fsc2 fsc2_clean mods


# make the main program from the c, flex and bison source files

fsc2: $(objects)
	$(CC) -o $@ $(objects) $(LIBS) $(LDFLAGS)

%.o: %.c $(headers)
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.y $(headers)
	$(BISON) $(BISONFLAGS) $(notdir $(patsubst %_parser.y,%,$<)) -o $@ $<

%.c: %.flex $(headers)
	$(FLEX) $(FLEXFLAGS)$(notdir $(patsubst %_lexer.flex,%,$<)) -o$@ $<
	sed -e 's/register char \*yy_cp, \*yy_bp;/register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@ > $@.x
	mv -f $@.x $@


# make the primary lexer that's utilized by the main program

fsc2_clean: fsc2_clean.o exceptions.o
	$(CC) -o $@ $^ -lfl -lm

fsc2_clean.c: fsc2_clean.flex $(headers)
	$(FLEX) -B -o$@ $<


# make the modules - the pulser and digitizer modules have to be handled
# specially since each has its own header file and is divided into several
# source files

mods: dg2020_f.so dg2020_b.so tds754a.so tds520a.so $(modules:.c=.so)

dg2020_f.so: ${dg2020_f:.c=.lo}
	$(CC) -shared -o $@ ${dg2020_f:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

dg2020_b.so: ${dg2020_b:.c=.lo}
	$(CC) -shared -o $@ ${dg2020_b:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

tds754a.so: ${tds754a:.c=.lo}
	$(CC) -shared -o $@ ${tds754a:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

tds520a.so: ${tds520a:.c=.lo}
	$(CC) -shared -o $@ ${tds520a:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

%.so: %.lo
	$(CC) -shared -o $@ $*.lo -lm $(LDFLAGS)
	chmod 644 $@

${dg2020_f:.c=.lo}: dg2020_f.h $(headers)
${dg2020_b:.c=.lo}: dg2020_b.h $(headers)
${tds754a:.c=.lo}: tds754a.h $(headers)
${tds520a:.c=.lo}: tds520a.h $(headers)

%.lo: %.c $(headers)
	$(CC) $(CFLAGS) -c -fPIC $< -o $*.lo


# install the program in $bindir and all the other stuff in $libdir and $auxdir

install:
	install -d $(bindir)
	install -m 6755 -o fsc2 -g fsc2 -s fsc2 $(bindir)
	install -d $(libdir)
	install -d $(auxdir)
	install -m 755 -o fsc2 -g fsc2 -s fsc2_clean $(libdir)
	install -m 644 -o fsc2 -g fsc2 Functions $(libdir)
	install -m 644 -o fsc2 -g fsc2 Devices $(libdir)
	install -m 644 -o fsc2 -g fsc2 -s *.so $(libdir)
	install -m 644 -o fsc2 -g fsc2 aux/*.xpm $(auxdir)


# make a tags file, needs still some changes to really work

tags:
	etags -T ${c_sources} *.flex *.y ${c_sources:.c=.h} $(headers)  \
		  ${modules} ${dg2020_f} dg2020_f.h ${dg2020_b} dg2020_b.h \
		  ${tds754a} tds754a.h ${tds520a} tds520a.h


# make all documentation

all-docs:
	$(MAKE) -C doc html ps

# make just the html documentation

html-docs:
	$(MAKE) -C doc html

# make just the Postscript documentation

ps-docs:
	$(MAKE) -C doc ps


# `clean' deletes just all the files remaining from the compilation while
# `distclean' also removes all executables and shared libraries

clean:
	$(RM) $(RMFLAGS) *.o *.output *_parser.c *_parser.h *_lexer.c \
					 fsc2_clean.c *.lo mem

distclean:
	$(MAKE) clean
	$(RM) $(RMFLAGS) fsc2 fsc2_clean *.so TAGS *~
	$(MAKE) -C doc distclean


pack:
	$(MAKE) distclean
	cd ..; tar -c fsc2 | gzip -c -9 > fsc2.tgz

# don't automatically delete intermediate files created by bison and flex

.PRECIOUS: %.c
