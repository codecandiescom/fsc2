# $Id$

c_sources     = fsc2.c pulser.c util.c exceptions.c variables.c vars_util.c \
			  	func.c func_basic.c func_util.c func_intact.c T.c phases.c  \
			  	exp.c devices.c run.c gpib_if.c comm.c chld_func.c          \
			  	graphics.c accept.c graph_handler_1d.c graph_handler_2d.c   \
			  	graph_cut.c loader.c bugs.c print.c conn.c ipc.c fsc2_rsc.c
bison_sources = $(filter-out gpib_parser.y,$(wildcard *_parser.y))
flex_sources  = $(filter-out gpib_lexer.l,$(wildcard *_lexer.l))
sources       = $(bison_sources:.y=.c) $(flex_sources:.l=.c) ${c_sources}
objects       = $(sources:.c=.o)
headers       = ${c_sources:.c=.h} global.h


export headers     # export headers to the modules Makefile


.SUFFIXES:
.PHONY: all clean distclean hi_res lo_res gpib_old gpib_jtt gpib_ni \
		pion.anorg.chemie.uni-frankfurt.de \
		moebi.physik.fu-berlin.de \
		door.physik.fu-berlin.de \
		crowley.physik.fu-berlin.de \
		crowley.none


all:
	$(MAKE) $(machine_name)                         # machine dependent stuff
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
	$(RM) $(RMFLAGS) *.o *.output *_parser.[ch] *_lexer.c fsc2_clean.c \
					 fsc2 fsc2_clean fsc2_connect mem gpib_if.[ch] \
					 fsc2_rsc.[ch] gpib_parser.y gpib_lexer.l *~


# Machine specific stuff: different GPIB interfaces and resource files \
# for the graphical interface are needed...

pion.anorg.chemie.uni-frankfurt.de:
	$(MAKE) hi_res
	$(MAKE) gpib_old

moebi.physik.fu-berlin.de:
	$(MAKE) low_res
	$(MAKE) gpib_jtt

crowley.physik.fu-berlin.de:
	$(MAKE) hi_res
	$(MAKE) gpib_jtt

door.physik.fu-berlin.de:
	$(MAKE) hi_res
	$(MAKE) gpib_ni

crowley.none:
	$(MAKE) hi_res
	$(MAKE) gpib_jtt


# How to create the resource files

hi_res: fsc2_rsc_hr.fd
	$(RM) $(RMFLAGS) fsc2_rsc_[hl]r.[ch]
	fdesign -convert fsc2_rsc_hr.fd
	cp fsc2_rsc_hr.c fsc2_rsc.c
	cp fsc2_rsc_hr.h fsc2_rsc.h
	echo "#define SIZE HI_RES" >> fsc2_rsc.h

lo_res: fsc2_rsc_lr.fd
	$(RM) $(RMFLAGS) fsc2_rsc_[hl]r.[ch]
	fdesign -convert fsc2_rsc_lr.fd
	cp fsc2_rsc_lr.c fsc2_rsc.c
	cp fsc2_rsc_lr.h fsc2_rsc.h
	echo "#define SIZE LOW_RES" >> fsc2_rsc.h


# How to create the GPIB interface files

gpib_old: gpib_if_old.[ch]
	$(RM) $(RMFLAGS) gpib_if.[ch]
	cp gpib_if_old.c gpib_if.c
	cp gpib_if_old.h gpib_if.h

gpib_jtt: gpib_if_jtt.[ch]
	$(RM) $(RMFLAGS) gpib_if.[ch]
	cp gpib_if_jtt.c gpib_if.c
	cp gpib_if_jtt.h gpib_if.h

gpib_ni: gpib_if_ni.[ch]
	$(RM) $(RMFLAGS) gpib_if.[ch]
	cp gpib_if_ni.c gpib_if.c
	cp gpib_if_ni.h gpib_if.h
	cp gpib_parser.yy gpib_parser.y
	cp gpib_lexer.ll gpib_lexer.l


# don't automatically delete intermediate files created by bison and flex

.PRECIOUS: %.c
