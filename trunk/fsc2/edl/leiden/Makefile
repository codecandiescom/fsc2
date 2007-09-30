J_BAND          := rulhmpc66.molphys.leidenuniv.nl
W_BAND          := rulhmpc89.molphys.leidenuniv.nl
EDL_J           := $(wildcard *_j.EDL)
EDL_W           := $(wildcard *_w.EDL)
EDL_SCRIPTS     := $(patsubst %_j.EDL,%.$(J_BAND),$(EDL_J)) \
	               $(patsubst %_w.EDL,%.$(W_BAND),$(EDL_W))
SIMPLE_J        := $(wildcard *_j.edl)
SIMPLE_W        := $(wildcard *_w.edl)
SIMPLE_SCRIPTS := $(patsubst %_j.edl,%.$(J_BAND),$(SIMPLE_J)) \
	              $(patsubst %_w.edl,%.$(W_BAND),$(EDL_W))

all: $(SIMPLE_SCRIPTS) $(EDL_SCRIPTS)

%.$(J_BAND): %_j.EDL
	@if [ ! -e $@ -o $< -nt $@ ]; then  \
		echo "fsc2_guify $< $@";        \
		fsc2_guify $< $@;               \
	fi

%.$(W_BAND): %_w.EDL
	@if [ ! -e $@ -o $< -nt $@ ]; then  \
		echo "fsc2_guify $< $@";        \
		fsc2_guify $< $@;               \
	fi

%.$(J_BAND): %_j.edl
	@if [ ! -e $@ -o $< -nt $@ ]; then  \
		echo "cp $< $@";                \
		echo "chmod 755 $@";            \
		cp $< $@;                       \
		chmod 755 $@ ;                  \
	fi

%.$(W_BAND): %_w.edl
	@if [ ! -e $@ -o $< -nt $@ ]; then  \
		echo "cp $< $@";                \
		echo "chmod 755 $@";            \
		cp $< $@;                       \
		chmod 755 $@                    \
	fi

install:
	@for f in *.molphys.leidenuniv.nl; do       \
		if [ $$f -nt ../../scripts/$$f ]; then  \
			echo "cp $$f ../../scripts";        \
	    	cp $$f ../../scripts;               \
		fi;                                     \
	done

clean:
	rm -f *.molphys.leidenuniv.nl *~
