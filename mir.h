/*
 * mir.h
 * Ryan Mallon
 * Medium-level Intermediate Representation
 *
 * Based on the MIR specification from Advanced Compiler Design and
 * Implementation by Steven S. Muchnick
 *
 */
#include <stdio.h>
#include "symtable.h"
#include "typechk.h"
#include "astparse.h"
#include "icg.h"

#ifndef _MIR_H_
#define _MIR_H_

/* MIR instruction opcodes */
enum {
  MIR_NOP,
  MIR_LABEL,
  MIR_BEGIN,
  MIR_END,
  MIR_RECEIVE,
  MIR_CALL,
  MIR_RETURN,
  MIR_IF,
  MIR_IF_NOT,
  MIR_JUMP,
  MIR_MEMBER,
  MIR_MEMBER_DEREF,
  MIR_MOVE,
  MIR_SMOVE,

  MIR_ADDR,
  MIR_STACK_ADDR,
  MIR_HEAP_ADDR,

  MIR_LOAD,
  MIR_STORE,
  MIR_STACK_LOAD,
  MIR_HEAP_LOAD,
  MIR_REG_LOAD,
  MIR_STACK_STORE,
  MIR_HEAP_STORE,
  MIR_REG_STORE,
  MIR_PUSH_ARG,
  MIR_LOAD_STRING,

  MIR_ADD,
  MIR_SUB,
  MIR_MUL,
  MIR_DIV,

  MIR_EQL,
  MIR_NEQ,
  MIR_LSS,
  MIR_LEQ,
  MIR_GTR,
  MIR_GEQ,

  MIR_MAX_OPCODES
};

/* MIR parameter types */
enum {
  MIR_ARG_VAL,
  MIR_ARG_REF
};

/* MIR operands */
enum {
  MIR_OP_VAR,
  MIR_OP_CONST,
  MIR_OP_REG,
  MIR_OP_SPILL_REG,
};

typedef struct {
  int optype;
  int indirect;

  name_record_t *var;
  type_info_t *type_info;
  int val;

} mir_operand_t;

/* Function argument for call instruction */
typedef struct {
  mir_operand_t *arg_val;
} mir_arg_t;

/* MIR instruction */
typedef struct {
  char *label;

  int opcode;
  mir_operand_t *operand[3];

  int num_args;
  mir_operand_t **args;

} mir_instr_t;

/* MIR instruction list */

struct mcfg_node_s;

typedef struct mir_node_s {
  mir_instr_t *instruction;
  int num;
  int src_line_num;

  int in_links;
  struct mir_node_s *jump;

  /* For inlining */
  struct mir_node_s *copy_node;

  /* Used for translating MIR to LIR */
  ic_list_t *ic_instr;

  /* Pointer to cfg node this is associated with */
  struct mcfg_node_s *cfg_node;

  struct mir_node_s *prev;
  struct mir_node_s *next;
} mir_node_t;

/* Func headers */
void mir_print(char *, char *);
void mir_print_instr(FILE *, int, mir_node_t *);
void mir_label(mir_node_t **, char *);
void mir_set_jump(mir_node_t *, mir_node_t *);
void mir_move_label(mir_node_t *, mir_node_t *);
void mir_add_arg(mir_node_t **, mir_operand_t *);
void mir_remove_node(mir_node_t *);
mir_node_t *mir_emit(int, mir_operand_t *, mir_operand_t *,
		     mir_operand_t *);
mir_node_t *mir_list_tail(void);
mir_node_t *mir_list_head(void);
mir_operand_t *mir_opr(expression_t *);
mir_operand_t *mir_opr_copy(mir_operand_t *);
mir_operand_t *mir_reg(int);
mir_operand_t *mir_spill_reg(int);
mir_operand_t *mir_var(name_record_t *);
mir_operand_t *mir_const(int);
mir_node_t *mir_add_instr(mir_node_t *, int, mir_operand_t *,
			  mir_operand_t *, mir_operand_t *);

// void mir_to_sym_lir(ig_graph_t *);
char *mir_opstr(mir_operand_t *, char *);
void mir_spill_may_aliases(void);
void mir_fold_constants(void);
void mir_munge_globals(void);
void mir_strip(void);
void mir_munge(void);
void mir_munge_pointers(void);
void mir_rhs_constants(void);
void mir_args_to_regs(int);
void mir_inline(void);
void mir_label_nodes(void);

#endif /* _MIR_H_ */
