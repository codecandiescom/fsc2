# $Id$

# The following variables determine where the program files will be copied to
# by 'make install' as well as the name of the GPIB configuration and log file
# together with its ownerships

#prefix         = /usr/local
#bindir         = $(prefix)/bin
#libdir         = $(prefix)/lib/fsc2

prefix         = .
bindir         = $(prefix)/.
libdir         = $(prefix)/.

GPIB_CONF      = /etc/gpib.conf
GPIB_LOG       = /tmp/gpib.log
GPIB_LOG_OWNER = nobody
GPIB_LOG_GROUP = nogroup

# The name of the awk tool is just passed to the programs via a macro

AWK            = /usr/bin/awk

# Here follow settings determining the make process - which compiler and
# tools to use together with the respective options

SHELL          = /bin/sh
CC             = gcc
CFLAGS         = -ggdb -Wall -Wwrite-strings -Wstrict-prototypes \
			     -Wmissing-prototypes -Wmissing-declarations -W \
				 # -DMAX_DEBUG # for debugging of digitizer
				 # -DDEBUG
				 # -DMDEBUG # for malloc debugging
LIBS           = -L/usr/X11R6/lib -lforms -lX11 -lgpib -lfl -lm -ldl \
				 # -lmcheck   # for malloc debugging
LDFLAGS        = -Wl,-E
BISON          = bison
BISONFLAGS     = -d -v -t -p
FLEX           = flex
FLEXFLAGS      = -B -P
RM             = -rm
RMFLAGS        = -f

bison_sources  = $(wildcard *_parser.y)
flex_sources   = $(wildcard *_lexer.flex)
c_sources      = fsc2.c fsc2_rsc.c pulser.c util.c exceptions.c variables.c \
				 func.c T.c phases.c exp.c devices.c gpib.c run.c comm.c \
				 chld_func.c graphics.c accept.c graph_handler_1d.c \
				 graph_handler_2d.c loader.c bugs.c print.c
sources        = $(bison_sources:.y=.c) $(flex_sources:.flex=.c) \
				 ${c_sources}
objects        = $(sources:.c=.o)
modules        = User_Functions.c tds754a.c sr510.c s_band.c er035m.c bh15.c
dg2020         = dg2020.c dg2020_gen.c dg2020_pulse.c dg2020_init.c \
				 dg2020_run.c dg2020_util.c dg2020_gpib.c
header         = ${c_sources:.c=.h} global.h

# Pass GPIB_LOG, GPIB_CONF and AWK via macros to the source files (don't
# forget to escape all `"' with backslashes!)

ifdef GPIB_LOG
    CFLAGS += -DGPIB_LOG=\"$(GPIB_LOG)\"
endif

ifdef GPIB_CONF
    CFLAGS += -DGPIB_CONF=\"$(GPIB_CONF)\"
endif

ifdef AWK
    CFLAGS += -DAWK=\"$(AWK)\"
endif

ifdef libdir
	CFLAGS += -Dlibdir=\"$(libdir)\"
endif

# Switch off all implicit rules and set suffices of file to be made by make

.SUFFIXES:
.SUFFIXES: .c .o .h
.PHONY: all clean


# default rule: make the main program, the modules and the primary lexer

all: fsc2 mods fsc2_clean


# make the main program from the c, flex and bison source files

fsc2: $(objects) $(header)
	$(CC) -o $@ $(objects) $(LIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.y $(header)
	$(BISON) $(BISONFLAGS) $(patsubst %_parser.y,%,$<) -o $@ $<

%.c: %.flex $(header)
	$(FLEX) $(FLEXFLAGS)$(patsubst %_lexer.flex,%,$<) -o$@ $<


# make the modules - the pulser module has to handles specially since it has
# its own header file and is divided into several source files

mods: dg2020.so $(modules:.c=.so)

dg2020.so: ${dg2020:.c=.lo} dg2020.h
	$(CC) -shared -o $@ ${dg2020:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

%.so: %.lo
	$(CC) -shared -o $@ $*.lo -lm $(LDFLAGS)
	chmod 644 $@

${dg2020:.c=.lo}: dg2020.h

%.lo: %.c $(header)
	$(CC) $(CFLAGS) -c -fPIC $< -o $*.lo


# make the primary lexer that's utilized by the main program

fsc2_clean: fsc2_clean.o exceptions.o
	$(CC) -o $@ $^ -lfl -lm

fsc2_clean.c: fsc2_clean.flex $(header)
	$(FLEX) -B -o$@ $<

# install the main program and the modules

install:
	cp fsc2 $(bindir)/fsc2
	chown fsc2.fsc2 $(bindir)/fsc2
	chmod 6755 $(bindir)/fsc2
	strip $(bindir)/fsc2
	cp *.so $(libdir)
	cp fsc2_clean $(libdir)
	cp *.xpm $(libdir)
	chown fsc2.fsc2 $(libdir)/*.so $(libdir)/*.xpm $(libdir)/fsc2_clean
	chmod 644 $(libdir)/*.so $(libdir)/*.xpm
	chmod 755 $(libdir)/fsc2_clean


tags:
	etags -T ${c_sources} *.flex *.y ${c_sources:.c=.h} $(header)


clean: 
	$(RM) $(RMFLAGS) *.o fsc2_clean fsc2 *.output *_parser.c *_parser.h \
		*_lexer.c fsc2_clean.c *.lo *.so TAGS mem


# don't delete intermediate files created by bison and flex

.PRECIOUS: %.c