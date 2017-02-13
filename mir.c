/*
 * mir.c
 * Ryan Mallon (2006)
 *
 * Medium-level Intermediate Representation
 *
 * Based on the MIR specification from Advanced Compiler Design and
 * Implementation by Steven S. Muchnick
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtable.h"
#include "typechk.h"
#include "scope.h"
#include "astparse.h"
#include "mir.h"
#include "icg.h"
#include "regalloc.h"
#include "cerror.h"
#include "cflags.h"

static const char *MIR_OP_STR[] = {
  NULL, "nop", "begin", "end", "receive", "call", "return", "if", "if not",
  "jump", ".", "->", "move", "addr", "stack addr", "heap addr", "load",
  "store", "stack load", "heap load", "reg load", "stack store", "heap store",
  "reg store", "push_arg", "load_string", "+", "-",
  "*", "/", "==", "!=", "<", "<=", ">", ">="
};

static void opstr(FILE *, mir_operand_t *);
static mir_node_t *mir_emit_load(mir_node_t *, mir_operand_t *, int);
static void mir_add_to_tail(mir_instr_t *);

static mir_node_t *list_head, *list_tail;
static int temp_regs = 0;
static int label = 1;

extern scope_node_t *global_scope;
extern int src_line_num;
extern compiler_options_t cflags;

/*
 * Construct a new MIR operand from an AST expression
 */
mir_operand_t *mir_opr(expression_t *expr) {
  mir_operand_t *op = malloc(sizeof(mir_operand_t));

  op->indirect = expr->indirect;
  op->var = NULL;

  switch(expr->op_type) {
  case OP_VAR:
    op->optype = MIR_OP_VAR;
    op->var = expr->var;
    break;
  case OP_REG:
    op->optype = MIR_OP_REG;
    op->val = expr->op_val;
    break;
  case OP_CONST:
    op->optype = MIR_OP_CONST;
    op->val = expr->op_val;
    break;
  default:
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Unknown expression type for MIR operand\n");
    break;
  }

  return op;
}

/*
 * Return a copy of a MIR operand
 */
mir_operand_t *mir_opr_copy(mir_operand_t *src) {
  mir_operand_t *op;

  if(!src)
    return NULL;

  op = malloc(sizeof(mir_operand_t));

  op->optype = src->optype;
  op->var = src->var;
  op->val = src->val;
  op->indirect = src->indirect;

  return op;
}

/*
 * Construct a new MIR operand for a temporary register
 */
mir_operand_t *mir_reg(int temp_reg) {
  mir_operand_t *op = malloc(sizeof(mir_operand_t));

  op->var = NULL;
  op->optype = MIR_OP_REG;
  op->val = temp_reg;
  op->indirect = 0;
  return op;
}

/*
 * Construct a new MIR operand for a spilled register
 */
mir_operand_t *mir_spill_reg(int temp_reg) {
  mir_operand_t *op = mir_reg(temp_reg);

  op->optype = MIR_OP_SPILL_REG;
  return op;
}

/*
 * Construct a new MIR operand for a temporary register
 */
mir_operand_t *mir_var(name_record_t *var) {
  mir_operand_t *op = malloc(sizeof(mir_operand_t));

  op->optype = MIR_OP_VAR;
  op->var = var;

  op->indirect = is_address_type(var->type_info);
  return op;
}

/*
 * Construct a new MIR operand for a constant
 */
mir_operand_t *mir_const(int const_val) {
  mir_operand_t *op = malloc(sizeof(mir_operand_t));

  op->var = NULL;
  op->optype = MIR_OP_CONST;
  op->val = const_val;
  op->indirect = 0;
  return op;
}

/*
 * Add a MIR instruction to the tail of the instruction list
 */
static void mir_add_to_tail(mir_instr_t *instr) {
  if(!list_head || !list_tail) {
    list_head = malloc(sizeof(mir_node_t));
    list_head->next = malloc(sizeof(mir_node_t));
    list_head->next->prev = list_head;
    list_head->prev = NULL;
    list_head->instruction = instr;

    list_head->in_links = 0;
    list_head->next->in_links = 0;

    list_tail = list_head;

  } else {
    list_tail = list_tail->next;

    list_tail->next = malloc(sizeof(mir_node_t));
    list_tail->next->prev = list_tail;

    list_tail->instruction = instr;
  }

  list_tail->src_line_num = src_line_num;
  list_tail->jump = NULL;
  list_tail->next->in_links = 0;
}

/*
 * Get the tail of the list
 */
mir_node_t *mir_list_tail(void) {
  return list_tail;
}

/*
 * Get the head of the list
 */
mir_node_t *mir_list_head(void) {
  return list_head;
}

/*
 * Add the generated MIR instruction after the given instruction
 */
mir_node_t *mir_add_instr(mir_node_t *node, int opcode, mir_operand_t *op0,
			  mir_operand_t *op1, mir_operand_t *op2) {
  mir_node_t *new_node;
  mir_instr_t *instr;

  /*
   * If we are generating an if instruction and the previous instruction is
   * a compare, then merge the two instructions
   */
  if(opcode == MIR_IF) {
    switch(list_tail->instruction->opcode) {
    case MIR_EQL:
    case MIR_NEQ:
    case MIR_LSS:
    case MIR_LEQ:
    case MIR_GTR:
    case MIR_GEQ:
      debug_printf(1, "Merging cmp/if instructions\n");
      node->instruction->operand[2] = mir_const(node->instruction->opcode);
      node->instruction->opcode = MIR_IF;
      return node;
    }
  }

  /* Create the new instruction */
  instr = malloc(sizeof(mir_instr_t));
  instr->label = NULL;

  instr->opcode = opcode;
  instr->operand[0] = op0;
  instr->operand[1] = op1;
  instr->operand[2] = op2;

  instr->num_args = 0;
  instr->args = NULL;


  if(node != list_tail) {
    new_node = malloc(sizeof(mir_node_t));

    /* Insert it into the list */
    new_node->next = node->next;
    new_node->prev = node;

    node->next->prev = new_node;
    node->next = new_node;

    new_node->instruction = instr;
    new_node->jump = NULL;
    new_node->in_links = 0;

    new_node->src_line_num = node->src_line_num;

    return new_node;

  } else {
    mir_add_to_tail(instr);
    return list_tail;
  }
}

/*
 * Remove a MIR instruction from the list
 */
void mir_remove_node(mir_node_t *node) {
  int i;

  if(node == list_head) {
    list_head = node->next;
    list_head->prev = NULL;

  } else if(node == list_tail) {
    list_tail = node->prev;
    list_tail->next = node->next;

  } else {
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }

  if(node->instruction->opcode == MIR_CALL) {
    /* Free arguments */
    int i;

    for(i = 0; i < node->instruction->num_args; i++)
      free(node->instruction->args[i]);
    free(node->instruction->args);
  }

  /* Free operands */
  for(i = 0; i < 3; i++)
    if(node->instruction->operand[i])
      free(node->instruction->operand[i]);

  /* Free label */
  if(node->instruction->label)
    free(node->instruction->label);

  free(node->instruction);
  free(node);
}

/*
 * Emit a MIR instruction
 */
mir_node_t *mir_emit(int opcode, mir_operand_t *op0, mir_operand_t *op1,
		     mir_operand_t *op2) {
  return mir_add_instr(list_tail, opcode, op0, op1, op2);
}

/*
 * Add a function argument to a MIR instruction
 */
void mir_add_arg(mir_node_t **node, mir_operand_t *arg) {
  if((*node)->instruction->num_args == 0)
    (*node)->instruction->args = malloc(sizeof(mir_arg_t *));
  else
    (*node)->instruction->args =
      realloc((*node)->instruction->args, sizeof(mir_arg_t *) *
	      (*node)->instruction->num_args + 1);

  (*node)->instruction->args[(*node)->instruction->num_args++] = arg;

}

/*
 * Label a MIR instruction
 */
void mir_label(mir_node_t **node, char *label) {
  (*node)->instruction->label = malloc(strlen(label) + 1);
  strcpy((*node)->instruction->label, label);
}

/*
 * Set the jump location for a MIR instruction
 */
void mir_set_jump(mir_node_t *src, mir_node_t *dest) {
  src->jump = dest;
  dest->in_links++;
}

/*
 * Move a label from one instruction to another
 */
void mir_move_label(mir_node_t *src, mir_node_t *dest) {
  mir_node_t *current = list_head;

  if(!src->in_links)
    return;

  while(current) {
    if(current->jump == src) {
      mir_set_jump(current, dest);

      debug_printf(1, "Moved label from (%p -> %p) to (%p -> %p)\n",
		   current, src, current, dest);
    }

    current = current->next;
  }
  src->in_links = 0;
}

/*
 * Allocate a temporary register
 */
int mir_alloc_reg(void) {
  return temp_regs++;
}

/*
 * Deallocate all temporary registers
 */
void mir_dealloc_regs(void) {
  temp_regs = 0;
}

/*
 * Return a string representation of a MIR operand
 * FIXME: This is a bit of a dirty hack
 */
char *mir_opstr(mir_operand_t *op, char *reg_prefix) {
  char *str = malloc(16);

  if(!op) {
    sprintf(str, "(null)");
    return str;
  }

  switch(op->optype) {
  case MIR_OP_VAR:
    sprintf(str, "%s", op->var->name);
    break;

  case MIR_OP_CONST:
    sprintf(str, "%d", op->val);
    break;

  case MIR_OP_REG:
    sprintf(str, "%s%d", reg_prefix, op->val);
    break;
  }

  return str;
}



/*
 * Label the nodes in a MIR listing
 */
void mir_label_nodes(void) {
  mir_node_t *current = list_head;


  label = 1;
  while(current) {
    if(current->in_links) {
      /* FIXME: Should really figure out correct size of label */
      current->instruction->label = malloc(8);
      sprintf(current->instruction->label, "L%d", label++);
    }

    current = current->next;
  }
}

/*
 * Strip the final unused instruction
 */
void mir_strip(void) {
  list_tail->next = NULL;
}

/*
 * Move all references to globals to load/store instructions. Global variables
 * are not allocated to registers.
 */
void mir_munge_globals(void) {
  mir_node_t *current = list_head;
  mir_instr_t *instr;
  int i, reg;

  while(current) {
    instr = current->instruction;

    switch(instr->opcode) {
    case MIR_CALL:

      /* Arguments */
      for(i = 0; i < instr->num_args; i++) {
	if(instr->args[i]->optype == MIR_OP_VAR &&
	   !is_address_type(instr->args[i]->var->type_info) &&
	   get_var_scope(instr->args[i]->var) == global_scope) {

	  reg = tr_alloc();

	  mir_add_instr(current->prev, MIR_HEAP_LOAD,
			instr->args[i], NULL, mir_reg(reg));
	  mir_move_label(current, current->prev);

	  instr->args[i] = mir_reg(reg);
	}
      }

      /* Def (if present) */
      if(instr->operand[2] && instr->operand[2]->optype == MIR_OP_VAR &&
	 !is_address_type(instr->operand[2]->var->type_info) &&
	 get_var_scope(instr->operand[2]->var) == global_scope) {
	reg = tr_alloc();
	mir_add_instr(current, MIR_HEAP_STORE,
		      mir_reg(reg), mir_reg(tr_alloc()), instr->operand[2]);
	instr->operand[2] = mir_reg(reg);
      }
      break;


    case MIR_ADDR:
      if(get_var_scope(instr->operand[0]->var) == global_scope)
	instr->opcode = MIR_HEAP_ADDR;


      break;

    default:
      /* Var use */
      if(instr->opcode != MIR_HEAP_LOAD && instr->opcode != MIR_HEAP_STORE) {
	for(i = 0; i < 2; i++)
	  if(instr->operand[i] && instr->operand[i]->optype == MIR_OP_VAR &&
	     get_var_scope(instr->operand[i]->var) == global_scope &&
	     instr->operand[i]->var->type_info->decl_type != TYPE_FUNCTION) {
	    reg = tr_alloc();

	    if(is_array_type(instr->operand[i]->var->type_info) ||
	       is_address_type(instr->operand[i]->var->type_info)) {
	      mir_add_instr(current->prev,
			    MIR_HEAP_ADDR, instr->operand[i],
			    NULL, mir_reg(reg));
	      mir_move_label(current, current->prev);

	    } else {
	      mir_add_instr(current->prev,
			    MIR_HEAP_LOAD, instr->operand[i],
			    NULL, mir_reg(reg));
	      mir_move_label(current, current->prev);
	    }

	    instr->operand[i] = mir_reg(reg);
	  }
      }

      if(instr->opcode != MIR_HEAP_LOAD && instr->opcode != MIR_HEAP_STORE) {
	/* Var def */
	if(instr->operand[2] && instr->operand[2]->optype == MIR_OP_VAR &&
	   get_var_scope(instr->operand[i]->var) == global_scope &&
	   instr->operand[i]->var->type_info->decl_type != TYPE_FUNCTION) {
	  reg = tr_alloc();

	  mir_add_instr(current, MIR_HEAP_STORE,
			mir_reg(reg), mir_reg(tr_alloc()), instr->operand[2]);
	  instr->operand[2] = mir_reg(reg);

	}
      }

      break;
    }

    current = current->next;
  }
}

/*
 * Munge the MIR instructions to fix up some things that aren't easy to do
 * during the actual code generation
 */
void mir_munge(void) {
  mir_node_t *current = list_head;
  mir_instr_t *instr;
  int reg, offset;

  while(current) {
    instr = current->instruction;

    switch(instr->opcode) {
    case MIR_RECEIVE:
      instr->operand[2]->indirect = 0;
      break;

    case MIR_MOVE:
      if(instr->operand[2]->indirect &&
	 instr->operand[0]->optype == MIR_OP_CONST) {
	/* Can't move constant values directly to a pointer */
	reg = tr_alloc();

	mir_add_instr(current->prev, MIR_MOVE,
		      instr->operand[0], NULL, mir_reg(reg));
	mir_move_label(current, current->prev);

	instr->operand[0] = mir_reg(reg);
      }

      if(instr->operand[0]->indirect && instr->operand[2]->indirect) {
	/* Can't move one pointer directly to another */
	reg = tr_alloc();

	//instr->operand[0]->indirect = 0;
	mir_add_instr(current->prev, MIR_MOVE,
		      instr->operand[0], NULL, mir_reg(reg));
	mir_move_label(current, current->prev);

	instr->operand[0] = mir_reg(reg);

      }

      break;

    case MIR_ADD:
    case MIR_SUB:
      if(instr->operand[0]->optype == MIR_OP_VAR &&
         ((is_address_type(instr->operand[0]->var->type_info) &&
           instr->operand[0]->indirect) ||
          is_array_type(instr->operand[0]->var->type_info))) {

	/* Move the address for pointer arithmetic to a separate instruction */
	reg = tr_alloc();

	mir_add_instr(current->prev, MIR_ADDR,
		      mir_opr_copy(instr->operand[0]), NULL, mir_reg(reg));
	mir_move_label(current, current->prev);

	free(instr->operand[0]);
	instr->operand[0] = mir_reg(reg);

      }
      break;

    }
    current = current->next;
  }
}

/*
 * Convert pointer instructions to load/stores in MIR
 */
void mir_munge_pointers(void) {
  mir_node_t *current = list_head;
  mir_instr_t *instr;
  mir_operand_t *dest;
  int i, reg, offset;

  while(current) {
    instr = current->instruction;

    switch(instr->opcode) {

    case MIR_STACK_LOAD:
    case MIR_HEAP_LOAD:
    case MIR_REG_LOAD:
    case MIR_STACK_STORE:
    case MIR_HEAP_STORE:
    case MIR_REG_STORE:
    case MIR_STACK_ADDR:
    case MIR_HEAP_ADDR:
      /* Do nothing */
      break;

    case MIR_CALL:
      instr->operand[0]->indirect = 0;

      /* Find dereferenced args */
      for(i = 0; i < instr->num_args; i++) {
	if(instr->args[i]->indirect) {
	  reg = tr_alloc();


	  mir_emit_load(current->prev, instr->args[i], reg);
	  mir_move_label(current, current->prev);
	  free(instr->args[i]);

	  instr->args[i] = mir_reg(reg);
	}
      }

      break;

    case MIR_ADDR:
      if(instr->operand[0]->indirect) {
	instr->operand[0]->indirect = 0;
	instr->opcode = MIR_REG_LOAD;

      } else {
	instr->opcode = MIR_STACK_ADDR;

	offset = instr->operand[0]->var->offset;
	free(instr->operand[0]);
	instr->operand[0] = mir_const(offset);
      }

      break;

    case MIR_MOVE:
      if(instr->operand[0]->indirect) {
	/* Src is a pointer */
	reg = tr_alloc();

	instr->operand[0]->indirect = 0;
	mir_add_instr(current->prev, MIR_REG_LOAD,
		      instr->operand[0], NULL, mir_reg(reg));
	mir_move_label(current, current->prev);

	instr->operand[0] = mir_reg(reg);

      } else if(instr->operand[2]->indirect) {
	/* Dest is a pointer */
	instr->operand[2]->indirect = 0;
	instr->opcode = MIR_REG_STORE;
      }

      break;

    default:
      /* Src operands */
      for(i = 0; i < 2; i++) {
	if(instr->operand[i] && instr->operand[i]->indirect) {
	  reg = tr_alloc();
	  mir_emit_load(current->prev, instr->operand[i], reg);
	  mir_move_label(current, current->prev);
	  free(instr->operand[i]);
	  instr->operand[i] = mir_reg(reg);
	}
      }

      /* Dest operands */
      if(instr->operand[2] && instr->operand[2]->indirect) {
	/* Dest is a pointer */
	reg = tr_alloc();

	instr->operand[2]->indirect = 0;
	mir_add_instr(current, MIR_REG_STORE, mir_reg(reg), NULL,
		      mir_opr_copy(instr->operand[2]));

	free(instr->operand[2]);
	instr->operand[2] = mir_reg(reg);
      }
    }

    current = current->next;
  }
}

/*
 * Generate a load instruction
 */
static mir_node_t *mir_emit_load(mir_node_t *node, mir_operand_t *src,
				 int dest) {
  mir_node_t *load;

  if(src->var) {
    if(src->indirect) {

      load = mir_add_instr(node, MIR_REG_LOAD, mir_opr_copy(src), NULL,
			   mir_reg(dest));
      load->instruction->operand[0]->indirect = 0;

      return load;
    }

    if(get_var_scope(src->var) == global_scope)
      return mir_add_instr(node, MIR_HEAP_LOAD, mir_opr_copy(src), NULL,
			   mir_reg(dest));
    else
      return mir_add_instr(node, MIR_STACK_LOAD, mir_const(src->var->offset),
			   NULL, mir_reg(dest));

  }


  load = mir_add_instr(node, MIR_REG_LOAD, mir_opr_copy(src), NULL,
		       mir_reg(dest));

  if(src->indirect)
    load->instruction->operand[0]->indirect = 0;

  return load;

}

/*
 * Ensure that arguments to call instructions that will be passed on the
 * stack are loaded into registers.
 */
void mir_args_to_regs(int first_stack_arg) {
  mir_node_t *current = list_head;
  mir_instr_t *instr;
  int i, reg;

  while(current) {
    instr = current->instruction;

    if(instr->opcode == MIR_CALL && instr->num_args > first_stack_arg) {
      for(i = first_stack_arg; i < instr->num_args; i++) {

	if(instr->args[i]->optype == MIR_OP_CONST) {
	  reg = tr_alloc();
	  mir_add_instr(current->prev, MIR_MOVE, instr->args[i], NULL,
			mir_reg(reg));
	  instr->args[i] = mir_reg(reg);
	}
      }
    }
    current = current->next;
  }
}

/*
 * Ensure that constant operands are always the second operand
 */
void mir_rhs_constants(void) {
  mir_node_t *current = list_head;
  mir_instr_t *instr;
  mir_operand_t *temp;

  while(current) {
    instr = current->instruction;

    switch(instr->opcode) {
    case MIR_ADD:
      if(instr->operand[0]->optype == MIR_OP_CONST) {
	temp = instr->operand[0];
	instr->operand[0] = instr->operand[1];
	instr->operand[1] = temp;
      }
      break;

    case MIR_SUB:
      if(instr->operand[0]->optype == MIR_OP_CONST) {
	int reg = tr_alloc();

	mir_add_instr(current->prev, MIR_MOVE,
		      instr->operand[0], NULL, mir_reg(reg));
	mir_move_label(current, current->prev);
	instr->operand[0] = mir_reg(reg);
      }
      break;

    }

    current = current->next;
  }
}


/*
 * Convert arithmetic operations involing two constants into move instructions
 * FIXME: This is currently used largely to avoid generating multiply
 * instructions that take two constants when doing array indicies. A more
 * thourogh constant folding optomisation would be better.
 */
void mir_fold_constants(void) {
  mir_node_t *current = list_head;
  mir_instr_t *instr;

  while(current) {
    instr = current->instruction;

    switch(instr->opcode) {
    case MIR_ADD:
      if(instr->operand[0]->optype == MIR_OP_CONST &&
	 instr->operand[1]->optype == MIR_OP_CONST) {
	instr->opcode = MIR_MOVE;
	instr->operand[0]->val =
	  instr->operand[0]->val + instr->operand[1]->val;

	free(instr->operand[1]);
	instr->operand[1] = NULL;
	break;
      }

    case MIR_MUL:
      if(instr->operand[0]->optype == MIR_OP_CONST &&
	 instr->operand[1]->optype == MIR_OP_CONST) {
	instr->opcode = MIR_MOVE;
	instr->operand[0]->val =
	  instr->operand[0]->val * instr->operand[1]->val;

	free(instr->operand[1]);
	instr->operand[1] = NULL;
	break;
      }

    }

    current = current->next;
  }
}



/*
 * Pointer analysis, sorta.
 * Must be done after mir_munge_pointers().
 * FIXME: Currently generates incorrect code for may-alias globals.
 */
void mir_spill_may_aliases(void) {
  mir_node_t *current = list_head;
  mir_instr_t *instr;
  int i, reg;

  while(current) {
    instr = current->instruction;

    switch(instr->opcode) {
    case MIR_ADDR:
    case MIR_STACK_ADDR:
    case MIR_HEAP_ADDR:

    case MIR_LOAD:
    case MIR_STACK_LOAD:
    case MIR_HEAP_LOAD:

      if(instr->operand[2] && instr->operand[2]->var &&
	 instr->operand[2]->var->may_alias)
	if(get_var_scope(instr->operand[2]->var) == global_scope)
	  mir_add_instr(current, MIR_HEAP_STORE,
			mir_opr_copy(instr->operand[2]),
			NULL, mir_opr_copy(instr->operand[2]));
	else
	  mir_add_instr(current, MIR_STACK_STORE,
			mir_opr_copy(instr->operand[2]),
			NULL, mir_const(instr->operand[2]->var->offset));

      break;


    case MIR_STORE:
    case MIR_STACK_STORE:
    case MIR_HEAP_STORE:
      /* Do nothing */
      break;

    case MIR_CALL:
      for(i = 0; i < instr->num_args; i++) {
	if(instr->args[i]->var && instr->args[i]->var->may_alias) {
	  reg = tr_alloc();

	  if(get_var_scope(instr->args[i]->var) == global_scope) {
	    mir_add_instr(current->prev, MIR_HEAP_LOAD,
			  instr->args[i], NULL, mir_reg(reg));
	    mir_move_label(current, current->prev);

	    instr->args[i] = mir_reg(reg);

	  } else {
	    mir_add_instr(current->prev, MIR_STACK_LOAD,
			  mir_const(instr->args[i]->var->offset), NULL,
			  mir_reg(reg));
	    mir_move_label(current, current->prev);

	    free(instr->args[i]);
	    instr->args[i] = mir_reg(reg);

	  }

	}
      }

      break;



    default:
      /* Var uses */
      for(i = 0; i < 2; i++) {
	if(instr->operand[i] && instr->operand[i]->var &&
	   instr->operand[i]->var->may_alias) {
	  reg = tr_alloc();

	  debug_printf(1, "Generating code for may-alias variable %s, op = %s\n",
		       instr->operand[i]->var->name, MIR_OP_STR[instr->opcode]);

	  mir_emit_load(current->prev, instr->operand[i], reg);
	  mir_move_label(current, current->prev);
	  free(instr->operand[i]);
	  instr->operand[i] = mir_reg(reg);

	}
      }

      /* Var defs */
      if(instr->operand[2] && instr->operand[2]->var &&
	 instr->operand[2]->var->may_alias) {

	debug_printf(1, "Generating store for may-alias %s\n",
		     instr->operand[2]->var->name);

	if(get_var_scope(instr->operand[2]->var) == global_scope)
	  mir_add_instr(current, MIR_HEAP_STORE,
			mir_opr_copy(instr->operand[2]), mir_reg(tr_alloc()),
			mir_opr_copy(instr->operand[2]));
	else
	  mir_add_instr(current, MIR_STACK_STORE,
			mir_opr_copy(instr->operand[2]), NULL,
			mir_const(instr->operand[i]->var->offset));

	//current = current->next;
      }
      break;
    }

    current = current->next;
  }
}



/*
 * Convert MIR instructions to LIR with symbolic registers
 */
void mir_to_sym_lir(ig_graph_t *ig_graph) {
  mir_node_t *current = list_head;
  mir_instr_t *instr;
  int i;

  while(current) {
    instr = current->instruction;

    /*
     * Convert operands to their respective temporary registers. Dereferenced
     * pointers and address variables (including arrays) are left in the code
     * since they need to be replaced with either load/store instructions or
     * absolute addresses later on.
     */

    switch(instr->opcode) {
    case MIR_CALL:
      /* Call instruction. Set args to symbolic registers */
      for(i = 0; i < instr->num_args; i++)
	if(instr->args[i]->optype != MIR_OP_CONST &&
	   !(instr->args[i]->optype == MIR_OP_VAR &&
	     instr->args[i]->indirect)) {
	  instr->args[i]->val = ig_sym_reg(ig_graph, instr->args[i]);
	  instr->args[i]->optype = MIR_OP_REG;
	}

      /* Dest */
      if(instr->operand[2] &&
	 instr->operand[2]->optype != MIR_OP_CONST) {
	instr->operand[2]->val = ig_sym_reg(ig_graph, instr->operand[2]);

	instr->operand[2]->optype = MIR_OP_REG;
      }


      break;

    case MIR_HEAP_ADDR:
      if(instr->operand[0]->optype == MIR_OP_REG)
	instr->operand[0]->val = ig_sym_reg(ig_graph, instr->operand[0]);
      /* Fall Through */

    case MIR_STACK_ADDR:
      if(instr->operand[2]->optype != MIR_OP_CONST) {
	instr->operand[2]->val = ig_sym_reg(ig_graph, instr->operand[2]);
	instr->operand[2]->optype = MIR_OP_REG;
      }
      break;

    default:
      for(i = 0; i < 3; i++) {

        if(instr->opcode == MIR_HEAP_LOAD && i == 0 ||
           instr->opcode == MIR_HEAP_STORE && i == 2)
          continue;

        if(instr->operand[i] &&
	   instr->operand[i]->optype != MIR_OP_CONST) {
	  instr->operand[i]->val = ig_sym_reg(ig_graph, instr->operand[i]);
	  instr->operand[i]->optype = MIR_OP_REG;
	}
      }

      break;
    }

    current = current->next;
  }
}


/*
 * Print out a single MIR instruction
 */
void mir_print_instr(FILE *fd, int tabbed, mir_node_t *node) {
  mir_instr_t *instr = node->instruction;
  int i;

  if(instr->label)
    fprintf(fd, "%s:", instr->label);
  if(instr->opcode != MIR_BEGIN && instr->opcode != MIR_END && tabbed)
    fprintf(fd, "\t");

  switch(instr->opcode) {
  case MIR_NOP:
    fprintf(fd, "nop");
    break;

  case MIR_LABEL:
    break;

  case MIR_BEGIN:
  case MIR_END:
    if(tabbed)
      fprintf(fd, "    %s", MIR_OP_STR[instr->opcode]);
    else
      fprintf(fd, "%s", MIR_OP_STR[instr->opcode]);

    if(instr->opcode == MIR_END && tabbed)
      fprintf(fd, "\n");
    break;

  case MIR_STACK_LOAD:
    fprintf(fd, "load [%%sp + %d], ", instr->operand[0]->val);
    opstr(fd, instr->operand[2]);
    break;

  case MIR_LOAD:
  case MIR_HEAP_LOAD:
  case MIR_REG_LOAD:
    fprintf(fd, "load [");
    opstr(fd, instr->operand[0]);
    fprintf(fd, "], ");
    opstr(fd, instr->operand[2]);
    break;

  case MIR_STACK_STORE:
    fprintf(fd, "store ");
    opstr(fd, instr->operand[0]);
    fprintf(fd, ", [%%sp + %d]", instr->operand[2]->val);
    break;

  case MIR_STORE:
  case MIR_HEAP_STORE:
  case MIR_REG_STORE:
    fprintf(fd, "store ");
    opstr(fd, instr->operand[0]);
    fprintf(fd, ", [");
    opstr(fd, instr->operand[2]);
    fprintf(fd, "]");

    if(instr->opcode == MIR_HEAP_STORE)
      fprintf(fd, ", temp reg = %d",
	      instr->operand[1] ? instr->operand[1]->val : -1);

    break;

  case MIR_PUSH_ARG:
    fprintf(fd, "push_arg [%%sp + %d]", instr->operand[2]->val);
    break;

  case MIR_MEMBER:
    fprintf(fd, "member ");
    opstr(fd, instr->operand[0]);
    fprintf(fd, ".");
    opstr(fd, instr->operand[1]);
    break;

  case MIR_MOVE:
    opstr(fd, instr->operand[2]);
    fprintf(fd, " <- ");
    opstr(fd, instr->operand[0]);
    break;

  case MIR_ADDR:
    opstr(fd, instr->operand[2]);
    fprintf(fd, " <- addr(");
    opstr(fd, instr->operand[0]);
    fprintf(fd, ")");
    break;

  case MIR_HEAP_ADDR:
    opstr(fd, instr->operand[2]);
    fprintf(fd, " <- addr(");
    opstr(fd, instr->operand[0]);
    fprintf(fd, ")");
    break;

  case MIR_STACK_ADDR:
    opstr(fd, instr->operand[2]);
    fprintf(fd, " <- addr(%%sp + %d)", instr->operand[0]->val);
    break;

  case MIR_JUMP:
    fprintf(fd, "jump %s", node->jump->instruction->label);
    break;

  case MIR_RECEIVE:
    fprintf(fd, "receive (");
    opstr(fd, instr->operand[2]);
    fprintf(fd, ")");
    break;

  case MIR_CALL:
    if(instr->operand[2]) {
      opstr(fd, instr->operand[2]);
      fprintf(fd, " <- ");
    }

    fprintf(fd, "call ");
    opstr(fd, instr->operand[0]);
    fprintf(fd, "(");

    for(i = 0; i < instr->num_args; i++) {
      opstr(fd, instr->args[i]);
      fprintf(fd, "%s", i == instr->num_args - 1 ? "" : ", ");
    }
    fprintf(fd, ")");
    break;

  case MIR_RETURN:
    fprintf(fd, "return ");
    opstr(fd, instr->operand[0]);
    break;

  case MIR_IF:
  case MIR_IF_NOT:
    fprintf(fd, "%s ", MIR_OP_STR[instr->opcode]);
    opstr(fd, instr->operand[0]);

    if(instr->operand[2]) {
      fprintf(fd, " %s ", MIR_OP_STR[instr->operand[2]->val]);
      opstr(fd, instr->operand[1]);
    }

    fprintf(fd, " goto %s", node->jump->instruction ?
	    node->jump->instruction->label : "???");
    break;

  default:
    opstr(fd, instr->operand[2]);
    fprintf(fd, " <- ");
    opstr(fd, instr->operand[0]);
    fprintf(fd, " %s ", MIR_OP_STR[instr->opcode]);
    opstr(fd, instr->operand[1]);
    break;

  }

  if(tabbed)
    fprintf(fd, "\n");

}

/*
 * Print full struct names
 */
static void op_print_parent(FILE *fd, name_record_t *var) {
  if(var->struct_parent)
    op_print_parent(fd, var->struct_parent);

  fprintf(fd, "%s.", var->name);
}

/*
 * Print out a MIR operand
 */
static void opstr(FILE *fd, mir_operand_t *op) {
  if(!op) {
    fprintf(fd, "(nil)");
    return;
  }

  if(op->indirect)
    fprintf(fd, "*");

  if(op->optype == MIR_OP_VAR) {
    if(op->var->struct_parent)
      op_print_parent(fd, op->var->struct_parent);

    fprintf(fd, "%s", op->var->name);

  } else if(op->optype == MIR_OP_CONST)
    fprintf(fd, "%d", op->val);
  else
    fprintf(fd, "%%t%d", op->val);
}

/*
 * Print out MIR instructions nicely
 */
void mir_print(char *basename, char *ext) {
  FILE *fd;
  char *filename;
  mir_node_t *current = list_head;
  mir_instr_t *instr;
  int i;

  if(!(cflags.flags & CFLAG_DEBUG_OUTPUT))
    return;

  filename = malloc(strlen(basename) + strlen(ext) + 2);
  strcpy(filename, basename);
  strcat(filename, ".");
  strcat(filename, ext);

  fd = fopen(filename, "w");

  while(current && current->instruction) {
    mir_print_instr(fd, 1, current);
    current = current->next;
  }

  fclose(fd);
}
