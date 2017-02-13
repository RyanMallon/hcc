HCC_DIR	= $(shell pwd)

CC	= gcc
CFLAGS	= -DHCC_DIR=\"$(HCC_DIR)\" -g -D_DEBUG_ERRORS
YACC	= bison
LEX	= flex
LDFLAGS	=

OBJS=	compiler.o	\
	ast.o		\
	ast_xml.o	\
	cerror.o	\
	debug.o		\
	symtable.o	\
	scope.o		\
	utype.o		\
	strings.o	\
	typechk.o	\
	cfg.o		\
	mir_cfg.o	\
	tempreg.o	\
	icg.o		\
	mir.o		\
	inline.o	\
	peephole.o	\
	astparse.o	\
	history.o	\
	regalloc.o	\
	stabs.o		\
	sparcgen.o	\
	parser.tab.o

compiler: $(OBJS)
	@echo "  LD\t$@"
	@$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o compiler

lex.yy.c: lex.l
	@echo "  LEX\t$<"
	@$(LEX) $<

parser.tab.c: parser.y lex.l
	@echo "  YACC\t$<"
	@$(YACC) -v -d $<

parser.tab.o: parser.tab.c lex.yy.c
	@echo "  CC\t$<"
	@$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	@echo "  CC\t$<"
	@$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: tests perform clean clean_tests
tests: 
	cd tests && make

perform:
	cd perform && make

clean:
	@echo "  CLEAN"
	@rm -f core* compiler *.o *~* lex.yy.c parser.tab.c \
	      parser.tab.h 2>/dev/null

clean_tests:
	rm -f tests/*.lir* tests/*.cfg* tests/*.ig* tests/*.mir* tests/*.s \
		tests/*.ic tests/*~* tests/*.xml tests/core*

clean_perform:
	rm -f perform/*.lir* perform/*.cfg* perform/*.ig* perform/*.mir* \
		perform/*.s perform/*.ic perform/*~* perform/*.xml \
		perform/core*
