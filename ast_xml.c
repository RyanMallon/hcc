/*
 * ast_xml.c
 *
 * Ryan Mallon (2005)
 *
 * Depth first parse of ast tree to produce
 * and xml document
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "ast.h"
#include "ast_xml.h"
#include "cerror.h"
#include "typechk.h"

static const int indent_size = 2;
static int indent;
static FILE *fd;

static void print_indent(void);
static void parse_node(ast_node_t *);

int ast2xml(ast_node_t *root, char *xml_file) {

  if(!root) {
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Cannot create xml file for empty ast tree\n");
    return -1;
  }

  if(!(fd = fopen(xml_file, "w"))) {
    compiler_error(CERROR_ERROR, CERROR_NO_LINE,
		   "Cannot open file %s for writing\n", xml_file);
    return -1;
  }

  /* Print XML header */
  fprintf(fd, "<?xml version=\"1.0\" ?>\n");

#ifdef USE_XML_DOCTYPE
  fprintf(fd, "<!DOCTYPE syntaxtree SYSTEM \"syntax.dtd\">\n");
#endif

  fprintf(fd, "<syntaxtree>\n");

  indent = indent_size;
  parse_node(root);

  fprintf(fd, "</syntaxtree>\n");
  fclose(fd);
}

static void print_indent(void) {
  int i;

  for(i = 0; i < indent; i++)
    fprintf(fd, " ");
}

static void parse_node(ast_node_t *current) {
  ast_node_t **children;
  int i;

  if(current->node_type == NODE_OPERATOR) {
    /* Non-terminal Node */

    /* Print opening tag */
    print_indent();
    fprintf(fd, "<nonterminal name=\"%s\"",
	    ast_node_name[current->node.op]);

    if(current->type_check)
      fprintf(fd, " type=\"%s, line=%d\"",
	      sprint_type_signature(current->type_check->type_info),
	      current->src_line_num);

    fprintf(fd, ">\n");

    indent += indent_size;

    /* Parse children */
    children = current->node.children;
    for(i = 0; i < current->node.num_children; i++)
      parse_node(current->node.children[i]);

    /* Print closing tag */
    indent -= indent_size;
    print_indent();
    fprintf(fd, "</nonterminal>\n");


  } else if(current->node_type == NODE_LEAF_ID) {
    /* Terminal Node (Identifier) */

    print_indent();
    fprintf(fd, "<terminal name=\"%s\"",
	    current->leaf.name);

    if(current->type_check)
      fprintf(fd, " type=\"%s, line=%d\"",
	      sprint_type_signature(current->type_check->type_info),
	      current->src_line_num);



    fprintf(fd, "/>\n");

  } else {
    /* Terminal Node (Number) */
    print_indent();
    if(current->node_type == NODE_LEAF_STRING)
      fprintf(fd, "<terminal name=\"String literal\"");
    else
      fprintf(fd, "<terminal name=\"%d\"", current->leaf.val);

    if(current->type_check)
      fprintf(fd, " type=\"%s, line=%d\"",
	      sprint_type_signature(current->type_check->type_info),
	      current->src_line_num);

    fprintf(fd, "/>\n");

  }
}
