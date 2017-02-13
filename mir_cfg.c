/*
 * mir_cfg.c
 * Ryan Mallon (2006)
 *
 * Control flow graph for medium-level intermediate representation.
 * This is far easier to build and more useful than the source level CFG.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "symtable.h"
#include "scope.h"
#include "mir.h"
#include "mir_cfg.h"
#include "cerror.h"
#include "cflags.h"

static void mcfg_add_node(mcfg_graph_t *, mcfg_node_t *);
static void mcfg_add_edge(mcfg_node_t *, mcfg_node_t *);
static void mcfg_adjust_edge(mcfg_node_t *, mcfg_node_t *, mcfg_node_t *);
static int liveness_at_node(mcfg_node_t *);

static void mcfg_add_global(mcfg_graph_t *, name_record_t *);

static mcfg_list_t *mcfg_list;

extern scope_node_t *global_scope;
extern compiler_options_t cflags;

/*
 * Create a new cfg node
 */
static mcfg_node_t *mcfg_mk_node(mir_node_t *mir_node, int type) {
  mcfg_node_t *cfg_node;

  cfg_node = malloc(sizeof(mcfg_node_t));

  cfg_node->mir_node = mir_node;
  mir_node->cfg_node = cfg_node;

  cfg_node->type = type;
  cfg_node->graph = NULL;

  cfg_node->pred = NULL;
  cfg_node->num_preds = 0;
  cfg_node->succ = NULL;
  cfg_node->num_succs = 0;

  cfg_node->in = NULL;
  cfg_node->out = NULL;
  cfg_node->num_ins = 0;
  cfg_node->num_outs = 0;

  cfg_node->var_def = NULL;
  cfg_node->var_use = NULL;
  cfg_node->num_var_use = 0;

  return cfg_node;
}

/*
 * Inserts a new cfg node into an existing CFG. This is used for inserting
 * load and store instructions for spill code generation during register
 * allocation
 */
void mcfg_insert_node(mir_node_t *mir_node) {
  int i;
  mcfg_node_t *cfg_node, *pred;

  cfg_node = mcfg_mk_node(mir_node, MCFG_NODE_STATEMENT);
  mcfg_add_node(mir_node->next->cfg_node->graph, cfg_node);

  /* Move all preds of the new nodes successor to the new node */
  for(i = 0; i < mir_node->next->cfg_node->num_preds; i++)
    mcfg_adjust_edge(mir_node->next->cfg_node->pred[i],
		     mir_node->next->cfg_node, cfg_node);

  mcfg_add_edge(cfg_node, mir_node->next->cfg_node);

}

/*
 * Create a new cfg graph
 */
static mcfg_graph_t *mcfg_mk_graph(void) {
  mcfg_graph_t *graph;

  graph = malloc(sizeof(mcfg_graph_t));
  graph->node = NULL;
  graph->num_nodes = 0;

  graph->num_globals = 0;
  graph->global = NULL;

  return graph;
}

/*
 * Return the cfg_node which matches the given mir_node
 */
static mcfg_node_t *mcfg_find_node(mcfg_graph_t *graph, mir_node_t *node) {
  int i;

  if(node->cfg_node)
    return node->cfg_node;

  for(i = 0; i < graph->num_nodes; i++)
    if(graph->node[i]->mir_node == node)
      return graph->node[i];

  return NULL;
}

/*
 * Create a new cfg var from a MIR operand
 */
mcfg_var_t *mcfg_var(mir_operand_t *operand) {
  mcfg_var_t *var;

  if(operand->optype == MIR_OP_CONST)
    return NULL;

  var = malloc(sizeof(mcfg_var_t));

  var->reg = operand->val;
  var->var = operand->var;
  var->indirect = operand->indirect;

  switch(operand->optype) {
  case MIR_OP_VAR:
    if(!operand->var)
      compiler_error(CERROR_WARN, CERROR_NO_LINE,
		     "null var in mcfg_var\n");

    var->type = MCFG_TYPE_VAR;
    break;

  case MIR_OP_REG:
    var->type = MCFG_TYPE_REG;
    //var->reg = operand->val;
    break;

  case MIR_OP_SPILL_REG:
    var->type = MCFG_TYPE_SPILL_REG;
    //var->reg = operand->val;
    break;

  default:
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Can't convert mir_op of unknown type %d\n",
		   operand->optype);
  }

  return var;
}

/*
 * Test if two mcfg_vars are the same
 */
int mcfg_var_match(mcfg_var_t *var1, mcfg_var_t *var2) {
  if(var1 == var2)
    return 1;

  if(var1->type == var2->type)
    if(((var1->type == MCFG_TYPE_REG || var1->type == MCFG_TYPE_SPILL_REG) &&
	var1->reg == var2->reg) ||
       (var1->type == MCFG_TYPE_VAR && var1->var == var2->var))
      return 1;

  return 0;
}

/*
 * Add a variable to the use list
 */
static void mcfg_add_var_use(mcfg_node_t *node, mcfg_var_t *var) {
  int i;

  /* Don't add the same var twice to the use list */
  for(i = 0; i < node->num_var_use; i++)
    if(mcfg_var_match(node->var_use[i], var)) {
      /* FIXME: free(var); */
      return;
    }

  /* Don't add functions */
  if(var->type == MCFG_TYPE_VAR &&
     var->var->type_info->decl_type == TYPE_FUNCTION)
    return;

  /* If the variable is a global, add it to the global list for this graph */
  if(var->type == MCFG_TYPE_VAR && get_var_scope(var->var) == global_scope)
    mcfg_add_global(node->graph, var->var);

  if(!node->var_use)
    node->var_use = malloc(sizeof(mcfg_var_t *));
  else
    node->var_use = realloc(node->var_use, sizeof(mcfg_var_t *) *
			    (node->num_var_use + 1));

  node->var_use[node->num_var_use++] = var;
}

/*
 * Add an in var
 */
static int mcfg_add_in_var(mcfg_node_t *node, mcfg_var_t *var) {
  int i;

  if(!var)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Attempting to add null var to in list");

  /* Don't add the same var twice to the in list */
  for(i = 0; i < node->num_ins; i++)
    if(mcfg_var_match(node->in[i], var))
      return 0;


  if(!node->in)
    node->in = malloc(sizeof(mcfg_var_t *));
  else
    node->in = realloc(node->in, sizeof(mcfg_var_t *) * (node->num_ins + 1));

  node->in[node->num_ins++] = var;
  return 1;
}

/*
 * Add a variable to the global list
 */
static void mcfg_add_global(mcfg_graph_t *graph, name_record_t *var) {
  int i;

  if(!var)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Attempting to add null var to global list");

  /* Dont add duplicates */
  for(i = 0; i < graph->num_globals; i++)
    if(graph->global[i] == var)
      return;

  graph->global = realloc(graph->global, sizeof(name_record_t *) *
                          (graph->num_globals + 1));

  graph->global[graph->num_globals++] = var;

}


/*
 * Add an out var
 */
static int mcfg_add_out_var(mcfg_node_t *node, mcfg_var_t *var) {
  int i;

  if(!var)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Attempting to add null var to out list");

  /* Don't add the same var twice to the out list */
  for(i = 0; i < node->num_outs; i++)
    if(mcfg_var_match(node->out[i], var))
      return 0;


  if(!node->out)
    node->out = malloc(sizeof(mcfg_var_t *));
  else
    node->out = realloc(node->out, sizeof(mcfg_var_t *) *
			(node->num_outs + 1));

  node->out[node->num_outs++] = var;
  return 1;
}

/*
 * Add a node to a graph
 */
static void mcfg_add_node(mcfg_graph_t *graph, mcfg_node_t *node) {
  if(!graph->num_nodes)
    graph->node = malloc(sizeof(mcfg_node_t *));
  else
    graph->node = realloc(graph->node, sizeof(mcfg_node_t *) *
			  (graph->num_nodes + 1));

  node->graph = graph;
  graph->node[graph->num_nodes++] = node;
}

/*
 * Add an edge between two nodes in a graph
 */
static void mcfg_add_edge(mcfg_node_t *src, mcfg_node_t *dest) {
  if(!dest || !src)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Invalid src(%p) or dest(%p) in mcfg_add_edge\n",
		   src, dest);

  if(!src->num_succs)
    src->succ = malloc(sizeof(mcfg_node_t *));
  else
    src->succ = realloc(src->succ,
			sizeof(mcfg_node_t *) * (src->num_succs + 1));

  if(!dest->num_preds)
    dest->pred = malloc(sizeof(mcfg_node_t *));
  else
    dest->pred = realloc(dest->pred,
			 sizeof(mcfg_node_t *) * (dest->num_preds + 1));

  src->succ[src->num_succs++] = dest;
  dest->pred[dest->num_preds++] = src;
}

/*
 * Change the destination of an edge
 */
static void mcfg_adjust_edge(mcfg_node_t *src, mcfg_node_t *dest,
			     mcfg_node_t *new_dest) {
  int i, j;

  /* Find the outgoing edge */
  for(i = 0; i < src->num_succs; i++)
    if(src->succ[i] == dest)
      break;

  /* Find the predecessor edge */
  for(j = 0; j < dest->num_preds; j++)
    if(dest->pred[j] == src)
      break;

  if(i == src->num_succs)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Cannot find specified successor edge\n");

  if(j == dest->num_preds)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Cannot find specified predecessor edge\n");

  /*
   * Delete the old edges. Shifts the the last edge in the array into the
   * position of the one being deleted, so it is done in O(1) time.
   */
  if(i != src->num_succs - 1)
    src->succ[i] = src->succ[src->num_succs - 1];

  if(j != dest->num_preds - 1)
    dest->pred[j] = dest->pred[dest->num_preds - 1];

  src->num_succs--;
  dest->num_preds--;

  /* Add the new edge */
  mcfg_add_edge(src, new_dest);
}

/*
 * Build the cfg for a function
 */
static mcfg_graph_t *mcfg_build_func(mir_node_t **mir_node) {
  name_record_t *func;
  mcfg_graph_t *graph;
  mcfg_node_t *node;
  mir_instr_t *instr;
  int i, j;

  graph = mcfg_mk_graph();

  /* Function entry */
  if((*mir_node)->instruction->opcode != MIR_LABEL)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Missing function in MIR code\n");

  func = get_func_entry((*mir_node)->instruction->label);
  mcfg_add_node(graph, mcfg_mk_node((*mir_node), MCFG_NODE_FUNC_ENTRY));

  *mir_node = (*mir_node)->next;
  while((*mir_node)->instruction->opcode != MIR_END) {

    switch((*mir_node)->instruction->opcode) {
    case MIR_JUMP:
      node = mcfg_mk_node((*mir_node), MCFG_NODE_JUMP);
      break;

    case MIR_IF:
    case MIR_IF_NOT:
      node = mcfg_mk_node((*mir_node), MCFG_NODE_CONDITIONAL);
      break;

    default:
      node = mcfg_mk_node((*mir_node), MCFG_NODE_STATEMENT);
      break;
    }

    mcfg_add_node(graph, node);

    *mir_node = (*mir_node)->next;
  }

  /* Function exit */
  mcfg_add_node(graph, mcfg_mk_node(*mir_node, MCFG_NODE_FUNC_EXIT));
  *mir_node = (*mir_node)->next;

  /* Calculate edges and variable def/uses */
  for(i = 0; i < graph->num_nodes; i++) {
    switch(graph->node[i]->type) {
    case MCFG_NODE_FUNC_EXIT:
      break;
    case MCFG_NODE_JUMP:
      mcfg_add_edge(graph->node[i],
		    mcfg_find_node(graph, graph->node[i]->mir_node->jump));
      break;

    case MCFG_NODE_CONDITIONAL:
      mcfg_add_edge(graph->node[i], graph->node[i + 1]);
      mcfg_add_edge(graph->node[i],
		    mcfg_find_node(graph, graph->node[i]->mir_node->jump));
      break;

    default:
      mcfg_add_edge(graph->node[i], graph->node[i + 1]);

      break;
    }
  }

  return graph;
}

/*
 * Build the control flow graph. Returns a list of graphs (one per function)
 */
mcfg_list_t *mcfg_build(mir_node_t *mir_list_head) {
  int i, index, def_funcs;;
  mir_node_t *current = mir_list_head;

  mcfg_list = malloc(sizeof(mcfg_list_t));

  def_funcs = mcfg_list->num_graphs = num_def_functions();
  mcfg_list->graph = malloc(sizeof(mcfg_graph_t *) * mcfg_list->num_graphs);

  for(i = 0, index = 0; i < def_funcs; i++) {

    if((cflags.flags & CFLAG_INLINE) && get_def_func(i)->func_inline)
      mcfg_list->num_graphs--;
    else
      mcfg_list->graph[index++] = mcfg_build_func(&current);
  }

  return mcfg_list;
}

/*
 * Build the def/use sets for a control flow graph
 */
void mcfg_build_defuse(mcfg_graph_t *graph) {
  mir_instr_t *instr;
  int i, j;

  for(i = 0; i < graph->num_nodes; i++) {

    /* Clear the def/use sets if necessary */
    if(graph->node[i]->var_def)
      graph->node[i]->var_def = NULL;

    if(graph->node[i]->var_use) {
      free(graph->node[i]->var_use);
      graph->node[i]->var_use = NULL;
      graph->node[i]->num_var_use = 0;
    }

    /*
     * Var def. Only ever one since we are working with three address code.
     * The storage location for a register storage is counted as a use rather
     * than a def, since it alters what is at the location of the register
     * and not the register itself.
     */
    instr = graph->node[i]->mir_node->instruction;
    if(instr->operand[2] && instr->operand[2]->optype != MIR_OP_CONST &&
       instr->opcode != MIR_REG_STORE && instr->opcode != MIR_HEAP_STORE) {
      graph->node[i]->var_def = mcfg_var(instr->operand[2]);

      /*
       * If the variable is a global, add it to the global list
       * for this graph
       */
      if(graph->node[i]->var_def->type == MCFG_TYPE_VAR &&
         get_var_scope(graph->node[i]->var_def->var) == global_scope)
	mcfg_add_global(graph, graph->node[i]->var_def->var);
    }

    /*
     * Var uses. Can be many since functions can pass several arguments. The
     * first operand in a load instruction is not var use since it is being
     * fetched from memory.
     */
    switch(instr->opcode) {
    case MIR_REG_STORE:
      mcfg_add_var_use(graph->node[i], mcfg_var(instr->operand[2]));
      goto default_handler;
      break;

    case MIR_HEAP_ADDR:
    case MIR_HEAP_LOAD:
      break;

    case MIR_CALL:
      for(j = 0; j < instr->num_args; j++)
	if(instr->args[j]->optype != MIR_OP_CONST)
	  mcfg_add_var_use(graph->node[i], mcfg_var(instr->args[j]));
      break;

    default_handler:
    default:
      for(j = 0; j < 2; j++)
	if(instr->operand[j] && instr->operand[j]->optype != MIR_OP_CONST)
	  mcfg_add_var_use(graph->node[i], mcfg_var(instr->operand[j]));

    }

#if 0
    for(j = 0; j < 2; j++)
      if(instr->operand[j] && instr->operand[j]->optype != MIR_OP_CONST)
        mcfg_add_var_use(graph->node[i], mcfg_var(instr->operand[j]));

    if(instr->opcode == MIR_REG_STORE)
      mcfg_add_var_use(graph->node[i], mcfg_var(instr->operand[2]));


    for(j = 0; j < instr->num_args; j++)
      if(instr->args[j]->optype != MIR_OP_CONST)
        mcfg_add_var_use(graph->node[i], mcfg_var(instr->args[j]));
#endif

  }

}

/*
 * Calculate the liveness for each variable in the given CFG.
 *
 * Calculates the live in and out sets.
 *
 */
void mcfg_calculate_liveness(mcfg_graph_t *graph) {
  mcfg_node_t *current;
  int i, j, k, changed, exit_node;

  /* Initialise the in set: in(n) = use(n) */
  for(i = 0; i < graph->num_nodes; i++) {
    /* Clear the in/out sets if necessary */
    if(graph->node[i]->in) {
      free(graph->node[i]->in);
      graph->node[i]->in = NULL;
      graph->node[i]->num_ins = 0;
    }

    if(graph->node[i]->out) {
      free(graph->node[i]->out);
      graph->node[i]->out = NULL;
      graph->node[i]->num_outs = 0;
    }

#if 0
    for(j = 0; j < graph->node[i]->num_var_use; j++)
      mcfg_add_in_var(graph->node[i], graph->node[i]->var_use[j]);
#endif

  }

  debug_printf(1, "Calculating liveness: ");
  changed = 1;

  while(changed) {
    changed = 0;
    debug_printf(1, ".");

    for(i = 0; i < graph->num_nodes; i++) {
      current = graph->node[i];
      current->visited = 0;

      if(current->type == MCFG_NODE_FUNC_EXIT)
	exit_node = i;
    }

    changed = liveness_at_node(graph->node[exit_node]);

  }
  debug_printf(1, "\n");
}

static int liveness_at_node(mcfg_node_t *node) {
  int i, j, changed = 0;

  if(node->visited)
    return changed;
  node->visited = 1;

  /* Add all out(n) that are not in def(n) to in(n) */
  for(i = 0; i < node->num_var_use; i++)
    mcfg_add_in_var(node, node->var_use[i]);

  for(i = 0; i < node->num_outs; i++)
    if(!node->var_def || !mcfg_var_match(node->out[i], node->var_def))
      if(mcfg_add_in_var(node, node->out[i]))
	changed = 1;

  /* Foreach successor s of n: out(n) = out(n) + in(s) */
  for(i = 0; i < node->num_succs; i++)
    for(j = 0; j < node->succ[i]->num_ins; j++)
      if(mcfg_add_out_var(node, node->succ[i]->in[j]))
	changed = 1;

  /* Visit each pred of this node */
  for(i = 0; i < node->num_preds; i++)
    if(liveness_at_node(node->pred[i]))
      changed = 1;

  return changed;
}


/*
 * Output a VCG graph file
 */
void vcg_output_mcfg(mcfg_list_t *mcfg_list, char *basename, char *ext) {
  FILE *fd;
  char *filename;
  mcfg_node_t *current;
  int i, j, k;

  if(!(cflags.flags & CFLAG_DEBUG_OUTPUT))
    return;

  filename = malloc(strlen(basename) + strlen(ext) + 2);
  strcpy(filename, basename);
  strcat(filename, ".");
  strcat(filename, ext);

  if(!(fd = fopen(filename, "w")))
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Cannot open file %s for writing\n", filename);

  /* Header */
  fprintf(fd, "graph: { title: \"%s\"\n", filename);
  fprintf(fd, "x: 60\ny: 60\nwidth: 700\nheight: 700\n");
  fprintf(fd, "layout_nearfactor: 0\nmanhatten_edges: yes\n");
  fprintf(fd, "yspace: 50\nxspace: 34\nxlspace: 25\nstretch: 40\n");
  fprintf(fd, "shrink: 100\ndisplay_edge_labels: yes\n");
  fprintf(fd, "finetuning: no\nnode.borderwidth: 3\nnode.color: lightcyan\n");
  fprintf(fd, "node.textcolor: darkred\nnode.bordercolor: red\n");
  fprintf(fd, "edge.color: darkgreen\n");

  /* Nodes */
  fprintf(fd, "\n\n");
  for(i = 0; i < mcfg_list->num_graphs; i++) {

    for(j = 0; j < mcfg_list->graph[i]->num_nodes; j++) {

      current = mcfg_list->graph[i]->node[j];

      fprintf(fd, "node: { title:\"%p\"\n\tlabel: \"", current);
      switch(current->type) {
      case MCFG_NODE_FUNC_ENTRY:
	fprintf(fd, "function entry");
	break;

      case MCFG_NODE_FUNC_EXIT:
	fprintf(fd, "function exit");
	break;

      case MCFG_NODE_JUMP:
	fprintf(fd, "jump");
	break;

      default:
	fprintf(fd, "statement");
	break;
      }

      fprintf(fd, ", instr={");
      mir_print_instr(fd, 0, current->mir_node);
      fprintf(fd, "}");


      /* def */
      if(current->var_def)
	if(current->var_def->type == MCFG_TYPE_REG)
	  fprintf(fd, "\ndef={%%t%d}", current->var_def->reg);
	else
	  fprintf(fd, "\ndef={%s}", current->var_def->var->name);

      /* uses */
      if(current->var_use) {
	fprintf(fd, "\nuse={");
	for(k = 0; k < current->num_var_use; k++) {
	  if(current->var_use[k]->type == MCFG_TYPE_REG)
	    fprintf(fd, "%%t%d", current->var_use[k]->reg);
	  else
	    fprintf(fd, "%s", current->var_use[k]->var->name);

	  fprintf(fd, "%s", k == current->num_var_use - 1 ? "}" : ", ");
	}
      }

      /* ins */
      if(current->in) {
	fprintf(fd, "\nins={");
	for(k = 0; k < current->num_ins; k++) {
	  if(current->in[k]->type == MCFG_TYPE_REG)
	    fprintf(fd, "%%t%d", current->in[k]->reg);
	  else
	    fprintf(fd, "%s", current->in[k]->var->name);

	  fprintf(fd, "%s", k == current->num_ins - 1 ? "}" : ", ");
	}

      }

      /* outs */
      if(current->out) {
	fprintf(fd, "\nouts={");
	for(k = 0; k < current->num_outs; k++) {
	  if(current->out[k]->type == MCFG_TYPE_REG)
	    fprintf(fd, "%%t%d", current->out[k]->reg);
	  else
	    fprintf(fd, "%s", current->out[k]->var->name);

	  fprintf(fd, "%s", k == current->num_outs - 1 ? "}" : ", ");
	}

      }

      fprintf(fd, "\" ");
      switch(current->type) {
      case MCFG_NODE_FUNC_ENTRY:
      case MCFG_NODE_FUNC_EXIT:
	fprintf(fd, "shape: ellipse color: yellow }\n");
	break;
      case MCFG_NODE_CONDITIONAL:
	fprintf(fd, "shape: rhomb color: lightgreen }\n");
	break;
      default:
	fprintf(fd, "}\n");
	break;
      }
    }
  }

  /* Edges */
  fprintf(fd, "\n\n");

  for(i = 0; i < mcfg_list->num_graphs; i++) {
    for(j = 0; j < mcfg_list->graph[i]->num_nodes; j++) {
      current = mcfg_list->graph[i]->node[j];

      for(k = 0; k < current->num_succs; k++) {
	fprintf(fd, "edge: ");
	fprintf(fd, "{ sourcename:\"%p\" targetname:\"%p\" ", current,
		current->succ[k]);
	fprintf(fd, "}\n");

      }
    }
  }

  /* Footer */
  fprintf(fd, "\n}\n");
  fclose(fd);
}
