# $Id$

c_sources      = fsc2.c pulser.c util.c exceptions.c variables.c vars_util.c \
				 func.c func_basic.c func_util.c func_intact.c T.c phases.c  \
				 exp.c devices.c run.c gpib_if.c comm.c chld_func.c          \
				 graphics.c accept.c graph_handler_1d.c graph_handler_2d.c   \
				 graph_cut.c loader.c bugs.c print.c conn.c ipc.c

# On door we need the parser and lexer for the GPIB configuration file

ifeq ($(shell hostname -f),door.physik.fu-berlin.de)
	c_source  += gpib_lexer.c gpib_parser.c
endif

# On moebi a version with low-resolution is needed - add the appropriate
# graphics files to the c files and set the SIZE flag correctly

ifeq ($(shell hostname -f),moebi.physik.fu-berlin.de)
	c_sources += fsc2_rsc_lr.c
	headers   += fsc2_rsc_lr.h
	CFLAGS    += -DSIZE=0
else
	c_sources += fsc2_rsc_hr.c
	headers   += fsc2_rsc_hr.h
	CFLAGS    += -DSIZE=1
endif

bison_sources  = $(wildcard *_parser.y)
flex_sources   = $(wildcard *_lexer.l)
sources        = $(bison_sources:.y=.c) $(flex_sources:.l=.c) \
				 ${c_sources}
objects        = $(sources:.c=.o)
headers        = ${c_sources:.c=.h} global.h


ifeq ($(shell hostname -f),door.physik.fu-berlin.de)
objects += /usr/local/lib/libgpib.a
endif



export headers


.SUFFIXES:
.PHONY: all clean distclean pion door


all:
	echo $(CFLAGS)
	if [ `hostname -f` = "pion.anorg.chemie.uni-frankfurt.de" ]; then \
		$(MAKE) pion; \
	fi
	if [ `hostname -f` = "door.physik.fu-berlin.de" ]; then \
		$(MAKE) door; \
	fi
	$(MAKE) fsc2 fsc2_clean fsc2_connect modules


# Make the main program from the c, flex and bison source files

fsc2: $(objects)
	$(CC) -o $@ $(objects) $(LIBS) $(LDFLAGS)

modules:
	$(MAKE) -C ../modules all


%.o: %.c $(headers)
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.y $(headers)
	$(BISON) $(BISONFLAGS) $(patsubst %_parser.y,%,$<) -o $@ $<

%.c: %.l $(headers)
	$(FLEX) $(FLEXFLAGS)$(patsubst %_lexer.l,%,$<) -o$@ $<
	sed -e 's/register char \*yy_cp, \*yy_bp;/\
		register char \*yy_cp = NULL, \*yy_bp = NULL;/' $@ > $@.x
	mv -f $@.x $@


# Make the primary lexer that's utilized by the main program

fsc2_clean: fsc2_clean.o exceptions.o
	$(CC) -o $@ $^ -lfl -lm

fsc2_clean.c: fsc2_clean.l $(headers)
	$(FLEX) -B -o$@ $<

fsc2_connect: fsc2_connect.o
	$(CC) -o $@ $^

fsc2_connect.o: fsc2_connect.c
	$(CC) $(CFLAGS) -c -o $@ $<


clean:
	$(RM) $(RMFLAGS) *.o *.output *_parser.c *_parser.h *_lexer.c \
					 fsc2_clean.c fsc2 fsc2_clean fsc2_connect mem *~


# Unfortunately, for the and door a different GPIB interface is needed...

pion:
	if [ ! -e gpib_if_new.c -a ! -e gpib_if_new.h ]; then \
		mv gpib_if.c gpib_if_new.c; \
		mv gpib_if.h gpib_if_new.h; \
		mv gpib_if_old.c gpib_if.c; \
		mv gpib_if_old.h gpib_if.h; \
	fi

door:
	if [ ! -e gpib_if_new.c -a ! -e gpib_if_new.h ]; then \
		mv gpib_if.c gpib_if_new.c; \
		mv gpib_if.h gpib_if_new.h; \
		mv gpib_if_ni.c gpib_if.c;  \
		mv gpib_if_ni.h gpib_if.h;  \
	fi
	if [ ! -e gpib_parser.y ]; then      \
		cp gpib_parser.yy gpib_parser.y; \
	fi
	if [ ! -e gpib_lexer.l ]; then       \
		cp gpib_lexer.ll gpib_lexer.l; \
	fi

# don't automatically delete intermediate files created by bison and flex

.PRECIOUS: %.c
