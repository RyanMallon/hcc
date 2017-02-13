/*
 * astparse.h
 * Ryan Mallon (2005)
 *
 * AST parsing header
 *
 */
#ifndef _ASTPARSE_H_
#define _ASTPARSE_H_

/*
 * Op types for expression_t
 * addr = absolute address in memory
 * reg  = register
 * offs = offset from base pointer
 */
enum {
  OP_ADDR,
  OP_REG,
  OP_OFFS,
  OP_BASE_OFFS,
  OP_CONST,
  OP_VAR,
  OP_AND
};

/* Return type for expressions */
typedef struct expression_s {
  int op_type;
  int op_val;

  struct expression_s *op_history;

  int size;
  int indirect;

  name_record_t *var;
} expression_t;

void parse_ast(ast_node_t *);
void parse_declarations(ast_node_t *);

#endif /* _ASTPARSE_H_ */
