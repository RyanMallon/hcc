#!/bin/bash
#
# Performance Tests
#

# Number of times to run tests
LOOPS=100
SLOOPS=10

RTLIB='../rtlib/print.c ../rtlib/timer.c'
HISTLIB='../rtlib/history.c'
AHISTLIB='../rtlib/harray.c'

AW_MAX='100'
SCALAR_MAX='30'

#
# Test 1: Compare history assignment (cyclic and non-cyclic) against normal variable assignment
#
function test1 {
    printf "Compiling scalar assign test: "

    # Control
    gcc $RTLIB $HISTLIB ../perform/control-assign.s -o control-assign
    printf "."

    for i in `seq 1 $SCALAR_MAX` ; do
	# Cyclic storage
	gcc $RTLIB $HISTLIB ../perform/scalar-assign$i.s -o scalar-assign$i
	printf "."
	gcc -O2 $RTLIB $HISTLIB ../perform/scalar-assign$i.s -o scalar-assign-opt$i
	printf "."
	gcc $RTLIB ../perform/inline-scalar-assign$i.s -o inline-scalar-assign$i
	printf "."
	gcc $RTLIB ../perform/hcc-scalar-assign$i.s -o hcc-scalar-assign$i
	printf "."

	# Flat storage
	gcc $RTLIB $HISTLIB ../perform/fscalar-assign$i.s -o fscalar-assign$i
	printf "."
	gcc -O2 $RTLIB $HISTLIB ../perform/fscalar-assign$i.s -o fscalar-assign-opt$i
	printf "."
	gcc $RTLIB ../perform/inline-fscalar-assign$i.s -o inline-fscalar-assign$i
	printf "."
	gcc $RTLIB ../perform/hcc-fscalar-assign$i.s -o hcc-fscalar-assign$i
	printf "."

    done
    printf ".\n"

    rm -f perform/scalar-assign/*.log

    printf "Running scalar assign test: "
    for i in `seq 1 $LOOPS` ; do
	# Control
	./control-assign | cut -d " " -f4 >> perform/scalar-assign/control.log
	printf "."

	for j in `seq 1 SCALAR_MAX` ; do
	    # Cyclic storage
	    ./scalar-assign$j | cut -d " " -f4 >> perform/scalar-assign/gcc$j.log
	    printf "."
	    ./scalar-assign-opt$j | cut -d " " -f4 >> perform/scalar-assign/gcc-opt$j.log
	    printf "."
	    ./inline-scalar-assign$j | cut -d " " -f4 >> perform/scalar-assign/hcc-inline$j.log
	    printf "."
	    ./hcc-scalar-assign$j | cut -d " " -f4 >> perform/scalar-assign/hcc$j.log
	    printf "."

	    # Flat storage
	     ./fscalar-assign$j | cut -d " " -f4 >> perform/fscalar-assign/gcc$j.log
	    printf "."
	    ./fscalar-assign-opt$j | cut -d " " -f4 >> perform/fscalar-assign/gcc-opt$j.log
	    printf "."
	    ./inline-fscalar-assign$j | cut -d " " -f4 >> perform/fscalar-assign/hcc-inline$j.log
	    printf "."
	    ./hcc-fscalar-assign$j | cut -d " " -f4 >> perform/fscalar-assign/hcc$j.log
	    printf "."

	done
    done
    printf "\n"
}

#
# Test 2: Compare performance of inline/non-inlined for multiple assignments
#
function test2 {
    printf "Compiling inline vs no-inline test: "
    for i in 1 2 4 8 16 32 64; do
	gcc $RTLIB ../perform/as-$i.s -o as-$i
	printf "."
	gcc $RTLIB ../perform/inline-as-$i.s -o inline-as-$i
	printf "."

	gcc $RTLIB ../perform/fas-$i.s -o fas-$i
	printf "."
	gcc $RTLIB ../perform/inline-fas-$i.s -o inline-fas-$i
	printf "."
    done

    rm -f perform/inline/*.log

    printf "\nRunning inline vs no-inline test"
    for i in `seq 1 $SLOOPS` ; do
	for j in 1 2 4 8 16 32 64; do
	    ./as-$j | cut -d " " -f4 >> perform/inline/as-$j.log
	    printf "."
	    ./inline-as-$j | cut -d " " -f4 >> perform/inline/inline-as-$j.log
	    printf "."

	     ./fas-$j | cut -d " " -f4 >> perform/inline/fas-$j.log
	    printf "."
	    ./inline-fas-$j | cut -d " " -f4 >> perform/inline/inline-fas-$j.log
	    printf "."
	done
    done
    printf "\n"
}

#
# Test 3: Compare array-wise O(d) and O(n) algorithms
#
function test3 {
    printf "Compiling array-wise O(d) vs O(n) test: "
    for i in `seq 1 $AW_MAX`; do
	gcc -O2 $RTLIB $AHISTLIB ../perform/d-aw-assign-$i-$AW_MAX.s -o d-aw-assign-$i-$AW_MAX
	printf "."
	gcc -O2 $RTLIB $AHISTLIB ../perform/n-aw-assign-$i-$AW_MAX.s -o n-aw-assign-$i-$AW_MAX
	printf "."
	gcc -O2 $RTLIB $AHISTLIB ../perform/d-aw-assign-$AW_MAX-$i.s -o d-aw-assign-$AW_MAX-$i
	printf "."
	gcc -O2 $RTLIB $AHISTLIB ../perform/n-aw-assign-$AW_MAX-$i.s -o n-aw-assign-$AW_MAX-$i
	printf "."
    done

    rm -f perform/awise/*.log

    printf "\nRunning array-wise O(d) vs O(n) test: "
    for j in `seq 1 $LOOPS`; do
	for i in `seq 1 $AW_MAX`; do
	    ./d-aw-assign-$i-$AW_MAX | cut -d " " -f4 >> perform/awise/d-aw-assign-$i-$AW_MAX.log
	    printf "."
	    ./n-aw-assign-$i-$AW_MAX | cut -d " " -f4 >> perform/awise/n-aw-assign-$i-$AW_MAX.log
	    printf "."
	    ./d-aw-assign-$AW_MAX-$i | cut -d " " -f4 >> perform/awise/d-aw-assign-$AW_MAX-$i.log
	    printf "."
	    ./n-aw-assign-$AW_MAX-$i | cut -d " " -f4 >> perform/awise/n-aw-assign-$AW_MAX-$i.log
	    printf "."
	done
    done
    printf "\n"
}

#
# Test 4: Array control
#
function test4 {
    printf "Compiling array control test: "
    for i in `seq 1 $AW_MAX`; do
	gcc -O2 $RTLIB ../perform/array-$i.s -o array-$i
	printf "."
    done

    rm -f perform/array/*.log

    printf "\nRunning array control test: "
    for j in `seq 1 $LOOPS`; do
	for i in `seq 1 $AW_MAX`; do
	    ./array-$i | cut -d " " -f4 >> perform/array/array-$i.log
	    printf "."
	done
    done
    printf "\n"
}

#
# Test 5: Index-wise test
#
function test5 {
    printf "Compiling index-wise test: "
    for i in `seq 1 $AW_MAX`; do
	for j in 1 10 50 100; do
	    gcc -O2 $RTLIB $AHISTLIB ../perform/iw-assign-$i-$j.s -o iw-assign-$i-$j
	    printf "."
	done
    done

    rm -f perform/iwise/*.log

    printf "\nRunning index-wise test: "
    for i in `seq 1 $AW_MAX`; do
	for j in 1 10 50 100; do
	    ./iw-assign-$i-$j | cut -d " " -f4 >> perform/iwise/iwise-$i-$j.log
	    printf "."
	done
    done
    printf "\n"
}

#
# Test 6: Scalar retrieval test
#
function test6 {
    printf "Compiling scalar retrieval test: "
    gcc $RTLIB ../perform/control-retrieve.s -o control
    for i in `seq 1 $SCALAR_MAX`; do
	gcc $RTLIB ../perform/scalar-retrieve-$i.s -o scalar-retrieve-$i
	printf "."
	gcc $RTLIB ../perform/fscalar-retrieve-$i.s -o fscalar-retrieve-$i
	printf "."
	gcc -O2 $RTLIB $HISTLIB ../perform/gcc-scalar-retrieve-$i.s -o gcc-scalar-retrieve-$i
	printf "."
	gcc -O2 $RTLIB $HISTLIB ../perform/gcc-fscalar-retrieve-$i.s -o gcc-fscalar-retrieve-$i
    done

    rm -f perform/retrieve/*.log

    printf "\nRunning scalar retrieval test: "

    for j in `seq 1 $LOOPS`; do
	./control | cut -d " " -f4 >> perform/retrieve/control.log

	for i in `seq 1 $SCALAR_MAX`; do
	    ./scalar-retrieve-$i | cut -d " " -f4 >> perform/retrieve/cyclic-$i.log
	    printf "."
	    ./fscalar-retrieve-$i | cut -d " " -f4 >> perform/retrieve/flat-$i.log
	    printf "."
	    ./gcc-scalar-retrieve-$i | cut -d " " -f4 >> perform/retrieve/gcc-cyclic-$i.log
	    printf "."
	    ./gcc-fscalar-retrieve-$i | cut -d " " -f4 >> perform/retrieve/gcc-flat-$i.log
	done
    done
    printf "\n"
}

#
# Test 7: Array retrieval test
#
function test7 {
    printf "Compiling array retrieval test: "

    # Controls
    for i in `seq 1 20`; do
	gcc $RTLIB ../perform/array-control-retrieve-$i.s -o control-$i;
	printf "."
    done

    for i in `seq 1 20`; do
	for j in `seq 1 20`; do
	    # Array-wise GCC
	    gcc $RTLIB $HISTLIB ../perform/aw-retrieve-$i-$j.s -o gcc-aw-retrieve-$i-$j
	    printf "."
	    gcc -O2 $RTLIB $HISTLIB ../perform/aw-retrieve-$i-$j.s -o gcc-opt-aw-retrieve-$i-$j
	    printf "."

	    # Array-wise HCC
	    gcc $RTLIB ../perform/hcc-aw-retrieve-$i-$j.s -o hcc-aw-retrieve-$i-$j
	    printf "."
	    gcc $RTLIB ../perform/inline-aw-retrieve-$i-$j.s -o inline-aw-retrieve-$i-$j
	    printf "."

	    # Index-wise GCC
	    gcc $RTLIB $HISTLIB ../perform/iw-retrieve-$i-$j.s -o gcc-iw-retrieve-$i-$j
	    printf "."
	    gcc -O2 $RTLIB $HISTLIB ../perform/iw-retrieve-$i-$j.s -o gcc-opt-iw-retrieve-$i-$j
	    printf "."

	    # Index-wise HCC
	    gcc $RTLIB ../perform/hcc-iw-retrieve-$i-$j.s -o hcc-iw-retrieve-$i-$j
	    printf "."
	    gcc $RTLIB ../perform/inline-iw-retrieve-$i-$j.s -o inline-iw-retrieve-$i-$j
	    printf "."
	done
    done

    printf "\nRunning array retrieval test: "

    rm -f perform/array-retrieve/*.log

    # Controls
    for i in `seq 1 20`; do
	./control-$i | cut -d " " -f4 >> perform/array-retrieve/control-$i.log
	printf "."
    done

    for k in `seq 1 $SLOOPS`; do
	for i in `seq 1 20`; do
	    for j in `seq 1 20`; do
		# Array-wise GCC
		./gcc-aw-retrieve-$i-$j | cut -d " " -f4 >> perform/array-retrieve/gcc-aw-retrieve-$i-$j.log
		printf "."
		./gcc-opt-aw-retrieve-$i-$j | cut -d " " -f4 >> perform/array-retrieve/gcc-opt-aw-retrieve-$i-$j.log
		printf "."

		# Array-wise HCC
		./hcc-aw-retrieve-$i-$j | cut -d " " -f4 >> perform/array-retrieve/hcc-aw-retrieve-$i-$j.log
		printf "."
		./inline-aw-retrieve-$i-$j | cut -d " " -f4 >> perform/array-retrieve/inline-aw-retrieve-$i-$j.log

		# Index-wise GCC
		./gcc-iw-retrieve-$i-$j | cut -d " " -f4 >> perform/array-retrieve/gcc-iw-retrieve-$i-$j.log
		printf "."
		./gcc-opt-iw-retrieve-$i-$j | cut -d " " -f4 >> perform/array-retrieve/gcc-opt-iw-retrieve-$i-$j.log
		printf "."

		# Index-wise HCC
		./hcc-iw-retrieve-$i-$j | cut -d " " -f4 >> perform/array-retrieve/hcc-iw-retrieve-$i-$j.log
		printf "."
		./inline-iw-retrieve-$i-$j | cut -d " " -f4 >> perform/array-retrieve/inline-iw-retrieve-$i-$j.log
	    done
	done
    done
    printf "\n"
    # Phew. Done ;-)
}

# Main
case $1 in
1) test1 ;;
2) test2 ;;
3) test3 ;;
4) test4 ;;
5) test5 ;;
6) test6 ;;
*) printf "Invalid test number\n" ;;
esac
exit
