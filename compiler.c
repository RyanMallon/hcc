/*
 * compiler.c
 * Ryan Mallon (2005)
 *
 * Main compiler module
 *
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include "ast.h"
#include "cfg.h"
#include "tempreg.h"
#include "regalloc.h"
#include "ast_xml.h"
#include "cflags.h"

static void usage(char *, int);
static char *strip_name(char *);

/* External functions */
extern void inline_funcs(void);
extern void remove_inlined_funcs(void);


/* The basename of the current source file */
char *basename;

/* Long Options */
enum {
  GETOPT_HELP = -255,
  OPTOMISE_REG_ASSIGN,
  FAST_HISTORY_ARRAYS,
  USE_LOCAL_HISTORY_LIB,
  HISTORY_ARG_SETTING,
  HISTORY_SIMPLE_STORE,
  HISTORY_AW_ORDER_D,
  HISTORY_INLINE,
  INLINE,
};

/* Command line options */
static struct option const long_options[] = {
  {"output-ic", no_argument, NULL, 'i'},
  {"output-hlic", no_argument, NULL, 'h'},
  {"output-name", required_argument, NULL, 'o'},
  {"debug-output", no_argument, NULL, 'x'},
  {"debug", required_argument, NULL, 'd'},
  {"debug-symbols", no_argument, NULL, 'g'},
  {"inline", no_argument, NULL, INLINE},

  {"history-simple-store", required_argument, NULL, HISTORY_SIMPLE_STORE},
  {"local-history-lib", no_argument, NULL, USE_LOCAL_HISTORY_LIB},
  {"history-arg", required_argument, NULL, HISTORY_ARG_SETTING},
  {"history-inline", no_argument, NULL, HISTORY_INLINE},
  {"history-aw-order-d", no_argument, NULL, HISTORY_AW_ORDER_D},

  {"target-regs", required_argument, NULL, 'r'},
  {"cpp-args", required_argument, NULL, 'p'},

  {"optomise-reg-assign", required_argument, NULL, OPTOMISE_REG_ASSIGN},
  {"help", no_argument, NULL, GETOPT_HELP},
  {NULL, 0, NULL, 0}
};

char *cwd;

/*
 * FIXME: The original code had my home directory hardcoded for locating
 *        the runtime library headers. I've changed this to be a #define
 *        passed to make, but it does mean that the compiler can only be
 *        run from the directory it was built in.
 */
const char *cpp_cmd = "/usr/bin/cpp -D __HCC__ "
  "-include " HCC_DIR "/rtlib/history.h "
  "-include " HCC_DIR "/rtlib/harray.h "
  "-I " HCC_DIR "/rtlib/ ";
const char *include_rtlib = " -include "HCC_DIR "/rtlib/history.c "
  "-include " HCC_DIR "/rtlib/harray.c ";

extern void yyinit(void);
extern FILE *yyin;
extern ast_node_t *ast;

compiler_options_t cflags;
char *infile, *command;
int yypipe[2];

/*
 * Print out the usage screen
 */
static void usage(char *prog_name, int exit_status) {
  printf("Usage: %s [options]... file\nOptions:\n", prog_name);

  printf("Standard options:\n");
  printf("  -i, --output-ic\t\tOutput IL (deprecated)\n");
  printf("  -h, --output-hlic\t\tOutput highlevel IC, overrides -i\n");
  printf("  -o, --output-name <file>\tPlace the output into <file>\n");
  printf("  -x, --debug-output\t\tGenerate various debug output files\n");
  printf("  -d, --debug <level>\t\tOuptut verbose debugging information\n");
  printf("  -r, --target-regs <regs>\tNumber of hard regs to allocate for\n");
  printf("  -p, --cpp-args <args>\t\tPass the given arguments to the C"
	 " preprocessor\n");
  printf("      --inline\t\t\tInline marked functions\n");
  printf("\nHistory variable options:\n");
  printf("      --history-simple-store <depth>\n");
  printf("\t\t\t\tUse simple storage below the given depth\n");
  printf("      --history-aw-order-d\tUse the O(d) algorithm for array-wise"
	 " arrays\n");
  printf("      --history-inline\t\tInline rtlib history functions\n");

  exit(exit_status);
}

/*
 * Strip a filename by removing the suffix after the final dot character
 */
static char *strip_name(char *filename) {
  int i;
  char *strip;

  if(!filename)
    return NULL;

  for(i = strlen(filename); i > 0; i--)
    if(filename[i] == '.')
      break;

  if(i) {
    strip = malloc(i + 1);
    strncpy(strip, filename, i);
    strip[i] = '\0';
  }

  return strip;
}

/*
 * Compiler entry point
 *
 * Get all the command line options and set up the cflags structure. Then run
 * each of the compiler passes.
 *
 * The current compiler passes are:
 *
 * Source Level:
 *   Pass 1: Build AST from source file.
 *
 * AST Level:
 *   Pass 2: Find variable declarations and setup scopes.
 *   Pass 3: Transform the AST to make the next passes simpler.
 *   Pass 4: Type checking.
 *   Pass 5: Intermediate code generation.
 *
 * IC Level:
 *   Pass 6: Peephole optomizations.
 *
 */
int main(int argc, char **argv) {
  int c, target_regs = 8;
  char *outfile = NULL, *cpp_args = "";

  /*
   * Control flow and interference graphs.
   * TODO: Move this code else where
   */
  cfg_list_t *cfg_lists;


  /* Get command line options */
  cflags.flags = CFLAG_CLEAR_ALL;
  cflags.debug_level = 0;
  cflags.history_arg_setting = HISTORY_ARG_TEMP_COPY;

  while((c = getopt_long(argc, argv, "iho:xgd:r:p:",
			 long_options, NULL)) != -1) {
    switch(c) {
    case 0:
      break;

    case 'i':
      /* Output IC code */
      cflags.flags |= CFLAG_OUTPUT_IC;
      break;

    case 'h':
      /* Output high level IC code */
      cflags.flags |= CFLAG_OUTPUT_HLIC;
      break;

    case 'g':
      /* Debug symbols (stabs) */
      cflags.flags |= CFLAG_OUTPUT_STABS;
      break;

    case 'x':
      /* Output the AST in XML form */
      cflags.flags |= CFLAG_DEBUG_OUTPUT;
      break;

    case 'o':
      /* Output filename */
      basename = malloc(strlen(optarg) + 1);
      strcpy(basename, optarg);
      break;

    case 'd':
      /* Debug level */
      cflags.debug_level = atoi(optarg);
      if(cflags.debug_level < 0 || cflags.debug_level > 3) {
	fprintf(stderr, "Error: Invalid debug level\n");
	exit(EXIT_FAILURE);
      }
      break;

    case 'r':
      /* Target hard-regs */
      target_regs = atoi(optarg);
      break;

    case 'p':
      /* cpp args */
      cpp_args = malloc(strlen(optarg) + 1);
      strcpy(cpp_args, optarg);
      break;

    case INLINE:
      cflags.flags |= CFLAG_INLINE;
      break;

    case OPTOMISE_REG_ASSIGN:
      if(!atoi(optarg))
	cflags.oflags &= ~OFLAG_REG_ASSIGN;
      else
	cflags.oflags |= OFLAG_REG_ASSIGN;
      break;

    case USE_LOCAL_HISTORY_LIB:
      cflags.flags |= CFLAG_USE_LOCAL_HISTORY_LIB;
      break;

    case HISTORY_SIMPLE_STORE:
      cflags.flags |= CFLAG_HISTORY_SIMPLE_STORE;
      cflags.history_simple_depth = atoi(optarg);
      break;

    case HISTORY_AW_ORDER_D:
      cflags.flags |= CFLAG_HISTORY_AW_ORDER_D;
      break;

    case HISTORY_ARG_SETTING:
      if(!strcmp(optarg, "disabled"))
	cflags.history_arg_setting = HISTORY_ARG_DISABLED;
      else if(!strcmp(optarg, "temp-copy"))
	cflags.history_arg_setting = HISTORY_ARG_TEMP_COPY;
      break;

    case HISTORY_INLINE:
      cflags.flags |= CFLAG_HISTORY_INLINE;
      break;

    case GETOPT_HELP:
      usage(argv[0], EXIT_SUCCESS);
      break;

    default:
      /* Invalid argument */
      usage(argv[0], EXIT_FAILURE);
      break;
    }
  }

  while(optind < argc) {
    infile = malloc(strlen(argv[optind]) + 1);
    strcpy(infile, argv[optind++]);
  }

  /*
   * Get the current working directory.
   * FIXME: Can't deal with paths longer than 1024 characters.
   */
  cwd = malloc(1024);
  getcwd(cwd, 1024);

  /* Make sure there is a source file */
  if(!infile) {
    fprintf(stderr, "Error: No input files specified\n");
    exit(EXIT_FAILURE);
  }

  /*
   * Get the basename for the output file.
   * If the -o argument is not given, then the use the basename from
   * the input file
   */
  if(!basename)
    basename = strip_name(infile);

  if((yyin = fopen(infile, "r")) == NULL) {
    fprintf(stderr, "Error: Couldn't open %s\n", infile);
    exit(EXIT_FAILURE);
  }

  if(!outfile) {
    if(cflags.flags & CFLAG_OUTPUT_IC) {
      outfile = malloc(strlen(basename) + 4);
      strcpy(outfile, basename);
      strcat(outfile, ".ic");
    }
  }

  /* Entry Point - Defaults to main if one isn't specified */
  if(!cflags.entry_point) {
    cflags.entry_point = malloc(5);
    strcpy(cflags.entry_point, "main");
  }

  /* Run the C Preprocessor and send its output to yyin */
  command = malloc(strlen(cpp_cmd) + strlen(cpp_args) +
		   strlen(include_rtlib) + strlen(infile) + 1);

  strcpy(command, cpp_cmd);
  if(cpp_args)
    strcat(command, cpp_args);
  if(cflags.flags & CFLAG_HISTORY_INLINE)
    strcat(command, include_rtlib);
  strcat(command, infile);

  debug_printf(1, "cpp command: %s\n", command);
  yyin = popen(command, "r");
  free(command);

  /* Parse the source file to create the AST */
  yyinit();
  yyparse();

  /* Var declaration an scope creation pass */
  create_global_scope();
  parse_declarations(ast);
  build_def_func_table();

  debug_printf(1, "\nSCOPE_LIST\n");
  print_scope_lists();
  debug_printf(1, "----\nDeclaration Parsing Done\n----\n");


  /* Graph building and first register allocation pass */
  tr_init();

  /* AST transform pass */
  if(!(cflags.flags & CFLAG_OUTPUT_HLIC))
    munge_assign_address(ast);

  munge_array_indicies(ast);
  munge_duplicate_unaries(ast);

  /* Type checking pass */
  type_check_node(ast);
  set_global_offset();

  debug_printf(1, "----\nType Checking Done\n----\n");

 xml:
  /* XML Output */
  if(cflags.flags & CFLAG_DEBUG_OUTPUT) {
    char *astfile, *scopefile;

    /* Syntax Tree */
    astfile = malloc(strlen(basename) + 9);
    strcpy(astfile, basename);
    strcat(astfile, ".ast.xml");
    ast2xml(ast, astfile);


    /* Scope Tree */
    scopefile = malloc(strlen(basename) + 8);
    strcpy(scopefile, basename);
    strcat(scopefile, ".nt.xml");
    print_xml_scope_list(scopefile);


    debug_printf(1, "Output xml files %s and %s\n", astfile, scopefile);

  }


  /* Compile pass */
  parse_ast(ast);
  ast_free(ast);

  debug_printf(1, "----\nParsing Done\n----\n");

  if(cflags.flags & CFLAG_OUTPUT_HLIC) {
    mcfg_list_t *cfg_list;
    ig_graph_t *ig_graph;

    mir_strip();
    mir_print(basename, "mir-pre");

    if(cflags.flags & CFLAG_INLINE) {
      inline_funcs();
      remove_inlined_funcs();
    }

    mir_print(basename, "mir-inline");

    /* Transform MIR into something useable */
    mir_munge_globals();
    mir_munge();
    mir_munge_pointers();
    mir_spill_may_aliases();
    mir_fold_constants();
    mir_rhs_constants();
    mir_args_to_regs(6);
    mir_label_nodes();
    mir_print(basename, "mir");

    cfg_list = mcfg_build(mir_list_head());
    global_load_store(cfg_list);

    /* Register allocation */
    allocate_registers(cfg_list, target_regs);

    /* Fingers crossed ;-) */
    gen_sparc_code(basename);

  }

  exit(EXIT_SUCCESS);
}
