/*
 * regalloc.c
 * Ryan Mallon (2006)
 *
 * Interference graph and register allocation
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "symtable.h"
#include "cfg.h"
#include "mir.h"
#include "mir_cfg.h"
#include "scope.h"
#include "regalloc.h"
#include "tempreg.h"
#include "cerror.h"
#include "icg.h"
#include "cflags.h"

typedef struct {
  name_record_t **vars;
  int num_vars;
} name_list_t;

typedef struct {
  int offset;
  int temp_reg;
} stack_loc_t;

typedef struct {
  int num_regs;
  stack_loc_t *reg;
} stack_loc_list_t;

extern compiler_options_t cflags;
extern char *basename;
extern scope_node_t *global_scope;

/* The adjacency node stack */
static int *stack;
static int sp;

/* Temporary stack locations */
static stack_loc_list_t *stack_temps = NULL;

/* Static functions */
static void adjust_neighbours(int, adj_node_t **, int);
static adj_node_t **build_adj_lists(ig_graph_t *);
static void free_adj_list(adj_node_t **, int);
static int min_colour(adj_node_t **, int, int, int);
static int assign_regs(adj_node_t **, int, int);
static int compare_ints(const void *, const void *);
static void compute_spill_costs(ig_graph_t *, adj_node_t **);
static void modify_code(adj_node_t **);
static void gen_spill_code(adj_node_t **, int);

void global_load_store(mcfg_list_t *);

static void init_temp_stack_locations(int);
static void free_temp_stack_locations(void);
static int assign_stack_location(int, int);
static int temp_stack_location(char *, int);
static void add_temps_to_scopes(void);

static void ig_prune(ig_graph_t *, int, adj_node_t **);
static void ig_add_link(ig_graph_t *, mcfg_var_t *, mcfg_var_t *);
static int ig_are_linked(ig_graph_t *, mcfg_var_t *, mcfg_var_t *);

/*
 * Register allocator
 */
void allocate_registers(mcfg_list_t *cfg, int nregs) {
  adj_node_t **adj_lists;
  ig_graph_t *ig_graph;
  int i, j, finished, passes = 1;
  char ext[10];

  /* FIXME: make_du_chains(); make_webs(); */


  finished = 0;
  while(!finished) {

    debug_printf(1, "----\nRegister allocation pass %d (hard regs = %d)\n----\n",
		 passes, nregs);

    /* Calculate def/use sets and liveness */
    for(i = 0; i < cfg->num_graphs; i++)
      mcfg_build_defuse(cfg->graph[i]);

    //global_load_store(cfg);

    for(i = 0; i < cfg->num_graphs; i++)
      mcfg_build_defuse(cfg->graph[i]);

    for(i = 0; i < cfg->num_graphs; i++)
      mcfg_calculate_liveness(cfg->graph[i]);

    sprintf(ext, "cfg%d.vcg", passes);
    vcg_output_mcfg(cfg, basename, ext);

    /* Construct the interference graph and build the adjacency lists */
    ig_graph = ig_build_graph(cfg);
    adj_lists = build_adj_lists(ig_graph);

    sprintf(ext, "ig%d.vcg", passes);
    vcg_output_ig(ig_graph, basename, ext);

    tr_set_lowest(ig_graph->num_sym_regs);
    mir_to_sym_lir(ig_graph);

    /* Output the IG and LIR code */
    sprintf(ext, "lir%d", passes);
    mir_print(basename, ext);

    compute_spill_costs(ig_graph, adj_lists);

    debug_printf(1, "Allocating for %d sym regs:\n", ig_graph->num_sym_regs);

#if 0
    for(i = 0; i < ig_graph->num_sym_regs; i++) {
      if(ig_graph->sym_reg[i]->type == MCFG_TYPE_VAR)
	debug_printf(1, "%s, ", ig_graph->sym_reg[i]->var->name);
      else
	debug_printf(1, "%%t%d, ", ig_graph->sym_reg[i]->reg);
    }
    debug_printf(1, "\n");
#endif

    ig_prune(ig_graph, nregs, adj_lists);
    if(!(finished = assign_regs(adj_lists, ig_graph->num_sym_regs, nregs)))
      gen_spill_code(adj_lists, ig_graph->num_sym_regs);

    if(!finished)
      free_adj_list(adj_lists, ig_graph->num_sym_regs);

    passes++;

    if(passes > 100)
      compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		     "Too many register allocation passes, aborting\n");
  }

  /* Add temporary stack locations to the scope table */
  add_temps_to_scopes();

  /* Print out the register allocation */
  for(i = 0; i < nregs; i++) {
    debug_printf(1, "Reg %d: ", i);
    for(j = 0; j < ig_graph->num_sym_regs; j++)
      if(adj_lists[j]->colour == i + 1)
	debug_printf(1, "s%d, ", j);
    debug_printf(1, "\n");
  }

  for(i = 0; i < ig_graph->num_sym_regs; i++)
    if(adj_lists[i]->var && adj_lists[i]->var->var)
      adj_lists[i]->var->var->reg_alloc = adj_lists[i]->colour;

  modify_code(adj_lists);

  mir_print(basename, "lir-ra");
  vcg_output_mcfg(cfg, basename, "cfg-ra.vcg");

}

/*
 * Build the adjacency lists
 */
static adj_node_t **build_adj_lists(ig_graph_t *graph) {
  adj_node_t **lists;
  int i, j, count;

  lists = malloc(sizeof(adj_node_t *) * graph->num_sym_regs);
  for(i = 0; i < graph->num_sym_regs; i++) {
    lists[i] = malloc(sizeof(adj_node_t));

    lists[i]->colour = 0;


    lists[i]->offset = -1;


    lists[i]->spill_cost = 10;
    lists[i]->spill = 0;
    lists[i]->on_stack = 0;

    lists[i]->var = graph->sym_reg[i];


    /* Connected nodes */
    lists[i]->adj_node = malloc(sizeof(int) * graph->num_sym_regs);
    lists[i]->rem_node = malloc(sizeof(int) * graph->num_sym_regs);

    lists[i]->num_adjs = 0;
    lists[i]->num_rems = 0;

    for(j = 0; j < graph->num_sym_regs; j++) {
      lists[i]->adj_node[j] = 0;
      lists[i]->rem_node[j] = 0;
    }

    for(j = 0; j < graph->num_sym_regs; j++)
      if(ig_are_linked(graph, graph->sym_reg[i], graph->sym_reg[j])) {
	lists[i]->adj_node[j] = 1;
	lists[i]->num_adjs++;
      }
  }

  return lists;
}

/*
 * Free the adjacency lists
 */
static void free_adj_list(adj_node_t **adj_list, int sregs) {
  int i;

  for(i = 0; i < sregs; i++) {
    if(adj_list[i]->adj_node)
      free(adj_list[i]->adj_node);
    if(adj_list[i]->rem_node)
      free(adj_list[i]->rem_node);
    free(adj_list[i]);
  }
  free(adj_list);
}



/*
 * Prune the interference graph by applying an R-colouring to it
 */
static void ig_prune(ig_graph_t *graph, int nregs, adj_node_t **adj_list) {
  int i, nodes_left, finished, spill_cost, spill_node;

  stack = malloc(sizeof(int) * graph->num_sym_regs);
  sp = 0;
  nodes_left = graph->num_sym_regs;

  while(nodes_left > 0) {

    /*
     * Apply the degree < R rule and push nodes onto the stack
     */
    finished = 0;
    while(!finished) {
      finished = 1;

      for(i = 0; i < graph->num_sym_regs; i++) {

	if(!adj_list[i]->on_stack && adj_list[i]->num_adjs < nregs) {

	  finished = 0;
	  adj_list[i]->on_stack = 1;
	  stack[sp++] = i;

	  adjust_neighbours(graph->num_sym_regs, adj_list, i);
	  nodes_left--;
	}
      }
    }

    if(nodes_left) {
      /*
       * Find the node with the lowest spill cost divided by its degree
       * and push it onto the stack. Start with a suitably large value
       * for spill_cost.
       */
      spill_cost = 100;
      spill_node = -1;

      for(i = 0; i < graph->num_sym_regs; i++) {
	if(!adj_list[i]->on_stack &&
	   adj_list[i]->spill_cost > 0 &&
	   (adj_list[i]->spill_cost / adj_list[i]->num_adjs) < spill_cost) {

	  spill_node = i;
	  spill_cost = adj_list[i]->spill_cost / adj_list[i]->num_adjs;
	}
      }

      if(spill_node == -1) {
	debug_printf(1, "====\n");
	for(i = 0; i < graph->num_sym_regs; i++)
	  debug_printf(1, "Reg %3d: Spill cost = %4d, adjacent = %3d, %s [%s]\n", i,
		       adj_list[i]->spill_cost, adj_list[i]->num_adjs,
		       adj_list[i]->on_stack ? "on stack" :
		       "not on stack", adj_list[i]->var->var ?
		       adj_list[i]->var->var->name : "nil");

	compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		       "Failed to find spillable node for regalloc\n");
      }


      debug_printf(1, "Selected node %d as spill candidate, cost = %d\n",
		   spill_node, adj_list[spill_node]->spill_cost);

      adj_list[spill_node]->on_stack = 1;
      stack[sp++] = spill_node;
      adjust_neighbours(graph->num_sym_regs, adj_list, spill_node);
      nodes_left--;
    }
  }
}

/*
 * Compute the cost of spilling each temporary
 * FIXME: Doesn't take nesting level into account yet.
 */
static void compute_spill_costs(ig_graph_t *graph, adj_node_t **adj_list) {
  mir_node_t *current = mir_list_head();
  mir_instr_t *instr;
  int i, *copies, *defs, *uses;

  copies = calloc(graph->num_sym_regs, sizeof(int));
  defs = calloc(graph->num_sym_regs, sizeof(int));
  uses = calloc(graph->num_sym_regs, sizeof(int));

  while(current) {
    instr = current->instruction;

    switch(instr->opcode) {
    case MIR_CALL:
      for(i = 0; i < instr->num_args; i++)
	if(instr->args[i]->optype != MIR_OP_CONST)
	  uses[instr->args[i]->val]++;

      if(instr->operand[2])
	defs[instr->operand[2]->val]++;

      break;

    case MIR_MOVE:
      if(instr->operand[0]->optype != MIR_OP_CONST)
        copies[instr->operand[0]->val]++;

      if(instr->operand[2]->optype != MIR_OP_CONST)
        defs[instr->operand[2]->val]++;
      break;

    case MIR_STACK_LOAD:
    case MIR_HEAP_LOAD:
    case MIR_REG_LOAD:
      defs[instr->operand[2]->val] += 10;
      break;

    case MIR_STACK_STORE:
    case MIR_HEAP_STORE:
    case MIR_REG_STORE:
      uses[instr->operand[0]->val] += 10;
      break;

    case MIR_ADDR:
    case MIR_HEAP_ADDR:
    case MIR_STACK_ADDR:
      defs[instr->operand[2]->val] += 10;
      break;

    default:
      if(instr->operand[2] && instr->operand[2]->optype != MIR_OP_CONST)
	defs[instr->operand[2]->val]++;

      for(i = 0; i < 2; i++)
	if(instr->operand[i] && instr->operand[i]->optype != MIR_OP_CONST)
	  uses[instr->operand[i]->val]++;

      break;
    }

    current = current->next;
  }

  /* Add up the spill costs */
  for(i = 0; i < graph->num_sym_regs; i++)
    adj_list[i]->spill_cost = copies[i] + defs[i] + uses[i];


  free(copies);
  free(defs);
  free(uses);
}

/*
 * Move the neighbours of the given node to its removed list and then
 * disconnect the given node from its neighbours
 */
static void adjust_neighbours(int sregs, adj_node_t **adj_list, int node) {
  int i, j;

  /* Don't need to do anything if this node has no neighbours */
  if(!adj_list[node]->adj_node)
    return;

  for(i = 0; i < sregs; i++) {
    if(adj_list[node]->adj_node[i]) {
      adj_list[node]->adj_node[i] = 0;
      adj_list[node]->rem_node[i] = 1;

      adj_list[node]->num_adjs--;
    }

    /* Disconnect from other nodes */
    if(adj_list[i]->adj_node[node]) {
      adj_list[i]->adj_node[node] = 0;
      adj_list[i]->rem_node[node] = 1;

      adj_list[i]->num_adjs--;
    }
  }
}

/*
 * Assign symbolic registers to real registers.
 */
static int assign_regs(adj_node_t **adj_list, int sregs, int nregs) {
  int i, node;
  int colour, no_spills;

  /* Pop nodes from the stack and attempt to assign a colour to them */
  no_spills = 1;
  while(sp) {

    node = stack[--sp];
    adj_list[node]->on_stack = 0;

    /* Reconnect to other nodes that aren't on the stack */
    for(i = 0; i < sregs; i++) {
      if(adj_list[node]->rem_node[i] && !adj_list[i]->on_stack) {

	adj_list[node]->adj_node[i] = 1;
	adj_list[node]->rem_node[i] = 0;

	adj_list[node]->num_adjs++;
      }

      if(adj_list[i]->rem_node[node] && !adj_list[i]->on_stack) {
	adj_list[i]->adj_node[node] = 1;
	adj_list[i]->rem_node[node] = 0;

	adj_list[i]->num_adjs++;
      }


    }

    if((colour = min_colour(adj_list, node, sregs, nregs)) > 0) {
      adj_list[node]->colour = colour;
      adj_list[node]->spill = 0;

    } else {
      /* Cant colour this node, mark it for spilling */
      debug_printf(1, "Cannot assign a colour to node %d, spill it\n", node);
      adj_list[node]->spill = 1;
      no_spills = 0;
    }
  }

  /* Finsihed with the stack */
  free(stack);

  return no_spills;
}

/*
 * Return the next available colour for the given node, or -1 if no more
 * colours are available.
 *
 * FIXME: This is slow
 *
 */
static int min_colour(adj_node_t **adj_list, int node, int sregs, int nregs) {
  int i, j, *used_colours, num_used, colour, duplicate;

  /* Build a list of used colours for this node */
  used_colours = malloc(sizeof(int) * nregs);
  num_used = 0;

  for(i = 0; i < sregs; i++)
    if(adj_list[node]->adj_node[i] && adj_list[i]->colour) {
      /* Don't add duplicates */
      for(j = 0, duplicate = 0; j < i && j < num_used; j++)
	if(used_colours[j] == adj_list[i]->colour)
	  duplicate = 1;

      if(!duplicate)
	used_colours[num_used++] = adj_list[i]->colour;
    }

  /* Sort the list in ascending order */
  qsort(used_colours, num_used, sizeof(int), compare_ints);

#if 0
  debug_printf(1, "Sorted colour list [%d]: ", node);
  for(i = 0; i < num_used; i++)
    debug_printf(1, "%d, ", used_colours[i]);
#endif

  /* Find the lowest available colour */
  if(num_used == 0)
    colour = 1;
  else {
    colour = 0;
    for(i = 0; i < num_used; i++)
      if(used_colours[i] != i + 1) {
	colour = i + 1;
	break;
      }

    if(!colour && num_used < nregs)
      colour = i + 1;
  }

#if 0
  debug_printf(1, ": first free = %d\n", colour);
#endif

  free(used_colours);
  return colour;
}

/*
 * Compare two ints for the qsort in min_colour
 */
static int compare_ints(const void *a, const void *b) {
  return *(int *)a > *(int *)b;
}

/*
 * Add load/store code for globals so they can be stored in registers. Globals
 * are loaded at the begining and stored at the end of each function. Globals
 * must also be loaded/stored before and after function calls
 */

/*
 * Generate load/store code for globals so they can be register allocated
 */
void global_load_store(mcfg_list_t *cfg) {
  mir_node_t *current = mir_list_head();
  mir_instr_t *instr;
  int i, index;
  mcfg_graph_t *graph;

  index = 0;

  while(current) {
    /* Add loads after the last argument is pushed */
    while(current->instruction->opcode == MIR_LABEL ||
          current->instruction->opcode == MIR_BEGIN ||
          current->instruction->opcode == MIR_PUSH_ARG)
      current = current->next;

    graph = cfg->graph[index++];

    /* Load globals at the start of each function */
    for(i = 0; i < graph->num_globals; i++)
      mcfg_insert_node(mir_add_instr(current->prev, MIR_HEAP_LOAD,
				     mir_var(graph->global[i]),
                                     NULL, mir_reg(tr_alloc())));



    while(current->instruction->opcode != MIR_END) {

      /* Generate loads before and stores after each function call */
      if(current->instruction->opcode == MIR_CALL) {
         /* Loads */
        for(i = 0; i < graph->num_globals; i++)
          mcfg_insert_node(mir_add_instr(current->prev, MIR_HEAP_LOAD,
					 mir_var(graph->global[i]), NULL,
                                         mir_reg(tr_alloc())));

        /* Stores */
        for(i = 0; i < graph->num_globals; i++)
          mcfg_insert_node(mir_add_instr(current, MIR_HEAP_STORE,
                                         mir_var(graph->global[i]),
                                         mir_reg(tr_alloc()),
                                         mir_reg(tr_alloc())));

      }

      current = current->next;
    }

#if 0
    /* Store globals at the end of each function */
    for(i = 0; i < graph->num_globals; i++)
      mcfg_insert_node(mir_add_instr(current->prev, MIR_HEAP_STORE,
				     mir_reg(tr_alloc()), mir_reg(tr_alloc()),
                                     mir_var(graph->global[i])));
#endif

    current = current->next;
  }
}


/*
 * Modify the MIR code, replacing symbolic registers with real ones
 */
void modify_code(adj_node_t **adj_list) {
  mir_node_t *current = mir_list_head();
  mir_instr_t *instr;
  int i;

  while(current) {

    instr = current->instruction;

    for(i = 0; i < 3; i++) {
      if(instr->operand[i] && instr->operand[i]->optype == MIR_OP_REG &&
	 adj_list[instr->operand[i]->val]->colour) {

	instr->operand[i]->val =
	  adj_list[instr->operand[i]->val]->colour - 1;
      }
    }

    /* Function arguments */
    if(instr->opcode == MIR_CALL)
      for(i = 0; i < instr->num_args; i++)
	if(instr->args[i]->optype == MIR_OP_REG &&
	   adj_list[instr->args[i]->val]->colour) {

	instr->args[i]->val =
	  adj_list[instr->args[i]->val]->colour - 1;
	}

    current = current->next;
  }
}

/*
 * Generate a spill load.
 * Returns a pointer to the newly created node, or null if an existing
 * node is modified.
 */
static mir_node_t *gen_spill_load(mir_node_t *mir_node, adj_node_t *spill_node,
				  char *func, int dest) {
  mir_node_t *new_node;
  int opcode, offset;

  if(spill_node->var->var) {

    offset = spill_node->var->var->offset;

    if(get_var_scope(spill_node->var->var) == global_scope)
      opcode = MIR_HEAP_LOAD;
    else
      opcode = MIR_STACK_LOAD;

  } else {
    opcode = MIR_STACK_LOAD;
    offset = temp_stack_location(func, spill_node->var->reg);
  }

  debug_printf(1, "Generating spill load: type = %s, offset = %d, dest reg = %d\n",
	       opcode == MIR_STACK_LOAD ? "stack load" : "heap load",
	       offset, dest);

#if 0
  if(mir_node->next->instruction->opcode == MIR_REG_LOAD) {
    /* If the instruction is already a reg load we just modify it */
    mir_node->next->instruction->opcode = opcode;
    free(mir_node->next->instruction->operand[0]);
    mir_node->next->instruction->operand[0] = mir_const(offset);

    return NULL;

  } else {
#endif
    new_node = mir_add_instr(mir_node, opcode, mir_const(offset), NULL,
			     mir_reg(dest));
    mcfg_insert_node(new_node);

#if 0
  }
#endif

  return new_node;
}

/*
 * Generate a spill store
 */
static mir_node_t *gen_spill_store(mir_node_t *mir_node,
				   adj_node_t *spill_node, char *func,
				   mir_operand_t *src) {
  mir_node_t *new_node;
  int opcode, offset;

  if(spill_node->var->var) {

    offset = spill_node->var->var->offset;

    if(get_var_scope(spill_node->var->var) == global_scope)
      opcode = MIR_HEAP_STORE;
    else
      opcode = MIR_STACK_STORE;

  } else {
    opcode = MIR_STACK_STORE;
    offset = temp_stack_location(func, spill_node->var->reg);
  }

  debug_printf(1, "Generating spill store: type = %s, offset = %d\n",
	       opcode == MIR_STACK_STORE ? "stack store" : "heap store",
	       offset);

#if 0
  if(mir_node->instruction->opcode == MIR_REG_STORE &&
     mir_node->instruction->operand[2] == src) {
    /* If the instruction is already a reg store we just modify it */
    mir_node->instruction->opcode = opcode;
    free(mir_node->instruction->operand[2]);
    mir_node->instruction->operand[2] = mir_const(offset);

  } else {
#endif
    new_node = mir_add_instr(mir_node, opcode, mir_opr_copy(src), NULL,
			     mir_const(offset));
    mcfg_insert_node(new_node);
#if 0
  }
#endif

  return new_node;

}

/*
 * Generate extra load/store instructions for spilled registers.
 */
static void gen_spill_code(adj_node_t **adj_list, int sregs) {
  mir_node_t *new_node, *current = mir_list_head();
  mir_instr_t *instr;
  mir_operand_t *dest;
  char *func_name;
  int i, j, reg, offset, loads, stores, opcode;

  debug_printf(1, "Generating spill code: %d sregs\n", sregs);

  loads = 0;
  stores = 0;

  /* TEMP: Count spills */
  for(i = 0, j = 0; i < sregs; i++)
    if(adj_list[i]->spill) j++;

  debug_printf(1, "Total spills  = %d\n", j);

  /* Initialise temporary stack locations */
  init_temp_stack_locations(sregs);

  while(current) {
    instr = current->instruction;

    if(instr->opcode == MIR_LABEL)
      func_name = instr->label;

    for(j = 0; j < sregs; j++) {

      if(adj_list[j]->spill) {

	if(instr->opcode == MIR_CALL) {
	  /* Arguments */
	  for(i = 0; i < instr->num_args; i++)
	    if(instr->args[i]->optype == MIR_OP_REG &&
	       instr->args[i]->val == j) {
	      debug_printf(1, "Generating load for call argument\n");

	    reg = tr_alloc();
	    gen_spill_load(current->prev, adj_list[j], func_name, reg);
	    free(instr->args[i]);
	    instr->args[i] = mir_reg(reg);
	    loads++;

#if 0
	    if(get_var_scope(adj_list[j]->var->var) == global_scope)
	      opcode = MIR_HEAP_LOAD;
	    else
	      opcode = MIR_STACK_LOAD;

	    new_node = mir_add_instr(current->prev, opcode,
				     mir_const(adj_list[j]->var->var->offset),
				     NULL, mir_reg(reg));
#endif

	    }
	}

	/* Use registers */
	for(i = 0; i < 2; i++) {
	  if(instr->operand[i] && instr->operand[i]->optype == MIR_OP_REG &&
	     instr->operand[i]->val == j) {

	    reg = tr_alloc();
	    if(gen_spill_load(current->prev, adj_list[j], func_name, reg)) {
	      free(instr->operand[i]);
	      instr->operand[i] = mir_reg(reg);
	    }

	    loads++;

	    break;
	  }
	}

	/* Def register */
	if(instr->operand[2] && instr->operand[2]->optype == MIR_OP_REG &&
	   instr->operand[2]->val == j) {

	  debug_printf(1, "Generating store for reg %d\n", j);

	  switch(instr->opcode) {


	  case MIR_RECEIVE:
	    debug_printf(1, "Replacing receive instruction with push arg\n");

	    instr->opcode = MIR_PUSH_ARG;
	    free(instr->operand[2]);
	    instr->operand[2] = mir_const(adj_list[j]->var->var->offset);

	    break;

	  default:
	    gen_spill_store(current, adj_list[j], func_name,
			    instr->operand[2]);

	    /* Skip the newly generated instruction */
	    current = current->next;

	    break;
	  }

	  stores++;
	}
      }
    }

    current = current->next;
  }

  debug_printf(1, "Generated %d load, %d store instructions\n", loads, stores);
}

/*
 * Initialise the temporary stack location lists
 */
static void init_temp_stack_locations(int sregs) {
  int i;

  if(!stack_temps) {
    stack_temps = malloc(num_def_functions() * sizeof(stack_loc_list_t));
    for(i = 0; i < num_def_functions(); i++) {
      stack_temps[i].reg = malloc(sregs * sizeof(stack_loc_t));
      stack_temps[i].num_regs = 0;
    }

  } else {
    for(i = 0; i < num_def_functions(); i++)
      stack_temps[i].reg = realloc(stack_temps[i].reg,
				   sregs * sizeof(stack_loc_t));
  }
}



/*
 * Free the temporary stack location lists
 */
static void free_temp_stack_locations(void) {
  int i;

  for(i = 0; i < num_def_functions(); i++)
    free(stack_temps[i].reg);
  free(stack_temps);
}

/*
 * Assign a stack location for spilling a temporary register
 */
static int assign_stack_location(int func_num, int temp_reg) {
  int offset, index = stack_temps[func_num].num_regs;

  stack_temps[func_num].reg[index].temp_reg = temp_reg;

  /* FIXME: Shouldn't assume 4 byte register size */
  if(index == 0)
    offset = 0;
  else
    offset = stack_temps[func_num].reg[index - 1].offset + 4;

  stack_temps[func_num].reg[index].offset = offset;
  stack_temps[func_num].num_regs++;

  return offset;
}

/*
 * Return the stack location for the given temporary. Assigns a new location
 * if one does not already exist.
 */
static int temp_stack_location(char *func_name, int temp_reg) {
  int i, index, base_offset;

  index = get_def_func_index(func_name);
  base_offset = max_scope_size(get_func_scope(func_name));

  debug_printf(1, "Looking up temp location for reg %d: ", temp_reg);

  for(i = 0; i < stack_temps[index].num_regs; i++)
    if(stack_temps[index].reg[i].temp_reg == temp_reg) {
      debug_printf(1, "found\n");
      return stack_temps[index].reg[i].offset + base_offset;
    }

  debug_printf(1, "creating new\n");
  return assign_stack_location(index, temp_reg) + base_offset;
}

/*
 * Add the temporary stack locations to the scope table
 * FIXME: Should add a new variable called ".temporaries" and give that
 * an appropriate size rather than just munging the size value of the scope.
 */
static void add_temps_to_scopes(void) {
  int i;
  scope_node_t *scope;

  if(!stack_temps)
    return;

  for(i = 0; i < num_def_functions(); i++) {
    scope = get_func_scope(get_def_func(i)->name);

    debug_printf(1, "Adding %d bytes to function scope %d\n",
		 stack_temps[i].num_regs * 4, i);
    /* FIXME: Shouldn't assume 4-byte register size */
    scope->size += stack_temps[i].num_regs * 4;
  }

  free_temp_stack_locations();
}




/*
 * Add a symbolic register to the IG
 */
static void ig_add_sym_reg(ig_graph_t *ig, mcfg_var_t *sym_reg) {
  int i;

  /* Don't add duplicates */
  for(i = 0; i < ig->num_sym_regs; i++)
    if(mcfg_var_match(ig->sym_reg[i], sym_reg)) {
      /* FIXME: free(sym_reg); */
      return;
    }

  if(!ig->sym_reg)
    ig->sym_reg = malloc(sizeof(name_record_t *));
  else
    ig->sym_reg = realloc(ig->sym_reg,
			  sizeof(name_record_t *) * (ig->num_sym_regs + 1));

  ig->sym_reg[ig->num_sym_regs++] = sym_reg;
}


/*
 * Add a link between two variables in the IG
 */
static void ig_add_link(ig_graph_t *ig, mcfg_var_t *sym_reg1,
			mcfg_var_t *sym_reg2) {
  int i, index1 = -1, index2 = -1;

  if(!sym_reg1 || !sym_reg2)
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Invalid sym_reg in ig_add_link\n");

  /* Don't need to link variables with themselves */
  if(mcfg_var_match(sym_reg1, sym_reg2))
    return;

  for(i = 0; i < ig->num_sym_regs; i++)
    if(mcfg_var_match(ig->sym_reg[i], sym_reg1))
      index1 = i;
    else if(mcfg_var_match(ig->sym_reg[i], sym_reg2))
      index2 = i;

  if(index1 == -1 || index2 == -1) {
    if(sym_reg1->type == MCFG_TYPE_VAR)
      debug_printf(1, "sym_reg1 = %s\n", sym_reg1->var->name);
    else
      debug_printf(1, "sym_reg1 = %%t%d\n", sym_reg1->reg);
    if(sym_reg2->type == MCFG_TYPE_VAR)
      debug_printf(1, "sym_reg2 = %s\n", sym_reg2->var->name);
    else
      debug_printf(1, "sym_reg2 = %%t%d\n", sym_reg2->reg);

    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Cannot find sym_reg in graph\n");
  }


  ig->links[index1][index2] = 1;
  ig->links[index2][index1] = 1;
}


/*
 * Test if two vars are connected in the IG
 */
static int ig_are_linked(ig_graph_t *ig, mcfg_var_t *sym_reg1,
			 mcfg_var_t *sym_reg2) {
  int i, index1, index2;

  /* Non-existant variables do not interfere */
  if(!sym_reg1 || !sym_reg2 || !ig->num_sym_regs)
    return 0;

  /* Variables are not linked to themselves */
  if(mcfg_var_match(sym_reg1, sym_reg2))
    return 0;

  for(i = 0; i < ig->num_sym_regs; i++)
    if(ig->sym_reg[i] == sym_reg1)
      index1 = i;
    else if(ig->sym_reg[i] == sym_reg2)
      index2 = i;

  return ig->links[index1][index2];
}

/*
 * Build the IG from the given CFG graph, each item in the IG is a
 * symbolic register.
 *
 * TODO: This will probably get slow for functions that have lots of vars/temps
 *
 * Currently this just treats each mcfg_var as a symbolic register. It should
 * use webs as symbolic registers, however this requires first
 * determining the reachibilty of the CFG, then construction du-chains and
 * finally building webs.
 */
ig_graph_t *ig_build_graph(mcfg_list_t *cfg_list) {
  ig_graph_t *ig_graph;
  mcfg_node_t *current;
  int i, j, k, l;

  /* Initialise the IG */
  ig_graph = malloc(sizeof(ig_graph_t));
  ig_graph->sym_reg = NULL;
  ig_graph->num_sym_regs = 0;
  ig_graph->num_links = 0;

  /* Add all variables */
  for(i = 0; i < cfg_list->num_graphs; i++) {
    for(j = 0; j < cfg_list->graph[i]->num_nodes; j++) {
      current = cfg_list->graph[i]->node[j];

      if(current->var_def)
	ig_add_sym_reg(ig_graph, current->var_def);
      for(k = 0; k < current->num_var_use; k++)
	ig_add_sym_reg(ig_graph, current->var_use[k]);

    }
  }

  if(!ig_graph->num_sym_regs)
    return;

  /* Initialise the links matrix */
  ig_graph->links = calloc(ig_graph->num_sym_regs, sizeof(int *));
  for(i = 0; i < ig_graph->num_sym_regs; i++)
    ig_graph->links[i] = calloc(ig_graph->num_sym_regs, sizeof(int));

  /* Calculate the interferences */
  for(i = 0; i < cfg_list->num_graphs; i++) {
    for(l = 0; l < cfg_list->graph[i]->num_nodes; l++) {
      current = cfg_list->graph[i]->node[l];

      /* Add links for def[n] -> out[n] */
      if(current->var_def && current->num_outs)
	for(j = 0; j < current->num_outs; j++)
	  ig_add_link(ig_graph, current->var_def, current->out[j]);

      /* Find links in the out sets */
      for(j = 0; j < current->num_outs; j++)
	for(k = 0; k < current->num_outs; k++)
	  ig_add_link(ig_graph, current->out[j], current->out[k]);

      /* Find links in the in sets */
      for(j = 0; j < current->num_ins; j++)
	for(k = 0; k < current->num_ins; k++)
	  ig_add_link(ig_graph, current->in[j], current->in[k]);

    }
  }

  return ig_graph;
}

/*
 * Return the symbolic register number for the given variable for the given
 * MIR operand. Will return -1 if no symbolic register can be found.
 */
int ig_sym_reg(ig_graph_t *graph, mir_operand_t *op) {
  int i;

  if(op->optype == MIR_OP_CONST)
    return -1;

  for(i = 0; i < graph->num_sym_regs; i++)
    if(op->optype == MIR_OP_VAR &&
       graph->sym_reg[i]->type == MCFG_TYPE_VAR &&
       op->var == graph->sym_reg[i]->var)
      return i;

    else if(op->optype == MIR_OP_REG &&
	    graph->sym_reg[i]->type == MCFG_TYPE_REG &&
	    op->val == graph->sym_reg[i]->reg)
      return i;


  return -1;
}


/*
 * Print out the interference graph as a vcg
 */
void vcg_output_ig(ig_graph_t *ig_graph, char *basename, char *ext) {
  FILE *fd;
  int i, j;
  char *filename;

  if(!(cflags.flags & CFLAG_DEBUG_OUTPUT))
    return;

  if(!ig_graph) {
    debug_printf(1, "Interference graph is null, no vcg file written\n");
    return;
  }

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
  fprintf(fd, "layout_nearfactor: 0\nmanhatten_edges: no\n");
  fprintf(fd, "yspace: 50\nxspace: 34\nxlspace: 25\nstretch: 40\n");
  fprintf(fd, "shrink: 100\ndisplay_edge_labels: yes\n");
  fprintf(fd, "finetuning: no\nnode.borderwidth: 3\nnode.color: lightcyan\n");
  fprintf(fd, "node.textcolor: darkred\nnode.bordercolor: red\n");
  fprintf(fd, "edge.color: darkgreen\n");

  /* Nodes */
  for(i = 0; i < ig_graph->num_sym_regs; i++) {
    fprintf(fd, "node: {title:\"%p\" label:\"s%d (", ig_graph->sym_reg[i], i);
    if(ig_graph->sym_reg[i]->type == MCFG_TYPE_VAR)
      fprintf(fd, "%s", ig_graph->sym_reg[i]->var->name);
    else
      fprintf(fd, "%%t%d", ig_graph->sym_reg[i]->reg);

    fprintf(fd, ")\"}\n");
  }

  /* Edges */
  for(i = 0; i < ig_graph->num_sym_regs; i++)
    for(j = i + 1; j < ig_graph->num_sym_regs; j++)
      if(ig_are_linked(ig_graph, ig_graph->sym_reg[i], ig_graph->sym_reg[j]))
	fprintf(fd, "edge: { sourcename:\"%p\" targetname:\"%p\""
		"arrowstyle: none }\n",
		ig_graph->sym_reg[i], ig_graph->sym_reg[j]);

  /* Footer */
  fprintf(fd, "\n}\n");
  fclose(fd);

}
