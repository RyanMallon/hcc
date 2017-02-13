#
# Makefile for running tests on a Sparc machine
# Ryan Mallon (2006)
#

CC=gcc
CFLAGS=-g3
DIFF=diff

LIB_SRCS=../rtlib/history.c ../rtlib/harray.c ../rtlib/print.c ../rtlib/timer.c

TESTS=	bubble \
	array \
	pointer \
	fib \
	fib-hist \
	ihist \
	primhist \
	f_primhist \
	awise \
	d-awise

GREEN=\033[1;32;40m
RED=\033[1;31;40m
GREY=\033[0m

PASSED=printf "$(GREEN)Passed$(GREY)\n"
FAILED=printf "$(RED)Failed$(GREY)\n"

test_suite: clean $(TESTS)

#
# Array-wise O(n) history test
#
awise: compile_awise
	@printf "Running array-wise O(n) history test:\t"; \
	awise > outputs/awise.out; \
	$(DIFF) outputs/awise.out outputs/awise.txt > /dev/null 2>&1; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

compile_awise: ../tests/awise.s
	@printf "Compiling array-wise O(n) history test:\t"; \
	$(CC) $(CFLAGS) $(LIB_SRCS) ../tests/awise.s -o awise; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

#
# Array-wise O(d) history test
#
d-awise: compile_d-awise
	@printf "Running array-wise O(d) history test:\t"; \
	d-awise > outputs/d-awise.out; \
	$(DIFF) outputs/d-awise.out outputs/awise.txt > /dev/null 2>&1; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

compile_d-awise: ../tests/d-awise.s
	@printf "Compiling array-wise O(d) history test:\t"; \
	$(CC) $(CFLAGS) $(LIB_SRCS) ../tests/d-awise.s -o d-awise; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

#
# Primitive (scalar) history variable test
#
primhist: compile_primhist
	@printf "Running scalar history test:\t\t"; \
	primhist > outputs/primhist.out; \
	$(DIFF) outputs/primhist.out outputs/primhist.txt > /dev/null 2>&1; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

compile_primhist: ../tests/primhist.s
	@printf "Compiling scalar history test:\t\t"; \
	$(CC) $(CFLAGS) $(LIB_SRCS) ../tests/primhist.s -o primhist; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

#
# Primitive history variable with flat storage test
#
f_primhist: compile_f_primhist
	@printf "Running flat scalar history test:\t"; \
	f_primhist > outputs/f_primhist.out; \
	$(DIFF) outputs/f_primhist.out outputs/f_primhist.txt > \
		/dev/null 2>&1; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

compile_f_primhist: ../tests/f_primhist.s
	@printf "Compiling flat scalar history test:\t"; \
	$(CC) $(CFLAGS) $(LIB_SRCS) ../tests/f_primhist.s -o f_primhist; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

#
# Indexed history test
#
ihist: compile_ihist
	@printf "Running indexed scalar history test:\t"; \
	ihist > outputs/ihist.out 2>&1 ; \
	$(DIFF) outputs/ihist.out outputs/ihist.txt > /dev/null 2>&1; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

compile_ihist: ../tests/ihist.s
	@printf "Compiling indexed scalar history test:\t"; \
	$(CC) $(CFLAGS) $(LIB_SRCS) ../tests/ihist.s -o ihist; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

#
# Array test
#
array: compile_array
	@printf "Running array test:\t\t\t"; \
	array > outputs/array.out; \
	$(DIFF) outputs/array.out outputs/array.txt > /dev/null 2>&1; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

compile_array: ../tests/array.s
	@printf "Compiling array test:\t\t\t"; \
	$(CC) $(CFLAGS) $(LIB_SRCS) ../tests/array.s -o array; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

#
# Pointer test
#
pointer: compile_pointer
	@printf "Running pointer test:\t\t\t"; \
	pointer > outputs/pointer.out; \
	$(DIFF) outputs/pointer.out outputs/pointer.txt > /dev/null 2>&1; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

compile_pointer: ../tests/pointer.s
	@printf "Compiling pointer test:\t\t\t"; \
	$(CC) $(CFLAGS) $(LIB_SRCS) ../tests/pointer.s -o pointer; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

#
# Fibonacci Sequence test
#
fib: compile_fib
	@printf "Running Fibonacci test:\t\t\t"; \
	fib > outputs/fib.out; \
	$(DIFF) outputs/fib.out outputs/fib.txt > /dev/null 2>&1; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

compile_fib: ../tests/fib.s
	@printf "Compiling Fibonacci test:\t\t"; \
	$(CC) $(CFLAGS) $(LIB_SRCS) ../tests/fib.s -o fib; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

#
# History Fibonacci Sequence test
#
fib-hist: compile_fib-hist
	@printf "Running history Fibonacci test:\t\t"; \
	fib-hist > outputs/fib-hist.out; \
	$(DIFF) outputs/fib-hist.out outputs/fib-hist.txt > /dev/null 2>&1; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

compile_fib-hist: ../tests/fib-hist.s
	@printf "Compiling history Fibonacci test:\t"; \
	$(CC) $(CFLAGS) $(LIB_SRCS) ../tests/fib-hist.s -o fib-hist; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

#
# Bubble sort test
#
bubble: compile_bubble
	@printf "Running bubble sort test:\t\t"; \
	bubble > outputs/bubble.out; \
	$(DIFF) outputs/bubble.out outputs/bubble.txt > /dev/null 2>&1; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

compile_bubble: ../tests/bubble.s
	@printf "Compiling bubble sort test:\t\t"; \
	$(CC) $(CFLAGS) $(LIB_SRCS) ../tests/bubble.s -o bubble; \
	if [ $$? = 0 ]; then $(PASSED); else $(FAILED); fi

clean:
	rm -f $(TESTS) core* outputs/*.out
