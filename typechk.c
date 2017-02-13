/*
 * Type Checker
 *
 * Ryan Mallon (2005)
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ast.h"
#include "icg.h"
#include "ast.h"
#include "symtable.h"
#include "typechk.h"
#include "utype.h"
#include "debug.h"
#include "cerror.h"
#include "cflags.h"

extern scope_node_t *global_scope, *current_scope;

static type_check_t *type_check_add(ast_node_t *);
static type_check_t *type_check_dereference(ast_node_t *);
static type_check_t *type_check_array(ast_node_t *);
static type_check_t *type_check_cast(ast_node_t *);
static type_check_t *type_check_assign_address(ast_node_t *);
static type_check_t *type_check_return(ast_node_t *);

static void munge_history(ast_node_t **);

/* Cflags */
extern compiler_options_t cflags;

/* Current struct name */
static name_record_t *current_struct = NULL;

/*
 * Type check an addition node
 */
type_check_t *type_check_add(ast_node_t *node) {
  type_check_t *e1, *e2, *check;

  debug_printf(1, "Typechecking add\n");

  check = malloc(sizeof(type_check_t));
  check->type_info = malloc(sizeof(type_info_t));

  e1 = type_check_node(node->node.children[0]);
  e2 = type_check_node(node->node.children[1]);

  /* Left hand side is a pointer or array type */
  if(is_array_type(e1->type_info) || is_address_type(e1->type_info)) {
    if(is_array_type(e2->type_info) ||
       is_address_type(e2->type_info) ||
       e2->type_info->length != 1 ||
       e2->type_info->signature[0] != TYPE_INTEGER)

      compiler_error(CERROR_ERROR, node->src_line_num,
		     "Invalid operands to binary +\n");
    else {
      copy_sig(e1->type_info, check->type_info);
      if(is_array_type(e1->type_info)) {
	remove_type_from_sig(&(check->type_info), TYPE_ARRAY);
	add_to_type_sig(&(check->type_info), TYPE_ADDRESS);
      }
      check->type_info->decl_type = TYPE_LVALUE;
      check->passed = 1;

      return check;
    }
  }

  /* Right hand side is a pointer */
  if(is_array_type(e2->type_info) || is_address_type(e2->type_info)) {
    if(is_array_type(e1->type_info) ||
       is_address_type(e1->type_info) ||
       e1->type_info->length != 1 ||
       e1->type_info->signature[0] != TYPE_INTEGER)

      compiler_error(CERROR_ERROR, node->src_line_num,
		     "Invalid operands to binary +\n");
    else {
      copy_sig(e2->type_info, check->type_info);
      if(is_array_type(e2->type_info)) {
	remove_type_from_sig(&(check->type_info), TYPE_ARRAY);
	add_to_type_sig(&(check->type_info), TYPE_ADDRESS);
      }
      check->type_info->decl_type = TYPE_LVALUE;
      check->passed = 1;

      return check;
    }
  }

  check->type_info = result_type(e1->type_info, e2->type_info, NODE_ADD);
  check->passed = 1;

  return check;
}

/*
 * Type check an assignment
 *
 * The left hand side of the assignment must be a valid lvalue
 *
 */
static type_check_t *type_check_assign(ast_node_t *node) {

  type_check_t *lhs, *rhs, *check;

  check = malloc(sizeof(type_check_t));
  check->type_info = malloc(sizeof(type_info_t));

  lhs = type_check_node(node->node.children[0]);
  rhs = type_check_node(node->node.children[1]);

  debug_printf(1, "Type check assign: lhs: ");
  print_type_signature(lhs->type_info);

#if 0
  if(lhs->type_info->decl_type != TYPE_LVALUE)
    compiler_error(CERROR_ERROR, node->src_line_num,
		   "Invalid lvalue in assignment\n");
#endif

 check->type_info = result_type(lhs->type_info, rhs->type_info, NODE_BECOMES);
 check->passed = 1;
 return check;
}


/*
 * Type check a dereference
 */
static type_check_t *type_check_dereference(ast_node_t *node) {
  type_check_t *expr, *check;

  check = malloc(sizeof(type_check_t));
  check->type_info = malloc(sizeof(type_info_t));

  /* Type check the child and copy its signature */
  expr = type_check_node(node->node.children[0]);
  copy_sig(expr->type_info, check->type_info);

  debug_printf(1, "Type checking dereference for: ");
  print_type_signature(expr->type_info);

  if(is_address_type(expr->type_info) || is_array_type(expr->type_info)) {
    check->type_info = remove_type_from_sig(&(check->type_info), TYPE_ADDRESS);
    check->type_info->indirect_levels++;
  } else
    compiler_error(CERROR_ERROR, node->src_line_num,
		   "Invalid operand to unary *\n");

  check->passed = 1;
  return check;
}

/*
 * Type check the assign address operation
 */
static type_check_t *type_check_assign_address(ast_node_t *node) {
  type_check_t *expr, *check;

  check = malloc(sizeof(type_check_t));
  check->type_info = malloc(sizeof(type_info_t));

  expr = type_check_node(node->node.children[0]);
  copy_sig(expr->type_info, check->type_info);

  if(expr->type_info->decl_type != TYPE_LVALUE &&
     expr->type_info->decl_type != TYPE_VARIABLE &&
     expr->type_info->decl_type != TYPE_ARGUMENT &&
     expr->type_info->decl_type != TYPE_USER)
    compiler_error(CERROR_ERROR, node->src_line_num,
		   "Invalid lvalue in assignment (tcaa) \n");

  add_to_type_sig(&(check->type_info), TYPE_ADDRESS);
  check->type_info->decl_type = TYPE_LVALUE;

  check->passed = 1;
  return check;
}


/*
 * Type check an address of operation
 */
static type_check_t *type_check_address_of(ast_node_t *node) {
  type_check_t *expr;

  expr = type_check_node(node->node.children[0]);

  if(expr->type_info->decl_type == TYPE_CONSTANT ||
     expr->type_info->decl_type == TYPE_RVALUE ||
     expr->type_info->indirect_levels == -1) {

    compiler_error(CERROR_ERROR, node->src_line_num,
		   "Invalid operand to unary &\n");

  }  else {
    debug_printf(1, "Unary &: ");
    print_type_signature(expr->type_info);
  }

  expr->type_info->indirect_levels--;
  expr->type_info->decl_type = TYPE_LVALUE;
  add_to_type_sig(&(expr->type_info), TYPE_ADDRESS);


  return expr;
}

/*
 * Type check an array
 */
static type_check_t *type_check_array(ast_node_t *node) {
  type_check_t *expr, *index, *check;
  int i;

  check = malloc(sizeof(type_check_t));
  check->type_info = malloc(sizeof(type_info_t));

  expr = type_check_node(node->node.children[0]);

  if(!is_array_type(expr->type_info) && !is_address_type(expr->type_info))
    compiler_error(CERROR_ERROR, node->src_line_num,
		   "subscripted value is neither array nor pointer\n");

  for(i = 1; i < node->node.num_children; i++) {
    index = type_check_node(node->node.children[i]);

    if(index->type_info->length != 1 ||
       index->type_info->signature[0] != TYPE_INTEGER)
      compiler_error(CERROR_ERROR, node->src_line_num,
		     "subscript is not an integer\n");

    /* Set the type for the array node */
    copy_sig(expr->type_info, check->type_info);
    check->type_info = remove_type_from_sig(&(check->type_info), TYPE_ARRAY);
  }


  check->passed = 1;
  return check;

}

/*
 * Type check a member usage
 *
 * FIXME: This function is a freakin' mess. Having the global
 * current_struct variable is a bit ugly.
 *
 */
static type_check_t *type_check_member(ast_node_t *node) {
  name_record_t *var, *member;
  type_check_t *check = NULL;

  /* Get the struct variable */
  var = get_var_entry(get_name_from_node(node));
  if(!var)
    compiler_error(CERROR_ERROR, CERROR_UNKNOWN_LINE,
		   "Struct %s undeclared\n", get_name_from_node(node));

  /* Type check the left hand side */
  debug_printf(1, "LEFT\n");
  type_check_node(node->node.children[0]);

  /* Set the current struct */
  if(!current_struct) {
    current_struct = var;
    debug_printf(1, "Setting current struct to %s\n", current_struct->name);
  }

  /* Type check the right hand side */
  type_check_node(node->node.children[1]);

  /* Get the member variable */
  member = get_utype_member(current_struct,
			    get_name_from_node(node->node.children[1]));
  if(!member)
    compiler_error(CERROR_ERROR, CERROR_UNKNOWN_LINE,
		   "Struct %s has no member called %s\n",
		   current_struct->name,
		   get_name_from_node(node->node.children[1]));

  /* Check if the member is also a struct */
  if(is_struct_type(member->type_info)) {
    char *name = get_name_from_node(node->node.children[1]);
    current_struct = get_utype_member(var, name);
    debug_printf(1, "Reset current struct to %s on RHS\n",
		 current_struct->name);
  } else
    current_struct = NULL;

  check = malloc(sizeof(type_check_t));
  check->type_info = malloc(sizeof(type_info_t));

  check->passed = 1;
  check->type_info = copy_sig(member->type_info, check->type_info);
  check->type_info->decl_type = TYPE_LVALUE;

  return check;

}

/*
 * Type check a cast.
 *
 * FIXME: This just assumes that all type casts are valid
 *
 */
type_check_t *type_check_cast(ast_node_t *node) {
  type_info_t *cast_type;
  type_check_t *check;
  ast_node_t *cast_node;

  check = malloc(sizeof(type_check_t));
  check->type_info = malloc(sizeof(type_info_t));

  /* Type check the uncast expression */
  // original_type = type_check_node(node->node.children[1]);

  cast_node = node->node.children[0]->node.children[0];
  cast_type = create_type_signature(TYPE_LVALUE,
				    get_type(node->node.children[0]),
				    cast_node);


  debug_printf(1, "Cast: New type = ");
  print_type_signature(cast_type);

  check->type_info = copy_sig(cast_type, check->type_info);
  check->passed = 1;

  return check;
}

/*
 * Type check a return statement
 */
type_check_t *type_check_return(ast_node_t *node) {
  type_check_t *check;
  name_record_t *func;

  if(node->node.num_children == 0) {
    check = malloc(sizeof(type_check_t));
    check->type_info = malloc(sizeof(type_info_t));
    check->passed = 1;
    check->type_info = create_type_signature(TYPE_RVALUE, TYPE_VOID, NULL);

    return check;
  }

  /* Check for a return value from a void function */
  func = get_func_entry(current_scope->name);
  if(is_primitive(func->type_info) &&
     primitive_type(func->type_info) == TYPE_VOID)
    compiler_error(CERROR_ERROR, node->src_line_num,
		   "Attempt to return a value from void function %s\n",
		   func->name);

  check = type_check_node(node->node.children[0]);
  check->passed = 1;

  return check;

}

type_check_t *type_check_node(ast_node_t *node) {
  int i;
  name_record_t *var;
  type_check_t *check;
  ast_node_t *name_node;
  int entered_scope;

  entered_scope = 0;

  check = malloc(sizeof(type_check_t));
  check->type_info = malloc(sizeof(type_info_t));
  check->passed = 0;

  switch(node->node_type) {
  case NODE_LEAF_NUM:
    check->passed = 1;

    check->type_info->length = 1;
    check->type_info->decl_type = TYPE_CONSTANT;
    check->type_info->signature = malloc(sizeof(int));
    check->type_info->signature[0] = TYPE_INTEGER;

    debug_printf(1, "Type checked constant int, value = %d\n", node->leaf.val);

    break;

  case NODE_LEAF_ID:
    if(current_struct) {
      var = get_utype_member(current_struct, node->leaf.name);
      if(!var)
	compiler_error(CERROR_ERROR, node->src_line_num,
		       "User defined type %s has no member called %s\n",
		       current_struct->name, node->leaf.name);
    } else {
      var = get_var_entry(node->leaf.name);
      if(!var)
	compiler_error(CERROR_ERROR, node->src_line_num,
		       "Variable %s undeclared\n", node->leaf.name);
    }

    check->passed = 1;
    check->type_info = type_sign_var(var, check->type_info);

    /* Address types can be changed to lvalues immediately */
    if(is_address_type(check->type_info))
      check->type_info->decl_type = TYPE_LVALUE;

#if 0
    if(check->type_info->decl_type == TYPE_USER) {
      debug_printf(1, "Marking utype %s\n", node->leaf.name);
      if(struct_name)
	free(struct_name);
      struct_name = malloc(strlen(node->leaf.name) + 1);
      strcpy(struct_name, node->leaf.name);

    }
#endif

    /*
     * History Variable Check
     *
     * If a variable has type history, its parent node in the AST is
     * not a history node and it is on the right hand side of an assignment
     * expression then the tree is altered to have a history zero node.
     */
    if(is_history_type(check->type_info) &&
       node->parent->node.op != NODE_HISTORY &&
       node->parent->node.op != NODE_ADDRESS &&
       (child_number(node, NODE_FUNC_CALL) != -1 ||
	child_number(node, NODE_BECOMES) == 1)) {

      munge_history(&node);

      /* Redo the type checking from the new history node */
      check = type_check_node(node->parent);
    }

    debug_printf(1, "Type checked variable %s, sig = ", node->leaf.name);
    print_type_signature(check->type_info);

    break;


  case NODE_LEAF_CHAR:
    check->passed = 1;

    check->type_info->length = 1;
    check->type_info->decl_type = TYPE_CONSTANT;
    check->type_info->signature = malloc(sizeof(int));
    check->type_info->signature[0] = TYPE_CHAR;

    debug_printf(1, "Type checked constant char literal, value = '%c'\n",
		 (char)node->leaf.val);
    break;

  case NODE_LEAF_STRING:
    check->passed = 1;

    check->type_info->length = 1;
    check->type_info->decl_type = TYPE_STRING_LITERAL;
    check->type_info->signature = malloc(sizeof(int));
    check->type_info->signature[0] = add_string_literal(node->leaf.name);

    debug_printf(1, "Type checked constant string literal, value = \"%s\"\n",
		 node->leaf.name);
    break;

  case NODE_OPERATOR:
    switch(node->node.op) {
    case NODE_DECL:
      check->passed = 1;
      check->type_info->length = 1;
      check->type_info->signature = malloc(sizeof(int));
      check->type_info->signature[0] = TYPE_NONE;

      return check;

    case NODE_CAST:
      check = type_check_cast(node);
      break;

    case NODE_FUNC:
      name_node = get_name_node(node);
      debug_printf(1, "Entering scope for function %s, prev = %p\n",
		   name_node->leaf.name, current_scope);

      if(current_scope != global_scope)
	compiler_error(CERROR_ERROR, node->src_line_num,
		       "AST is borken: Tried to enter a function body from"
		       " non-global scope\n");

      /* TODO: This gets done lots. Turn it into a function? */
      for(i = 0; i < current_scope->num_children; i++)
	if(!strcmp(current_scope->children[i]->name, name_node->leaf.name))
	  current_scope = current_scope->children[i];

      /* FIXME: This shouldn't be borken */
      current_scope->prev = global_scope;


      if(current_scope == global_scope)
	compiler_error(CERROR_ERROR, node->src_line_num,
		       "Function %s undeclared\n", name_node->leaf.name);

      for(i = 1; i < node->node.num_children; i++)
	type_check_node(node->node.children[i]);

      current_scope = global_scope;

      break;

    case NODE_BLOCK:


      if(node->node.scope_tag != -1) {
	current_scope = current_scope->children[node->node.scope_tag];
	entered_scope = 1;
      }

      for(i = 0; i < node->node.num_children; i++)
	type_check_node(node->node.children[i]);

      if(entered_scope)
	current_scope = current_scope->prev;

      break;

    case NODE_HISTORY:
      check = type_check_node(node->node.children[0]);
      remove_type_from_sig(&check->type_info, TYPE_HISTORY);
      break;

    case NODE_FUNC_CALL: {
      char *func_name;
      scope_node_t *func_scope;
      type_info_t *arg_type, *expected_type;

      /* Get the function info */
      func_name = get_name_from_node(node);
      func_scope = get_func_arg_scope(func_name);

      /* Don't typecheck varidic functions */
      if(get_func_entry(func_name) &&
         get_func_entry(func_name)->num_args == FUNC_VAR_ARGS) {
	debug_printf(1, "Typechecking varidic function %s\n", func_name);

        for(i = 1; i < node->node.num_children; i++)
          type_check_node(node->node.children[i]);


	check = type_check_node(node->node.children[0]);
	break;
      }

      for(i = 0; i < node->node.num_children; i++)
	type_check_node(node->node.children[i]);

      /* Check that the function arguments match its prototype */
      for(i = 1; i < node->node.num_children; i++) {
	arg_type = node->node.children[i]->type_check->type_info;
	expected_type = func_scope->name_table[i - 1]->type_info;

	/* Temp-copy method */
	if(cflags.history_arg_setting == HISTORY_ARG_TEMP_COPY) {
	  if(is_history_type(arg_type) && is_address_type(expected_type)) {
	    remove_type_from_sig(&arg_type, TYPE_HISTORY);
	    debug_printf(1, "Converted by-ref history argument\n");
	  }
	}

	/*
	 * A variable of type: history->object can be auto-coerced into
	 * type object when passing to a function. This is not so straight
	 * forward if the type is pointer->history->object
	 */
#if 0
	if(is_history_type(arg_type) && !is_address_type(arg_type))
	  remove_type_from_sig(&arg_type, TYPE_HISTORY);
#endif


	if(!compare_type_sigs(arg_type, expected_type)) {

	  debug_printf(1, "Expected type: ");
	  print_type_signature(expected_type);
	  debug_printf(1, "Found type: ");
	  print_type_signature(arg_type);

	  compiler_error(CERROR_ERROR, node->src_line_num,
			 "Function %s argument %d is wrong type\n",
			 func_name, i);

	}
      }


      check = type_check_node(node->node.children[0]);
      break;
    }

    case NODE_MEMBER:
      check = type_check_member(node);
      debug_printf(1, "NODE_MEMBER: ");
      print_type_signature(check->type_info);

      break;

    case NODE_BECOMES:
      check = type_check_assign(node);
      break;

    case NODE_ADD:
      check = type_check_add(node);
      break;

    case NODE_POINTER:
      check = type_check_dereference(node);
      break;

    case NODE_ADDRESS:
      check = type_check_address_of(node);
      break;

    case NODE_ASSIGN_ADDRESS:
      check = type_check_assign_address(node);
      break;

    case NODE_ARRAY:
      check = type_check_array(node);
      break;

    case NODE_RETURN:
      check = type_check_return(node);
      break;

    default:
      /*
       * Default operation: type check all the children and then assign
       * the type of this node to the type of the first child
       */
      for(i = 1; i < node->node.num_children; i++)
	type_check_node(node->node.children[i]);

      check = type_check_node(node->node.children[0]);

      break;
    }

    break;
  }

  if(!check->passed) {
    check->passed = 1;
    check->type_info->length = 0;
    check->type_info->decl_type = TYPE_NONE;
  }

  node->type_check = check;

  return check;
}

/*
 * Determine the resulting type, when the given operator
 * is applied to the two given types
 *
 */
type_info_t *result_type(type_info_t *t1, type_info_t *t2, int op) {

  type_info_t *result;
  result = malloc(sizeof(type_info_t));

  if(t1->length == 1 && t2->length == 1) {
    /*
     * Both primitive types
     * FIXME: Currently assuming both are integers
     */
    if(is_string_type(t1) && is_string_type(t2)) {
      /* Result of two strings/string literals is a string */
      result->decl_type = TYPE_RVALUE;
      result->length = 1;
      result->signature = malloc(sizeof(int) * result->length);
      result->signature[0] = TYPE_STRING;
      return result;
    }

    result->decl_type = TYPE_RVALUE;
    result->length = 1;
    result->signature = malloc(sizeof(int) * result->length);
    result->signature[0] = t1->signature[0];

    return result;
  }

  if(op == NODE_BECOMES) {
    /* FIXME: Assumes type is the same as lhs */
    copy_sig(t1, result);

    return result;
  }

  if(op == NODE_ADD || op == NODE_SUB) {
    /* Can only have one 'address of' in an add or subtract */
    if(t1->signature[0] == TYPE_ADDRESS && t2->signature[0] == TYPE_ADDRESS) {
      fprintf(stderr, "Invalid operands to binary addop\n");
      exit(EXIT_FAILURE);
    }

    if(t1->signature[0] == TYPE_ADDRESS) {
      /* Cast the operation to the left hands type */
      result = copy_sig(t1, result);
      result->decl_type = TYPE_RVALUE;

      return result;
    }

    if(t2->signature[0] == TYPE_ADDRESS) {
      /* Cast the operation to the right hands type */
      result = copy_sig(t2, result);
      result->decl_type = TYPE_RVALUE;

      return result;
    }
  }

  if(op == NODE_MUL || op == NODE_DIV) {
    /* Cannot use mulops with pointers */
    if(t1->signature[0] == TYPE_ADDRESS || is_array_type(t1) ||
       t2->signature[0] == TYPE_ADDRESS || is_array_type(t2)) {
      fprintf(stderr, "Invalid operands to binary mulop\n");
      exit(EXIT_FAILURE);
    }

    /* FIXME: Just assume both types are same and return left hands type */
    result = copy_sig(t1, result);
    result->decl_type = TYPE_RVALUE;

    return result;
  }


  /* FIXME: Just assume both types are same and return left hands type */
  result = copy_sig(t1, result);
  result->decl_type = TYPE_RVALUE;

  return result;
}

static void munge_history(ast_node_t **node) {
  int i;
  ast_node_t *history_node, *depth_node;

  history_node = mk_node(NODE_HISTORY, (*node)->src_line_num);
  depth_node = mk_num_leaf(0, (*node)->src_line_num);

  /* Type info for the depth node */
  depth_node->type_check = malloc(sizeof(type_check_t));
  depth_node->type_check->passed = 1;
  depth_node->type_check->type_info =
    create_type_signature(TYPE_CONSTANT, TYPE_INTEGER, NULL);

  /* Find the parent of the current node */
  for(i = 0; i < (*node)->parent->node.num_children; i++)
    if((*node)->parent->node.children[i] == *node) {
      (*node)->parent->node.children[i] = history_node;
      history_node->parent = (*node)->parent;
      break;
    }

  append_child(history_node, *node);
  append_child(history_node, depth_node);

}


/*
 * Return the corresponding type qualifier for the given node type
 */
int get_type(ast_node_t *current) {
  int type = -1;

  switch(current->node.op) {
  case NODE_VOID:
    type = TYPE_VOID;
    break;

  case NODE_INT:
    type = TYPE_INTEGER;
    break;

  case NODE_CHAR:
    type = TYPE_CHAR;
    break;

  case NODE_STRING:
    type = TYPE_STRING;
    break;

  case NODE_STRUCT:
    type = utype_def_index(current->node.children[0]->leaf.name);

    if(type != -1) {
      type <<= TYPE_SHIFT;
      type |= TYPE_UTYPE;
    }
    break;
  }

  return type;
}

/*
 * Compare two type signatures
 *
 * Messy: TYPE_POINTER and TYPE_ADDRESS are regarded as being the same thing.
 * This allows the signature of a variable usage to properly be compared
 * against a variable definition signature.
 *
 * TODO: Because the two types are basically just different ways of saying the
 * same thing, it would probably be better to rewrite it a single type. Perhaps
 * just TYPE_INDIRECT?
 *
 */
int compare_type_sigs(type_info_t *sig1, type_info_t *sig2) {
  int i;

  if(sig1->length != sig2->length)
    return 0;

  /* String literals are compatible with strings */
  if((sig1->decl_type == TYPE_STRING_LITERAL && is_primitive(sig2) &&
      primitive_type(sig2) == TYPE_STRING) ||
     (sig2->decl_type == TYPE_STRING_LITERAL && is_primitive(sig1) &&
      primitive_type(sig1) == TYPE_STRING))
    return 1;

  for(i = 0; i < sig1->length; i++) {
    if(!((sig1->signature[i] == TYPE_POINTER &&
	sig2->signature[i] == TYPE_ADDRESS) ||
       (sig1->signature[i] == TYPE_ADDRESS &&
	sig2->signature[i] == TYPE_POINTER)))

      if(sig1->signature[i] != sig2->signature[i])
	return 0;
  }

  return 1;
}


/*
 * Remove the leftmost instance of the given type from the type string
 */
type_info_t *remove_type_from_sig(type_info_t **sig, int type) {
  int i;

  /* Remove the leftmost instance of type from the type signature */
  for(i = 0; i < (*sig)->length; i++)
    if(type == TYPE_ARRAY && ((*sig)->signature[i] & TYPE_MASK) == TYPE_ARRAY)
      break;
    else if(type == TYPE_HISTORY &&
	    ((*sig)->signature[i] & TYPE_MASK) == TYPE_HISTORY)
      break;
    else if(type == TYPE_ADDRESS && (*sig)->signature[i] == TYPE_POINTER)
      break;
    else if((*sig)->signature[i] == type)
      break;

  if(i == (*sig)->length)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Attempt to nonexistent remove type %d from signature\n",
		   type);

  (*sig)->length--;
  for(; i < (*sig)->length; i++)
    (*sig)->signature[i] = (*sig)->signature[i + 1];

  (*sig)->signature = realloc((*sig)->signature,
			      sizeof(unsigned int) * (*sig)->length);

  return *sig;
}

/*
 * Add the given type to the left of the given type string
 */
type_info_t *add_to_type_sig(type_info_t **sig, int type) {
  int i;

  (*sig)->length++;
  (*sig)->signature = realloc((*sig)->signature, sizeof(unsigned int) *
			      (*sig)->length);

  for(i = (*sig)->length - 1; i > 0; i--)
    (*sig)->signature[i] = (*sig)->signature[i - 1];

  (*sig)->signature[i] = type;

  return *sig;
}

/*
 * Create a pointer to type. Used for history variable pointers
 */
type_info_t *create_ptr_to_type(int decl_type, int type, int levels) {
  type_info_t *sig;
  int i;

  sig = malloc(sizeof(type_info_t));
  sig->decl_type = decl_type;
  sig->length = levels + 1;

  sig->signature = malloc(sizeof(unsigned int) * sig->length);

  for(i = 0; i < levels; i++)
    sig->signature[i] = TYPE_POINTER;
  sig->signature[levels] = type;

  return sig;
}

/*
 * Create an array type
 */
type_info_t *create_array_type(int decl_type, int type, int size) {
  type_info_t *sig;

  sig = malloc(sizeof(type_info_t));
  sig->decl_type = decl_type;
  sig->length = 2;

  sig->signature = malloc(sizeof(unsigned int) * sig->length);

  sig->signature[0] = TYPE_ARRAY;
  sig->signature[0] |= (size << TYPE_SHIFT);

  sig->signature[1] = type;

  return sig;
}

/*
 * Create a type signature
 */
type_info_t *create_type_signature(int decl_type, int type,
				   ast_node_t *subtree) {
  int i, j, k, index = 0, num_indicies;
  type_info_t *sig;
  ast_node_t *current;

  /*
   * The type signature is a list of types, the primitive
   * type is always stored last in the list. Arrays types
   * have the size stored in the top 16 bits of the int.
   */
  sig = malloc(sizeof(type_info_t));

  if(subtree && subtree->node_type == NODE_OPERATOR) {
    sig->length = count_nodes(subtree);

    current = subtree;
    while(current->node_type == NODE_OPERATOR) {
      if(current->node.op == NODE_ARRAY || current->node.op == NODE_HISTORY) {
	sig->length--;
      }
      current = current->node.children[0];
    }

  } else
    sig->length = 1;

  sig->decl_type = decl_type;
  sig->signature = malloc(sizeof(unsigned int) * sig->length);
  sig->signature[sig->length - 1] = type;

  if(!subtree)
    return sig;

  for(i = 0; i < sig->length - 1; i++) {
    switch(subtree->node.op) {
    case NODE_POINTER:
      sig->signature[i] = TYPE_POINTER;
      break;

    case NODE_ARRAY:
      index = i;

      for(j = 1; j < subtree->node.num_children; j++, i++) {

	for(k = i; k > index + i; k--)
	  sig->signature[k] = sig->signature[k - 1];

	sig->signature[index] = subtree->node.children[j]->leaf.val;
        sig->signature[index] <<= 16;
	sig->signature[index++] |= TYPE_ARRAY;
      }
      i--;
      break;

    case NODE_HISTORY:
      sig->signature[i] = subtree->node.children[1]->leaf.val;
      sig->signature[i] <<= 16;
      sig->signature[i] |= TYPE_HISTORY;

      break;

    }
    subtree = subtree->node.children[0];
  }

  return sig;
}

/*
 * Copy a type signature
 */
type_info_t *copy_sig(type_info_t *src, type_info_t *dest) {
  int i;

  dest->decl_type = src->decl_type;
  dest->length = src->length;
  dest->signature = malloc(sizeof(int) * dest->length);

  dest->indirect_levels = src->indirect_levels;

  for(i = 0; i < dest->length; i++)
    dest->signature[i] = src->signature[i];

  return dest;
}

/*
 * Type sign a usage of a variable.
 */
type_info_t *type_sign_var(name_record_t *var, type_info_t *sig) {
  int i, base_type;

  if(!var)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Attempting to type-check non-existant variable\n");

  if(!var->type_info)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Variable %s has no associated type\n", var->name);

  base_type = var->type_info->signature[var->type_info->length - 1];

  debug_printf(1, "Var_record sig:");
  print_type_signature(var->type_info);

  sig->decl_type = var->type_info->decl_type;
  sig->length = var->type_info->length;
  sig->signature = malloc(sizeof(int) * sig->length);

  sig->indirect_levels = 0;
  sig->num_indicies = 0;
  sig->indicies = NULL;

  for(i = 0; i < sig->length; i++) {
    if(var->type_info->signature[i] == TYPE_POINTER)
      sig->signature[i] = TYPE_ADDRESS;
    else
      sig->signature[i] = var->type_info->signature[i];

  }

  return sig;
}

/*
 * Return whether or not the given type is an array type
 */
int is_array_type(type_info_t *sig) {
  int i;

  if(!sig)
    return 0;

  for(i = 0; i < sig->length; i++)
    if((sig->signature[i] & TYPE_MASK) == TYPE_ARRAY)
      return 1;
  return 0;
}

/*
 * Return whether or not a type signature represents an address type
 */
int is_address_type(type_info_t *sig) {
  int i;

  if(!sig)
    return 0;

  for(i = 0; i < sig->length; i++)
    if(sig->signature[i] == TYPE_ADDRESS || sig->signature[i] == TYPE_POINTER)
      return 1;
  return 0;
}

/*
 * Return whether or not a type signature represents a primitive type.
 *
 * FIXME: Why are address types primitive?
 *
 */
int is_primitive(type_info_t *sig) {
  if(!sig)
    return 0;

  if(sig->decl_type == TYPE_USER)
    return 0;

  if(sig->length == 1)
    return 1;

  if(sig->length == 2 && sig->signature[0] == TYPE_ADDRESS)
    return 1;

  return 0;
}

int is_string_type(type_info_t *sig) {
  int i;

  if(!sig)
    return 0;

  if(sig->decl_type == TYPE_STRING_LITERAL)
    return 1;

  for(i = 0; i < sig->length; i++)
    if(sig->signature[i] == TYPE_STRING)
      return 1;

  return 0;
}

int is_struct_type(type_info_t *sig) {
  int i;
  if(!sig)
    return 0;

  if((sig->signature[sig->length - 1] & TYPE_MASK) == TYPE_UTYPE)
      return 1;

  return 0;
}

int is_history_type(type_info_t *sig) {
  int i;

  for(i = 0; i < sig->length; i++)
    if((sig->signature[i] & TYPE_MASK) == TYPE_HISTORY)
      return 1;
  return 0;
}

int history_array_type(type_info_t *sig) {
  int i;

  if(!sig)
    return 0;

  for(i = 0; i < sig->length; i++)
    if((sig->signature[i] & TYPE_MASK) == TYPE_HISTORY)
      if(i > 0 && (sig->signature[i - 1] & TYPE_MASK) == TYPE_ARRAY)
	return HISTORY_INDEX_WISE;
      else
	return HISTORY_ARRAY_WISE;

  /* Oops */
  return HISTORY_NONE;
}

int get_history_depth(type_info_t *sig) {
  int i;

  if(!sig)
    return 0;

  for(i = 0; i < sig->length; i++)
    if((sig->signature[i] & TYPE_MASK) == TYPE_HISTORY)
      return sig->signature[i] >> 16;
  return 0;
}

/*
 * Return the primitive type for a given type
 */
int primitive_type(type_info_t *sig) {
  if(!sig)
    return TYPE_NONE;

  return sig->signature[sig->length - 1];
}

/*
 * Return the number of dimensions in an array type
 */
int get_num_dimensions(type_info_t *sig) {
  int i, count = 0;

  if(!sig)
    return 0;

  for(i = 0; i < sig->length; i++)
    if((sig->signature[i] & TYPE_MASK) == TYPE_ARRAY)
      count++;

  return count;
}

/*
 * Get the size of the specified dimension
 *
 * Note: Dimensions start at one, not zero
 */
int get_dimension_size(type_info_t *sig, int index) {
  int i;

  for(i = 0; i < sig->length; i++)
    if((sig->signature[i] & TYPE_MASK) == TYPE_ARRAY) {
      index--;

      if(index == 0)
	return sig->signature[i] >> 16;
    }

  /* Shouldn't get here */
  return 0;
}

/*
 * Return the number of elements in an array
 */
int get_num_elements(type_info_t *sig) {
  int i, elements = 0;

  for(i = 0; i < get_num_dimensions(sig); i++)
    elements += get_dimension_size(sig, i + 1);

  return elements;
}

/*
 * Cast an array type to the pointer equivalent
 */
type_info_t *cast_array_to_pointer(type_info_t *sig) {
  int i;
  type_info_t *cast;

  cast = malloc(sizeof(type_info_t));
  cast->length = sig->length;
  cast->signature = malloc(sizeof(int) * cast->length);

  for(i = 0; i < cast->length - 1; i++)
    cast->signature[i] = TYPE_ADDRESS;
  cast->signature[cast->length - 1] = sig->signature[sig->length - 1];

  cast->indirect_levels = -1;

  return cast;
}

/*
 * TODO: print_type_signature should call sprint_type_signature and
 * print to stdout, to prevent duplication of code.
 */
void print_type_signature(type_info_t *sig) {
  int i;

  debug_printf(1, "<%d> ", sig->length);

  switch(sig->decl_type) {
  case TYPE_VARIABLE:
    debug_printf(1, "(variable) ");
    break;
  case TYPE_FUNCTION:
    debug_printf(1, "(function) ");
    break;
  case TYPE_ARGUMENT:
    debug_printf(1, "(argument) ");
    break;
  case TYPE_CONSTANT:
    debug_printf(1, "(constant) ");
    break;
  case TYPE_STRING_LITERAL:
    debug_printf(1, "(string literal) ");
    break;
  case TYPE_LVALUE:
    debug_printf(1, "(lvalue) ");
    break;
  case TYPE_RVALUE:
    debug_printf(1, "(rvalue) ");
    break;
  case TYPE_USER:
    debug_printf(1, "(struct) ");
    break;
  case TYPE_MEMBER:
    debug_printf(1, "(member) ");
    break;
  default:
    debug_printf(1, "(unknown %d) ", sig->decl_type);
    break;
  }

  for(i = 0; i < sig->length; i++) {

    if(sig->decl_type == TYPE_STRING_LITERAL && i == sig->length - 1) {
      debug_printf(1, "string literal, id = %d\n", sig->signature[i]);
      break;
    }

    switch(sig->signature[i] & TYPE_MASK) {
    case TYPE_VOID:
      debug_printf(1, "void\n");
      break;

    case TYPE_INTEGER:
      debug_printf(1, "int\n");
      break;

    case TYPE_CHAR:
      debug_printf(1, "char\n");
      break;

    case TYPE_STRING:
      debug_printf(1, "string\n");
      break;

    case TYPE_POINTER:
      debug_printf(1, "pointer to ");
      break;

    case TYPE_ADDRESS:
      debug_printf(1, "address of ");
      break;

    case TYPE_ARRAY:
      debug_printf(1, "array (size %d) of ", (sig->signature[i] &
				      TYPE_VALUE_MASK) >> TYPE_SHIFT);
      break;

    case TYPE_UTYPE:
      debug_printf(1, "user type (%d)\n", (sig->signature[i] &
				      TYPE_VALUE_MASK) >> TYPE_SHIFT);
      break;

    case TYPE_HISTORY:
      debug_printf(1, "history of ");
      break;

    default:
      debug_printf(1, "Unknown type (%d) of ", sig->signature[i]);
      break;
    }
  }
}

char *sprint_type_signature(type_info_t *sig) {
  int i;
  char temp[64], *buf;

  if(!sig || !sig->length)
    return NULL;

  /* Assumes that signatures are not longer than 64 characters */
  buf = malloc(sig->length * 64);
  sprintf(buf, "<%d> ", sig->length);

  switch(sig->decl_type) {
  case TYPE_NONE:
    sprintf(buf, "(none)");
    break;
  case TYPE_VARIABLE:
    sprintf(buf, "(variable) ");
    break;
  case TYPE_FUNCTION:
    sprintf(buf, "(function) ");
    break;
  case TYPE_ARGUMENT:
    sprintf(buf, "(argument) ");
    break;
  case TYPE_CONSTANT:
    sprintf(buf, "(constant) ");
    break;
  case TYPE_STRING_LITERAL:
    sprintf(buf, "(string literal) ");
    break;
  case TYPE_LVALUE:
    sprintf(buf, "(lvalue) ");
    break;
  case TYPE_RVALUE:
    sprintf(buf, "(rvalue) ");
    break;
  case TYPE_USER:
    sprintf(buf, "(struct) ");
    break;
  case TYPE_MEMBER:
    sprintf(buf, "(member) ");
    break;
  default:
    sprintf(buf, "(unknown %d) ", sig->decl_type);
    break;
  }

  for(i = 0; i < sig->length; i++) {

    if(sig->decl_type == TYPE_STRING && i == sig->length - 1) {
      sprintf(temp, "string literal, id = %d", sig->signature[i]);
      break;
    }

    switch(sig->signature[i] & TYPE_MASK) {
    case TYPE_VOID:
      sprintf(temp, "void");
      break;

    case TYPE_INTEGER:
      sprintf(temp, "int");
      break;

    case TYPE_CHAR:
      sprintf(temp, "char");
      break;

    case TYPE_STRING:
      sprintf(temp, "string");
      break;

    case TYPE_POINTER:
      sprintf(temp, "pointer to ");
      break;

    case TYPE_ADDRESS:
      sprintf(temp, "address of ");
      break;

    case TYPE_ARRAY:
      sprintf(temp, "array (size %d) of ", (sig->signature[i] &
					     TYPE_VALUE_MASK) >> TYPE_SHIFT);
      break;

    case TYPE_UTYPE:
      sprintf(temp, "user type (%d)", (sig->signature[i] &
					 TYPE_VALUE_MASK) >> TYPE_SHIFT);
      break;

    case TYPE_HISTORY:
      sprintf(temp, "history (depth %d) ", (sig->signature[i] &
				     TYPE_HISTORY_MASK) >> 16);
      break;

    default:
      sprintf(temp, "Unknown type (%d) of ", sig->signature[i]);
      break;
    }
    strncat(buf, temp, strlen(temp));
  }

  return buf;
}

char *sprint_type_name(name_record_t *entry) {
  char *buf = malloc(strlen(entry->name) + 64);

  switch(entry->type_info->decl_type) {
  case TYPE_FUNCTION:
    sprintf(buf, "%s is a function returning ", entry->name);
    break;
  case TYPE_VARIABLE:
    sprintf(buf, "%s is a variable of type ", entry->name);
    break;
  case TYPE_ARGUMENT:
    sprintf(buf, "%s is a function argument of type ", entry->name);
    break;
  }

  return buf;
}


/*
 * Print out a type signature
 */
void print_type_name(name_record_t *entry) {
  switch(entry->type_info->decl_type) {
  case TYPE_FUNCTION:
    debug_printf(1, "%s is a function returning ", entry->name);
    break;
  case TYPE_VARIABLE:
    debug_printf(1, "%s is a variable of type ", entry->name);
    break;
  case TYPE_ARGUMENT:
    debug_printf(1, "%s is a function argument of type ", entry->name);
    break;
  }
}
