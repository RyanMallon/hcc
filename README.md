History Capable Compiler (HCC)
==============================

This is the compiler that I wrote as part of my master's research. The
"history capable" part refers to my research being on history
variables, which are able to have their previously assigned values
retrieved using an array-like syntax (see my thesis
https://www.cosc.canterbury.ac.nz/research/reports/MastTheses/2007/mast_0706.pdf
for all the gory details).

Release and License
-------------------

I have released this code as-is, in the hope that it is interesting or
useful to somebody.

I have only made a handful of fixes to get it to build on a modern
Linux system, otherwise the code is exactly as I left it. It is not
indicative of my current style of coding. Much of the code is
embarrassing to look at now (who in their right mind uses two space
indentation?).

This code is public domain. The code may be freely incorporated into
other projects, either source or binary.

Development
-----------

I needed a compiler to test the implementation of history variables
for my research. Initially the compiler started as a set of extensions
to Professor Tad Takaoka's (my supervisor) PL/0 compiler
(https://www.cosc.canterbury.ac.nz/tad.takaoka/cosc230/). The PL/0
compiler generated code for a simple stack based machine. This quickly
became limiting, and I started from scratch building a compiler with a
more conventional design using an abstract syntax tree (AST) and
targeting an abstract machine with infinite registers. Initially I
wrote a virtual machine for the runtime, which became difficult to
maintain. I had not at the time learned x86 assembler, but I did know
MIPS. My university had a number of SPARC V8 server machines, and so I
ended up writing a SPARC backend.

A friend had lent me a copy of the book Advanced Compiler Design and
Implementation by Steven Muchnick. This and the Dragon book
(Compilers: Principals, Techniques and Tools by Alfred Aho, Monica
Lam, Ravi Sethi and Jeffrey Ullman) helped enormously with the design
and implementation of HCC. In particular, the intermediate code
representation used in HCC is heavily based on the MIR (medium
intermediate representation) language designed by Muchnick.

Features
--------

HCC has a number of features, many of which came about due to my
interest in learning about compiler design:

 * Frontend/backend design. The compiler frontend consists of the lexer
   and parser. It generates an intermediate code representation. The
   backend takes the intermediate code and generates code for the target
   machine.

 * The lexer and parser use Lex and Bison.

 * The parser generates an abstract syntax tree (AST). The are some basic
   transformations made to the AST before it is parsed to generate the
   intermediate representation.

 * Control flow graph (CFG) with variable liveness tracking.

 * The parser generates an intermediate code representation called MIR
   (medium-level intermediate representation). The MIR is a generic
   assembly language with infinite registers.

 * Graph colouring register allocator with spill cost calculations.

 * SPARC V8 code generator. The code generator produces an assembler source
   file, which can then be passed to an assembler such as the GNU Assembler.

 * Simple type-checker.

 * Support for the STABS debugging format.

 * The compiler can generate a large amount of debug information about the
   compilation process, including VCG and XML representations of many of
   the intermediate forms (AST, CFG, etc).

 * The compiler supports a C-like language with strings, arrays,
   pointers and user defined types (structures). It has some basic
   interoperability with C.

History Variables
-----------------

The subject of my research was variables which maintain their previous
values. A history variable is declared with a "depth", which is the
number of previous values it can store. An array-like syntax is used
to fetch previously stored values. The following program declares an
integer history variable, assigns three values to it, and then prints
them out:

```
int main()
{
    var int hvar<:3:>;

    hvar := 1;
    hvar := 2;
    hvar := 3;

    printf("hvar = %d, %d, %d\n", hvar, hvar<:1:>, hvar<:2>);
    return 0;
}
```

History can also be applied to complex types such as strings, pointers
and arrays. In the case of arrays two types of history are defined:
index-wise, where each array index has its own independent history,
and array-wise, where assigning to any index will cause a snapshot of
the entire array to be stored to history.

The sample files and my original thesis provide further information
about the history variable implementation.

Runtime Library
---------------

A runtime library is included which provides support for some of the
more complex history variable operations, and also some wrapper around
standard C library functions for printing, timing, etc. The runtime
library sources are in the rtlib directory.

Building and Running
--------------------

The compiler can be built on a Linux system using Make. The build
process hardcodes the build directory into the compiler so that it is
able to find the runtime library files. This means that the compiler
must be run from the directory is was built in.

To run the compiler you need to pass the ```-h``` flag to instruct the
compiler to generate high-level intermediate code (MIR), and the name
of the source file to compile. For example, to compile the bubble-sort
test program:

```
./compiler -h tests/bubble.hc
```

This will produce the file ```tests/bubble.s```, this can be compiled
using a SPARC V8 compiler (either native or cross-compiler):

```
sparc-v8-gcc tests/bubble.s -o tests/bubble
```

To run the compiled programs you will need SPARC V8 machine or an
emulator. From memory the original SPARC machine I developed against
was running Solaris, but it should work with Linux as the host also.

Sample and Test Programs
------------------------

There are a number of sample programs in the tests and perform
directories. These sample programs are probably in various states of
disrepair. Many where written to exercise some particular feature of
the compiler as I wrote it.

The sparc directory contains scripts which where used to perform
automated testing of the compiler. Running the following on a SPARC V8
machine will run the automated tests:

```
cd sparc
./run_tests.sh
```
