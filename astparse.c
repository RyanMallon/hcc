/*
 * astparse.c
 * Ryan Mallon (2005)fg
 *
 * Functions for parsing the AST to produce intermediate code
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include "ast.h"
#include "ast_xml.h"
#include "typechk.h"
#include "symtable.h"
#include "utype.h"
#include "cerror.h"
#include "icg.h"
#include "mir.h"
#include "tempreg.h"
#include "strings.h"
#include "astparse.h"
#include "debug.h"
#include "cflags.h"
#include "history.h"

/* Type check macros */
#define node_type_info(n) n->type_check->type_info
#define child_type_info(n, c) n->node.children[c]->type_check->type_info

extern int alloced_regs(void);

static int generate_member_load(ast_node_t *);

static expression_t *parse_member(ast_node_t *);
static expression_t *parse_var(ast_node_t *);
static expression_t *parse_expression(ast_node_t *);

static void parse_statement(ast_node_t *);
static void parse_func(ast_node_t *);
static void parse_block(ast_node_t *);

static void parse_decl_subtrees(ast_node_t *);

static int is_in_register(char *);
static void clear_register_stores(void);

extern scope_node_t *global_scope, *current_scope;

extern compiler_options_t cflags;
ic_list_t *entry_point_jump;

/* Current structure */
static name_record_t *current_struct = NULL;

/* Current array index. Used for history array functions */
static expression_t *array_index = NULL;

int src_line_num;

/*
 * Parse a struct member.
 */
expression_t *parse_member(ast_node_t *expr) {
  ast_node_t *name_node, *current;
  name_record_t *var, *member;
  expression_t *result;
  int reg, i, base_reg, indirect_levels = 0;

  /* Get struct name */
  name_node = expr;
  while(name_node->node_type == NODE_OPERATOR)
    name_node = name_node->node.children[0];

  var = get_var_entry(name_node->leaf.name);

  /* Parse left hand side */
  result = parse_expression(expr->node.children[0]);
  base_reg = result->op_val;

  if(!current_struct)
    current_struct = result->var;

  /* Parse right hand side */
  result = parse_expression(expr->node.children[1]);
  member = result->var;

  reg = tr_alloc();
  mir_emit(MIR_ADD, mir_var(current_struct), mir_const(member->offset),
	   mir_reg(reg));

  /* Check if the member is also a struct */
  if(is_struct_type(member->type_info)) {
    char *name = get_name_from_node(expr->node.children[1]);
    current_struct = get_utype_member(var, name);
  } else
    current_struct = NULL;

  result->op_type = OP_REG;
  result->size = sizeof_type(member->type_info);
  result->var = member;
  result->op_val = reg;


  return result;
}



/*
 * Parse a variable
 */
static expression_t *parse_var(ast_node_t *var_node) {
  name_record_t *var;
  name_record_t *func;
  expression_t *result;
  int i, offset;

  src_line_num = var_node->src_line_num;

  if(var_node->node.op == NODE_MEMBER)
    return parse_member(var_node);

  if(var_node->node.op == NODE_CAST)
    compiler_error(CERROR_ERROR, var_node->src_line_num,
		   "Illegal use of cast\n");

  result = malloc(sizeof(expression_t));
#if 0
  result->ast_node = var_node;
#endif

  if(var_node->node_type != NODE_LEAF_ID)
    compiler_error(CERROR_ERROR, var_node->src_line_num,
		   "Attempting to parse non-leaf variable\n");

  if(!current_struct)
    var = get_var_entry(var_node->leaf.name);
  else
    var = get_utype_member(current_struct, var_node->leaf.name);

  if(!var)
    compiler_error(CERROR_ERROR, var_node->src_line_num,
		   "Variable %s undeclared\n", var_node->leaf.name);


  result->op_type = OP_VAR;
  result->indirect = 0;
  result->size = sizeof_type(var->type_info);
  result->var = var;

  return result;
}

/*
 * Parse an expression
 *
 * TODO: Sizes are just assumed to be the size of the lefthand side type.
 *
 */
static expression_t *parse_expression(ast_node_t *expr) {
  expression_t *result, *t1, *t2;
  mir_node_t *mcurrent, *mlabel1, *mlabel2, *mlabel3;

  int i, size, reg;

  src_line_num = expr->src_line_num;

  result = malloc(sizeof(expression_t));
#if 0
  result->ast_node = expr;
#endif

  if(expr->node_type == NODE_OPERATOR) {
    switch(expr->node.op) {
    case NODE_ADD:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      array_index = t2;

      /* Pass the variable name for arrays and pointers */
      if(t1->var && (is_array_type(t1->var->type_info) ||
		     is_address_type(t1->var->type_info)))
	result->var = t1->var;
      else if(t2->var && (is_array_type(t2->var->type_info) ||
			  is_address_type(t2->var->type_info)))
	result->var = t2->var;

      result->op_history = t1->op_history;
      result->size = t1->size;
      result->op_type = OP_REG;
      result->op_val = tr_alloc();

      if(expr->type_check) {
	if(is_address_type(node_type_info(expr))) {
	  /* Pointer Addition. Ptr/Array must be on left */
	  size = sizeof_type(node_type_info(expr));

	  /*
	   * For index-wise arrays pointer addition adds a full
	   * size of the array.
	   */
	  if(is_history_type(t1->var->type_info) &&
	     is_array_type(t1->var->type_info) &&
	     history_array_type(t1->var->type_info) == HISTORY_INDEX_WISE)
	    size *= get_num_elements(t1->var->type_info);

	  mir_emit(MIR_MUL, mir_opr(t2), mir_const(size),
		   mir_opr(result));
	  mir_emit(MIR_ADD, mir_opr(t1), mir_opr(result),
		   mir_opr(result));

	} else {
	  /* Normal addition */
  mir_emit(MIR_ADD, mir_opr(t1), mir_opr(t2),
		   mir_opr(result));
	}
      }

#if 0
      /* Call strcat for strings, add op for everything else */
      if(expr->type_check && is_primitive(node_type_info(expr)) &&
	 is_string_type(node_type_info(expr)))
	string_strcat(t1->op_val, t2->op_val, result->op_val);

#endif

      break;

    case NODE_SUB:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      /* Pass the variable name for arrays and pointers */
      if(t1->var && (is_array_type(t1->var->type_info) ||
		     is_address_type(t1->var->type_info)))
	result->var = t1->var;
      else if(t2->var && (is_array_type(t2->var->type_info) ||
			  is_address_type(t2->var->type_info)))
	result->var = t2->var;

      result->op_type = OP_REG;
      result->size = t1->size;
      result->op_val = tr_alloc();


      if(expr->type_check) {
	if(is_address_type(node_type_info(expr))) {
	  /* Pointer subtraction */
	  size = sizeof_type(node_type_info(expr));
	  mir_emit(MIR_MUL, mir_opr(t2), mir_const(size), mir_opr(result));
	  mir_emit(MIR_SUB, mir_opr(t1), mir_opr(result), mir_opr(result));

	} else {
	  /* Normal subtraction */
	  mir_emit(MIR_SUB, mir_opr(t1), mir_opr(t2),
		   mir_opr(result));
	}
      }

      break;

    case NODE_MINUS:
      if(expr->node.children[0]->node_type == NODE_LEAF_NUM) {
	expr->node.children[0]->leaf.val = -expr->node.children[0]->leaf.val;
      }

      result = parse_expression(expr->node.children[0]);
      break;

    case NODE_DIV:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->size = t1->size;
      result->op_val = tr_alloc();

      mir_emit(MIR_DIV, mir_opr(t1), mir_opr(t2),
	       mir_opr(result));

      break;

    case NODE_MUL:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->size = t1->size;
      result->op_val = tr_alloc();

      mir_emit(MIR_MUL, mir_opr(t1), mir_opr(t2),
	       mir_opr(result));

      break;

    case NODE_OR:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->size = t1->size;
      result->op_val = tr_alloc();

      reg = tr_alloc();

      mir_emit(MIR_MOVE, mir_const(1), NULL,
	       mir_opr(result));

      mir_emit(MIR_NEQ, mir_opr(t1), mir_const(0),
	       mir_reg(reg));
      mlabel1 = mir_emit(MIR_IF, mir_reg(reg), NULL, NULL);

      mir_emit(MIR_NEQ, mir_opr(t2), mir_const(0),
	       mir_reg(reg));
      mlabel2 = mir_emit(MIR_IF, mir_reg(reg), NULL, NULL);
      mlabel3 = mir_emit(MIR_MOVE, mir_const(0), NULL,
			 mir_opr(result));

      mir_set_jump(mlabel1, mlabel3->next);
      mir_set_jump(mlabel2, mlabel3->next);

      break;

    case NODE_AND:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->size = t1->size;
      result->op_val = tr_alloc();

      reg = tr_alloc();

      mir_emit(MIR_MOVE, mir_const(0), NULL,
	       mir_opr(result));

      mir_emit(MIR_EQL, mir_opr(t1), mir_const(0),
	       mir_reg(reg));
      mlabel1 = mir_emit(MIR_IF, mir_reg(reg), NULL, NULL);

      mir_emit(MIR_EQL, mir_opr(t1), mir_const(0),
	       mir_reg(reg));
      mlabel2 = mir_emit(MIR_IF, mir_reg(reg), NULL, NULL);
      mlabel3 = mir_emit(MIR_MOVE, mir_const(1), NULL,
			 mir_opr(result));

      mir_set_jump(mlabel1, mlabel3->next);
      mir_set_jump(mlabel2, mlabel3->next);

      break;

    case NODE_EQL:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);


      result->op_val = tr_alloc();
      result->op_type = OP_REG;
      result->size = t1->size;

      mir_emit(MIR_MOVE, mir_const(0), NULL,
	       mir_reg(result->op_val));
      mlabel1 = mir_emit(MIR_IF, mir_opr(t1), mir_opr(t2),
			 mir_const(MIR_NEQ));
      mlabel2 = mir_emit(MIR_MOVE, mir_const(1), NULL,
			 mir_reg(result->op_val));

      mir_set_jump(mlabel1, mlabel2->next);

#if 0
      /* String equality */
      if(cflags.flags & CFLAG_OUTPUT_IC) {
	if(expr->type_check && is_primitive(node_type_info(expr)) &&
	   is_string_type(node_type_info(expr))) {
	  string_strcmp(t1->op_val, t2->op_val);

	  label1 = ic_emit(IC_BEQ, result->op_val, IC_RV, -1,
			   IC_REG, IC_REG, IC_LABEL);

	} else {
	  label1 = ic_emit(IC_BEQ, t1->op_val, t2->op_val, -1,
			   IC_REG, IC_REG, IC_LABEL);
	}

	label2 = ic_emit(IC_LIT, 0, result->op_val, 0,
			 IC_CONST, IC_REG, IC_NONE);

	ic_set_jump(&label1, label2->next);
      }
#endif
      break;

    case NODE_NEQ:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->size = t1->size;
      result->op_val = tr_alloc();

      mir_emit(MIR_MOVE, mir_const(0), NULL,
	       mir_reg(result->op_val));
      mlabel1 = mir_emit(MIR_IF, mir_opr(t1), mir_opr(t2),
			 mir_const(MIR_EQL));
      mlabel2 = mir_emit(MIR_MOVE, mir_const(1), NULL,
			 mir_reg(result->op_val));

      mir_set_jump(mlabel1, mlabel2->next);


      break;

    case NODE_LSS:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->size = t1->size;
      result->op_val = tr_alloc();

      mir_emit(MIR_MOVE, mir_const(0), NULL,
	       mir_reg(result->op_val));
      mlabel1 = mir_emit(MIR_IF, mir_opr(t1), mir_opr(t2),
			 mir_const(MIR_GEQ));
      mlabel2 = mir_emit(MIR_MOVE, mir_const(1), NULL,
			 mir_reg(result->op_val));

      mir_set_jump(mlabel1, mlabel2->next);

      break;

    case NODE_LEQ:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->size = t1->size;
      result->op_val = tr_alloc();

      mir_emit(MIR_MOVE, mir_const(0), NULL,
	       mir_reg(result->op_val));
      mlabel1 = mir_emit(MIR_IF, mir_opr(t1), mir_opr(t2),
			 mir_const(MIR_GTR));
      mlabel2 = mir_emit(MIR_MOVE, mir_const(1), NULL,
			 mir_reg(result->op_val));

      mir_set_jump(mlabel1, mlabel2->next);


      break;

    case NODE_GTR:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->size = t1->size;
      result->op_val = tr_alloc();

      mir_emit(MIR_MOVE, mir_const(0), NULL,
	       mir_reg(result->op_val));
      mlabel1 = mir_emit(MIR_IF, mir_opr(t1), mir_opr(t2),
			 mir_const(MIR_LEQ));
      mlabel2 = mir_emit(MIR_MOVE, mir_const(1), NULL,
			 mir_reg(result->op_val));

      mir_set_jump(mlabel1, mlabel2->next);


      break;

    case NODE_GEQ:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->size = t1->size;
      result->op_val = tr_alloc();

      mir_emit(MIR_MOVE, mir_const(0), NULL,
	       mir_reg(result->op_val));
      mlabel1 = mir_emit(MIR_IF, mir_opr(t1), mir_opr(t2),
			 mir_const(MIR_LSS));
      mlabel2 = mir_emit(MIR_MOVE, mir_const(1), NULL,
			 mir_reg(result->op_val));

      mir_set_jump(mlabel1, mlabel2->next);

      break;

    case NODE_CAST:
      t1 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->op_val = t1->op_val;
      result->var = t1->var;

      result->size = t1->size;

      break;

    case NODE_POINTER:
      t1 = parse_expression(expr->node.children[0]);

      if(!t1->indirect) {
	result->indirect = 1;
	result->op_type = t1->op_type;

	result->op_val = t1->op_val;
	result->var = t1->var;

      } else {
	int temp_reg = tr_alloc();

	result->indirect = 1;
	result->op_type = OP_REG;
	result->op_val = temp_reg;

	mir_emit(MIR_MOVE, mir_opr(t1), NULL, mir_reg(temp_reg));
      }

#if 0
      result->op_type = OP_REG;
      result->var = t1->var;

      if(is_array_type(t1->var->type_info) &&
	 is_history_type(t1->var->type_info)) {
	result->op_val = history_array_load(t1->var, t1->op_val,
					    t1->op_history);
      } else {
	//apply_unary(&t1, TYPE_POINTER);
	result->op_val = t1->op_val;
      }

      result->size = t1->size;
#endif

      break;

    case NODE_ADDRESS: {
      int temp_reg;

      t1 = parse_var(expr->node.children[0]);

      if(is_history_type(t1->var->type_info))
	temp_reg = history_address(t1);
      else {
	temp_reg = tr_alloc();
	mir_emit(MIR_ADDR, mir_opr(t1), NULL, mir_reg(temp_reg));
      }

      /* Dont mark history cycles as may-aliases */
      if(!is_history_type(t1->var->type_info)) {
	debug_printf(1, "Marking %s as a may-alias\n", t1->var->name);
	t1->var->may_alias = 1;
      }

      result->op_type = OP_REG;
      result->op_val = temp_reg;
      break;
    }

    case NODE_HISTORY:
      t1 = parse_expression(expr->node.children[0]);
      t2 = parse_expression(expr->node.children[1]);

      result->op_type = OP_REG;
      result->var = t1->var;

      result->op_val = t1->op_val;
      result->op_history = parse_expression(expr->node.children[1]);

      if(is_array_type(t1->var->type_info)) {
	result->op_val = history_array_load(t1->var, array_index, t2);
	result->op_type = OP_REG;

      } else {
	result->op_val = history_load(t1->var, t2);
	result->op_type = OP_REG;
      }

      break;

    case NODE_FUNC_CALL: {
      name_record_t *func;
      scope_node_t *func_scope;
      char *func_name;
      expression_t **func_args;
      int offset, num_func_args, num_history_byrefs = 0;

      /* Get function name and entry */
      func_name = get_name_from_node(expr->node.children[0]);
      func = get_func_entry(func_name);

      /* Copy arguments onto the stack */
      func_scope = get_func_arg_scope(func_name);
      reg = tr_alloc();

      num_func_args = func->num_args;
      if(num_func_args == -1)
        num_func_args = expr->node.num_children - 1;

      if(num_func_args)
	func_args = malloc(sizeof(expression_t *) * num_func_args);

      for(i = num_func_args - 1; i >= 0; i--) {
	func_args[i] = parse_expression(expr->node.children[i + 1]);

#if 0
	if(cflags.history_arg_setting == HISTORY_ARG_TEMP_COPY) {
	  if(result->var &&
	     is_history_type(result->var->type_info) &&
	     is_address_type(result->var->type_info))
	    num_history_byrefs++;
	}
#endif

      }

      mcurrent = mir_emit(MIR_CALL, mir_var(func), NULL,
			  NULL);
      for(i = 0; i < num_func_args; i++)
	mir_add_arg(&mcurrent, mir_opr(func_args[i]));


      result->op_val = tr_alloc();
      result->op_type = OP_REG;
      result->size = 0;

      //free(func_args);
      break;
    }

    case NODE_MEMBER:
      result = parse_member(expr);

#if 0
      /* Load the member */
      if(!is_array_type(t1->var->type_info)) {
	result->op_val = tr_alloc();
	ic_emit(IC_LOD, t1->op_val, result->op_val, 0,
		IC_REG_ADDR, IC_REG, IC_NONE);
      } else
	result->op_val = t1->op_val;

      result->size = IC_SIZEOF_INT;
      result->op_type = OP_REG;
      result->var = t1->var;
#endif

      break;

    default:
      /* Bugger. This probably won't happen */
      debug_printf(1, "expr->node_type = %s\n", ast_node_name[expr->node_type]);
      compiler_error(CERROR_WARN, expr->src_line_num,
		     "Error in expression, node type = %s\n",
		     ast_node_name[expr->node_type]);
      break;
    }

  } else if(expr->node_type == NODE_LEAF_NUM) {
    /* Constant value */
    result->op_val = expr->leaf.val;
    result->op_type = OP_CONST;
    result->size = IC_SIZEOF_INT;

  } else if(expr->node_type == NODE_LEAF_CHAR) {
    /* Character literal */

    result->op_val = expr->leaf.val;
    result->op_type = OP_CONST;
    result->size = IC_SIZEOF_CHAR;

  } else if(expr->node_type == NODE_LEAF_STRING) {
    /* String literal */

    int id = expr->type_check->type_info->signature[0];

    result->op_val = tr_alloc();
    result->op_type = OP_REG;
    result->size = IC_SIZEOF_CHAR;

    mir_emit(MIR_LOAD_STRING, mir_const(id), NULL, mir_reg(result->op_val));

  } else {
    /* Variable */
    t1 = parse_var(expr);

    result->op_type = OP_VAR;
    result->size = t1->size;
    result->var = t1->var;
  }

  return result;
}

/*
 * Parse a statement
 */
static void parse_statement(ast_node_t *statement) {
  ic_list_t *t1, *t2, *current;
  mir_node_t *mt1, *mt2, *mcurrent;

  int instr, reg;
  expression_t *e1, *e2, *result;

  src_line_num = statement->src_line_num;

  switch(statement->node.op) {
  case NODE_ATOMIC:
    debug_printf(1, "Entering atomic block\n");
    history_enter_atomic();

    parse_block(statement->node.children[statement->node.num_children - 1]);

    debug_printf(1, "Leaving atomic block\n");
    history_exit_atomic();
    break;

  case NODE_WHILE:
    mt1 = mir_list_tail();
    e1 = parse_expression(statement->node.children[0]);

    mt2 = mir_emit(MIR_IF, mir_opr(e1), mir_const(0),
		   mir_const(MIR_EQL));
    parse_block(statement->node.children[1]);

    /* Backpatch the loop */
    mcurrent = mir_emit(MIR_JUMP, NULL, NULL, NULL);
    mir_set_jump(mcurrent, mt1->next);

    /* Backpatch the condition */
    mcurrent = mir_list_tail();
    mir_set_jump(mt2, mcurrent->next);

    break;

  case NODE_DO:
    mt1 = mir_list_tail();
    parse_block(statement->node.children[1]);

    e1 = parse_expression(statement->node.children[0]);
    mt2 = mir_emit(MIR_IF, mir_opr(e1), mir_const(0),
		   mir_const(MIR_EQL));

    /* Backpatch the loop */
    mcurrent = mir_emit(MIR_JUMP, NULL, NULL, NULL);
    mir_set_jump(mcurrent, mt1->next);

    /* Backpatch the condition */
    mcurrent = mir_list_tail();
    mir_set_jump(mt2, mcurrent->next);
    break;

  case NODE_IF:
    e1 = parse_expression(statement->node.children[0]);

    mt1 = mir_emit(MIR_IF, mir_opr(e1), mir_const(0),
		   mir_const(MIR_EQL));

    parse_block(statement->node.children[1]);

    mt2 = mir_list_tail();
    mir_set_jump(mt1, mt2->next);

    break;

  case NODE_FOR:
    parse_statement(statement->node.children[0]);

    mt1 = mir_list_tail();
    e1 = parse_expression(statement->node.children[1]);

    mt2 = mir_emit(MIR_IF, mir_opr(e1), mir_const(0),
		   mir_const(MIR_EQL));

    parse_block(statement->node.children[3]);
    parse_statement(statement->node.children[2]);

    /* Backpatch the loop */
    mcurrent = mir_emit(MIR_JUMP, NULL, NULL, NULL);
    mir_set_jump(mcurrent, mt1->next);

    /* Backpatch the condition */
    mcurrent = mir_list_tail();
    mir_set_jump(mt2, mcurrent->next);

    break;

  case NODE_RETURN:
    e1 = parse_expression(statement->node.children[0]);
    mir_emit(MIR_RETURN, mir_opr(e1), NULL, NULL);

    break;

  case NODE_BECOMES:
    result = malloc(sizeof(expression_t));

    /*
     * Parse the rhs of the expression first so we know what we are storing
     * before actually attempting to store it.
     */
    e2 = parse_expression(statement->node.children[1]);
    e1 = parse_expression(statement->node.children[0]);

#if 0
    if(e1->var && is_history_type(e1->var->type_info) &&
       is_array_type(e1->var->type_info) &&
       history_array_type(e1->var->type_info) == HISTORY_ARRAY_WISE &&
       history_in_atomic()) {

      /* Add this to the pending clist writes */
      history_add_clist_write(e1->var);
    }
#endif


    /* Check if this is a history variable */
    if(e1->var && is_history_type(e1->var->type_info)) {
      if(is_array_type(e1->var->type_info))
	history_array_store(e1, array_index, e2);
      else
	history_store(e1, e2);
      break;
    }

    /*
     * MIR code generation.
     * If the RHS is a single value or the LHS is a register then
     * generate a move instruction
     */
    if(e2->op_type == OP_VAR || e2->op_type == OP_CONST || e2->indirect ||
       e1->op_type == OP_REG) {

      mir_emit(MIR_MOVE, mir_opr(e2), NULL, mir_opr(e1));

    } else if(e1->op_type == OP_VAR) {
      /*
       * Backpatch the previous instruction to assign to the variable on
       * the LHS of the assignment expression
       */
      debug_printf(1, "Modifying existing MIR instruction\n");

      mir_node_t *instr = mir_list_tail();
      instr->instruction->operand[2] = mir_opr(e1);
    }
    break;


  case NODE_FUNC_CALL:
    parse_expression(statement);
    break;

  default:
    compiler_error(CERROR_WARN, statement->src_line_num,
		   "Unexpected node type in parse_statment, op=%s\n",
		   ast_node_name[statement->node.op]);
    break;
  }
}

/*
 * Parse a function
 */
static void parse_func(ast_node_t *func) {
  ast_node_t *name_node;
  name_record_t *func_record;
  type_info_t *sig;
  int i, num_args;

  src_line_num = func->src_line_num;

  /* Get the function name */
  name_node = func->node.children[0];
  while(name_node->node_type == NODE_OPERATOR)
    name_node = name_node->node.children[0];

  debug_printf(1, "Function name = %s\n", name_node->leaf.name);
  current_scope = enter_function_scope(name_node->leaf.name);
  init_history_vars();

  func_record = get_func_entry(name_node->leaf.name);
  if(!func_record)
    compiler_error(CERROR_WARN, name_node->src_line_num,
		   "Function %s undeclared\n", name_node->leaf.name);

  num_args = 0;
  for(i = 1; i < func->node.num_children; i++)
    if(func->node.children[i]->node.op != NODE_BLOCK)
      num_args++;

  if(num_args < func_record->num_args)
    compiler_error(CERROR_WARN, func->src_line_num,
		   "Function %s has fewer arguments than declaration "
		   "(%d < %d)\n", name_node->leaf.name, num_args,
		   func_record->num_args);
  else if(num_args > func_record->num_args)
    compiler_error(CERROR_WARN, func->src_line_num,
		   "Function %s has more arguments than declaration "
		   "(%d > %d)\n", name_node->leaf.name, num_args,
		   func_record->num_args);

  /* Prologue */
  scope_node_t *arg_scope;

  mir_node_t *mn = mir_emit(MIR_LABEL, NULL, NULL, NULL);
  func_record->mir_first = mn->next;

  mir_label(&mn, func_record->name);
  mir_emit(MIR_BEGIN, NULL, NULL, NULL);

  /* Recieve args */
  arg_scope = get_func_arg_scope(func_record->name);

  for(i = 0; i < func_record->num_args; i++)
    mir_emit(MIR_RECEIVE, NULL, NULL, mir_var(arg_scope->name_table[i]));

  /* Backpatch jump if this is the entry point */
  if(!strcmp(cflags.entry_point, name_node->leaf.name))
    if(cflags.flags & CFLAG_OUTPUT_IC)
      ic_set_jump(&entry_point_jump, func_record->ic_offset->next);

  /* Body */
  parse_block(func->node.children[num_args + 1]);

  /* Epilogue */
  current_scope = global_scope;
  if(cflags.flags & CFLAG_OUTPUT_IC)
    ic_emit(IC_RET, 0, 0, 0, IC_NONE, IC_NONE, IC_NONE);

  if(cflags.flags & CFLAG_OUTPUT_HLIC)
    mir_emit(MIR_END, NULL, NULL, NULL);

  func_record->mir_last = mir_list_tail();
}


/*
 * Parse a block of statements
 */
static void parse_block(ast_node_t *block) {
  int i, scope = 0;

  src_line_num = block->src_line_num;

  /* Allow for empty functions */
  if(!block)
    return;

  if(block->node.scope_tag != -1) {
    debug_printf(1, "Entering new scope\n");
    current_scope = current_scope->children[block->node.scope_tag];
    init_history_vars();
  }

  for(i = 0; i < block->node.num_children; i++)
    if(block->node.children[i]->node.op != NODE_DECL)
      parse_statement(block->node.children[i]);

  debug_printf(1, "Leaving scope\n");

  if(block->node.scope_tag != -1)
    current_scope = current_scope->prev;

}

void parse_declarations(ast_node_t *ast) {
  parse_decl_subtrees(ast);
  set_scope_offsets();
  current_scope = global_scope;
}

/*
 * Set the global scope offset
 * FIXME: This is a bit messy.
 *
 */
void set_global_offset(void) {
  int i, data_offset;

  data_offset = string_table_size();

  for(i = 0; i < global_scope->num_records; i++)
    global_scope->name_table[i]->offset += data_offset;

}

/*
 * Parse function declarations
 */
static void parse_decl_funcs(ast_node_t *current) {
  int i;
  ast_node_t *name_node;

  src_line_num = current->src_line_num;

  for(i = 0; i < current->node.num_children; i++) {
    if(current->node.children[i]->node.op == NODE_FUNC ||
       current->node.children[i]->node.op == NODE_INLINE_FUNC){
      name_node = get_name_node(current->node.children[i]);
      new_func_scope(name_node->leaf.name);
    }
    current_scope = global_scope;
  }

  debug_printf(1, "Parse decl funcs done\n");
  print_scope_lists();

}

/*
 * Parse the declaration subtrees
 */
static void parse_decl_subtrees(ast_node_t *current) {
  int i, j;
  scope_node_t *scope;
  ast_node_t *name_node;

  src_line_num = current->src_line_num;

  if(current->node_type == NODE_OPERATOR) {

    switch(current->node.op) {
    case NODE_ROOT:
      /*
       * Go backwards since the global declarations are now the last
       * child in the tree
       */
      i = current->node.num_children - 1;
      if(current->node.children[i]->node.op != NODE_DECL) {
	global_scope = parse_decls(NULL);
	current_scope = global_scope;
      }

      for(i = current->node.num_children - 1; i >= 0; i--)
	if(current->node.children[i]->node.op == NODE_DECL) {

	  debug_printf(1, "Parsing global declarations, ast child %d\n", i);

	  global_scope = parse_decls(current->node.children[i]);

	  current_scope = global_scope;
	  debug_printf(1, "NODE_ROOT: current_scope = %p\n", current_scope);

	} else
	  parse_decl_subtrees(current->node.children[i]);

      debug_printf(1, "NODE_ROOT: global_scope = %p\n", global_scope);

      break;

    case NODE_BLOCK:
      debug_printf(1, "NODE_BLOCK: current_scope = %p\n", current_scope);

      current->node.scope_tag = -1;

      for(i = 0; i < current->node.num_children; i++) {
	if(current->node.children[i]->node.op == NODE_DECL) {
	  current->node.scope_tag =
	    add_scope(parse_decls(current->node.children[i]));

	  debug_printf(1, "New scope has %d name records\n",
		       current_scope->num_records);


	}
	else
	  parse_decl_subtrees(current->node.children[i]);

      }

      if(current->node.scope_tag != -1)
	current_scope = current_scope->prev;

      break;


    case NODE_DECL:
      debug_printf(1, "WARNING: NODE_DECL found during decl parse!\n");
      break;

    case NODE_FUNC:
    case NODE_INLINE_FUNC:
      name_node = get_name_node(current);
      current_scope = get_func_arg_scope(name_node->leaf.name);

      if(!current_scope)
	compiler_error(CERROR_ERROR, current->src_line_num,
		       "Cannot find scope for function %s\n",
		       name_node->leaf.name);

      debug_printf(1, "NODE_FUNC: Adding to scope %s\n", current_scope->name);

      for(i = 0; i < current->node.num_children; i++)
	if(current->node.children[i]->node.op == NODE_BLOCK)
	  parse_decl_subtrees(current->node.children[i]);

      current_scope = global_scope;
      break;

    default:
      for(i = 0; i < current->node.num_children; i++)
	parse_decl_subtrees(current->node.children[i]);
      break;
    }
  }
}

/*
 * Depth first parse of the AST
 */
void parse_ast(ast_node_t *ast)
{
    int i;

    src_line_num = ast->src_line_num;

    /* Initialisation */
    current_scope = global_scope;
    init_history_vars();

    /* Parse the AST */
    for(i = 0; i < ast->node.num_children; i++) {

	if(i == 0) {
	    if(cflags.flags & CFLAG_OUTPUT_IC) {
		ic_emit(IC_JMP, -1, 0, 0, IC_LABEL, IC_NONE, IC_NONE);
		entry_point_jump = ic_current_instruction();
	    }
	}


	switch(ast->node.children[i]->node.op) {
	case NODE_DECL:
	    break;

	case NODE_FUNC:
	case NODE_INLINE_FUNC:
	    debug_printf(1, "Parsing function\n");
	    parse_func(ast->node.children[i]);
	    break;

	default:
	    compiler_error(CERROR_ERROR, CERROR_NO_LINE, "AST is borken\n");
	    break;
	}
    }
}
