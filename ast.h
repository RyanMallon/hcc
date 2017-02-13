/*
 * ast.h
 *
 * Ryan Mallon (2005)
 *
 */
#ifndef _AST_H_
#define _AST_H_

typedef enum {
    NODE_OPERATOR,
    NODE_LEAF_NUM,
    NODE_LEAF_ID,
    NODE_LEAF_CHAR,
    NODE_LEAF_STRING,
} node_type_t;

typedef enum {
    NODE_ROOT,
    NODE_DECL,
    NODE_BLOCK,
    NODE_VOID,
    NODE_INT,
    NODE_CHAR,
    NODE_STRING,
    NODE_FUNC,
    NODE_INLINE_FUNC,
    NODE_FUNC_CALL,
    NODE_STRUCT_DEF,
    NODE_STRUCT,
    NODE_MEMBER,
    NODE_POINTER,
    NODE_ADDRESS,
    NODE_MINUS,
    NODE_ASSIGN_ADDRESS,
    NODE_CAST,
    NODE_ARRAY,
    NODE_BECOMES,
    NODE_FOR,
    NODE_WHILE,
    NODE_DO,
    NODE_IF,
    NODE_RETURN,
    NODE_ADD,
    NODE_SUB,
    NODE_MUL,
    NODE_DIV,
    NODE_LSS,
    NODE_LEQ,
    NODE_GTR,
    NODE_GEQ,
    NODE_NEQ,
    NODE_EQL,
    NODE_OR,
    NODE_AND,
    NODE_SIZEOF,

    NODE_VARARGS,

    NODE_HISTORY,
    NODE_HISTORY_ARRAY,
    NODE_ATOMIC,

    MAX_NODE_OP
} node_op_t;

static char *ast_node_name[MAX_NODE_OP] = {
    "root",
    "declarations",
    "block",
    "void",
    "int",
    "char",
    "string",
    "func",
    "inline_func",
    "func_call",
    "struct_def",
    "struct",
    "member",
    "pointer",
    "address",
    "minus",
    "assign_address",
    "cast",
    "array",
    "becomes",
    "for",
    "while",
    "do",
    "if",
    "return",
    "add",
    "sub",
    "mul",
    "div",
    "lss",
    "leq",
    "gtr",
    "geq",
    "neq",
    "eql",
    "or",
    "and",
    "sizeof",

    "varargs",

    "history",
    "history_array",
    "atomic"
};

/* Type information */
#ifndef _TYPE_INFO_T_
#define _TYPE_INFO_T_
typedef struct {
    int decl_type;
    int length;
    unsigned int *signature;

    int indirect_levels;
    int num_indicies;
    int *indicies;
} type_info_t;
#endif /* TYPE_INFO_T_ */

/* Type check Info */
typedef struct {
    int passed;
    type_info_t *type_info;
} type_check_t;

/* AST node */
typedef struct ast_node_s {
    struct ast_node_s *parent;
    node_type_t node_type;

    int src_line_num;
    type_check_t *type_check;

#ifdef ALLOW_UNAMED_UNIONS
    union {
#endif
	struct {
	    int op;
	    int scope_tag;

	    int num_children;
	    struct ast_node_s **children;
	} node;

	struct {
#ifdef ALLOW_UNAMED_UNIONS
	    union {
#endif
		char *name;
		int val;
#ifdef ALLOW_UNAMED_UNIONS
	    };
#endif
	} leaf;

#ifdef ALLOW_UNAMED_UNIONS
    };
#endif

} ast_node_t;


/* Node making/freeing functions */
ast_node_t *mk_id_leaf(char *, int);
ast_node_t *mk_num_leaf(int, int);
ast_node_t *mk_char_leaf(char, int);
ast_node_t *mk_string_leaf(char *, int);
ast_node_t *mk_node(int, int);
void mk_root_node(void);
void ast_free(ast_node_t *);

/* AST tree building functions */
void add_list_node(ast_node_t ***, ast_node_t *);
void append_children(ast_node_t *, ast_node_t **);
void append_child(ast_node_t *, ast_node_t *);

/* Misc funcs */
int child_number(ast_node_t *, int);
int ast_assign_side(ast_node_t *);
ast_node_t *ast_assign_node(ast_node_t *);
int count_nodes(ast_node_t *);
ast_node_t **get_leaf_list(ast_node_t *, ast_node_t ***, int *);
int is_child_of(ast_node_t *, int);

struct name_table_s *ast_def_func(int);

/* Mungers */
void munge_ast(void);
void munge_assign_address(ast_node_t *);
void munge_duplicate_unaries(ast_node_t *);
void munge_rhs_members(ast_node_t *);
void munge_array_indicies(ast_node_t *);

#endif /* _AST_H_ */
