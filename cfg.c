/*
 * cfg.c
 *
 * Ryan Mallon (2005)
 *
 * Control Flow Graph and variable liveness
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ast.h"
#include "symtable.h"
#include "scope.h"
#include "cfg.h"
#include "cerror.h"

cfg_node_t *cfg;
cfg_list_t *cfg_list;
cfg_node_t **cfg_nodes;
int num_cfg_nodes;

extern ast_node_t *ast;
extern ic_info_t ic_info[MAX_IC_OPS];

extern scope_node_t *current_scope, *global_scope;

static cfg_node_t *cfg_mk_node(int, ast_node_t *);
static cfg_graph_t *cfg_mk_graph(void);
static void cfg_add_var_use(cfg_node_t *, name_record_t *);
static void cfg_add_edge(cfg_node_t *, cfg_node_t *, int);
static void cfg_add_graph_node(cfg_graph_t *, cfg_node_t *);

static cfg_list_t *cfg_build_lists(void);
static cfg_graph_t *cfg_build_func(ast_node_t *);
static cfg_graph_t *cfg_build_block(ast_node_t *);
static cfg_graph_t *cfg_build_statement(ast_node_t *);

static void cfg_calculate_liveness(cfg_graph_t *);

static void cfg_output_vcg(cfg_list_t *, char *);
static void cfg_print_nodes(cfg_list_t *);


/*
 * Build the CFG and output it as a VCG file
 */
cfg_list_t *cfg_build(void) {
  cfg_list_t *cfg_lists;
  int i;

  cfg_lists = cfg_build_lists();

  for(i = 0; i < cfg_lists->num_graphs; i++)
    cfg_calculate_liveness(cfg_lists->graph[i]);

  return cfg_lists;
}

/*
 * Create a new cfg node
 */
static cfg_node_t *cfg_mk_node(int type, ast_node_t *start) {

  cfg_nodes = realloc(cfg_nodes, sizeof(cfg_node_t *) * (num_cfg_nodes + 1));
  cfg_nodes[num_cfg_nodes] = malloc(sizeof(cfg_node_t));

  cfg_nodes[num_cfg_nodes]->node_type = type;

  cfg_nodes[num_cfg_nodes]->num_edges = 0;
  cfg_nodes[num_cfg_nodes]->num_preds = 0;
  cfg_nodes[num_cfg_nodes]->edge = NULL;
  cfg_nodes[num_cfg_nodes]->pred = NULL;
  cfg_nodes[num_cfg_nodes]->edge_label = NULL;
  cfg_nodes[num_cfg_nodes]->flow_edge_set = 0;

  cfg_nodes[num_cfg_nodes]->num_in = 0;
  cfg_nodes[num_cfg_nodes]->num_out = 0;
  cfg_nodes[num_cfg_nodes]->in = NULL;
  cfg_nodes[num_cfg_nodes]->out = NULL;

#if 0
  cfg_nodes[num_cfg_nodes]->ast_node = start;
#endif

  cfg_nodes[num_cfg_nodes]->num_var_use = 0;
  cfg_nodes[num_cfg_nodes]->var_use = NULL;
  cfg_nodes[num_cfg_nodes]->var_def = NULL;

  return cfg_nodes[num_cfg_nodes++];
}

/*
 * Create a new cfg graph
 */
static cfg_graph_t *cfg_mk_graph(void) {
  cfg_graph_t *graph;

  graph = malloc(sizeof(cfg_graph_t));
  graph->first = NULL;
  graph->last = NULL;

  graph->num_nodes = 0;
  graph->nodes = NULL;

  return graph;
}


/*
 * Add a variable to the use list
 */
static void cfg_add_var_use(cfg_node_t *node, name_record_t *var) {
  int i;

  debug_printf(1, "Adding use of variable %s to CFG\n", var->name);

  /* Don't add the same var twice to the use list */
  for(i = 0; i < node->num_var_use; i++)
    if(node->var_use[i] == var)
      return;

  if(!node->var_use)
    node->var_use = malloc(sizeof(name_record_t *));
  else
    node->var_use = realloc(node->var_use, sizeof(name_record_t *) *
			       (node->num_var_use + 1));

  node->var_use[node->num_var_use++] = var;
}

/*
 * Add an edge to a cfg node
 */
static void cfg_add_edge(cfg_node_t *source, cfg_node_t *target, int label) {

  /* Add successor edge (source->target) */
  if(!source->edge) {
    source->edge = malloc(sizeof(cfg_node_t *));
    source->edge_label = malloc(sizeof(int));

  } else {
    source->edge = realloc(source->edge,
			   sizeof(cfg_node_t *) * (source->num_edges + 1));
    source->edge_label = realloc(source->edge_label,
				 sizeof(int) * (source->num_edges + 1));
  }

  source->flow_edge_set = 1;
  source->edge[source->num_edges] = target;
  source->edge_label[source->num_edges++] = label;

  /* Add predecessor edge (target->source) */
  if(!target->pred)
    target->pred = malloc(sizeof(cfg_node_t *));
  else
    target->pred = realloc(target->pred,
			   sizeof(cfg_node_t *) * (target->num_preds + 1));

  target->pred[target->num_preds++] = source;


}

/*
 * Add a node to a cfg graph
 */
static void cfg_add_graph_node(cfg_graph_t *graph, cfg_node_t *node) {
  if(!graph->num_nodes)
    graph->nodes = malloc(sizeof(cfg_node_t *));
  else
    graph->nodes = realloc(graph->nodes, sizeof(cfg_node_t *) *
			      (graph->num_nodes + 1));

  graph->nodes[graph->num_nodes++] = node;
}


/*
 * Build the cfg's for the entire program
 *
 * Calls cfg_build_func for each function in the ast.
 *
 */
cfg_list_t *cfg_build_lists(void) {
  cfg_list_t *list;
  int count, i, j;

  /* Init the cfg_nodes */
  num_cfg_nodes = 0;
  cfg_nodes = malloc(sizeof(cfg_node_t *));

  /*
   * Count the number of functions
   * TODO: Probably a better way of doing this, ie from the global nametable?
   */
  count = 0;
  for(i = 0; i < ast->node.num_children; i++)
    if(ast->node.children[i]->node.op == NODE_FUNC)
      count++;

  list = malloc(sizeof(cfg_list_t));
  list->num_graphs = count;
  list->graph = malloc(sizeof(cfg_graph_t) * count);

  count = 0;
  for(i = 0; i < ast->node.num_children; i++)
    if(ast->node.children[i]->node.op == NODE_FUNC)
      list->graph[count++] = cfg_build_func(ast->node.children[i]);

  return list;
}

/*
 * Returns a cfg graph representing a function
 */
cfg_graph_t *cfg_build_func(ast_node_t *func) {
  ast_node_t *block;
  cfg_graph_t *graph, *block_graph;
  cfg_node_t *entry_node, *exit_node;
  scope_node_t *func_scope;
  char *func_name;
  int i, j;

  func_name = get_name_from_node(func);
  func_scope = get_func_arg_scope(func_name);

  /* Enter function scope */
  current_scope = enter_function_scope(func_name);

  /* Create a node for the function entry */
  entry_node = cfg_mk_node(CFG_NODE_FUNC_ENTRY, func);

  for(i = 0; i < func_scope->num_records; i++)
    cfg_add_var_use(entry_node, func_scope->name_table[i]);

  block = func->node.children[func->node.num_children - 1];
  block_graph = cfg_build_block(block);

  /* Create a node for the function exit */
  exit_node = cfg_mk_node(CFG_NODE_FUNC_EXIT, func);

  /*
   * Add any variables that are being passed by reference (ie pointers)
   * to the use list of the function exit node
   */
  for(i = 0; i < func_scope->num_records; i++)
    if(is_address_type(func_scope->name_table[i]->type_info))
      cfg_add_var_use(exit_node, func_scope->name_table[i]);



  /* Build and return the graph */
  graph = cfg_mk_graph();
  cfg_add_graph_node(graph, entry_node);
  for(i = 0; i < block_graph->num_nodes; i++)
    cfg_add_graph_node(graph, block_graph->nodes[i]);
  cfg_add_graph_node(graph, exit_node);

  /* Connect the func entry and exit nodes */
  cfg_add_edge(entry_node, graph->nodes[1], CFG_EDGE_NORMAL);
  if(graph->num_nodes > 2)
    cfg_add_edge(graph->nodes[graph->num_nodes - 2], exit_node,
		 CFG_EDGE_NORMAL);

  /* Exit function scope */
  current_scope = global_scope;

  return graph;
}

/*
 *
 */
cfg_graph_t *cfg_build_block(ast_node_t *block) {
  cfg_graph_t **block_statement;
  cfg_graph_t *graph;
  int i, j;

  /* Enter block scope */
  if(block->node.scope_tag != -1)
    current_scope = current_scope->children[block->node.scope_tag];

  graph = cfg_mk_graph();
  block_statement = malloc(sizeof(cfg_graph_t *) *
			   block->node.num_children);

  /* Build sub-graphs for each statement in the block */
  for(i = 0; i < block->node.num_children; i++)
    block_statement[i] = cfg_build_statement(block->node.children[i]);

  /* Merge the block statements into one big graph */
  for(i = 0; i < block->node.num_children; i++)
    for(j = 0; j < block_statement[i]->num_nodes; j++)
      cfg_add_graph_node(graph, block_statement[i]->nodes[j]);

  /* Connect the edges */
  for(i = 1; i < graph->num_nodes; i++)
    if((graph->nodes[i - 1]->node_type == CFG_NODE_STATEMENT ||
	graph->nodes[i - 1]->node_type == CFG_NODE_FUNC_ENTRY ||
	graph->nodes[i - 1]->node_type == CFG_NODE_CONDITIONAL_EXIT) &&
       !graph->nodes[i - 1]->flow_edge_set)
      cfg_add_edge(graph->nodes[i - 1], graph->nodes[i], CFG_EDGE_NORMAL);

  /* Exit block scope */
  if(block->node.scope_tag != -1)
    current_scope = current_scope->prev;

  return graph;
}

/*
 * Returns a cfg graph representing a statement
 *
 * For an assignment statement, a graph containing a single node is returned.
 * For compound statements such as while, if, etc a graph is returned.
 *
 */
cfg_graph_t *cfg_build_statement(ast_node_t *statement) {
  cfg_graph_t *graph, *block_graph;
  cfg_node_t *entry_node, *exit_node;
  ast_node_t *block, **leaf_list;
  name_record_t *var;
  int num_leaves, i, j;

  graph = cfg_mk_graph();

  switch(statement->node.op) {
  case NODE_BECOMES:
  case NODE_RETURN:
  case NODE_FUNC_CALL:
    /* Simple statements */

    entry_node = cfg_mk_node(CFG_NODE_STATEMENT, statement);
    if(statement->node.op != NODE_DECL && statement->node.op != NODE_FUNC_CALL)
      entry_node->var_def = get_var_entry(get_name_from_node(statement));

    /* Var uses */
    if(statement->node.op == NODE_BECOMES)
      block = statement->node.children[1];
    else
      block = statement;

    if(block) {
      num_leaves = 0;
      leaf_list = malloc(sizeof(ast_node_t **));
      leaf_list = get_leaf_list(block, &leaf_list, &num_leaves);

      for(i = 0; i < num_leaves; i++)
	if(leaf_list[i]->node_type == NODE_LEAF_ID) {

	  /* Don't add struct or function names */
	  var = get_var_entry(leaf_list[i]->leaf.name);
	  if(utype_index(var) == -1 &&
	     var->type_info->decl_type != TYPE_FUNCTION)
	    cfg_add_var_use(entry_node, var);

	}
    }

    cfg_add_graph_node(graph, entry_node);

    break;

  case NODE_FOR:
  case NODE_WHILE:
    /* Conditional Loops */

    entry_node = cfg_mk_node(CFG_NODE_CONDITIONAL, statement);
    exit_node = cfg_mk_node(CFG_NODE_CONDITIONAL_EXIT, NULL);

    /* Add def for for loops */
    if(statement->node.op == NODE_FOR)
      entry_node->var_def = get_var_entry(get_name_from_node(statement));

    /* Var uses */
    num_leaves = 0;
    leaf_list = malloc(sizeof(ast_node_t **));
    leaf_list = get_leaf_list(statement->node.children[0], &leaf_list,
			      &num_leaves);

    for(i = 0; i < num_leaves; i++)
      if(leaf_list[i]->node_type == NODE_LEAF_ID)
	cfg_add_var_use(entry_node, get_var_entry(leaf_list[i]->leaf.name));

    block = statement->node.children[statement->node.num_children - 1];
    block_graph = cfg_build_block(block);

    cfg_add_graph_node(graph, entry_node);
    for(i = 0; i < block_graph->num_nodes; i++)
      cfg_add_graph_node(graph, block_graph->nodes[i]);
    cfg_add_graph_node(graph, exit_node);

    /* Connect the loop true and false edges */
    if(graph->num_nodes > 2)
      cfg_add_edge(entry_node, graph->nodes[1], CFG_EDGE_TRUE);
    cfg_add_edge(entry_node, exit_node, CFG_EDGE_FALSE);

    /* Connect the loop return */
    cfg_add_edge(graph->nodes[graph->num_nodes - 2], entry_node,
		 CFG_EDGE_LOOP);

    break;

  case NODE_IF:
    /* Conditional Control Flow */

    entry_node = cfg_mk_node(CFG_NODE_CONDITIONAL, statement);
    exit_node = cfg_mk_node(CFG_NODE_CONDITIONAL_EXIT, NULL);

    /* Var uses */
    num_leaves = 0;
    leaf_list = malloc(sizeof(ast_node_t **));
    leaf_list = get_leaf_list(statement->node.children[0], &leaf_list,
			      &num_leaves);

    for(i = 0; i < num_leaves; i++)
      if(leaf_list[i]->node_type == NODE_LEAF_ID)
	cfg_add_var_use(entry_node, get_var_entry(leaf_list[i]->leaf.name));

    block = statement->node.children[statement->node.num_children - 1];
    block_graph = cfg_build_block(block);

    cfg_add_graph_node(graph, entry_node);
    for(i = 0; i < block_graph->num_nodes; i++)
      cfg_add_graph_node(graph, block_graph->nodes[i]);
    cfg_add_graph_node(graph, exit_node);

    /* Connect the control flow edges */
    cfg_add_edge(entry_node, graph->nodes[1], CFG_EDGE_TRUE);
    cfg_add_edge(entry_node, exit_node, CFG_EDGE_FALSE);
    cfg_add_edge(graph->nodes[graph->num_nodes - 2], exit_node,
		 CFG_EDGE_NORMAL);

  default:
    break;
  }

  return graph;

}

/*
 * Add a variable to a nodes in list
 */
int cfg_add_in_var(cfg_node_t *node, name_record_t *var) {
  int i;

  if(!node->in)
    node->in = malloc(sizeof(name_record_t *));
  else {
    for(i = 0; i < node->num_in; i++)
      if(node->in[i] == var)
	return 0;
    node->in = realloc(node->in, sizeof(name_record_t *) * (node->num_in + 1));
  }

  node->in[node->num_in++] = var;
  return 1;
}

/*
 * Add a variable to a nodes out list
 */
int cfg_add_out_var(cfg_node_t *node, name_record_t *var) {
  int i;

  if(!node->out)
    node->out = malloc(sizeof(name_record_t *));
  else {
    for(i = 0; i < node->num_out; i++)
      if(node->out[i] == var)
	return 0;
    node->out = realloc(node->out, sizeof(name_record_t *) *
			(node->num_out + 1));
  }

  node->out[node->num_out++] = var;
  return 1;
}

/*
 * Calculate the liveness for each variable in the given CFG.
 *
 * Calculates the live in and out sets.
 *
 */
void cfg_calculate_liveness(cfg_graph_t *graph) {
  cfg_node_t *current;
  int i, j, k, changed;

  /* Initialise the in set: in(n) = use(n) */
  for(i = 0; i < graph->num_nodes; i++)
    for(j = 0; j < graph->nodes[i]->num_var_use; j++)
      cfg_add_in_var(graph->nodes[i], graph->nodes[i]->var_use[j]);

  debug_printf(1, "Calculating liveness: ");
  changed = 1;

  while(changed) {
    changed = 0;
    debug_printf(1, ".");
    for(i = 0; i < graph->num_nodes; i++) {
      current = graph->nodes[i];

      /* Add all out(n) that are not in def(n) to in(n) */
      for(j = 0; j < current->num_out; j++)
	if(current->out[j] != current->var_def)
	  if(cfg_add_in_var(current, current->out[j]))
	    changed = 1;

      /* Foreach pred p of n: out(p) = out(p) + in(n) */
      for(j = 0; j < current->num_preds; j++)
	for(k = 0; k < current->num_in; k++)
	  if(cfg_add_out_var(current->pred[j], current->in[k]))
	    changed = 1;

    }
  }
  debug_printf(1, "\n");
}


/*
 * Output a VCG graph file
 */
void vcg_output_cfg(cfg_list_t *cfg_list, char *basename) {
  FILE *fd;
  char *filename;
  cfg_node_t *current;
  int i, j, k;

  filename = malloc(strlen(basename) + 9);
  strcpy(filename, basename);
  strcat(filename, ".cfg.vcg");

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
  for(i = 0; i < num_cfg_nodes; i++) {
    current = cfg_nodes[i];

    fprintf(fd, "node: { title:\"%p\"\n\tlabel: \"", current);
    switch(current->node_type) {
    case CFG_NODE_FUNC_ENTRY:
      fprintf(fd, "function entry");
      break;
    case CFG_NODE_FUNC_EXIT:
      fprintf(fd, "function exit");
      break;
    case CFG_NODE_CONDITIONAL_EXIT:
      fprintf(fd, "conditional exit");
      break;
    default:
#if 0
      fprintf(fd, "%s", ast_node_name[current->ast_node->node.op]);
#endif
      break;
    }

    /* defs */
    if(current->var_def)
      fprintf(fd, ", def={%s}", current->var_def->name);

    /* uses */
    if(current->var_use) {
      fprintf(fd, "\nuse={");
      for(j = 0; j < current->num_var_use; j++)
	fprintf(fd, "%s%s", current->var_use[j]->name,
		(j == (current->num_var_use - 1)) ? "}" : ", ");
    }

    /* ins */
    if(current->in) {
      fprintf(fd, "\nins={");
      for(j = 0; j < current->num_in; j++)
	fprintf(fd, "%s%s", current->in[j]->name,
		(j == (current->num_in - 1)) ? "}" : ", ");
    }

    /* outs */
    if(current->out) {
      fprintf(fd, "\nouts={");
      for(j = 0; j < current->num_out; j++)
	fprintf(fd, "%s%s", current->out[j]->name,
		(j == (current->num_out - 1)) ? "}" : ", ");
    }


    fprintf(fd, "\" ");
    switch(current->node_type) {
      case CFG_NODE_FUNC_ENTRY:
    case CFG_NODE_FUNC_EXIT:
      fprintf(fd, "shape: ellipse color: yellow }\n");
      break;
    case CFG_NODE_CONDITIONAL:
      fprintf(fd, "shape: rhomb color: lightgreen }\n");
      break;
    case CFG_NODE_CONDITIONAL_EXIT:
      fprintf(fd, "color: darkblue textcolor: red }\n");
      break;
    default:
      fprintf(fd, "}\n");
      break;
    }
  }

  /* Edges */
  fprintf(fd, "\n\n");
  for(i = 0; i < num_cfg_nodes; i++) {
    current = cfg_nodes[i];

    for(j = 0; j < current->num_edges; j++) {
      if(current->edge_label[j] == CFG_EDGE_TRUE ||
	 current->edge_label[j] == CFG_EDGE_FALSE)
	fprintf(fd, "bentnearedge: ");
      else if(current->edge_label[j] == CFG_EDGE_LOOP)
	fprintf(fd, "backedge: ");
      else
	fprintf(fd, "edge: ");

      fprintf(fd, "{ sourcename:\"%p\" targetname:\"%p\" ", current,
	      current->edge[j]);

      if(current->edge_label[j] == CFG_EDGE_TRUE)
	fprintf(fd, "label: \"true\"");
      else if(current->edge_label[j] == CFG_EDGE_FALSE)
	fprintf(fd, "label: \"false\"");
      else if(current->edge_label[j] == CFG_EDGE_LOOP)
	fprintf(fd, "label: \"loop\"");

      fprintf(fd, "}\n");

    }
  }

  /* Footer */
  fprintf(fd, "\n}\n");
  close(fd);
}

/*
 * Print CFG nodes
 */
void cfg_print_nodes(cfg_list_t *cfg_list) {
  int i, j, k;
  cfg_node_t *current;

  for(i = 0; i < cfg_list->num_graphs; i++) {

    debug_printf(1, "Function(%d)\n", i);

    for(j = 0; j < cfg_list->graph[i]->num_nodes; j++) {
      current = cfg_list->graph[i]->nodes[j];

#if 0
      debug_printf(1, "    Node(%d): op = %s\n", j,
		   ast_node_name[current->ast_node->node.op]);
#endif

      if(current->var_def)
	debug_printf(1, "\tdef var = %s\n", current->var_def->name);

      if(current->var_use) {
	debug_printf(1, "\tuse vars = ");
	for(k = 0; k < current->num_var_use; k++)
	  debug_printf(1, "%s%s", current->var_use[k]->name,
		       (k == (current->num_var_use - 1)) ? "\n" : ", ");

      }

    }

  }

}
