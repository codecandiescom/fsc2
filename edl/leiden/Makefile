J_BAND              := rulhmpc66.molphys.leidenuniv.nl
W_BAND              := rulhmpc89.molphys.leidenuniv.nl

# Scripts that need to be converted with fsc2_guify

EDL_J               := $(wildcard *_j.EDL)
EDL_W               := $(wildcard *_w.EDL)
EDL_SCRIPTS         := $(patsubst %_j.EDL,%.$(J_BAND),$(EDL_J)) \
	                   $(patsubst %_w.EDL,%.$(W_BAND),$(EDL_W))

# Simple EDL scripts to be loaded directly into fsc2

SIMPLE_EDL_J        := $(wildcard *_j.edl)
SIMPLE_EDL_W        := $(wildcard *_w.edl)
SIMPLE_EDL_SCRIPTS  := $(patsubst %_j.edl,%.$(J_BAND),$(SIMPLE_EDL_J)) \
	                   $(patsubst %_w.ed;,%.$(W_BAND),$(SIMPLE_EDL_W))

# Perl scripts

SIMPLE_PERL_J       := $(wildcard *_j)
SIMPLE_PERL_W       := $(wildcard *_w)
SIMPLE_PERL_SCRIPTS := $(patsubst %_j,%.$(J_BAND),$(SIMPLE_PERL_J)) \
	                   $(patsubst %_w,%.$(W_BAND),$(SIMPLE_PERL_W))

all: $(SIMPLE_EDL_SCRIPTS) $(SIMPLE_PERL_SCRIPTS) $(EDL_SCRIPTS)

%.$(J_BAND): %_j
	@if [ ! -e $@ -o $< -nt $@ ]; then          \
		echo "cp $< $@";                        \
		echo "chmod 755 $@";                    \
		cp $< $@;                               \
		chmod 755 $@ ;                          \
	fi

%.$(W_BAND): %_w
	@if [ ! -e $@ -o $< -nt $@ ]; then          \
		echo "cp $< $@";                        \
		echo "chmod 755 $@";                    \
		cp $< $@;                               \
		chmod 755 $@ ;                          \
	fi

%.$(J_BAND): %_j.EDL
	@if [ ! -e $@ -o $< -nt $@ ]; then          \
		echo "fsc2_guify $< $@";                \
		fsc2_guify $< $@;                       \
	fi

%.$(W_BAND): %_w.EDL
	@if [ ! -e $@ -o $< -nt $@ ]; then          \
		echo "fsc2_guify $< $@";                \
		fsc2_guify $< $@;                       \
	fi

%.$(J_BAND): %_j.edl
	@if [ ! -e $@ -o $< -nt $@ ]; then          \
		echo "cp $< $@";                        \
		echo "chmod 755 $@";                    \
		cp $< $@;                               \
		chmod 755 $@ ;                          \
	fi

%.$(W_BAND): %_w.edl
	@if [ ! -e $@ -o $< -nt $@ ]; then          \
		echo "cp $< $@";                        \
		echo "chmod 755 $@";                    \
		cp $< $@;                               \
		chmod 755 $@                            \
	fi

install:
	@for f in *.molphys.leidenuniv.nl; do       \
		if [    $$f -nt ../../scripts/$$f       \
			 -o ! -e ../../scripts/$$f ]; then  \
			echo "cp $$f ../../scripts";        \
	    	cp $$f ../../scripts;               \
		fi;                                     \
	done

clean:
	rm -f *.molphys.leidenuniv.nl *~
