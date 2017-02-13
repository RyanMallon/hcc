HCC		= ../compiler
HCCFLAGS	= -h

TESTS =	primhist.s	\
	f_primhist.s	\
	ihist.s		\
	bubble.s	\
	array.s		\
	fib.s		\
	fib-hist.s	\
	pointer.s	\
	awise.s		\
	d-awise.s

all: clean $(TESTS)

%.s: %.hc
	@echo "  HCC\t$<"
	@$(HCC) $(HCCFLAGS) $<

f_primhist.s: primhist.hc
	@echo "  HCC\t$<"
	@$(HCC) $(HCCFLAGS) --history-simple-store 10 -o $@ $<

d-awise.s: awise.hc
	@echo "  HCC\t$<"
	@$(HCC) $(HCCFLAGS) --history-aw-order-d -o $@ $<

clean:
	@echo "  CLEAN"
	@rm -f *.s
