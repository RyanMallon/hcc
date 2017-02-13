/*
 * sparcgen.c
 * Ryan Mallon (2006)
 *
 * Sparc V8 backend code generator. Converts code from LIR (low level
 * intermediate code) to Sparc assembly. The generated code is written to a
 * file which can then be compiled using gas or gcc.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtable.h"
#include "typechk.h"
#include "scope.h"
#include "ast.h"
#include "mir.h"
#include "history.h"
#include "strings.h"
#include "cerror.h"
#include "cflags.h"
#include "sparc.h"

/* Stabs functions */
extern void stabs_init_type_table(void);
extern void stabs_print_function(FILE *, name_record_t *, int, int);
extern void stabs_print_types(FILE *);
extern void stabs_print_file_info(FILE *);
extern void stabs_print_line_num(FILE *, int, char *);

static int max_call_args(mir_node_t *);
static char *sparc_opstr(mir_operand_t *, int);

extern compiler_options_t cflags;
extern scope_node_t *global_scope;

typedef struct sparc_instr_s {
  int val[3];
  int type[3];

  struct sparc_instr_s *jump;
  struct sparc_instr_s *next;

} sparc_instr_t;

static int current_arg;

/*
 * Return an aligned address
 */
int align(int addr) {
  if(addr % STACK_ALIGN != 0)
    addr += STACK_ALIGN - (addr % STACK_ALIGN);

  return addr;
}

/*
 * Return the frame pointer offset for a variables offset
 */
int fp_offset(int offset, int frame_size, int offset_words) {
  return frame_size - sp_offset(offset, offset_words);
}

int sp_offset(int offset, int offset_words) {
  return offset + STACK_LOCAL_BASE + (offset_words * WORD_SIZE);
}


/*
 * Generate sparc code for a function
 */
void gen_sparc_func(FILE *fd, mir_node_t **node) {
  name_record_t *func = NULL;
  mir_instr_t *instr;
  char *op_str[3];
  int i, done, frame_size = 0, offset_words = 0, line_num;

  line_num = 0;
  done = 0;
  while(*node && !done) {
    instr = (*node)->instruction;

    if(cflags.flags & CFLAG_OUTPUT_STABS) {
      if(func && (*node)->src_line_num > line_num) {
	line_num = (*node)->src_line_num;
	stabs_print_line_num(fd, line_num, func->name);
      }
    }

    /* Get the operand strings */
    for(i = 0; i < 3; i++)
      op_str[i] = sparc_opstr(instr->operand[i], offset_words);

    if(instr->opcode != MIR_LABEL && instr->label)
      fprintf(fd, ".%s:\n", instr->label);

    switch(instr->opcode) {
    case MIR_NOP:
      break;

    case MIR_LABEL:
      /* Function header */
      current_arg = 0;
      func = get_func_entry(instr->label);

      /* Stack frame size, must be 8 byte aligned */
      i = max_call_args(*node);
      offset_words = max_call_args(*node);
      frame_size = align(STACK_HEADER_SIZE +
			 max_scope_size(get_func_scope(func->name)) +
			 (offset_words * WORD_SIZE));

      if(cflags.flags & CFLAG_OUTPUT_STABS)
        stabs_print_function(fd, func, frame_size, offset_words);

      fprintf(fd, "\t.global %s\n", func->name);
      fprintf(fd, "\t.type %s,#function\n", func->name);
      fprintf(fd, "\t.proc 020\n");
      fprintf(fd, "%s:\n", func->name);

      if(frame_size > MAX_CONST) {
	fprintf(fd, "\tsethi\t%%hi(%d), %%l0\n", frame_size & HIGH_BIT_MASK);
	fprintf(fd, "\tor\t%%l0, %d, %%l0\n", frame_size & LOW_BIT_MASK);
	fprintf(fd, "\tneg\t%%l0\n");
	fprintf(fd, "\tsave\t%%sp, %%l0, %%sp\n");

      } else
	fprintf(fd, "\tsave\t%%sp, -%d, %%sp\n", frame_size);

      break;

    case MIR_ADDR:
      if(instr->operand[0]->optype == MIR_OP_VAR)
	fprintf(fd, "\tadd\t%s, %d, %s\n", SP_PREFIX,
		sp_offset(instr->operand[0]->var->offset, offset_words),
		op_str[2]);
      else
	fprintf(fd, "\tld\t[%s], %s\n", op_str[0], op_str[2]);

      break;

    case MIR_STACK_ADDR: {
      int offset = sp_offset(instr->operand[0]->val, offset_words);

      if(offset > MAX_CONST) {
	fprintf(fd, "\tsethi\t%%hi(%d), %s\n", offset & HIGH_BIT_MASK,
		op_str[2]);
	fprintf(fd, "\tor\t%s, %d, %s\n", op_str[2], offset & LOW_BIT_MASK,
		op_str[2]);
	fprintf(fd, "\tadd\t%s, %s, %s\n", SP_PREFIX, op_str[2], op_str[2]);

      } else
	fprintf(fd, "\tadd\t%s, %d, %s\n", SP_PREFIX, offset, op_str[2]);
	break;

    }

    case MIR_MOVE:
      if(instr->operand[0]->indirect)
	fprintf(fd, "\tld\t%s, %s\n", op_str[0], op_str[2]);

      else if(instr->operand[2]->indirect)
	fprintf(fd, "\tst\t%s, %s\n", op_str[0], op_str[2]);

      else
	fprintf(fd, "\tmov\t%s, %s\n", op_str[0], op_str[2]);
      break;

    case MIR_ADD:
	fprintf(fd, "\tadd\t%s, %s, %s\n", op_str[0], op_str[1], op_str[2]);
      break;

    case MIR_SUB:
      fprintf(fd, "\tsub\t%s, %s, %s\n", op_str[0], op_str[1], op_str[2]);
      break;

    case MIR_MUL:
      fprintf(fd, "\tsmul\t%s, %s, %s\n", op_str[0], op_str[1], op_str[2]);
      break;

    case MIR_JUMP:
      fprintf(fd, "\tba\t.%s\n", (*node)->jump->instruction->label);
      fprintf(fd, "\tnop\n");
      break;

    case MIR_IF:
      if(!instr->operand[2]) {
	fprintf(fd, "\tcmp\t%s, 0\n", op_str[0]);
	fprintf(fd, "\tbe\t");

      } else {
	fprintf(fd, "\tcmp\t%s, %s\n", op_str[0], op_str[1]);

	switch(instr->operand[2]->val) {
	case MIR_EQL: fprintf(fd, "\tbe\t"); break;
	case MIR_NEQ: fprintf(fd, "\tbne\t"); break;
	case MIR_LSS: fprintf(fd, "\tbl\t"); break;
	case MIR_LEQ: fprintf(fd, "\tble\t"); break;
	case MIR_GTR: fprintf(fd, "\tbg\t"); break;
	case MIR_GEQ: fprintf(fd, "\tbge\t"); break;
	}
      }

      fprintf(fd, ".%s\n", (*node)->jump->instruction->label);
      fprintf(fd, "\tnop\n");
      break;

    case MIR_EQL:
      fprintf(fd, "\tbe\t");
      break;

    case MIR_NEQ:
      fprintf(fd, "\tbne\t");
      break;

    case MIR_LSS:
      fprintf(fd, "\tbl\t");
      break;

    case MIR_LEQ:
      fprintf(fd, "\tble\t");
      break;

    case MIR_GTR:
      fprintf(fd, "\tbg\t");
      break;

    case MIR_GEQ:
      fprintf(fd, "\tbge\t");
      break;

    case MIR_LOAD_STRING:
      fprintf(fd, "\tsethi\t%%hi(.string%d), %s\n", instr->operand[0]->val,
              op_str[2]);
      fprintf(fd, "\tor\t%s, %%lo(.string%d), %s\n", op_str[2],
              instr->operand[0]->val, op_str[2]);
      break;

    case MIR_REG_LOAD:
      fprintf(fd, "\tld\t[%s], %s\n", op_str[0], op_str[2]);
      break;

    case MIR_HEAP_LOAD:
      fprintf(fd, "\tsethi\t%%hi(%s), %s\n", instr->operand[0]->var->name,
	      op_str[2]);
      fprintf(fd, "\tor\t%s, %%lo(%s), %s\n", op_str[2],
	      instr->operand[0]->var->name, op_str[2]);
      fprintf(fd, "\tld\t[%s], %s\n", op_str[2], op_str[2]);
      break;

    case MIR_HEAP_ADDR:

      fprintf(fd, "\tsethi\t%%hi(%s), %s\n",
	      instr->operand[0]->optype == MIR_OP_VAR ?
	      instr->operand[0]->var->name : op_str[0],
	      op_str[2]);

      fprintf(fd, "\tor\t%s, %%lo(%s), %s\n", op_str[2],
	      instr->operand[0]->optype == MIR_OP_VAR ?
	      instr->operand[0]->var->name : op_str[0], op_str[2]);
      break;

    case MIR_STACK_LOAD: {
      int offset = sp_offset(instr->operand[0]->val, offset_words);

      if(offset > MAX_CONST) {
	fprintf(fd, "\tsethi\t%%hi(%d), %s\n", offset & HIGH_BIT_MASK,
		SCRATCH_REG);
	fprintf(fd, "\tor\t%s, %d, %s\n", SCRATCH_REG, offset & LOW_BIT_MASK,
		SCRATCH_REG);
	fprintf(fd, "\tld\t[%s + %s], %s\n", SP_PREFIX, SCRATCH_REG, op_str[2]);

      } else
	fprintf(fd, "\tld\t[%s + %d], %s\n", SP_PREFIX, offset, op_str[2]);


      break;
    }

    case MIR_REG_STORE:
      fprintf(fd, "\tst\t%s, [%s]\n", op_str[0], op_str[2]);
      break;

    case MIR_STACK_STORE: {
      int offset = sp_offset(instr->operand[2]->val, offset_words);

      if(offset > MAX_CONST) {
	fprintf(fd, "\tsethi\t%%hi(%d), %%g1\n", offset & HIGH_BIT_MASK);
	fprintf(fd, "\tor\t%%g1, %d, %%g1\n", offset & LOW_BIT_MASK);
	fprintf(fd, "\tst\t%s, [%s + %%g1]\n", op_str[0], SP_PREFIX);

      } else
	fprintf(fd, "\tst\t%s, [%s + %d]\n", op_str[0], SP_PREFIX, offset);
      break;
    }

    case MIR_HEAP_STORE:
      fprintf(fd, "\tsethi\t%%hi(%s), %s\n", instr->operand[2]->var->name,
	      op_str[1]);
      fprintf(fd, "\tor\t%s, %%lo(%s), %s\n", op_str[1],
	      instr->operand[2]->var->name, op_str[1]);
      fprintf(fd, "\tst\t%s, [%s]\n", op_str[0], op_str[1]);

      break;

    case MIR_RECEIVE:
    case MIR_PUSH_ARG:
    {
      char *arg_str = sparc_opstr((*node)->instruction->operand[2],
				  frame_size);

      /* Additional arguments passed on stack */
      if(current_arg >= REG_ARGS) {
	/* Argument on stack */
	fprintf(fd, "\tld\t[%s + %d], %s\n", FP_PREFIX,
		STACK_ARGS_BASE + ((current_arg - REG_ARGS) * WORD_SIZE),
		arg_str);
      } else {
	/* Argument in register */
	if((*node)->instruction->opcode == MIR_RECEIVE)
	  fprintf(fd, "\tmov\t%%i%d, %s\n", current_arg++, arg_str);
	else
	  fprintf(fd, "\tst\t%%i%d, [%s + %d]\n", current_arg++,
		  SP_PREFIX, sp_offset((*node)->instruction->operand[2]->val,
				       offset_words));
      }

      free(arg_str);

      break;
    }

    case MIR_CALL: {
      char *arg_str;

      /* Additional arguments passed on stack */
      if(instr->num_args > REG_ARGS) {
	for(i = REG_ARGS; i < instr->num_args; i++) {
	  arg_str = sparc_opstr(instr->args[i], frame_size);

	  fprintf(fd, "\tst\t%s, [%s + %d]\n", arg_str, SP_PREFIX,
		  ((i - REG_ARGS) * WORD_SIZE) + STACK_ARGS_BASE);

	  free(arg_str);
	}
      }

      /* First six arguments go in registers */
      for(i = 0; i < instr->num_args && i < REG_ARGS; i++) {
	arg_str = sparc_opstr(instr->args[i], frame_size);

	if(instr->args[i]->indirect)
	  fprintf(fd, "\tld\t%s, %%o%d\n", arg_str, i);
	else
	  fprintf(fd, "\tmov\t%s, %%o%d\n", arg_str, i);

	free(arg_str);
      }

      fprintf(fd, "\tcall\t%s, 0\n", instr->operand[0]->var->name);
      fprintf(fd, "\tnop\n");

      /* Return value */
      if(instr->operand[2])
	fprintf(fd, "\tmov\t%%o0, %s\n", op_str[2]);

      break;
    }

    case MIR_RETURN:
      fprintf(fd, "\tmov\t%s, %%i0\n", op_str[0]);
      fprintf(fd, "\tret\n");
      fprintf(fd, "\trestore\n");
      break;

    case MIR_END:
      /*
       * Function footer.
       * Only generate ret/restore if prev instruction is not a return
       */
      if((*node)->prev->instruction->opcode != MIR_RETURN) {
	fprintf(fd, "\tret\n");
	fprintf(fd, "\trestore\n");
      }
      fprintf(fd, ".%s_end:\n", func->name);
      fprintf(fd, "\t.size %s,.%s_end-%s\n", func->name,
	      func->name, func->name);
      done = 1;
      break;

    }

    /* Free the operand strings */
    for(i = 0; i < 3; i++)
      free(op_str[i]);

    *node = (*node)->next;
  }
}


/*
 * Generate sparc code from IC code
 */
void gen_sparc_code(char *basename) {
  FILE *fd;
  mir_node_t *current;
  int i;

  char *filename = malloc(strlen(basename) + 3);
  strcpy(filename, basename);
  strcat(filename, ".s");

  if(!(fd = fopen(filename, "w")))
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Cannot open file %s for writing\n", filename);;

  current = mir_list_head();

  /* Header */
  fprintf(fd, "\t.file \"%s.hc\"\n", basename);
  if(cflags.flags & CFLAG_OUTPUT_STABS)
    stabs_print_file_info(fd);
  fprintf(fd, "gcc2_compiled.:\n");

  /* Read-only data. Strings */
  if(num_strings()) {
    fprintf(fd, ".section\t\".rodata\"\n");
    for(i = 0; i < num_strings(); i++)
      fprintf(fd, ".string%d:\t.asciz \"%s\"\n", i,
              get_string_literal(i)->string);
  }

  /*
   * BSS (Unitialised data) Segment
   * FIXME: Currently aligns everything on 4-byte boundaries.
   */
  fprintf(fd, ".section\t\".bss\"\n");
  for(i = 0; i < global_scope->num_records; i++)
    if(global_scope->name_table[i]->type_info->decl_type != TYPE_FUNCTION &&
       !is_history_type(global_scope->name_table[i]->type_info) &&
       global_scope->name_table[i]->name[0] != '.')
      fprintf(fd, "\t.common %s, %d, %d\n", global_scope->name_table[i]->name,
	      sizeof_type(global_scope->name_table[i]->type_info), 4);

  /*
   * Data (initialised) segment. Since HistoryC doesn't allow variables
   * to be defined when they are declared this is only used for initialising
   * global history variables
   */
  fprintf(fd, ".section\t\".data\"\n");
  for(i = 0; i < global_scope->num_records; i++)
    if(is_history_type(global_scope->name_table[i]->type_info)) {
      name_record_t *cur_var, *ptr_var, *hvar = global_scope->name_table[i];

      fprintf(fd, "\t.align 4\n");
      fprintf(fd, "\t.type %s, #object\n", hvar->name);
      fprintf(fd, "\t.size %s, %d\n", hvar->name,
	      sizeof_type(hvar->type_info) + sizeof_history(hvar->type_info));
      fprintf(fd, "%s:\n", hvar->name);
      fprintf(fd, "\t.uaword 0\n");
      fprintf(fd, "\t.skip %d\n\n", sizeof_history(hvar->type_info));

      if(!(cflags.flags & CFLAG_HISTORY_SIMPLE_STORE &&
	   get_history_depth(hvar->type_info) < cflags.history_simple_depth)) {

	/* Current value */
	cur_var = history_current_entry(hvar);

	fprintf(fd, "\t.align 4\n");
	fprintf(fd, "\t.type %s, #object\n", cur_var->name);
	fprintf(fd, "\t.size %s, %d\n", cur_var->name,
		sizeof_type(cur_var->type_info));
	fprintf(fd, "%s:\n", cur_var->name);
	fprintf(fd, "\t.uaword 0\n\n");

	/* Pointer */
	ptr_var = history_ptr_entry(hvar);

	fprintf(fd, "\t.align 4\n");
	fprintf(fd, "\t.type %s, #object\n", ptr_var->name);
	fprintf(fd, "\t.size %s, %d\n", ptr_var->name,
		sizeof_type(ptr_var->type_info));
	fprintf(fd, "%s:\n", ptr_var->name);
	fprintf(fd, "\t.uaword %s\n\n", hvar->name);
      }
    }

  /* Text Segment */
  fprintf(fd, ".section\t\".text\"\n");

  if(cflags.flags & CFLAG_OUTPUT_STABS) {
    fprintf(fd, ".Ltext0:\n");
    stabs_init_type_table();
    stabs_print_types(fd);
  }

  fprintf(fd, "\t.align 4\n");

  /* Functions */
  for(i = 0; i < num_def_functions(); i++)
    gen_sparc_func(fd, &current);

  /* Footer */
  fprintf(fd, "\t.ident \"HCC: History Capable Compiler\"\n");

}

/*
 * Return the maximum of call arguments above REG_ARGS for a function
 */
static int max_call_args(mir_node_t *node) {
  int max_args = REG_ARGS;

  while(node->instruction->opcode != MIR_END) {
    if(node->instruction->opcode == MIR_CALL &&
       node->instruction->num_args > max_args)
      max_args = node->instruction->num_args;

    node = node->next;
  }

  return max_args - REG_ARGS;
}

/*
 * Return a string representation of a MIR operand
 * FIXME: This is a bit of a dirty hack
 */
static char *sparc_opstr(mir_operand_t *op, int offset_words) {
  char *str = malloc(16);

  if(!op) {
    sprintf(str, "(null)");
    return str;
  }

  switch(op->optype) {
  case MIR_OP_VAR:
    if(op->indirect)
      sprintf(str, "[%s + %d]", SP_PREFIX, sp_offset(op->var->offset,
						     offset_words));
    else
      sprintf(str, "(%s + %d)", SP_PREFIX, sp_offset(op->var->offset,
						     offset_words));
    break;

  case MIR_OP_CONST:
    sprintf(str, "%d", op->val);
    break;

  case MIR_OP_REG:
    //if(op->indirect)
    //   sprintf(str, "[%s%d]", LOCAL_PREFIX, op->val);
    //else
      sprintf(str, "%s%d", LOCAL_PREFIX, op->val);
    break;
  }

  return str;
}
