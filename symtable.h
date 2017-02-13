/*
 * symtable.h
 *
 * Ryan Mallon (2005)
 *
 * Symbol Table
 *
 */
#ifndef _SYMTABLE_H_
#define _SYMTABLE_H_

#include "ast.h"
#include "icg.h"

typedef enum {
  TYPE_NONE,
  TYPE_VARIABLE,
  TYPE_FUNCTION,
  TYPE_ARGUMENT,
  TYPE_CONSTANT,
  TYPE_STRING_LITERAL,
  TYPE_USER,
  TYPE_MEMBER,
  TYPE_LVALUE,
  TYPE_RVALUE
} decl_type_t;

typedef enum {
  TYPE_VOID,
  TYPE_INTEGER,
  TYPE_CHAR,
  TYPE_STRING,
  TYPE_POINTER,
  TYPE_ARRAY,
  TYPE_ADDRESS,
  TYPE_UTYPE,

  TYPE_HISTORY
} type_enum;

enum {
  HISTORY_NONE,
  HISTORY_ARRAY_WISE,
  HISTORY_INDEX_WISE,
};

/* Type information */
#ifndef _TYPE_INFO_T_
#define _TYPE_INFO_T_
typedef struct {
  int decl_type;
  int length;
  unsigned int *signature;

  int indirect_levels;
  int num_indicies;
  int *indicies;
} type_info_t;
#endif /* TYPE_INFO_T_ */

/* Name Record */
struct mir_node_s;
typedef struct name_table_s {
  char *name;

  ic_list_t *ic_offset;
  unsigned int offset;

  /* Struct members */
  struct name_table_s **members;
  struct name_table_s *struct_parent;

  /* Function arguments */
  int num_args;

  /* Type Information */
  type_info_t *type_info;

  int reg;
  int reg_alloc;
  int storage_level;

  /* Variables marked as may_alias are not assigned to registers */
  int may_alias;

  /* For functions */
  int func_inline;
  struct mir_node_s *mir_first;
  struct mir_node_s *mir_last;

  int type;
} name_record_t;

#define FUNC_VAR_ARGS -1

#include "scope.h"
scope_node_t *parse_decls(ast_node_t *);

void exit_function(name_record_t *);

name_record_t *enter(char *);
name_record_t *add_entry(scope_node_t *, name_record_t *);
name_record_t *get_var_entry(char *);
name_record_t *get_func_entry(char *);
name_record_t *get_arg_entry(name_record_t *, char *);

int sizeof_type(type_info_t *);
int sizeof_primitive(int);
int sizeof_history(type_info_t *);
int array_size(type_info_t *, int);

void build_def_func_table(void);
void free_def_func_table(void);

name_record_t *declare_entry_point(void);
name_record_t *declare_builtin_func(char *, type_info_t *);
name_record_t *add_func_arg(scope_node_t *, char *, type_info_t *);

ast_node_t *get_name_node(ast_node_t *);
char *get_name_from_node(ast_node_t *);

int entry_size(type_info_t *);

int num_def_functions(void);
name_record_t *get_def_func(int);
int get_def_func_index(char *);

#endif /* _SYMTABLE_H_ */
