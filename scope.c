/*
 * scope.c
 * Ryan Mallon (2005)
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "ast.h"
#include "typechk.h"
#include "symtable.h"
#include "utype.h"
#include "cerror.h"
#include "error.h"
#include "icg.h"
#include "cflags.h"


extern compiler_options_t cflags;

/* Scope List */
scope_node_t *global_scope, *current_scope;

static void set_scope_offset(scope_node_t *);

/*
 * Build the global scope node
 */
void create_global_scope(void) {
  global_scope = current_scope = NULL;
}

/*
 * Create a new top-level scope node for a function
 */
scope_node_t *new_func_scope(char *name) {
  scope_node_t *scope;

  debug_printf(1, "Creating function scope for %s\n", name);

  scope = malloc(sizeof(scope_node_t));
  scope->name = malloc(strlen(name) + 1);
  strcpy(scope->name, name);

  scope->prev = global_scope;
  scope->offset = 0;
  scope->size = 0;
  scope->num_records = 0;
  scope->num_children = 0;
  scope->children = NULL;
  scope->name_table = NULL;

  return scope;
}

/*
 * Return the number of declared functions
 */
int num_decl_functions(void) {
  int i, count;

  for(i = 0, count = 0; i < global_scope->num_records; i++)
    if(global_scope->name_table[i]->type_info->decl_type == TYPE_FUNCTION)
      count++;

  return count;
}

/*
 * Get the size of the global scope
 */
int global_scope_size(void) {
  int size;

  size = global_scope->size;

  if(size % IC_SIZEOF_INT != 0)
    size += (IC_SIZEOF_INT - (size % IC_SIZEOF_INT));

  return size;
}

/*
 * Add a new scope node after the current scope,
 * Return the child index number
 */
int add_scope(scope_node_t *scope) {
  int num = current_scope->num_children;

  debug_printf(1, "Adding a new scope, current_scope = %p\n", current_scope);

  if(!num)
    current_scope->children = malloc(sizeof(scope_node_t *));
  else
    current_scope->children = realloc(current_scope->children,
				      sizeof(scope_node_t *) * (num + 1));

  /* Copy the name of the current scope to the new scope */
  scope->name = malloc(strlen(current_scope->name) + 1);
  strcpy(scope->name, current_scope->name);

  scope->prev = current_scope;
  scope->children = NULL;
  current_scope->children[num] = scope;
  current_scope->num_children++;

  current_scope = scope;

  return num;
}

/*
 * Enter a new scope (NOT USED)
 *
 * Scopes are entered by checking the scope_tag on block nodes, ie:
 *   if(block->node.scope_tag != -1)
 *     current_scope = current_scope->children[block->node.scope_tag];
 *
 * FIXME: This never gets called. Remove it?
 *
 */
scope_node_t *enter_scope(void) {
  int i, offset;
  debug_printf(1, "-- Entering scope --\n");
  return current_scope;
}

scope_node_t *enter_function_scope(char *name) {
  int i;

  debug_printf(1, "Entering scope for function %s\n", name);

  for(i = 0; i < global_scope->num_children; i++) {
    if(!strcmp(global_scope->children[i]->name, name)) {
      return global_scope->children[i];

    }
  }
  return NULL;
}

/*
 * Return the current scope depth
 */
int get_scope_depth(void) {
  int depth = 0;
  scope_node_t *current = current_scope;

  while(current->prev) {
    depth++;
    current = current->prev;
  }

  return depth;
}

/*
 * Return the maximum size
 */
int max_scope_size(scope_node_t *scope) {
  int i, size, max_size;

  if(scope->num_children == 0)
    return scope->size;

  if(scope->num_records > 0 && scope->size == 0)
    debug_printf(1, "WARNING: Scope has records but zero size, record 1 = %s\n",
		 scope->name_table[0]->name);

  max_size = 0;
  for(i = 0; i < scope->num_children; i++) {
    size = max_scope_size(scope->children[i]);
    if(size > max_size)
      max_size = size;
  }

  return max_size + scope->size;
}

/*
 * Return the size of the global scope (ie data segment)
 */
int data_seg_size(void) {
  return max_scope_size(global_scope);
}

/*
 * Return the scope that the given var is in.
 *
 * TODO: This function can probably be improved greatly
 *
 */
scope_node_t *get_var_scope(name_record_t *var) {
  int i;
  scope_node_t *current = current_scope;

  if(var->type_info->decl_type == TYPE_MEMBER)
    var = var->struct_parent;

  while(current) {
    for(i = 0; i < current->num_records; i++)
      if(!strcmp(current->name_table[i]->name, var->name))
	return current;

    current = current->prev;
  }


  return NULL;
}

/*
 * Get the index of a global variable
 */
int global_index(name_record_t *var) {
  int i;

  for(i = 0; i < global_scope->num_records; i++)
    if(global_scope->name_table[i] == var)
      return i;

  return -1;
}

/*
 * Get the scope for a function
 */
scope_node_t *get_func_scope(char *name) {
  int i;

  for(i = 0; i < global_scope->num_children; i++)
    if(!strcmp(global_scope->children[i]->name, name))
      return global_scope->children[i];

  return NULL;
}

/*
 * Return the scope for the given functions arguments
 */
scope_node_t *get_func_arg_scope(char *name) {
  int i;

  for(i = 0; i < global_scope->num_children; i++)
    if(!strcmp(global_scope->children[i]->name, name))
      return global_scope->children[i];

  return NULL;
}

scope_node_t *parse_function_scope(char *name) {
  int i, offset;

  offset = current_scope->offset + current_scope->size;

  if(current_scope != global_scope)
    compiler_error(CERROR_ERROR, CERROR_UNKNOWN_LINE,
		   "Attempt to parse function body from non-global scope\n");

  debug_printf(1, "Entering function scope %s, offset = %d\n", name, offset);

  for(i = 0; i < global_scope->num_records; i++) {
    if(!strcmp(global_scope->name_table[i]->name, name)) {
      current_scope = global_scope->children[i];
      current_scope->offset = offset;
      return current_scope;
    }
  }
  return NULL;
}

/*
 * Remove the current scope from the list
 */
void exit_scope(void) {
  int i;

  ic_emit(IC_ADD, IC_SP, current_scope->size, IC_SP, IC_REG, IC_CONST, IC_REG);

  debug_printf(1, "Leaving scope\n");

  if(current_scope == global_scope)
    compiler_error(CERROR_ERROR, CERROR_UNKNOWN_LINE,
		   "Attempted to remove global scope node\n");

  if(!current_scope->prev)
    compiler_error(CERROR_ERROR, CERROR_UNKNOWN_LINE,
		   "Current scope doesn't have a previous scope\n");

  current_scope = current_scope->prev;

}


/*
 * Align the offsets in the scope table
 */
void align_scope_offsets(int (*align_func)(int)) {
  int i, base;

#if 0
  base = 0;

  for(i = 0; i < num_records; i++)
    if(global_scope->name_table[i]->type_info->decl_type != TYPE_FUNCTION) {



    }
#endif

}

/*
 * Adjust the offsets for variables in a given scope
 */
void adjust_scope_offsets(scope_node_t *scope, int offset) {
  int i;

  for(i = 0; i < scope->num_records; i++)
    scope->name_table[i]->offset += offset;

  for(i = 0; i < scope->num_children; i++)
    adjust_scope_offsets(scope->children[i], offset);
}

/*
 * Set up the offsets for each scope
 *
 * Function arguments have their offsets set up when their scope is created.
 * Each scope is offset realitive to the previous scope.
 *
 */
void set_scope_offsets(void) {
  int i, j;
  scope_node_t *current;

  for(i = 0; i < global_scope->num_children; i++) {
    current = global_scope->children[i];
    current->offset = 0;

    set_scope_offset(current);
  }
}

static void set_scope_offset(scope_node_t *current) {
  int i, offset;

  debug_printf(1, "Setting scope offsets for %s\n", current->name);

  if(current->num_children == 0)
    offset = 0;
  else
    offset = max_scope_size(current) - current->size;

  for(i = 0; i < current->num_records; i++) {
    current->name_table[i]->offset = offset;
    offset += entry_size(current->name_table[i]->type_info);

  }

  for(i = 0; i < current->num_children; i++)
    set_scope_offset(current->children[i]);
}

void print_scope_list(scope_node_t *scope, int level) {
  int i, j;

  debug_printf(1, "Scope: %s (%d name records, %d children, s = %d)\n",
	       scope->name, scope->num_records, scope->num_children,
	       scope->size);

  for(i =  0; i < scope->num_records; i++) {
    debug_printf(1, "\t%s (%d, %d): ", scope->name_table[i]->name,
		 sizeof_type(scope->name_table[i]->type_info),
		 scope->name_table[i]->offset);

    if(scope->name_table[i]->type_info)
      print_type_signature(scope->name_table[i]->type_info);
    else
      debug_printf(1, "\n");

  }



  for(i = 0; i < scope->num_children; i++)
    print_scope_list(scope->children[i], level + 1);

}

/*
 * DEBUG: Print out the scope lists
 */
void print_scope_lists(void) {
  int i, j, counter;
  scope_node_t *current;

  print_scope_list(global_scope, 0);

}

/*
 * Indented version of printf
 *
 * TODO: Should put this, along with some other functions in a common utils
 * file
 */
static void iprintf(FILE *fd, int indent, char *fmt, ...) {
  int i;
  va_list args;

  va_start(args, fmt);

  for(i = 0; i < indent; i++)
    fprintf(fd, " ");

  vfprintf(fd, fmt, args);

  va_end(args);
}

static void print_xml_scope_children(FILE *fd, int indent,
				     scope_node_t *current) {
  int i;

  if(current->name)
    iprintf(fd, indent, "<func name=\"%s\" size=\"%d\">\n", current->name,
	    current->size);
  else
    iprintf(fd, indent, "<scope size=\"%d\">\n", current->size);

  indent += 2;

  for(i = 0; i < current->num_records; i++)
    iprintf(fd, indent, "<var name=\"%s\" type=\"%s\"/>\n",
	    current->name_table[i]->name,
	    sprint_type_signature(current->name_table[i]->type_info));

  for(i = 0; i < current->num_children; i++)
    print_xml_scope_children(fd, indent, current->children[i]);

  indent -= 2;

  if(current->name)
    iprintf(fd, indent, "</func>\n");
  else
    iprintf(fd, indent, "</scope>\n");


}

void print_xml_scope_list(char *filename) {
  FILE *fd;
  scope_node_t *current = global_scope;
  int i, indent = 0;

  fd = fopen(filename, "w");

  /* XML Header */
  fprintf(fd, "<?xml version=\"1.0\" ?>\n");

  fprintf(fd, "<global size=\"%d\">\n", current->size);
  indent += 2;

  for(i = 0; i < current->num_records; i++) {
    iprintf(fd, indent, "<var name=\"%s\" type=\"%s\"/>\n",
	    current->name_table[i]->name,
	    sprint_type_signature(current->name_table[i]->type_info));
  }

  for(i = 0; i < current->num_children; i++)
    print_xml_scope_children(fd, indent, current->children[i]);


  indent -= 2;
  fprintf(fd, "</global>\n");

  fclose(fd);

}
