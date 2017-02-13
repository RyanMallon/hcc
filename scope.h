/*
 * scope.h
 * Ryan Mallon (2005)
 *
 */
#ifndef _SCOPE_H_
#define _SCOPE_H_

#include "symtable.h"


/* Scope Information */
#ifndef _SCOPE_NODE_T_
#define _SCOPE_NODE_T_
typedef struct scope_node_s {
  char *name;

  int num_records;
  int num_children;

  int size;
  int offset;

  name_record_t **name_table;

  struct scope_node_s *prev;
  struct scope_node_s **children;

} scope_node_t;
#endif /* _SCOPE_NODE_T */

void print_scope_lists(void);

int num_decl_functions(void);
scope_node_t *enter_scope(void);
void exit_scope(void);
scope_node_t *new_func_scope(char *);
int add_scope(scope_node_t *);
scope_node_t *parse_function_scope(char *);
scope_node_t *enter_function_scope(char *);

scope_node_t *get_var_scope(name_record_t *);
scope_node_t *get_func_scope(char *);
scope_node_t *get_func_arg_scope(char *);

int get_scope_depth(void);
int global_index(name_record_t *);

int max_scope_size(scope_node_t *);
void set_scope_offsets(void);
void adjust_scope_offsets(scope_node_t *, int);

void print_xml_scope_list(char *);

#endif /* _SCOPE_H_ */
