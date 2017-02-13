/*
 * symtable.c
 * Ryan Mallon (2005)
 *
 * Symbol Table
 *
 * FIXME: All name records should be added to the scope name tables using the
 * add_entry function instead of being done manually.
 *
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ast.h"
#include "typechk.h"
#include "symtable.h"
#include "scope.h"
#include "utype.h"
#include "cerror.h"
#include "error.h"
#include "icg.h"
#include "cflags.h"

/* Defined functions table */
static name_record_t **def_func;
static int num_def_funcs;

/* Static functions */
static name_record_t *parse_simple_decl(int, int, ast_node_t *);
static void parse_struct_def(ast_node_t *);
static scope_node_t *parse_func_args(ast_node_t *);
static name_record_t *parse_inner_struct(user_type_t *, int, name_record_t *);

static void alloc_array(name_record_t *);
static int sizeof_utype(type_info_t *);

extern compiler_options_t cflags;
extern ast_node_t *ast;
extern scope_node_t *global_scope, *current_scope;

/* Base for setting variable offsets */
static int base;

/*
 * Parse a declaration subtree
 * Returns the storage size for all declarations in the subtree
 *
 * FIXME: Dangling pointers?
 *
 */
scope_node_t *parse_decls(ast_node_t *decl_tree) {
  int i, j, type;
  decl_type_t decl_type;
  ast_node_t *current, *name_node;
  scope_node_t *scope;
  name_record_t *var;

  scope = malloc(sizeof(scope_node_t));
  scope->name = NULL;
  scope->size = 0;
  scope->num_records = 0;
  scope->num_children = 0;

  scope->prev = NULL;
  scope->children = NULL;
  scope->name_table = NULL;

  base = 0;

  scope->name_table = malloc(sizeof(name_record_t *));
  scope->children = malloc(sizeof(scope_node_t *));

  /* Entry point */
  if(current_scope == global_scope) {
    scope->name_table[scope->num_records++] = declare_entry_point();
    scope->children[scope->num_children] = new_func_scope(cflags.entry_point);
    scope->children[scope->num_children++]->prev = scope;
  }

  /* No global declarations */
  if(!decl_tree)
    return scope;

  for(i = 0; i < decl_tree->node.num_children; i++) {
    current = decl_tree->node.children[i];

    switch(current->node.op) {
    case NODE_FUNC:
    case NODE_INLINE_FUNC:

      decl_type = TYPE_FUNCTION;
      type = get_type(current->node.children[0]);

      name_node = current->node.children[0];
      while(name_node->node_type == NODE_OPERATOR)
	name_node = name_node->node.children[0];

      /* Add function name record */
      //add_entry(scope, parse_simple_decl(decl_type, type, current));

      /* Add function name record */
      scope->name_table = realloc(scope->name_table, sizeof(name_record_t *)
                                  * (scope->num_records + 1));
      scope->name_table[scope->num_records] = malloc(sizeof(name_record_t));

      scope->name_table[scope->num_records++] =
        parse_simple_decl(decl_type, type, current);

      /* Mark inline functions */
      if(current->node.op == NODE_INLINE_FUNC) {
	debug_printf(1, "Marking func %s for inlining\n",
		     scope->name_table[scope->num_records - 1]->name);

	scope->name_table[scope->num_records - 1]->func_inline = 1;
      }

      /* Add function scope and arguments */
      if(!scope->children)
	scope->children = malloc(sizeof(scope_node_t *));
      else
	scope->children = realloc(scope->children, sizeof(scope_node_t *) *
				  (scope->num_children + 1));

      scope->children[scope->num_children] = parse_func_args(current);
      scope->children[scope->num_children++]->prev = scope;

      if(current->node.num_children > 1 && current->node.children[1] &&
	 current->node.children[1]->node.op == NODE_VARARGS)
	scope->name_table[scope->num_records - 1]->num_args = FUNC_VAR_ARGS;
      else
	scope->name_table[scope->num_records - 1]->num_args =
	  scope->children[scope->num_children - 1]->num_records;

      break;

    case NODE_STRUCT_DEF:
      parse_struct_def(current);
      break;

    case NODE_STRUCT:
      decl_type = TYPE_USER;
      type = get_type(current);
      add_entry(scope, parse_simple_decl(decl_type, type, current));

      break;

    default:
      /* Standard Variable */

      decl_type = TYPE_VARIABLE;
      type = get_type(current);

      for(j = 0; j < current->node.num_children; j++) {
	var = add_entry(scope, parse_simple_decl(decl_type, type,
						 current->node.children[j]));

	/* Entering a history variable */
	if(is_history_type(var->type_info))
	  history_add_internal_names(var, scope);

      }
      break;

    }
  }
  return scope;
}

/*
 * Parse the arguments for a function and return a new scope
 * containing the arguments in a nametable.
 */
static scope_node_t *parse_func_args(ast_node_t *subtree) {
  ast_node_t *name_node, *current;
  scope_node_t *scope;
  int i, offset;

  offset = IC_STACK_FRAME_HEADER_BYTES;

  name_node = subtree->node.children[0];
  while(name_node->node_type == NODE_OPERATOR)
    name_node = name_node->node.children[0];

  scope = new_func_scope(name_node->leaf.name);

  if(subtree->node.num_children > 1 && subtree->node.children[1] &&
     subtree->node.children[1]->node.op == NODE_VARARGS)
    scope->num_records = 0;
  else
    scope->num_records = subtree->node.num_children - 1;

  if(!scope->num_records) {
    /* Function has no arguments */
    scope->size = IC_STACK_FRAME_HEADER_BYTES;
    return scope;
  }

  scope->name_table = malloc(sizeof(name_record_t *) * scope->num_records);

  for(i = scope->num_records; i > 0; i--) {
    scope->name_table[i - 1] = malloc(sizeof(name_record_t));

    name_node = subtree->node.children[i];
    while(name_node->node_type == NODE_OPERATOR)
      name_node = name_node->node.children[0];
    current = subtree->node.children[i];

    scope->name_table[i - 1] = enter(name_node->leaf.name);
    scope->name_table[i - 1]->type_info =
      create_type_signature(TYPE_ARGUMENT, get_type(current),
			    current->node.children[0]);

    scope->name_table[i - 1]->offset = offset;
    offset += entry_size(scope->name_table[i - 1]->type_info);

    debug_printf(1, "Entered arg(%s): ", name_node->leaf.name);
    print_type_signature(scope->name_table[i - 1]->type_info);

  }
  scope->size = offset;

  return scope;

}


/*
 * Parse a structure definition. This creates a user_type_t for the
 * struct. Instances of a structure defintion will create entries in the
 * nametable.
 */
static void parse_struct_def(ast_node_t *subtree) {
  ast_node_t *current;
  user_type_t *entry;
  int type, i, base = 0;
  char *name;

  /* Get the name */
  name = get_name_from_node(subtree);

  entry = add_utype(name, subtree->node.num_children - 1);
  debug_printf(1, "Struct definition: name = %s\n", entry->name);

  /* Parse Members */
  for(i = 0; i < subtree->node.num_children - 1; i++) {
    current = subtree->node.children[i + 1];

    if((type = get_type(current)) == -1)
      compiler_error(CERROR_ERROR, subtree->src_line_num,
		     "Undefined type %s in structure definition\n",
		     current->node.children[0]->leaf.name);

    /* Get member name */
    if(current->node_type == NODE_OPERATOR &&
       current->node.op == NODE_STRUCT)
      name = get_name_from_node(current->node.children[1]);
    else
      name = get_name_from_node(current);

    /* Copy the name */
    entry->members[i].name = malloc(strlen(name) + 1);
    strcpy(entry->members[i].name, name);

    /* Copy the type */
    if((type & TYPE_MASK) == TYPE_UTYPE)
      entry->members[i].type_info =
	create_type_signature(TYPE_MEMBER, type, current->node.children[1]);
    else
      entry->members[i].type_info =
	create_type_signature(TYPE_MEMBER, type, current->node.children[0]);

    entry->members[i].offset = base;
    base += entry_size(entry->members[i].type_info);

    debug_printf(1, "  Adding member %s to struct def %s at offset %d. Type: ",
		 entry->members[i].name, entry->name, entry->members[i].offset);
    print_type_signature(entry->members[i].type_info);

  }
}

/*
 * Parse a nested structure
 */
static name_record_t *parse_inner_struct(user_type_t *utype, int member_num,
					 name_record_t *parent) {
  name_record_t *entry;
  int i;

  debug_printf(1, "Creating an inner struct of user type %s called %s, type = ",
	       utype->name, utype->members[member_num].name);

  entry = enter(utype->members[member_num].name);

  /* Type sign */
  entry->type_info = malloc(sizeof(type_info_t));
  copy_sig(utype->members[member_num].type_info, entry->type_info);
  print_type_signature(entry->type_info);

  /* Struct offsets are from the bottom */
  entry->offset = base;
  base += entry_size(entry->type_info);

  /* Add the members */
  utype = get_utype(primitive_type(entry->type_info));
  entry->members = malloc(sizeof(name_record_t *) * utype->num_members);
  for(i = 0; i < utype->num_members; i++) {
    entry->members[i] = enter(utype->members[i].name);

    entry->members[i]->struct_parent = parent;
    entry->members[i]->offset = utype->members[i].offset;

    /* Copy type signature from struct def */
    entry->members[i]->type_info = malloc(sizeof(type_info_t));
    copy_sig(utype->members[i].type_info, entry->members[i]->type_info);

    debug_printf(1, "  Added inner member %s, offset = %d, type = ",
		 entry->members[i]->name, entry->members[i]->offset);
    print_type_signature(entry->members[i]->type_info);
  }

  return entry;
}

/*
 * Parse a simple variable or function declaration
 */
static name_record_t *parse_simple_decl(int decl_type, int type,
					ast_node_t *subtree) {
  ast_node_t *current, *name_node = subtree;
  name_record_t *entry, *arg;
  user_type_t *utype;
  int i, size = 0;

  while(name_node->node_type == NODE_OPERATOR)
    name_node = name_node->node.children[0];

  switch(decl_type) {
  case TYPE_USER:

    if(!(utype = get_utype(type)))
      compiler_error(CERROR_ERROR, subtree->src_line_num,
		     "Type %s undefined\n",
		     get_name_from_node(subtree));


    entry = enter(get_name_from_node(subtree->node.children[1]));
    debug_printf(1, "Creating an instance of user type %s called %s, type = ",
		 utype->name, entry->name);

    /* Base type in signature is index in utype table */
    entry->type_info = create_type_signature(decl_type, type,
					     subtree->node.children[1]);
    print_type_signature(entry->type_info);

    /* Struct offsets are from the bottom */
    entry->offset = base;
    base += entry_size(entry->type_info);

    /* Add the members */
    entry->members = malloc(sizeof(name_record_t *) * utype->num_members);
    debug_printf(1, "Adding %d members to outer struct\n", utype->num_members);
    for(i = 0; i < utype->num_members; i++) {

      if(is_struct_type(utype->members[i].type_info))
	entry->members[i] = parse_inner_struct(utype, i, entry);
      else {
	entry->members[i] = enter(utype->members[i].name);

	/* Copy type signature from struct def */
	entry->members[i]->type_info = malloc(sizeof(type_info_t));
	copy_sig(utype->members[i].type_info, entry->members[i]->type_info);
      }

      entry->members[i]->struct_parent = entry;
      entry->members[i]->offset = utype->members[i].offset;
      entry->members[i]->may_alias = 1;

      debug_printf(1, "  Added member %s, offset = %d, type = ",
		   entry->members[i]->name, entry->members[i]->offset);
      print_type_signature(entry->members[i]->type_info);
    }


    break;

  case TYPE_VARIABLE:
    entry = enter(name_node->leaf.name);

    entry->type_info = create_type_signature(decl_type, type, subtree);

    /* Check that the type is complete */
    if(is_primitive(entry->type_info) &&
       primitive_type(entry->type_info) == TYPE_VOID)
      compiler_error(CERROR_ERROR, subtree->src_line_num,
		     "Variable %s has an incomplete type\n", entry->name);

    /*
     * All entries other than primitives chars must be aligned
     * on IC_POINTER_SIZE-byte boundaries.
     */
    if(!(is_primitive(entry->type_info) &&
	 primitive_type(entry->type_info) == TYPE_CHAR)) {

      if((base % IC_SIZEOF_POINTER) != 0)
	base += IC_SIZEOF_POINTER - (base % IC_SIZEOF_POINTER);
    }


    entry->offset = base;
    base += entry_size(entry->type_info);

    debug_printf(1, "Created var(%s, %d): type_sig = ",
		 entry->name, entry->offset);
    print_type_signature(entry->type_info);

    break;

  case TYPE_FUNCTION:
    entry = enter(name_node->leaf.name);

    debug_printf(1, "Subtree has %d children\n", subtree->node.num_children);

    /* Initial location of function */
    entry->ic_offset = NULL;

    current = subtree->node.children[0];
    type = get_type(current);

    entry->type_info = create_type_signature(decl_type, type,
					     current->node.children[0]);

    debug_printf(1, "Entering function %s, type sig = ", name_node->leaf.name);
    print_type_signature(entry->type_info);

    break;

  default:
    compiler_error(CERROR_ERROR, subtree->src_line_num,
		   "Unknown type in declaration\n");
    break;
  }

  return entry;

}

/*
 * Create a name record for a variable
 *
 * TODO: Check for duplicate definitions
 *
 */
name_record_t *enter(char *name) {
  int i;

  name_record_t *entry;

  entry = malloc(sizeof(name_record_t));
  entry->name = malloc(strlen(name) + 1);
  strcpy(entry->name, name);

  entry->num_args = 0;
  entry->type_info = NULL;

  entry->members = NULL;

  entry->reg = -1;
  entry->reg_alloc = -1;
  entry->storage_level = -1;
  entry->func_inline = 0;

  /* Mark all variable as may-aliases if debugging is enabled */
  if(cflags.flags & CFLAG_OUTPUT_STABS)
    entry->may_alias = 1;
  else
    entry->may_alias = 0;

  return entry;
}

/*
 * Add a name record to a scopes name table
 */
name_record_t *add_entry(scope_node_t *scope, name_record_t *var) {
  scope_node_t *func_scope;

  scope->name_table = realloc(scope->name_table, sizeof(name_record_t *)
			      * (scope->num_records + 1));
  scope->name_table[scope->num_records] = var;

  if(scope->name_table[scope->num_records]->type_info->decl_type
     == TYPE_FUNCTION) {

    scope->children = realloc(scope->children, sizeof(scope_node_t *) *
			      (scope->num_children + 1));

    scope->children[scope->num_children] = new_func_scope(var->name);
    scope->children[scope->num_children++]->prev = scope;

    debug_printf(1, "Creating func scope, name = %s\n", var->name);

  } else {
    scope->size +=
      entry_size(scope->name_table[scope->num_records]->type_info);
  }

  return scope->name_table[scope->num_records++];
}

/*
 * Return the size of an entry
 */
int entry_size(type_info_t *entry) {
  return sizeof_type(entry) + sizeof_history(entry);
}

/*
 * Return the size of the primitives
 *
 * TODO: Calculate the sizeof structs
 */
int sizeof_primitive(int primitive) {
  switch(primitive) {
  case TYPE_VOID:
    return -1;
    break;
  case TYPE_INTEGER:
    return IC_SIZEOF_INT;
    break;
  case TYPE_CHAR:
    return IC_SIZEOF_CHAR;
    break;
  case TYPE_STRING:
    return IC_SIZEOF_POINTER;
    break;
  default:
    compiler_error(CERROR_WARN, CERROR_NO_LINE,
		   "Unknown primitive type passed to "
		   "sizeof_primitive. Default return IC_SIZEOF_INT\n");
    return IC_SIZEOF_INT;

  }
}

/*
 * Return the size of a user define type
 */
static int sizeof_utype(type_info_t *type) {
  user_type_t *utype = get_utype(type->signature[type->length - 1]);
  int i, size;

  size = 0;
  for(i = 0; i < utype->num_members; i++)
    size += entry_size(utype->members[i].type_info);

  return size;
}

/*
 * Return the size of an array
 */
static int sizeof_array(type_info_t *type) {
  int i, size, element_size;

  element_size = sizeof_primitive(type->signature[type->length - 1]);

  size = element_size;
  /* Array indicies are offset from 1 */
  for(i = 1; i <= get_num_dimensions(type); i++)
    size *= get_dimension_size(type, i);

#if 0
  if(get_num_dimensions(type) == 1) {
    size = (get_dimension_size(type, 1) * element_size);

    return size;
  }

  if(get_num_dimensions(type) == 2) {
    size = (get_dimension_size(type, 1) * get_dimension_size(type, 2)) *
      element_size;

    return size;
  }
#endif

  return size;

}

/*
 * Return the size of the history data associated with a type
 *
 * Does not include the size of the pointers/changelists/etc.
 *
 */
int sizeof_history(type_info_t *type) {
  int size, history_size;

  if(!is_history_type(type))
    return 0;

  size = sizeof_type(type);

  if(is_array_type(type)) {
    /* Array */
    history_size = ((get_num_elements(type) *
		     sizeof_primitive(primitive_type(type))) *
		    (get_history_depth(type) - 1));

  } else {
    /* Simple variable or member */
    history_size = get_history_depth(type) * size;
  }

  return history_size;
}

/*
 * Return the size of a type
 */
int sizeof_type(type_info_t *type) {
  int i, size = IC_SIZEOF_INT;

  if(is_primitive(type) && !is_struct_type(type))
    return sizeof_primitive(primitive_type(type));

  if(is_array_type(type))
    return sizeof_array(type);

  if(type->decl_type == TYPE_FUNCTION)
    return 0;

  if(type->signature[0] == TYPE_POINTER)
    return IC_SIZEOF_POINTER;

  if(is_struct_type(type)) {
    size = sizeof_utype(type);
  }

  return size;
}

/*
 * Return the size of the given dimension of an array
 */
int array_size(type_info_t *sig, int dim) {
  return (sig->signature[dim] >> 16);
}

void exit_function(name_record_t *func) {
  debug_printf(1, "exit_function: ");
  exit_scope();
}

/*
 * Return the name record for the given variable
 *
 * Search backwards from the current scope
 *
 */
name_record_t *get_var_entry(char *var_name) {
  int i;
  scope_node_t *current = current_scope;

  if(!var_name)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Attempted lookup of invalid variable name\n");

  current = current_scope;

  while(current != NULL) {
    for(i = 0; i < current->num_records; i++)
      if(!strcmp(var_name, current->name_table[i]->name))
	return current->name_table[i];

    current = current->prev;

  }
  return NULL;
}

/*
 * Return the entry for the specified function argument
 */
name_record_t *get_arg_entry(name_record_t *func, char *arg_name) {
  scope_node_t *arg_scope;
  int i;

  arg_scope = get_func_arg_scope(func->name);

  for(i = 0; i < arg_scope->num_records; i++) {
    if(!strcmp(arg_scope->name_table[i]->name, arg_name))
      return arg_scope->name_table[i];
  }

  return NULL;
}

/*
 * Build the table of defined functions
 */
void build_def_func_table(void) {
  int i;
  name_record_t *func;

  /* Count how many defined functions there are */
  num_def_funcs = 0;
  for(i = 0; i < ast->node.num_children; i++)
    if(ast->node.children[i]->node.op == NODE_FUNC ||
       ast->node.children[i]->node.op == NODE_INLINE_FUNC)
      num_def_funcs++;

  if(num_def_funcs == 0)
    return;

  /* Build the table */
  def_func = malloc(num_def_funcs * sizeof(name_record_t *));
  for(i = 0; i < num_def_funcs; i++) {
    func = ast_def_func(i);
    def_func[i] = func;
  }
}

/*
 * Free the defined function table
 */
void free_def_func_table(void) {
  free(def_func);
  num_def_funcs = 0;
}

/*
 * Return the number of defined functions
 */
int num_def_functions(void) {
  return num_def_funcs;
}

/*
 * Return the given defined function
 */
name_record_t *get_def_func(int index) {
  return def_func[index];
}

/*
 * Return the given defined function
 */
int get_def_func_index(char *func_name) {
  int i;

  for(i = 0; i < num_def_funcs; i++)
    if(!strcmp(def_func[i]->name, func_name))
      return i;

  return -1;
}

name_record_t *get_func_entry(char *var_name) {
  int i;

#if 0
  if(!var_name)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Attemped lookup of null function\n");
#endif


  for(i = 0; i < global_scope->num_records; i++)
    if(!strcmp(var_name, global_scope->name_table[i]->name))
      if(global_scope->name_table[i]->type_info->decl_type == TYPE_FUNCTION)
	return global_scope->name_table[i];

  return NULL;
}

int get_index_size(char *var_name, int index) {
  name_record_t *record;

  record = get_var_entry(var_name);
  if(!record)
    return -1;

  if((record->type_info->signature[index] & TYPE_MASK) != TYPE_ARRAY)
    return -1;

  return (record->type_info->signature[index] >> 16);

}

/*
 * Insert a declaration for the entry point
 *
 * Currently defined as: func int entry_point();
 *
 */
name_record_t *declare_entry_point(void) {
  name_record_t *entry;

  entry = enter(cflags.entry_point);

  entry->num_args = 0;
  entry->offset = 0;
  entry->func_inline = 0;
  entry->type_info = malloc(sizeof(type_info_t));
  entry->type_info->signature = malloc(sizeof(unsigned int));
  entry->type_info->length = 1;
  entry->type_info->decl_type = TYPE_FUNCTION;
  entry->type_info->signature[0] = TYPE_INTEGER;

  debug_printf(1, "Creating entry point (%s) record. Type sig = ",
	       cflags.entry_point);
  print_type_signature(entry->type_info);

  return entry;
}

/*
 * Create a built-in function
 */
name_record_t *declare_builtin_func(char *name, type_info_t *type_info) {
  name_record_t *entry;

  entry = enter(name);

  entry->num_args = 0;
  entry->offset = 0;
  entry->func_inline = 0;
  entry->type_info = type_info;

  debug_printf(1, "Added builtin function %s, type = ", entry->name);
  print_type_signature(entry->type_info);

  add_entry(global_scope, entry);

  return entry;
}

/*
 * Add an argument to a function
 */
name_record_t *add_func_arg(scope_node_t *func_scope, char *name,
			    type_info_t *type_info) {
  name_record_t *entry;

  entry = enter(name);
  entry->type_info = type_info;
  add_entry(func_scope, entry);

  get_func_entry(func_scope->name)->num_args++;

  return entry;
}



/*
 * Find the name node, at the bottom left of the current subtree
 */
ast_node_t *get_name_node(ast_node_t *current) {
  while(current->node_type == NODE_OPERATOR)
    current = current->node.children[0];

  if(current->node_type != NODE_LEAF_ID)
    compiler_error(CERROR_ERROR, current->src_line_num,
		   "Cannot find name for %s node\n",
		   ast_node_name[current->node.op]);

  return current;
}

char *get_name_from_node(ast_node_t *current) {
  ast_node_t *name_node = get_name_node(current);
  return name_node->leaf.name;
}
