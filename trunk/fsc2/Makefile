# $Id$

# The following variables determine where the program files will be copied to
# by 'make install' as well as the name of the GPIB configuration and log file
# together with its ownerships

#prefix         = /usr/local
#bindir         = $(prefix)/bin
#libdir         = $(prefix)/lib/fsc2

prefix         = .
bindir         = $(prefix)/.
libdir         = $(prefix)/

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
c_sources      = fsc2.c fsc2_rsc.c assign.c util.c exceptions.c variables.c \
				 func.c T.c phases.c ppcheck.c pulse.c exp.c devices.c \
				 gpib.c run.c comm.c chld_func.c graphics.c accept.c \
				 graph_handler_1d.c graph_handler_2d.c loader.c bugs.c
sources        = $(bison_sources:.y=.c) $(flex_sources:.flex=.c) \
				 ${c_sources}
objects        = $(sources:.c=.o)
modules        = User_Functions.c dg2020.c tds754a.c sr510.c s_band.c \
				 er035m.c bh15.c
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

all: tags fsc2 mods fsc2_clean

fsc2: $(objects) $(header)
	$(CC) -o $@ $(objects) $(LIBS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.y $(header)
	$(BISON) $(BISONFLAGS) $(patsubst %_parser.y,%,$<) -o $@ $<

%.c: %.flex $(header)
	$(FLEX) $(FLEXFLAGS)$(patsubst %_lexer.flex,%,$<) -o$@ $<

mods: $(modules:.c=.so)

%.so: %.c $(header)
	$(CC) $(CFLAGS) -c -fPIC $< -o $*.lo
	$(CC) -shared -o $@ $*.lo -lm $(LDFLAGS)
	chmod 644 $@


fsc2_clean: fsc2_clean.o exceptions.o
	$(CC) $(CFLAGS) -O2 -lm -o $@ $^ $(LIBS)

fsc2_clean.c: fsc2_clean.flex $(header)
	$(FLEX) -B -o$@ $<


tags:
	etags -T ${c_sources} *.flex *.y ${c_sources:.c=.h} $(header)


clean: 
	$(RM) $(RMFLAGS) *.o fsc2_clean fsc2 *.output *_parser.c *_parser.h \
		*_lexer.c fsc2_clean.c *.lo *.so TAGS mem


# don't delete intermediate files created by bison and flex

.PRECIOUS: %.c