bison_sources  = $(wildcard *_parser.y)
flex_sources   = $(wildcard *_lexer.flex)
sources        = $(bison_sources:.y=.c) $(flex_sources:.flex=.c) \
			     fsc2.c assign.c util.c exceptions.c variables.c func.c T.c \
				 phases.c ppcheck.c pulse.c defaults.c prim_exp.c
objects        = $(sources:.c=.o)
CFLAGS         = -ggdb -Wall -Wwrite-strings -Wstrict-prototypes \
			     -Wmissing-prototypes -Wmissing-declarations -DDEBUG


fsc2: $(objects) fsc_clean fsc2.h
	gcc $(CFLAGS) -lm -o $@ $(objects)

split_lexer.c: split_lexer.flex fsc2.h
	flex -Psplit -osplit_lexer.c split_lexer.flex

assign_parser.c assign_parser.h: assign_parser.y
	bison -d -v -t -p assign -o assign_parser.c assign_parser.y

assign_lexer.c: assign_lexer.flex fsc2.h
	flex -Passign -oassign_lexer.c assign_lexer.flex

defaults_parser.c defaults_parser.h: defaults_parser.y fsc2.h
	bison -d -v -t -p defaults -o defaults_parser.c defaults_parser.y

defaults_lexer.c: defaults_lexer.flex fsc2.h
	flex -Pdefaults -odefaults_lexer.c defaults_lexer.flex

vars_parser.c vars_parser.h: vars_parser.y fsc2.h
	bison -d -v -t -p variables -o vars_parser.c vars_parser.y

vars_lexer.c: vars_lexer.flex fsc2.h
	flex -Pvariables -ovars_lexer.c vars_lexer.flex

phases_parser.c phases_parser.h: phases_parser.y fsc2.h
	bison -d -v -t -p phases -o phases_parser.c phases_parser.y

phases_lexer.c: phases_lexer.flex fsc2.h
	flex -Pphases -ophases_lexer.c phases_lexer.flex

preps_parser.c preps_parser.h: preps_parser.y fsc2.h
	bison -d -v -t -p preps -o preps_parser.c preps_parser.y

preps_lexer.c: preps_lexer.flex fsc2.h
	flex -Ppreps -opreps_lexer.c preps_lexer.flex

prim_exp_run_parser.c: prim_exp_run_parser.y fsc2.h
	bison -v -t -p prim_exp_run -o prim_exp_run_parser.c prim_exp_run_parser.y

prim_exp_lexer.c: prim_exp_lexer.flex fsc2.h
	flex -Pprim_exp -oprim_exp_lexer.c prim_exp_lexer.flex

condition_parser.c: condition_parser.y fsc2.h
	bison -v -t -p condition -o condition_parser.c condition_parser.y

%.o: %.c
	gcc $(CFLAGS) -c -o $@ $<

fsc_clean: fsc_clean.o exceptions.o util.o T.o util.o variables.o
	gcc $(CFLAGS) -O2 -lm -o $@ fsc_clean.o exceptions.o util.o T.o variables.o

fsc_clean.c: fsc_clean.flex fsc2.h
	flex -ofsc_clean.c fsc_clean.flex


.PHONY:
clean: 
	   rm $(objects) fsc_clean fsc2 *.output *_parser.c *_parser.h *_lexer.c \
	   fsc_clean.c fsc_clean.o TAGS

tags:
	etags -T *.c *.h *.flex *.y *.h
