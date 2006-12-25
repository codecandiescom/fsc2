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
	fsc2_guify $< $@

%.$(W_BAND): %_w.EDL
	fsc2_guify $< $@

%.$(J_BAND): %_j.edl
	cp $< $@
	chmod 755 $@

%.$(W_BAND): %_w.edl
	cp $< $@
	chmod 755 $@

install:
	cp *.molphys.leidenuniv.nl ../../scripts

clean:
	rm -f *.molphys.leidenuniv.nl *~
