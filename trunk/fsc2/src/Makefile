# $Id$

c_sources      = fsc2.c pulser.c util.c exceptions.c variables.c vars_util.c \
				 func.c func_basic.c func_util.c func_intact.c T.c phases.c  \
				 exp.c devices.c run.c comm.c chld_func.c graphics.c         \
                 accept.c graph_handler_1d.c graph_handler_2d.c graph_cut.c  \
				 loader.c bugs.c print.c conn.c ipc.c

ifeq ($(shell hostname -f),pion.chemie.uni-frankfurt.de)
	c_sources += gpib_if_old.c
endif

ifeq ($(shell hostname -f),door.physik.fu-berlin.de)
	c_sources += gpib_if_ni.c
endif

ifeq ($(shell hostname -f),moebi.physik.fu-berlin.de)
	c_sources += gpib_if.c
endif

ifeq ($(shell hostname -f),crowley.physik.fu-berlin.de)
	c_sources += gpib_if.c
endif

bison_sources  = $(wildcard *_parser.y)
flex_sources   = $(wildcard *_lexer.l)
sources        = $(bison_sources:.y=.c) $(flex_sources:.l=.c) \
				 ${c_sources}
objects        = $(sources:.c=.o)
headers        = ${c_sources:.c=.h} global.h


# on moebi a version with low-resolution is needed - add the appropriate
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

ifeq ($(shell hostname -f),pion.chemie.uni-frankfurt.de)
	CFLAGS    += -DIS_STILL_LIBC1
endif


export headers


.SUFFIXES:
.PHONY: all clean distclean


all:
	$(MAKE) fsc2 fsc2_clean fsc2_connect modules


# make the main program from the c, flex and bison source files

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


# make the primary lexer that's utilized by the main program

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


# don't automatically delete intermediate files created by bison and flex

.PRECIOUS: %.c
