/*
 * ast.c
 *
 * Ryan Mallon (2005)
 *
 * Abstract Syntax Tree Routines
 *
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ast.h"
#include "symtable.h"
#include "typechk.h"
#include "ast_xml.h"
#include "cflags.h"

static ast_node_t *allocate_node();

ast_node_t *ast;
ast_node_t **nodes;
int num_nodes = 0;

extern unsigned int yy_line_num;
extern compiler_options_t cflags;

/*
 * Allocate memory for an ast node
 */
static ast_node_t *allocate_node() {
  num_nodes++;
  ast_node_t *current;

  if(num_nodes == 1) {
    nodes = malloc(sizeof(ast_node_t *));
    nodes[0] = malloc(sizeof(ast_node_t));

    current = nodes[0];

  } else {
    nodes = realloc(nodes, num_nodes * sizeof(ast_node_t *));
    nodes[num_nodes - 1] = malloc(sizeof(ast_node_t));

    current = nodes[num_nodes - 1];

  }

  if(!current) {
    fprintf(stderr, "Error allocating memory for AST node\n");
    exit(EXIT_FAILURE);
  }

  return current;
}

/*
 * Create a leaf node for storing a variable
 */
ast_node_t *mk_id_leaf(char *id, int line_number) {
  ast_node_t *current;

  current = allocate_node();
  current->src_line_num = line_number;
  current->node_type = NODE_LEAF_ID;
  current->leaf.name = malloc(strlen(id) + 1);
  current->type_check = NULL;
  strcpy(current->leaf.name, id);

  return current;
}

/*
 * Create a leaf node for storing a number
 */
ast_node_t *mk_num_leaf(int num, int line_number) {
  ast_node_t *current;

  current = allocate_node();
  current->src_line_num = line_number;
  current->node_type = NODE_LEAF_NUM;
  current->leaf.val = num;
  current->type_check = NULL;

  return current;
}

/*
 * Create a leaf node for storing a character literal
 */
ast_node_t *mk_char_leaf(char ch, int line_number) {
  ast_node_t *current;

  current = mk_num_leaf((int)ch, line_number);
  current->node_type = NODE_LEAF_CHAR;
  current->src_line_num = line_number;

  return current;
}

/*
 * Create a leaf node for storing a string literal
 */
ast_node_t *mk_string_leaf(char *str, int line_number) {
  ast_node_t *current;

  current = mk_id_leaf(str, line_number);
  current->node_type = NODE_LEAF_STRING;

  return current;
}

/*
 * Create an internal node
 */
ast_node_t *mk_node(int op, int line_number) {
  ast_node_t *current;

  current = allocate_node();
  current->parent = NULL;
  current->node_type = NODE_OPERATOR;
  current->src_line_num = line_number;
  current->node.op = op;
  current->node.num_children = 0;
  current->node.children = NULL;
  current->type_check = NULL;

  return current;
}

/*
 * Create/add to a list of nodes
 *
 * Argh modifying list arguments via triple indirect pointers is nasty
 *
 */
void add_list_node(ast_node_t ***list, ast_node_t *current) {

  int count = 0, child = 0;

  /* Count how many nodes in this list so far */
  if(!(*list))
    (*list) = malloc(sizeof(ast_node_t *) * 2);
  else {

    while((*list)[child++])
      count++;

    (*list) = realloc((*list), sizeof(ast_node_t *) * (count + 2));
  }

  (*list)[count] = malloc(sizeof(ast_node_t));
  (*list)[count] = current;
  (*list)[++count] = NULL;
}

/*
 * Append a list of nodes as children of another node
 */
void append_children(ast_node_t *current, ast_node_t **children) {
  int count = 0, child = 0, i;

  if(!children)
    return;

  /* Count children */
  while(children[child++])
    count++;

  if(!current->node.children || !current->node.num_children) {
    current->node.children = children;
    current->node.num_children = count;

    for(i = 0; i < count; i++)
      children[i]->parent = current;

  } else {
    /* Appending more children to an existing list */
    for(i = 0; i < count; i++)
      append_child(current, children[i]);

  }
}

/*
 * Append a single node as a child for another node
 */
void append_child(ast_node_t *current, ast_node_t *child) {

  child->parent = current;

  if(current->node.num_children == 0)
    current->node.children = malloc(sizeof(ast_node_t *));
  else
    current->node.children =
      realloc(current->node.children,
	      sizeof(ast_node_t *) * (current->node.num_children + 1));

  current->node.children[current->node.num_children] =
    malloc(sizeof(ast_node_t));

  current->node.children[current->node.num_children] = child;
  current->node.num_children++;
}

/*
 * Remove all children from the given node.
 * TODO: Free the removed children?
 *
 */
ast_node_t *remove_children(ast_node_t *current) {

  current->node.children = NULL;
  current->node.num_children = 0;

  return current;
}

/*
 * Return a list of all the leaf nodes in the current subtree
 *
 * Triple-indirect pointers make my head explode ;-)
 *
 */
ast_node_t **get_leaf_list(ast_node_t *subtree, ast_node_t ***list,
			   int *list_size) {
  int i;

  if(subtree->node_type != NODE_OPERATOR) {
    /* Leaf node: Add it to the list */

    if(!(*list))
      *list = malloc(sizeof(ast_node_t *));
    else
      *list = realloc(*list, sizeof(ast_node_t *) * ((*list_size) + 1));

    (*list)[(*list_size)++] = subtree;

    return *list;
  }

  for(i = 0; i < subtree->node.num_children; i++)
    *list = get_leaf_list(subtree->node.children[i], list, list_size);

  return *list;
}

/*
 * Return which child number the ast node current is of the closest
 * node_type.
 *
 * TODO: Unecessary recursion. Write an iterative version.
 *
 */
int child_number(ast_node_t *current, int node_type) {

  if(!current->parent)
    return -1;

  if(current->parent->node.op == node_type) {
    int i;
    for(i = 0; i < current->parent->node.num_children; i++)
      if(current->parent->node.children[i] == current)
	return i;
  }

  return child_number(current->parent, node_type);
}

/*
 * Return whether or not this node is in a subtree of a node of
 * the given type
 */
int is_child_of(ast_node_t *current, int node_type) {
  while(current->parent && current->node.op != node_type)
    current = current->parent;

  return (current->node.op == node_type);
}


/*
 * Munge the AST to make it nicer for parsing.
 */
void munge_ast(void) {
  if(!(cflags.flags & CFLAG_OUTPUT_HLIC))
    munge_assign_address(ast);

  munge_array_indicies(ast);
  munge_rhs_members(ast);
  munge_duplicate_unaries(ast);
}

/*
 * Insert an assign_address node on the left hand side of all expressions
 */
void munge_assign_address(ast_node_t *subtree) {
  ast_node_t *addr_of;
  int i;

  if(subtree->node_type != NODE_OPERATOR)
    return;

  if(subtree->node.op == NODE_BECOMES) {

    addr_of = mk_node(NODE_ASSIGN_ADDRESS, subtree->src_line_num);
    addr_of->node.num_children = 1;
    addr_of->node.children = malloc(sizeof(ast_node_t *));
    addr_of->node.children[0] = subtree->node.children[0];
    subtree->node.children[0] = addr_of;

    /* Fix up parent pointers */
    addr_of->parent = subtree;
    addr_of->node.children[0]->parent = addr_of;

    return;
  }

  for(i = 0; i < subtree->node.num_children; i++)
    munge_assign_address(subtree->node.children[i]);

}

/*
 * Convert array indicies in expressions to pointer notation
 *
 * TODO: Dangling pointers again
 *
 */
void munge_array_indicies(ast_node_t *subtree) {
  int i, j;
  ast_node_t *current, *array_tree, *left_tree, *right_tree;

  if(subtree->node_type != NODE_OPERATOR)
    return;

  /* Do not munge array declarations */
  if(subtree->node.op == NODE_DECL)
    return;

  for(i = 0; i < subtree->node.num_children; i++) {
    current = subtree->node.children[i];

    if(current->node_type == NODE_OPERATOR && current->node.op == NODE_ARRAY) {

      left_tree = current->node.children[0];
      right_tree = current->node.children[1];

      array_tree = mk_node(NODE_POINTER, current->src_line_num);
      append_child(array_tree, mk_node(NODE_ADD, current->src_line_num));
      append_child(array_tree->node.children[0], left_tree);
      append_child(array_tree->node.children[0], right_tree);

      subtree->node.children[i] = array_tree;
      array_tree->parent = subtree;

      /* More than 1 dimension */
      for(j = 2; j < current->node.num_children; j++) {
	left_tree = array_tree;
	right_tree = current->node.children[j];

	array_tree = mk_node(NODE_POINTER, current->src_line_num);
	append_child(array_tree, mk_node(NODE_ADD, current->src_line_num));
	append_child(array_tree->node.children[0], left_tree);
	append_child(array_tree->node.children[0], right_tree);

	subtree->node.children[i] = array_tree;
	array_tree->parent = subtree;
      }

    }
  }

  for(i = 0; i < subtree->node.num_children; i++)
    munge_array_indicies(subtree->node.children[i]);


}

/*
 * Add a pointer node above members that appear on the right hand
 * side of expressions.
 */
void munge_rhs_members(ast_node_t *subtree) {
  ast_node_t *ptr_node, *parent, *current, *ptr_subtree;
  int child, i;

  if(subtree->node_type != NODE_OPERATOR)
    return;

  if(subtree->node.op == NODE_MEMBER) {
    child = ast_assign_side(subtree);
    current = ast_assign_node(subtree);
    ptr_subtree = current->node.children[child];

    ptr_node = mk_node(NODE_POINTER, subtree->src_line_num);
    append_child(ptr_node, ptr_subtree);

    current->node.children[child] = ptr_node;
    ptr_node->parent = current;

  }

  for(i = 0; i < subtree->node.num_children; i++)
      munge_rhs_members(subtree->node.children[i]);
}

/*
 * Remove any consecutive unary operators that cancel each other out.
 *
 * TODO: Fix dangling pointer problems in this function
 *
 * TODO: Need to be able to strip unaries that are separated by a member
 * node for arrays of structs to work correctly.
 *
 */
void munge_duplicate_unaries(ast_node_t *subtree) {
  int i;
  ast_node_t *new_child, *current, *member;

  if(subtree->node_type != NODE_OPERATOR)
    return;


  for(i = 0; i < subtree->node.num_children; i++) {

    current = subtree->node.children[i];

    if(current->node_type == NODE_OPERATOR &&
       current->node.children[0]->node_type == NODE_OPERATOR) {

      if(current->node.op == NODE_ADDRESS ||
	 current->node.op == NODE_ASSIGN_ADDRESS)
	if(current->node.children[0]->node.op == NODE_POINTER) {
	  new_child = current->node.children[0]->node.children[0];
	  subtree->node.children[i] = new_child;

	  /* Fix parent reference */
	  new_child->parent = subtree;

	  munge_duplicate_unaries(subtree);
	}


      if(current->node.op == NODE_POINTER)
	if(current->node.children[0]->node.op == NODE_ADDRESS ||
	   current->node.children[0]->node.op == NODE_ASSIGN_ADDRESS) {
	  new_child = current->node.children[0]->node.children[0];
	  subtree->node.children[i] = new_child;

	  /* Fix parent reference */
	  new_child->parent = subtree;

	  munge_duplicate_unaries(subtree);
	}


    }
  }

  for(i = 0; i < subtree->node.num_children; i++)
    munge_duplicate_unaries(subtree->node.children[i]);

}

/*
 * Return which side of a func_call or assignment expression the given
 * node appears on. 0 = lhs.
 *
 * TODO: Can probably be rewritten iteratively.
 */
int ast_assign_side(ast_node_t *node) {
  ast_node_t *parent;
  int i;

  parent = node->parent;

  if(parent->node.op == NODE_BECOMES || parent->node.op == NODE_FUNC_CALL)
    for(i = 0; i < parent->node.num_children; i++)
      if(parent->node.children[i] == node)
	return i;


  return ast_assign_side(parent);
}

/*
 * Return the nearest func_call or assignment parent node.
 */
ast_node_t *ast_assign_node(ast_node_t *current) {
  while(current) {
    if(current->node.op == NODE_FUNC_CALL || current->node.op == NODE_BECOMES)
      return current;

    current = current->parent;
  }

  /* Couldn't find it */
  return NULL;
}

/*
 *
 */
struct name_table_s *ast_def_func(int index) {
  int i, count;

  for(i = 0, count = 0; i < ast->node.num_children; i++)
    if(ast->node.children[i]->node.op == NODE_FUNC ||
       ast->node.children[i]->node.op == NODE_INLINE_FUNC)
      if(count++ == index)
	return get_func_entry(get_name_from_node(ast->node.children[i]));

  return NULL;
}


/*
 * Count the number of nodes in the subtree
 */
int count_nodes(ast_node_t *subtree) {
  int i, count = 0;
  ast_node_t *current = subtree;

  if(current->node_type != NODE_OPERATOR ||
     current->node.num_children == 0)
    return 1;

  for(i = 0; i < current->node.num_children; i++) {
    count += count_nodes(current->node.children[i]);
  }
  return count + 1;
}

/*
 * Create the root node
 */
void mk_root_node() {

  if((ast = malloc(sizeof(ast_node_t))) == NULL) {
    fprintf(stderr, "Error: Error allocating memory for AST\n");
    exit(EXIT_FAILURE);
  }

  ast->parent = NULL;
  ast->node_type = NODE_OPERATOR;
  ast->node.op = NODE_ROOT;
  ast->node.children = NULL;
  ast->node.num_children = 0;
}

/*
 * Free an AST node and all its subchildren
 */
void ast_free(ast_node_t *node) {
  int i;

  if(node->node_type != NODE_OPERATOR) {
    /* Leaf node */
    if((node->node_type == NODE_LEAF_ID || node->node_type == NODE_LEAF_STRING)
       && node->leaf.name)
      free(node->leaf.name);

    free(node);
    return;
  }

  for(i = 0; i < node->node.num_children; i++)
    ast_free(node->node.children[i]);
}
