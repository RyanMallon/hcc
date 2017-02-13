/*
 * history.c
 * Ryan Mallon (2005)
 *
 * History variable support routines for compiler. The atomic construct
 * is also handled here.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "typechk.h"
#include "symtable.h"
#include "typechk.h"
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

static void history_init_array(name_record_t *);
static void history_write_pending_clist(void);

static void history_store_scalar(expression_t *, expression_t *);
static int history_load_scalar(name_record_t *, expression_t *);
static void history_init_scalar(name_record_t *);

extern scope_node_t *global_scope, *current_scope;
extern compiler_options_t cflags;

static int atomic_counter = 0;
static int max_clist_size = 0;
static int num_clist_pending = 0;
static name_record_t **clist_pending = NULL;

static int num_atomic_modifieds = 0;
static name_record_t **atomic_modified;

#define CLIST_MAGIC 65535

/*
 * Enter an atomic block
 */
void history_enter_atomic(void) {
  int i;

  if(atomic_counter)
    compiler_error(CERROR_WARN, CERROR_NO_LINE,
		   "Nested atomic blocks have no effect\n");

  atomic_counter++;

  /* Clear the atomic modified list */
  for(i = 0; i < num_atomic_modifieds; i++)
    atomic_modified[i] = NULL;

  num_atomic_modifieds = 0;
}

/*
 * Exit an atomic block
 */
void history_exit_atomic(void) {
  int i;

  // history_write_pending_clist();

  atomic_counter--;

  /* Force updates on all modified history variables */
  for(i = 0; i < num_atomic_modifieds; i++) {
    expression_t *dest, *src;

    /*
     * Create expression_t structs for the dest (the history var) and the
     * source (its copy variable) and then call the normal history store
     * function
     *
     */
    dest = malloc(sizeof(expression_t));
    src = malloc(sizeof(expression_t));

    src->op_type = OP_VAR;
    src->var = history_copy_entry(atomic_modified[i]);

    dest->op_type = OP_VAR;
    dest->var = atomic_modified[i];

    history_store(dest, src);

    free(src);
    free(dest);
  }
}

/*
 * Test to see if we are in an atomic block
 */
int history_in_atomic(void) {
  return atomic_counter;
}

/*
 * Mark a history variable as modified by an atomic block
 */
static void mark_atomic_modified(name_record_t *var) {
  int i;

  if(!atomic_counter)
    return;

  for(i = 0; i < num_atomic_modifieds; i++)
    if(atomic_modified[i] == var)
      return;

  atomic_modified = realloc(atomic_modified, sizeof(name_record_t *) *
                            (num_atomic_modifieds + 1));

  atomic_modified[num_atomic_modifieds++] = var;
}

/*
 * Get the var entry for a history pointer
 */
name_record_t *history_ptr_entry(name_record_t *hvar) {
  char *name;
  name_record_t *ptr;

  name = calloc(strlen(hvar->name) + 6, sizeof(char));
  name[0] = '.';
  strcat(name, hvar->name);
  strcat(name, "_ptr");

  ptr = get_var_entry(name);
  free(name);
  return ptr;
}

/*
 * Get the current value variable for the given history variable
 */
name_record_t *history_current_entry(name_record_t *hvar) {
  char *name;
  name_record_t *current;

  name = calloc(strlen(hvar->name) + 6, sizeof(char));
  name[0] = '.';
  strcat(name, hvar->name);
  strcat(name, "_cur");

  current = get_var_entry(name);
  free(name);
  return current;
}

/*
 * Get current copy (varphi) entry for the given history variable
 */
name_record_t *history_copy_entry(name_record_t *hvar) {
  char *name;
  name_record_t *copy;

  name = calloc(strlen(hvar->name) + 6, sizeof(char));
  name[0] = '.';
  strcat(name, hvar->name);
  strcat(name, "_cpy");

  copy = get_var_entry(name);
  free(name);
  return copy;
}

/*
 * Get the clist entry for the given history variable
 */
name_record_t *history_clist_entry(name_record_t *hvar) {
  char *name;
  name_record_t *current;

  name = calloc(strlen(hvar->name) + 8, sizeof(char));
  name[0] = '.';

  strcat(name, hvar->name);
  strcat(name, "_clist");

  current = get_var_entry(name);
  free(name);
  return current;
}

/*
 * Get the clist pointer entry for the given history variable
 */
name_record_t *history_clist_ptr_entry(name_record_t *hvar) {
  char *name;
  name_record_t *current;

  name = calloc(strlen(hvar->name) + 12, sizeof(char));
  name[0] = '.';

  strcat(name, hvar->name);
  strcat(name, "_clist_ptr");

  current = get_var_entry(name);
  free(name);
  return current;
}

/*
 * Take the address of a history variable
 */
int history_address(expression_t *src) {
  int temp_reg;
  temp_reg = tr_alloc();

  if(is_array_type(src->var->type_info)) {

  } else {
    /* New version returns the base address for both cyclic and flat */
    mir_emit(MIR_ADDR, mir_opr(src), NULL, mir_reg(temp_reg));

#if 0
    /* Scalar */
    if(cflags.flags & CFLAG_HISTORY_SIMPLE_STORE &&
       get_history_depth(src->var->type_info) <= cflags.history_simple_depth)
      mir_emit(MIR_ADDR, mir_opr(src), NULL, mir_reg(temp_reg));
    else {
      mir_emit(MIR_ADDR, mir_var(src->var), NULL, mir_reg(temp_reg));
      src->var->may_alias = 1;
    }
#endif
  }

  return temp_reg;
}

/*
 * Add a pending write to the change list for an array-wise
 * history array. When the current atomic block exits, each variable
 * in the pending list has the magic number written into its change list
 */
void history_add_clist_write(name_record_t *var) {
  int i;

  /* Make sure we don't add duplicates */
  if(clist_pending)
    for(i = 0; i < num_clist_pending; i++)
      if(clist_pending[i] == var)
	return;

  if(!clist_pending)
    clist_pending = malloc(sizeof(name_record_t *) * max_clist_size);

  clist_pending[num_clist_pending++] = var;
}

/*
 * Write and clear the pending change list writes
 */
static void history_write_pending_clist(void) {
  int i, reg, offset, ptr_addr;

  reg = tr_alloc();

  for(i = 0; i < num_clist_pending; i++) {
    /* Call hist_aw_clist(cptr, index, esize, depth, value) */
    offset = IC_STACK_FRAME_HEADER_BYTES + IC_SIZEOF_POINTER;

    ptr_addr = clist_pending[i]->offset;

    /* Value (base of array to copy) */
    ic_emit(IC_LIT, ptr_addr, reg, 0, IC_CONST, IC_REG, IC_NONE);
    ic_emit(IC_STO, reg, IC_SP, -offset, IC_REG, IC_REG_BASE, IC_CONST_OFFS);

    /* depth */
    offset += 4;
    ic_emit(IC_LIT, get_history_depth(clist_pending[i]->type_info), reg, 0,
	    IC_CONST, IC_REG, IC_NONE);
    ic_emit(IC_STO, reg, IC_SP, -offset, IC_REG, IC_REG_BASE, IC_CONST_OFFS);

    /* esize */
    offset += 4;
    ic_emit(IC_LIT,
	    sizeof_primitive(primitive_type(clist_pending[i]->type_info)),
	    reg, 0, IC_CONST, IC_REG, IC_NONE);
    ic_emit(IC_STO, reg, IC_SP, -offset, IC_REG, IC_REG_BASE, IC_CONST_OFFS);

    /* index (CLIST_MAGIC) */
    offset += 4;
    ic_emit(IC_LIT, CLIST_MAGIC, reg, 0, IC_CONST, IC_REG, IC_NONE);
    ic_emit(IC_STO, reg, IC_SP, -offset, IC_REG, IC_REG_BASE, IC_CONST_OFFS);

    /* cptr */
    offset += 4;
    ptr_addr = clist_pending[i]->offset + IC_SIZEOF_POINTER +
      (sizeof_type(clist_pending[i]->type_info) *
       (get_history_depth(clist_pending[i]->type_info) + 2));
    ic_emit(IC_LIT, ptr_addr, reg, 0, IC_CONST, IC_REG, IC_NONE);
    ic_emit(IC_STO, reg, IC_SP, -offset, IC_REG, IC_REG_BASE, IC_CONST_OFFS);

    ic_emit_str(IC_CAL, 0, 0, "hist_aw_clist",
		IC_LABEL, IC_STRING_OP, IC_NONE);
  }

  tr_dealloc();

  num_clist_pending = 0;
}

/*
 * Add the internal variables to the name table for the given history variable.
 * The history variable itself is renamed as the cycle and new entry is
 * created for the current value.
 */
void history_add_internal_names(name_record_t *hvar, scope_node_t *scope) {
  char *ptr_name, *cur_name, *cpy_name;
  name_record_t *cur, *ptr, *cpy;

  /* Pointer name */
  ptr_name = calloc(strlen(hvar->name) + 6, sizeof(char));
  ptr_name[0] = '.';
  strcat(ptr_name, hvar->name);
  strcat(ptr_name, "_ptr");

  /* Current value name */
  cur_name = calloc(strlen(hvar->name) + 6, sizeof(char));
  cur_name[0] = '.';
  strcat(cur_name, hvar->name);
  strcat(cur_name, "_cur");

  /* Copy value (for atomic blocks) */
  cpy_name = calloc(strlen(hvar->name) + 6, sizeof(char));
  cpy_name[0] = '.';
  strcat(cpy_name, hvar->name);
  strcat(cpy_name, "_cpy");

  if(is_array_type(hvar->type_info)) {
    if(history_array_type(hvar->type_info) == HISTORY_INDEX_WISE) {
      /* Index-wise array */
      ptr = enter(ptr_name);
      ptr->may_alias = 1;
      ptr->type_info =
	create_ptr_to_type(hvar->type_info->decl_type,
			   primitive_type(hvar->type_info), 2);

      cur = enter(cur_name);
      cur->may_alias = 1;
      cur->type_info =
	create_array_type(hvar->type_info->decl_type,
			  primitive_type(hvar->type_info),
			  get_dimension_size(hvar->type_info, 1));

      add_entry(scope, ptr);
      add_entry(scope, cur);

    } else {
      /* Array-wise */
      if(cflags.flags & CFLAG_HISTORY_AW_ORDER_D) {
	/* O(d) algorithm */
	char *clist_name, *clist_ptr_name;
	name_record_t *clist, *clist_ptr;

	/* Clist name */
	clist_name = calloc(strlen(hvar->name) + 8, sizeof(char));
	clist_name[0] = '.';
	strcat(clist_name, hvar->name);
	strcat(clist_name, "_clist");

	/* Clist pointer name */
	clist_ptr_name = calloc(strlen(hvar->name) + 12, sizeof(char));
	clist_ptr_name[0] = '.';
	strcat(clist_ptr_name, hvar->name);
	strcat(clist_ptr_name, "_clist_ptr");

	ptr = enter(ptr_name);
	ptr->may_alias = 1;
	ptr->type_info =
	  create_ptr_to_type(hvar->type_info->decl_type,
			     primitive_type(hvar->type_info), 2);

	cur = enter(cur_name);
	cur->may_alias = 1;
	cur->type_info =
	  create_array_type(hvar->type_info->decl_type,
			    primitive_type(hvar->type_info),
			    get_dimension_size(hvar->type_info, 1));

	clist = enter(clist_name);
	clist->may_alias = 1;
	clist->type_info =
	  create_array_type(hvar->type_info->decl_type, TYPE_INTEGER,
			    get_history_depth(hvar->type_info) * 2);

	clist_ptr = enter(clist_ptr_name);
	clist_ptr->may_alias = 1;
	clist_ptr->type_info =
	  create_ptr_to_type(hvar->type_info->decl_type,
			     primitive_type(hvar->type_info), 1);

	add_entry(scope, ptr);
	add_entry(scope, cur);
	add_entry(scope, clist);
	add_entry(scope, clist_ptr);

	free(clist_name);
	free(clist_ptr_name);

      } else {
	/* O(n) algorithm */

	ptr = enter(ptr_name);
	ptr->may_alias = 1;
	ptr->type_info =
	  create_ptr_to_type(hvar->type_info->decl_type,
			     primitive_type(hvar->type_info), 1);

	cur = enter(cur_name);
	cur->may_alias = 1;
	cur->type_info =
	  create_array_type(hvar->type_info->decl_type,
			    primitive_type(hvar->type_info),
			    get_dimension_size(hvar->type_info, 1));

	add_entry(scope, ptr);
	add_entry(scope, cur);
      }
    }

  } else {

    /* Copy variable is needed for both flat and cyclic storage */
    cpy = enter(cpy_name);
    cpy->type_info =
      create_type_signature(hvar->type_info->decl_type,
			    primitive_type(hvar->type_info), NULL);
    add_entry(scope, cpy);

    if(cflags.flags & CFLAG_HISTORY_SIMPLE_STORE &&
       get_history_depth(hvar->type_info) < cflags.history_simple_depth) {
      /* Scalar: Flat storage */
      scope->size += sizeof_history(hvar->type_info);

    } else {
      /* Scalar: Cyclic storage */
      ptr = enter(ptr_name);
      ptr->may_alias = 1;
      ptr->type_info = create_ptr_to_type(hvar->type_info->decl_type,
					  primitive_type(hvar->type_info), 1);

      cur = enter(cur_name);
      cur->type_info =
	create_type_signature(hvar->type_info->decl_type,
			      primitive_type(hvar->type_info), NULL);

      add_entry(scope, ptr);
      add_entry(scope, cur);
    }
  }

  free(ptr_name);
  free(cur_name);
}

/*
 * Ouput Initialisation calls for any history variables
 * that are declared in the current scope.
 */
void init_history_vars(void) {
  int i, reg, offset;

  /* Global history variables are initialised in the data segment */
  if(current_scope == global_scope)
    return;

  reg = tr_alloc();

  debug_printf(1, "Checking current scope (%d) for history vars\n",
	       current_scope->num_records);

  /*
   * FIXME: Determine which types need a history descriptor initialised
   */
  for(i = 0; i < current_scope->num_records; i++) {
    if(current_scope->name_table[i]->type_info->decl_type == TYPE_VARIABLE &&
       is_history_type(current_scope->name_table[i]->type_info)) {

      if(is_array_type(current_scope->name_table[i]->type_info)) {
	history_init_array(current_scope->name_table[i]);

	/* Work out how many array-wise arrays there are */
	if(history_array_type(current_scope->name_table[i]->type_info) ==
	   HISTORY_ARRAY_WISE)
	  max_clist_size++;

      } else {
	/*
	 * Don't initialise primitive history variables if simple storage
	 * is enabled and the are below the given the depth
	 */
	if(!(cflags.flags & CFLAG_HISTORY_SIMPLE_STORE) &&
	   get_history_depth(current_scope->name_table[i]->type_info) >
	   cflags.history_simple_depth) {

	  history_init_scalar(current_scope->name_table[i]);

	}
      }
    }
  }
  tr_dealloc();
}

/*
 * Initialise a scalr history variable
 */
static void history_init_scalar(name_record_t *hvar) {
  mir_node_t *instr;

  /* Initialise the pointer */
  instr = mir_emit(MIR_ADDR, mir_var(hvar), NULL,
		   mir_var(history_ptr_entry(hvar)));
  instr->instruction->operand[2]->indirect = 0;
}

/*
 * Initialise a history array
 */
static void history_init_array(name_record_t *hvar) {
  int addr_reg, ptr_reg, cpy_reg;
  mir_node_t *call, *node;

  if(history_array_type(hvar->type_info) == HISTORY_INDEX_WISE) {
    /* Index-wise */
    addr_reg = tr_alloc();
    ptr_reg = tr_alloc();

    mir_emit(MIR_ADDR, mir_var(hvar), NULL, mir_reg(addr_reg));
    node = mir_emit(MIR_ADDR, mir_var(history_ptr_entry(hvar)), NULL,
		    mir_reg(ptr_reg));
    node->instruction->operand[0]->indirect = 0;

    call = mir_emit(MIR_CALL, mir_var(get_func_entry("__hist_iw_init__")),
	     NULL, NULL);
    mir_add_arg(&call, mir_reg(addr_reg));
    mir_add_arg(&call, mir_reg(ptr_reg));
    mir_add_arg(&call, mir_const(get_num_elements(hvar->type_info)));

  } else {
    /* Array-wise */
    if(cflags.flags & CFLAG_HISTORY_AW_ORDER_D) {
      /* O(d) algorithm */
      int clist_reg, clist_ptr_reg;

      addr_reg = tr_alloc();
      ptr_reg = tr_alloc();
      clist_reg = tr_alloc();
      clist_ptr_reg = tr_alloc();

      mir_emit(MIR_ADDR, mir_var(hvar), NULL, mir_reg(addr_reg));
      node = mir_emit(MIR_ADDR, mir_var(history_ptr_entry(hvar)), NULL,
		      mir_reg(ptr_reg));
      node->instruction->operand[0]->indirect = 0;

      mir_emit(MIR_ADDR, mir_var(history_clist_entry(hvar)), NULL,
	       mir_reg(clist_reg));
      node = mir_emit(MIR_ADDR, mir_var(history_clist_ptr_entry(hvar)), NULL,
		      mir_reg(clist_ptr_reg));
      node->instruction->operand[0]->indirect = 0;

      call = mir_emit(MIR_CALL, mir_var(get_func_entry("__hist_daw_init__")),
		      NULL, NULL);
      mir_add_arg(&call, mir_reg(addr_reg));
      mir_add_arg(&call, mir_reg(ptr_reg));
      mir_add_arg(&call, mir_reg(clist_reg));
      mir_add_arg(&call, mir_reg(clist_ptr_reg));
      mir_add_arg(&call, mir_const(get_history_depth(hvar->type_info)));

    } else {
      /* O(n) algorithm */
      node = mir_emit(MIR_ADDR, mir_var(hvar), NULL,
		      mir_var(history_ptr_entry(hvar)));
      node->instruction->operand[2]->indirect = 0;
    }

  }
}

/*
 * Store the value src in the history variable dest
 */
void history_store(expression_t *dest, expression_t *src) {

  if(history_in_atomic()) {
    mark_atomic_modified(dest->var);

    /* Assign to the copy variable */
    mir_emit(MIR_MOVE, mir_opr(src), NULL,
	     mir_var(history_copy_entry(dest->var)));

    return;
  }

  if(cflags.flags & CFLAG_HISTORY_SIMPLE_STORE &&
     get_history_depth(dest->var->type_info) < cflags.history_simple_depth) {

    /* Flat storage: __hist_fp_store__(addr, depth, value) */
    mir_node_t *call;
    int addr_reg;

    addr_reg = tr_alloc();

    mir_emit(MIR_ADDR, mir_opr(dest), NULL, mir_reg(addr_reg));
    call = mir_emit(MIR_CALL, mir_var(get_func_entry("__hist_fp_store__")),
		    NULL, NULL);
    mir_add_arg(&call, mir_reg(addr_reg));
    mir_add_arg(&call, mir_const(get_history_depth(dest->var->type_info)));
    mir_add_arg(&call, mir_opr(src));

  } else
    history_store_scalar(dest, src);

}

/*
 * Generate instructions for storing a primitive (scalar) history variable
 */
static void history_store_scalar(expression_t *dest, expression_t *src) {
  mir_node_t *call, *mir;
  name_record_t *func;
  name_record_t *arg;
  int temp_reg, ptr_reg, cur_reg;

  /* Assign to the current value only when in an atomic block */
  if(history_in_atomic()) {
    mir_emit(MIR_MOVE, mir_opr(src), NULL,
	     mir_var(history_current_entry(dest->var)));
    return;
  }

  temp_reg = tr_alloc();
  ptr_reg = tr_alloc();
  cur_reg = tr_alloc();

  func = get_func_entry("__hist_p_store__");

  /* Address of the pointer */
  mir = mir_emit(MIR_ADDR, mir_var(history_ptr_entry(dest->var)), NULL,
		 mir_reg(ptr_reg));
  mir->instruction->operand[0]->indirect = 0;

#if 0
  /* Address of the current value */
  mir = mir_emit(MIR_ADDR, mir_var(history_current_entry(dest->var)),
		 NULL, mir_reg(cur_reg));
  mir->instruction->operand[0]->indirect = 0;
#endif

  /* Address of history variable */
  mir_emit(MIR_ADDR, mir_var(dest->var), NULL, mir_reg(temp_reg));

  /* Call */
  call = mir_emit(MIR_CALL, mir_var(func), NULL, NULL);

  /* Args */
  mir_add_arg(&call, mir_reg(temp_reg));
  mir_add_arg(&call, mir_reg(ptr_reg));
  mir_add_arg(&call, mir_var(history_current_entry(dest->var)));
  mir_add_arg(&call, mir_const(get_history_depth(dest->var->type_info)));
  // mir_add_arg(&call, mir_opr(src));

  /* Assign the new current value */
  mir_emit(MIR_MOVE, mir_opr(src), NULL,
           mir_var(history_current_entry(dest->var)));
}

/*
 * Store the value in reg at the location of the history array var.
 */
void history_array_store(expression_t *dest, expression_t *index,
			 expression_t *src) {
  int addr_reg, ptr_reg, cur_reg;
  mir_node_t *call, *node;
  name_record_t *func;

  addr_reg = tr_alloc();
  ptr_reg = tr_alloc();
  cur_reg = tr_alloc();

  if(history_array_type(dest->var->type_info) == HISTORY_INDEX_WISE) {
    /* Index-wise */
    mir_emit(MIR_ADDR, mir_var(dest->var), NULL, mir_reg(addr_reg));
    mir_emit(MIR_ADDR, mir_var(history_current_entry(dest->var)), NULL,
	     mir_reg(cur_reg));
    node = mir_emit(MIR_ADDR, mir_var(history_ptr_entry(dest->var)), NULL,
		    mir_reg(ptr_reg));
    node->instruction->operand[0]->indirect = 0;

    call = mir_emit(MIR_CALL, mir_var(get_func_entry("__hist_iw_store__")),
		    NULL, NULL);
    mir_add_arg(&call, mir_reg(addr_reg));
    mir_add_arg(&call, mir_reg(ptr_reg));
    mir_add_arg(&call, mir_reg(cur_reg));
    mir_add_arg(&call, mir_opr(index));
    mir_add_arg(&call, mir_const(get_num_elements(dest->var->type_info)));
    mir_add_arg(&call, mir_const(get_history_depth(dest->var->type_info)));
    mir_add_arg(&call, mir_opr(src));

  } else {
    /* Array-wise */
    mir_emit(MIR_ADDR, mir_var(dest->var), NULL, mir_reg(addr_reg));
    mir_emit(MIR_ADDR, mir_var(history_current_entry(dest->var)), NULL,
	     mir_reg(cur_reg));
    node = mir_emit(MIR_ADDR, mir_var(history_ptr_entry(dest->var)), NULL,
		    mir_reg(ptr_reg));
    node->instruction->operand[0]->indirect = 0;

    if(cflags.flags & CFLAG_HISTORY_AW_ORDER_D) {
      /* O(d) storage */
      int clist_reg, clist_ptr_reg;

      clist_reg = tr_alloc();
      clist_ptr_reg = tr_alloc();

      mir_emit(MIR_ADDR, mir_var(history_clist_entry(dest->var)), NULL,
	       mir_reg(clist_reg));
      node = mir_emit(MIR_ADDR, mir_var(history_clist_ptr_entry(dest->var)),
		      NULL, mir_reg(clist_ptr_reg));
      node->instruction->operand[0]->indirect = 0;

      call = mir_emit(MIR_CALL, mir_var(get_func_entry("__hist_daw_store__")),
		      NULL, NULL);
      mir_add_arg(&call, mir_reg(addr_reg));
      mir_add_arg(&call, mir_reg(ptr_reg));
      mir_add_arg(&call, mir_reg(cur_reg));
      mir_add_arg(&call, mir_const(get_history_depth(dest->var->type_info)));
      mir_add_arg(&call, mir_const(get_num_elements(dest->var->type_info)));
      mir_add_arg(&call, mir_reg(clist_reg));
      mir_add_arg(&call, mir_reg(clist_ptr_reg));
      mir_add_arg(&call, mir_opr(index));
      mir_add_arg(&call, mir_opr(src));

    } else {
      /* O(n) storage */
      call = mir_emit(MIR_CALL, mir_var(get_func_entry("__hist_naw_store__")),
		      NULL, NULL);
      mir_add_arg(&call, mir_reg(addr_reg));
      mir_add_arg(&call, mir_reg(ptr_reg));
      mir_add_arg(&call, mir_reg(cur_reg));
      mir_add_arg(&call, mir_const(get_history_depth(dest->var->type_info)));
      mir_add_arg(&call, mir_const(get_num_elements(dest->var->type_info)));
      mir_add_arg(&call, mir_opr(index));
      mir_add_arg(&call, mir_opr(src));
    }
  }
}

/*
 * Load the value of the history variable
 */
int history_load(name_record_t *hvar, expression_t *history) {
  if(!is_array_type(hvar->type_info))
    if(cflags.flags & CFLAG_HISTORY_SIMPLE_STORE &&
       get_history_depth(hvar->type_info) < cflags.history_simple_depth) {

      /* Flat storage */
      if(cflags.flags & CFLAG_HISTORY_INLINE) {
	/* Inline version */
	int hist_reg = tr_alloc(), reg = tr_alloc();

	mir_emit(MIR_ADDR, mir_var(hvar), NULL, mir_reg(reg));
	mir_emit(MIR_MUL, mir_opr(history),
		 mir_const(sizeof_type(hvar->type_info)), mir_reg(hist_reg));
	mir_emit(MIR_ADD, mir_reg(reg), mir_reg(hist_reg), mir_reg(reg));
	mir_emit(MIR_REG_LOAD, mir_reg(reg), NULL, mir_reg(reg));

	return reg;

      } else {
	/* Call version: __hist_fp_load__(addr, depth) */
	mir_node_t *call;
	int temp_reg = tr_alloc(), ret_reg = tr_alloc();
	name_record_t *func = get_func_entry("__hist_fp_load__");

	mir_emit(MIR_ADDR, mir_var(hvar), NULL, mir_reg(temp_reg));
	call = mir_emit(MIR_CALL, mir_var(func), NULL, mir_reg(ret_reg));
	mir_add_arg(&call, mir_reg(temp_reg));
	mir_add_arg(&call, mir_opr(history));

	return ret_reg;
      }

    } else
      return history_load_scalar(hvar, history);
}

/*
 * Generate instructions for loading a primitive (scalar) history variable
 */
static int history_load_scalar(name_record_t *hvar, expression_t *history) {
  mir_node_t *call, *mir;
  name_record_t *func;
  name_record_t *arg;
  char *name, *ptr;
  int temp_reg, ret_reg, ptr_reg;


  ret_reg = tr_alloc();

  if(history->op_type == OP_CONST) {
    if(history->op_val == 0) {
      /* Optimised depth 0: t = .x_cur */
      mir_emit(MIR_MOVE, mir_var(history_current_entry(hvar)), NULL,
	       mir_reg(ret_reg));
      return ret_reg;

    } else if(history->op_val == 1) {
      /* Optimised depth 1: t = *.x_ptr */
      mir = mir_emit(MIR_MOVE, mir_var(history_ptr_entry(hvar)), NULL,
		     mir_reg(ret_reg));
      mir->instruction->operand[0]->indirect = 1;
      return ret_reg;
    }
  }

  temp_reg = tr_alloc();
  func = get_func_entry("__hist_p_load__");

  /* Call and history variable address */
  mir_emit(MIR_ADDR, mir_var(hvar), NULL, mir_reg(temp_reg));
  call = mir_emit(MIR_CALL, mir_var(func), NULL, mir_reg(ret_reg));
  mir_add_arg(&call, mir_reg(temp_reg));

  mir_add_arg(&call, mir_var(history_current_entry(hvar)));

  mir_add_arg(&call, mir_var(history_ptr_entry(hvar)));
  call->instruction->args[2]->indirect = 0;

  mir_add_arg(&call, mir_const(get_history_depth(hvar->type_info)));
  mir_add_arg(&call, mir_opr(history));

  return ret_reg;
}

int history_array_load(name_record_t *var, expression_t *index,
		       expression_t *history) {
  int addr_reg, ptr_reg, ret_reg, cur_reg;
  mir_node_t *call, *node;
  name_record_t *func;

  addr_reg = tr_alloc();
  cur_reg = tr_alloc();
  ret_reg = tr_alloc();

  if(history_array_type(var->type_info) == HISTORY_INDEX_WISE) {
    /* Index-wise */
    mir_emit(MIR_ADDR, mir_var(var), NULL, mir_reg(addr_reg));
    mir_emit(MIR_ADDR, mir_var(history_current_entry(var)), NULL,
	     mir_reg(cur_reg));

    call = mir_emit(MIR_CALL, mir_var(get_func_entry("__hist_iw_load__")),
		    NULL, mir_reg(ret_reg));

    mir_add_arg(&call, mir_reg(addr_reg));
    mir_add_arg(&call, mir_reg(cur_reg));
    mir_add_arg(&call, mir_var(history_ptr_entry(var)));
    call->instruction->args[2]->indirect = 0;
    mir_add_arg(&call, mir_opr(index));
    mir_add_arg(&call, mir_const(get_history_depth(var->type_info)));
    mir_add_arg(&call, mir_opr(history));
    mir_add_arg(&call, mir_const(get_num_elements(var->type_info)));

  } else {
    /* Array-wise */
    mir_emit(MIR_ADDR, mir_var(var), NULL, mir_reg(addr_reg));
    mir_emit(MIR_ADDR, mir_var(history_current_entry(var)), NULL,
	     mir_reg(cur_reg));

    call = mir_emit(MIR_CALL, mir_var(get_func_entry("__hist_aw_load__")),
		    NULL, mir_reg(ret_reg));

    mir_add_arg(&call, mir_reg(addr_reg));
    mir_add_arg(&call, mir_reg(cur_reg));
    mir_add_arg(&call, mir_var(history_ptr_entry(var)));
    call->instruction->args[2]->indirect = 0;

    mir_add_arg(&call, mir_const(get_history_depth(var->type_info)));
    mir_add_arg(&call, mir_const(get_num_elements(var->type_info)));
    mir_add_arg(&call, mir_opr(index));
    mir_add_arg(&call, mir_opr(history));

  }
  return ret_reg;
}
