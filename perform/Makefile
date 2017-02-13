#
# perform/makefile
# Ryan Mallon (2006)
#
CC		= gcc
CFLAGS		= -g3
HCC		= ../compiler
HCCFLAGS	= -h
MAXDEPTH	= 30

RTLIB =	../rtlib/print.c	\
	../rtlib/history.c	\
	../rtlib/timer.c

TESTS =	scalar-assign.s		\
	control-assign.s	\
	assign-size.s		\
	scalar-retrieve.s	\
	control-retrieve.s	\
	aw-assign

all: $(TESTS)

aw-assign:
	build_test awise

scalar-assign.s:
	$(HCC) $(HCCFLAGS) -p "-DDEPTH=1 " scalar-assign.hc; \
	for i in `seq 1 30`; do \
		printf "$(HCC) $(HCCFLAGS) -p \"-DDEPTH=$$i \" scalar-assign.hc -o scalar-assign$$i\n" \
		$(HCC) $(HCCFLAGS) -p "-DDEPTH=$$i " scalar-assign.hc \
			-o scalar-assign$$i; \
		printf "$(HCC) $(HCCFLAGS) -p \"-DDEPTH=$$i \" --history-inline --inline scalar-assign.hc -o inline-scalar-assign$$i\n" \
		$(HCC) $(HCCFLAGS) -p "-DDEPTH=$$i " \
			--history-inline --inline scalar-assign.hc \
			-o inline-scalar-assign$$i; \
		printf "$(HCC) $(HCCFLAGS) -p \"-DDEPTH=$$i \" --history-inline scalar-assign.hc -o hcc-scalar-assign$$i\n" \
		$(HCC) $(HCCFLAGS) -p "-DDEPTH=$$i " \
			--history-inline scalar-assign.hc \
			-o hcc-scalar-assign$$i; \
		printf "$(HCC) $(HCCFLAGS) -p \"-DDEPTH=$$i \" --history-simple-store 11 scalar-assign.hc -o fscalar-assign$$i\n" \
		$(HCC) $(HCCFLAGS) -p "-DDEPTH=$$i " \
			--history-simple-store 11 scalar-assign.hc \
			-o fscalar-assign$$i; \
	done;

control-assign.s:
	$(HCC) $(HCCFLAGS) control-assign.hc

assign-size.s:
	for i in 1 2 4 8 16 32 64; do \
		$(HCC) $(HCCFLAGS) -p "-DASSIGNS=$$i" assign-size.hc \
			--history-inline -o assign-size$$i; \
		$(HCC) $(HCCFLAGS) -p "-DASSIGNS=$$i" assign-size.hc \
			--history-inline --inline -o inline-assign-size$$i; \
	done;

scalar-retrieve.s:
	$(HCC) $(HCCFLAGS) scalar-retrieve.hc; \
	$(HCC) $(HCCFLAGS) --history-simple-store 2 scalar-retrieve.hc \
		-o fscalar-retrieve

control-retrieve.s:
	$(HCC) $(HCCFLAGS) control-retrieve.hc;
