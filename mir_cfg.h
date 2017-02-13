/*
 * mir_cfg.h
 * Ryan Mallon (2006)
 *
 */
#ifndef _MIR_CFG_H_
#define _MIR_CFG_H_

#include "symtable.h"
#include "mir.h"

typedef struct {
  int type;

  name_record_t *var;
  int reg;
  int indirect;

} mcfg_var_t;

enum {
  MCFG_TYPE_NONE,
  MCFG_TYPE_VAR,
  MCFG_TYPE_REG,
  MCFG_TYPE_SPILL_REG,
};

typedef struct mcfg_node_s {
  int type;
  mir_node_t *mir_node;

  mcfg_var_t *var_def;
  mcfg_var_t **var_use;
  int num_var_use;

  /* in/out sets */
  mcfg_var_t **in;
  mcfg_var_t **out;
  int num_ins;
  int num_outs;

  int visited;

  struct mcfg_graph_s *graph;

  struct mcfg_node_s **succ;
  struct mcfg_node_s **pred;
  int num_succs;
  int num_preds;

} mcfg_node_t;

/* Node labels */
enum {
  MCFG_NODE_STATEMENT,
  MCFG_NODE_CONDITIONAL,
  MCFG_NODE_JUMP,
  MCFG_NODE_FUNC_ENTRY,
  MCFG_NODE_FUNC_EXIT
};

typedef struct mcfg_graph_s {
  mcfg_node_t **node;
  int num_nodes;

  /* Bitmap of globals used in this graph */
  int num_globals;
  name_record_t **global;

} mcfg_graph_t;

typedef struct {
  mcfg_graph_t **graph;
  int num_graphs;
} mcfg_list_t;

/* def/use chain */
typedef struct {
  int length;
  mcfg_node_t **node;
} du_chain_t;

mcfg_list_t *mcfg_build(mir_node_t *);
int mcfg_var_match(mcfg_var_t *, mcfg_var_t *);
mcfg_var_t *mcfg_var(mir_operand_t *);
void mcfg_insert_node(mir_node_t *);
void mcfg_calculate_liveness(mcfg_graph_t *);
void mcfg_build_defuse(mcfg_graph_t *);

#endif /* _MIR_CFG_H_ */
