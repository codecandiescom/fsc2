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

# The name of the awk tool is passed to the programs via a macro

AWK            = /usr/bin/awk

# Here follow settings determining the make process - which compiler and
# tools to use together with the respective options

SHELL          = /bin/sh
CC             = gcc
CFLAGS        += -ggdb -Wall -Wwrite-strings -Wstrict-prototypes \
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
############### Don't change anything below this line ######################
############################################################################


bison_sources  = $(wildcard *_parser.y)
flex_sources   = $(wildcard *_lexer.flex)
c_sources      = fsc2.c fsc2_rsc.c pulser.c util.c exceptions.c variables.c \
				 func.c T.c phases.c exp.c devices.c gpib.c run.c comm.c \
				 chld_func.c graphics.c accept.c graph_handler_1d.c \
				 graph_handler_2d.c loader.c bugs.c print.c
sources        = $(bison_sources:.y=.c) $(flex_sources:.flex=.c) \
				 ${c_sources}
objects        = $(sources:.c=.o)
modules        = User_Functions.c tds754a.c sr510.c sr810.c s_band.c \
				 aeg_x_band.c er035m.c bh15.c hp8647a.c
dg2020_f       = dg2020_f.c dg2020_gen_f.c dg2020_pulse_f.c dg2020_init_f.c \
				 dg2020_run_f.c dg2020_util_f.c dg2020_gpib_f.c
dg2020_b       = dg2020_b.c dg2020_gen_b.c dg2020_pulse_b.c dg2020_util_b.c \
				 dg2020_init_b.c dg2020_run_b.c dg2020_gpib_b.c
tds754a        = tds754a.c tds754a_gpib.c tds754a_util.c
header         = ${c_sources:.c=.h} global.h

# Pass GPIB_LOG, GPIB_CONF and AWK via macros to the source files
# (don't forget to escape all double quotes (`"') with backslashes!)

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
	sed -e 's/register char \*yy_cp, \*yy_bp;/register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@ > $@.x
	mv -f $@.x $@

# make the modules - the pulser and digitizer modules have to be handled
# specially since each has its own header file and is divided into several
# source files

mods: dg2020_f.so dg2020_b.so tds754a.so $(modules:.c=.so)

dg2020_f.so: ${dg2020_f:.c=.lo} dg2020_f.h
	$(CC) -shared -o $@ ${dg2020_f:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

dg2020_b.so: ${dg2020_b:.c=.lo} dg2020_b.h
	$(CC) -shared -o $@ ${dg2020_b:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

tds754a.so: ${tds754a:.c=.lo} tds754a.h
	$(CC) -shared -o $@ ${tds754a:.c=.lo} -lm $(LDFLAGS)
	chmod 644 $@

%.so: %.lo
	$(CC) -shared -o $@ $*.lo -lm $(LDFLAGS)
	chmod 644 $@

${dg2020_f:.c=.lo}: dg2020_f.h
${dg2020_b:.c=.lo}: dg2020_b.h
${tds754a:.c=.lo}: tds754a.h

%.lo: %.c $(header)
	$(CC) $(CFLAGS) -c -fPIC $< -o $*.lo


# make the primary lexer that's utilized by the main program

fsc2_clean: fsc2_clean.o exceptions.o
	$(CC) -o $@ $^ -lfl -lm

fsc2_clean.c: fsc2_clean.flex $(header)
	$(FLEX) -B -o$@ $<


# install the program in $bindir and all the other stuff in $libdir

install:
	install -d $(bindir)
	install -m 6755 -o fsc2 -g fsc2 -s fsc2 $(bindir)
	install -d $(libdir)
	install -m 755 -o fsc2 -g fsc2 -s fsc2_clean $(libdir)
	install -m 644 -o fsc2 -g fsc2 -s *.so $(libdir)
	install -m 644 -o fsc2 -g fsc2 -s *.xpm $(libdir)


tags:
	etags -T ${c_sources} *.flex *.y ${c_sources:.c=.h} $(header)  \
		  ${modules} ${dg2020_f} dg2020_f.h ${dg2020_b} dg2020_b.h \
		  ${tds754a} tds754a.h


clean: 
	$(RM) $(RMFLAGS) *.o fsc2_clean fsc2 *.output *_parser.c *_parser.h \
		*_lexer.c fsc2_clean.c *.lo *.so TAGS mem


# don't delete intermediate files created by bison and flex

.PRECIOUS: %.c