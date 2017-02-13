/*
 * inline.c
 * Ryan Mallon (2006)
 *
 * Function inlining
 *
 */
#include "symtable.h"
#include "scope.h"
#include "mir.h"
#include "cerror.h"
#include "cflags.h"

static void copy_locals(scope_node_t *, scope_node_t *);

/*
 * Function inlining.
 */
void inline_funcs(void) {
  name_record_t *func, *dest_func;
  mir_node_t *src, *dest, *call, *current = mir_list_head();
  mir_instr_t *src_instr;
  int i;

  while(current) {

    if(current->instruction->opcode == MIR_LABEL)
      dest_func = get_func_entry(current->instruction->label);


    if(current->instruction->opcode == MIR_CALL &&
       (func = current->instruction->operand[0]->var)->func_inline) {
      debug_printf(1, "Inlining function %s in %s\n",
		   func->name, dest_func->name);

      func = current->instruction->operand[0]->var;

      copy_locals(get_func_scope(func->name),
		  get_func_scope(dest_func->name));

      call = current;
      src = func->mir_first->next;

      /* Copy the instructions */
      while(src != func->mir_last) {
	src_instr = src->instruction;

	dest = mir_add_instr(current, src_instr->opcode,
			     mir_opr_copy(src_instr->operand[0]),
			     mir_opr_copy(src_instr->operand[1]),
			     mir_opr_copy(src_instr->operand[2]));

	/* Copy arguments for call instructions */
	if(src_instr->opcode == MIR_CALL)
	  for(i = 0; i < src_instr->num_args; i++)
	    mir_add_arg(&dest, mir_opr_copy(src_instr->args[i]));

	/* Fix up return instructions */
	if(src_instr->opcode == MIR_RETURN && src_instr->operand[0]) {
	  dest->instruction->operand[2] =
	    mir_opr_copy(call->instruction->operand[2]);
	  dest->instruction->opcode = MIR_MOVE;
	}


	/* Check for recursively inlined functions */
	if(dest->instruction->opcode == MIR_CALL &&
	   dest->instruction->operand[0]->var->func_inline) {
	  compiler_error(CERROR_WARN, CERROR_NO_LINE,
			 "Disabling inlining for recursive function %s\n",
			 dest->instruction->operand[0]->var->name);
	  dest->instruction->operand[0]->var->func_inline = 0;
	}

	src->copy_node = dest;
	dest->jump = src->jump;

	src = src->next;
	current = current->next;

      }
      /* For jumps to the function exit */
      src->copy_node = current->next;


      /* Fix up the jumps */
      src = call->next;
      while(src != current) {
	if(src->jump) {
	  debug_printf(1, "Inline jump: From (-> %p) to (%p -> %p)\n",
		       src->jump, src, src->jump->copy_node);

	  mir_set_jump(src, src->jump->copy_node);
	}

	src = src->next;
      }

      /* Fix up the arguments */
      src = call->next;
      for(i = 0; i < func->num_args; i++, src = src->next) {
	src->instruction->operand[0] =
	  mir_opr_copy(call->instruction->args[i]);
	src->instruction->operand[2]->indirect = 0;
	src->instruction->opcode = MIR_MOVE;
      }

      /* Replace the call instruction with a nop */
      for(i = 0; i < call->instruction->num_args; i++)
	call->instruction->args[i] = NULL;
      call->instruction->num_args = 0;

      for(i = 0; i < 3; i++)
	call->instruction->operand[i] = NULL;

      call->instruction->opcode = MIR_NOP;

      //mir_add_instr(call, MIR_NOP, NULL, NULL, NULL);
      //mir_remove_node(call);

    }
    current = current->next;
  }
  debug_printf(1, "----\nInlining done\n----\n");
}

/*
 * Copy the local variables from func src to dest
 */
static void copy_locals(scope_node_t *src, scope_node_t *dest) {
  int base, i;
  name_record_t *current;

  base = max_scope_size(dest);

  for(i = 0; i < src->num_records; i++) {
    current = add_entry(dest, src->name_table[i]);
    current->offset += base;

    debug_printf(1, "\tCopying local variable %s, new offset = %d\n",
		 src->name_table[i]->name, src->name_table[i]->offset);
  }

  for(i = 0; i < src->num_children; i++)
    copy_locals(src->children[i], dest);
}

/*
 * Remove successfully inlined functions
 */
void remove_inlined_funcs(void) {
  int i;
  name_record_t *func;
  mir_node_t *current, *next;

  for(i = 0; i < num_def_functions(); i++) {
    func = get_def_func(i);

    if(func->func_inline) {
      current = func->mir_first->prev;

      while(current != func->mir_last) {
	next = current->next;
	mir_remove_node(current);
	current = next;
      }
      mir_remove_node(current);
    }
  }
}
