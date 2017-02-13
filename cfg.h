/*
 * cfg.h
 *
 * Ryan Mallon (2005)
 *
 * Control Flow Graph
 *
 */
#ifndef _CFG_H_
#define _CFG_H_

#include "symtable.h"

/* Edge labels */
enum {
  CFG_EDGE_NORMAL,
  CFG_EDGE_TRUE,
  CFG_EDGE_FALSE,
  CFG_EDGE_LOOP
};

/* CFG Types */
enum {
  CFG_SOURCE,
  CFG_IC
};

/* Node labels */
enum {
  CFG_NODE_STATEMENT,
  CFG_NODE_CONDITIONAL,
  CFG_NODE_CONDITIONAL_EXIT,
  CFG_NODE_FUNC_ENTRY,
  CFG_NODE_FUNC_EXIT
};

typedef struct cfg_node_s {
  int node_type;

  /* Edges */
  int num_edges;
  int num_preds;
  struct cfg_node_s **edge;
  struct cfg_node_s **pred;
  int *edge_label;
  int flow_edge_set;

  /* Part of the AST this represents */
  ast_node_t *ast_node;

  /* Liveness */
  int num_in;
  int num_out;
  name_record_t **in;
  name_record_t **out;

  /* Variables */
  int num_var_use;
  name_record_t **var_use;
  name_record_t *var_def;

} cfg_node_t;

typedef struct {
  cfg_node_t *first;
  cfg_node_t *last;
  cfg_node_t **nodes;
  int num_nodes;
} cfg_graph_t;

typedef struct {
  int num_graphs;
  cfg_graph_t **graph;
} cfg_list_t;

cfg_list_t *cfg_build(void);
void vcg_output_cfg(cfg_list_t *, char *);


#endif /* _CFG_H_ */
