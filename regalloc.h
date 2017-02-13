/*
 * regalloc.h
 * Ryan Mallon (2005)
 *
 * Interference graph and register allocation
 *
 */
#ifndef _REGALLOC_H_
#define _REGALLOC_H_

#include "symtable.h"
#include "mir_cfg.h"

/*
 * Interference Graph (IG)
 *
 * The connections are stored as a 2d bitmap
 */
typedef struct {
  int num_sym_regs;
  mcfg_var_t **sym_reg;

  int **links;
  int num_links;
} ig_graph_t;

typedef struct adj_node_s {
  int colour;
  int offset;
  int location;
  int spill_cost;
  int spill;

  mcfg_var_t *var;

  int on_stack;

  int num_adjs;
  int *adj_node;

  int num_rems;
  int *rem_node;
} adj_node_t;

/* Storage allocation types */
enum {
  ALLOC_STACK,
  ALLOC_HEAP
};


void vcg_output_ig(ig_graph_t *, char *, char *);
ig_graph_t *ig_build_graph(mcfg_list_t *);
void allocate_registers(mcfg_list_t *, int);
int ig_sym_reg(ig_graph_t *graph, mir_operand_t *op);

#endif /* _REGALLOC_H_ */
