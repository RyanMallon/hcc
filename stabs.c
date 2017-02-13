/*
 * stabs.c
 * Ryan Mallon (2006)
 *
 * Stabs debugging symbols output
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include "symtable.h"
#include "typechk.h"
#include "scope.h"
#include "sparc.h"

static int stabs_print_type(FILE *, type_info_t *);
static void stabs_print_locals(FILE *, scope_node_t *, int, int);

static type_info_t **type_table;
static int num_types;
extern char *cwd, *basename;

#define STABS_INT 0

/*
 * Create the type table
 */
void stabs_init_type_table(void) {
  num_types = 3;
  type_table = malloc(sizeof(type_info_t *) * num_types);

  type_table[0] = create_type_signature(TYPE_CONSTANT, TYPE_INTEGER, NULL);
  type_table[1] = create_type_signature(TYPE_CONSTANT, TYPE_CHAR, NULL);
  type_table[2] = create_type_signature(TYPE_CONSTANT, TYPE_VOID, NULL);
}

/*
 * Print out the stabs type defintions
 */
void stabs_print_types(FILE *fd) {
  fprintf(fd, ".stabs \"int:t(0,0)=r(0,0);0020000000000;0017777777777;\","
	  "128,0,0,0\n");
  fprintf(fd, ".stabs \"char:t(0,1)=r(0,1);0;127;\",128,0,0,0\n");
  fprintf(fd, ".stabs \"void:t(0,2)=(0,2)\",128,0,0,0\n");
}

/*
 * Print out the source file information
 */
void stabs_print_file_info(FILE *fd) {
  fprintf(fd, ".stabs \"%s/%s.hc\",100,0,0,.Ltext0\n", cwd, basename);
}

/*
 * Print the stab for a function, its arguments and local variables
 */
void stabs_print_function(FILE *fd, name_record_t *func,
			  int frame_size, int offset_words) {
  int i;
  scope_node_t *arg_scope;

  /* Function name and return type */
  fprintf(fd, ".stabs \"%s:F", func->name);
  stabs_print_type(fd, func->type_info);
  fprintf(fd, "\",36,0,0,%s\n", func->name);

  /* Arguments */
  arg_scope = get_func_arg_scope(func->name);

  for(i = 0; i < func->num_args; i++) {
    fprintf(fd, ".stabs \"%s:P", arg_scope->name_table[i]->name);
    stabs_print_type(fd, arg_scope->name_table[i]->type_info);
    fprintf(fd, "\",64,0,0,%d\n", SPARC_INS + i);

    if(arg_scope->name_table[i]->reg_alloc != -1) {
      fprintf(fd, ".stabs \"%s:r", arg_scope->name_table[i]->name);
      stabs_print_type(fd, arg_scope->name_table[i]->type_info);
      fprintf(fd, "\",64,0,0,%d\n", SPARC_LOCALS + i);
    }
  }

  /* Locals */
  if(arg_scope->num_children)
    stabs_print_locals(fd, arg_scope->children[0], frame_size, offset_words);
}

/*
 * Print the stabs for local variables
 */
static void stabs_print_locals(FILE *fd, scope_node_t *scope,
			       int frame_size, int offset_words) {
  int i;

  for(i = 0; i < scope->num_records; i++) {

    /* Change dots to underscores for internal history names */
    if(scope->name_table[i]->name[0] == '.')
      fprintf(fd, ".stabs \"_%s:", scope->name_table[i]->name + 1);
    else
      fprintf(fd, ".stabs \"%s:", scope->name_table[i]->name);

    stabs_print_type(fd, scope->name_table[i]->type_info);

    if(is_array_type(scope->name_table[i]->type_info) ||
       is_history_type(scope->name_table[i]->type_info)) {
      /* Array or history cycle */
      int size;
      type_info_t *type = scope->name_table[i]->type_info;
      type_info_t *a_type = create_type_signature(TYPE_CONSTANT,
						  primitive_type(type), NULL);

      fprintf(fd, "=ar");
      stabs_print_type(fd, type_table[STABS_INT]);

      if(is_array_type(type))
	size = get_dimension_size(type, 1) - 1;
      else
	size = get_history_depth(type) - 1;

      fprintf(fd, ";0;%d;", size);
      stabs_print_type(fd, a_type);
      free(a_type);

    }

    fprintf(fd, "\",128,0,0,-%d\n", fp_offset(scope->name_table[i]->offset,
					      frame_size, offset_words));

  }

  for(i = 0; i < scope->num_children; i++)
    stabs_print_locals(fd, scope->children[i], frame_size, offset_words);
}



/*
 * Print the stabs for a line number
 */
void stabs_print_line_num(FILE *fd, int line_num, char *func_name) {
  fprintf(fd, ".stabn 68,0,%d,.LLM%d-%s\n", line_num, line_num, func_name);
  fprintf(fd, ".LLM%d:\n", line_num);
}

/*
 * Add a new type to the table
 */
static int stabs_add_type(type_info_t *type) {
  type_table = realloc(type_table, sizeof(type_info_t *) * (num_types + 1));
  type_table[num_types] = malloc(sizeof(type_info_t));
  copy_sig(type, type_table[num_types]);
  return num_types++;
}

/*
 * Return the stab type
 */
static int stabs_type(type_info_t *type) {
  int i;

  for(i = 0; i < num_types; i++)
    if(compare_type_sigs(type, type_table[i]))
      return i;

  return -1;
}

/*
 * Print out the stab type. Returns 1 if the printed type already existed,
 * or 0 if a new type was created.
 */
static int stabs_print_type(FILE *fd, type_info_t *type) {
  int index;

  if((index = stabs_type(type)) == -1) {
    /* Create a new type */
    fprintf(fd, "(0,%d)", stabs_add_type(type));

    while(is_address_type(type)) {
      fprintf(fd, "=*");
      remove_type_from_sig(&type, TYPE_ADDRESS);

      if(stabs_print_type(fd, type))
	break;
    }
    return 0;

  } else {
    fprintf(fd, "(0,%d)", index);
    return 1;
  }
}
