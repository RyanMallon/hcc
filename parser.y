%{
/*
 * parser.y
 * Ryan Mallon (2005)
 *
 * Bison Parser.
 *
 * Thanks to Jutta Degener, who's ANSI C grammar greatly helped me in
 * constructing the grammar for this parser.
 * See: http://www.lysator.liu.se/c/ANSI-C-grammar-y.html
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "ast.h"

void yyerror(const char *);
void yyinit(void);

#ifndef YYLTYPE
typedef struct {
  int first_line;
  int first_column;
  int last_line;
  int last_column;
} yyltype;
#define YYLTYPE yyltype
#endif

#ifdef YYLSP_NEEDED
YYLTYPE yylloc;
#endif


typedef struct {
  int line_number;

  char *str_val;
  int int_val;
  ast_node_t *ast_node;
  ast_node_t **ast_list;
}  __yystype__;
#define YYSTYPE __yystype__

extern ast_node_t *ast;
static ast_node_t *global_decls;

void yyinit(void) {
  mk_root_node();
  global_decls = mk_node(NODE_DECL, -1);
}

%}

/* Token defintions */
%token SYM_VAR SYM_FUNC SYM_STRUCT SYM_VOID SYM_INT SYM_CHAR SYM_STRING
%token SYM_FOR SYM_WHILE SYM_DO SYM_IF SYM_THEN SYM_RETURN
%token SYM_LPAREN SYM_RPAREN SYM_LBRACKET SYM_RBRACKET
%token SYM_LBRACE SYM_RBRACE SYM_COLON SYM_SIZEOF SYM_INLINE
%token SYM_SEMI SYM_COMMA SYM_PERIOD SYM_ARROW SYM_AMPER SYM_BECOMES
%token SYM_PLUS SYM_MINUS SYM_TIMES SYM_SLASH SYM_OR SYM_AND
%token SYM_LSS SYM_GTR SYM_LEQ SYM_GEQ SYM_NEQ SYM_EQL SYM_VARARGS

%token SYM_LHIST SYM_RHIST SYM_ATOMIC SYM_DOLLARS

 /* Starting rule */
%start program

 /*
%union {
  char *str_val;
  int int_val;
  ast_node_t *ast_node;
  ast_node_t **ast_list;
}
 */

%token <str_val> SYM_ID "[id]"
%token <int_val> SYM_NUM "[number]"
%token <int_val> SYM_CHARLIT "[charlit]"
%token <str_val> SYM_STRINGLIT "[stringlit]"

 /* Rule types */
%type<ast_node> global_declaration declaration func_declaration function
%type<ast_node> type pointer declarator declarator2 identifier statement
%type<ast_node> program_chunk assignment_statement block
%type<ast_node> expression addop mulop relop
%type<ast_node> add_expression mult_expression cast_expression unary_expression
%type<ast_node> base_expression postfix_expression unary_op
%type<ast_node> or_expression and_expression relational_expression

%type<ast_node> history_declaration atomic_block
%type<ast_list> atomic_list

%type<int_val> func_inline

%type<ast_list> declarations identifier_list array_declaration
%type<ast_list> func_params_decl param_decl_list
%type<ast_list> statement_list identifier_arguments
%type<ast_list> struct_member_list struct_members

 /* Error mode */
%error-verbose

%%

/* Starting rule */
program : program_chunk {
  if(global_decls->node.num_children)
    append_child(ast, global_decls);
}
;

program_chunk : /* Epsilon */
{
  $$ = NULL;
}
| program_chunk global_declaration
| program_chunk func_declaration
| program_chunk function
{
  append_child(ast, $2);
}
;

declarations : /* Epsilon */
{
  $$ = NULL;
}
| declarations declaration
{
  add_list_node(&$$, $2);
}
;

declaration : SYM_VAR type identifier_list SYM_SEMI
{
  append_children($2, $3);
  $$ = $2;
}
| SYM_VAR SYM_STRUCT struct_member_list declarator SYM_SEMI
{
  $$ = mk_node(NODE_STRUCT_DEF, @2.first_line);
  append_child($$, $4);
  append_children($$, $3);
}
;

global_declaration : SYM_VAR type identifier_list SYM_SEMI
{
  append_children($2, $3);
  append_child(global_decls, $2);
}
| SYM_VAR SYM_STRUCT struct_member_list declarator SYM_SEMI
{
  ast_node_t *current = mk_node(NODE_STRUCT_DEF, @2.first_line);

  append_child(current, $4);
  append_children(current, $3);
  append_child(global_decls, current);
}
;

struct_member_list : SYM_LBRACE struct_members SYM_RBRACE
{
  $$ = $2;
}
;

struct_members : type declarator SYM_SEMI
{
  ast_node_t *current = $1;
  append_child(current, $2);
  $$ = NULL;
  add_list_node(&$$, current);
}
| struct_members type declarator SYM_SEMI
{
  ast_node_t *current = $2;
  append_child(current, $3);
  add_list_node(&$$, current);
}
;

identifier_list : declarator
{
  $$ = NULL;
  add_list_node(&$$, $1);
}
| identifier_list SYM_COMMA declarator
{
  add_list_node(&$$, $3);
}
;

type : SYM_VOID {$$ = mk_node(NODE_VOID, @1.first_line);}
| SYM_CHAR {$$ = mk_node(NODE_CHAR, @1.first_line);}
| SYM_STRING {$$ = mk_node(NODE_STRING, @1.first_line);}
| SYM_INT {$$ = mk_node(NODE_INT, @1.first_line);}
| SYM_ID {
  $$ = mk_node(NODE_STRUCT, @1.first_line);
  append_child($$, mk_id_leaf($1, @1.first_line));
}
;

declarator : pointer declarator2
{
  ast_node_t *current = $1;

  while(current->node.num_children && current->node.children[0])
    current = current->node.children[0];

  append_child(current, $2);
}
| declarator2
{
  $$ = $1;
}
;

declarator2 : SYM_ID history_declaration
{
  if($2 == NULL)
    $$ = mk_id_leaf($1, @1.first_line);
  else {
    $$ = mk_node(NODE_HISTORY, @2.first_line);
    append_child($$, mk_id_leaf($1, @1.first_line));
    append_child($$, $2);
  }
}
| SYM_ID history_declaration array_declaration history_declaration
{
  if($2 != NULL && $4 != NULL) {
    fprintf(stderr, "Error: Two history types specified for array\n");
    exit(EXIT_FAILURE);
  }

  if($2 != NULL) {
    /* Array-Wise History */
    ast_node_t *array_node;

    array_node = mk_node(NODE_ARRAY, @1.first_line);
    append_child(array_node, mk_id_leaf($1, @3.first_line));
    append_children(array_node, $3);

    $$ = mk_node(NODE_HISTORY, @4.first_line);
    append_child($$, array_node);
    append_child($$, $2);

  } else if($4 != NULL) {
    /* Index-Wise History */
    ast_node_t *history_node;

    history_node = mk_node(NODE_HISTORY, @2.first_line);
    append_child(history_node, mk_id_leaf($1, @1.first_line));
    append_child(history_node, $4);

    $$ = mk_node(NODE_ARRAY, @3.first_line);
    append_child($$, history_node);
    append_children($$, $3);

  } else {
    $$ = mk_node(NODE_ARRAY, @3.first_line);
    append_child($$, mk_id_leaf($1, @1.first_line));
    append_children($$, $3);
  }
}
;

history_declaration : /* Epsilon */
{
  $$ = NULL;
}
| SYM_LHIST SYM_NUM SYM_RHIST
{
  $$ = mk_num_leaf($2, @2.first_line);
}
;

pointer : pointer SYM_TIMES
{
  $$ = mk_node(NODE_POINTER, @2.first_line);
  append_child($$, $1);
}
| SYM_TIMES
{
  $$ = mk_node(NODE_POINTER, @1.first_line);
}
;

array_declaration : array_declaration SYM_LBRACKET SYM_NUM SYM_RBRACKET
{
  add_list_node(&$$, mk_num_leaf($3, @3.first_line));
}
| SYM_LBRACKET SYM_NUM SYM_RBRACKET
{
  $$ = NULL;
  add_list_node(&$$, mk_num_leaf($2, @2.first_line));
}
;

identifier : SYM_ID
{
  $$ = mk_id_leaf($1, @1.first_line);
}
;

identifier_arguments : expression
{
  $$ = NULL;
  add_list_node(&$$, $1);
}
| identifier_arguments SYM_COMMA expression
{
  add_list_node(&$$, $3);
}
;

func_inline:  /* Epsilon */
{
  $$ = 0;
}
| SYM_INLINE
{
  $$ = 1;
}
;

func_declaration: SYM_FUNC func_inline type pointer SYM_ID func_params_decl
  SYM_SEMI
{
  ast_node_t *func, *name;

  if($2)
    func = mk_node(NODE_INLINE_FUNC, @1.first_line);
  else
    func = mk_node(NODE_FUNC, @1.first_line);

  append_child(func, $3);
  append_child(func->node.children[0], $4);

  name = func->node.children[0];
  while(name->node.num_children && name->node.children[0])
    name = name->node.children[0];
  append_child(name, mk_id_leaf($5, @5.first_line));

  append_children(func, $6);
  append_child(global_decls, func);
}
| SYM_FUNC func_inline type SYM_ID func_params_decl SYM_SEMI
{
  ast_node_t *func;

  if($2)
    func = mk_node(NODE_INLINE_FUNC, @1.first_line);
  else
    func = mk_node(NODE_FUNC, @1.first_line);

  append_child(func, $3);
  append_child(func->node.children[0], mk_id_leaf($4, @4.first_line));

  append_children(func, $5);
  append_child(global_decls, func);
}
;

func_params_decl : SYM_LPAREN SYM_RPAREN
{
  $$ = NULL;
}
| SYM_LPAREN SYM_VARARGS SYM_RPAREN
{
  ast_node_t *node, *leaf;

  node = mk_node(NODE_VARARGS, @2.first_line);
  leaf = mk_id_leaf("\0", @2.first_line);
  append_child(node, leaf);

  $$ = NULL;
  add_list_node(&$$, node);

}
| SYM_LPAREN param_decl_list SYM_RPAREN
{
  $$ = $2;
}
;

param_decl_list : type declarator
{
  append_child($1, $2);
  $$ = NULL;
  add_list_node(&$$, $1);
}
| param_decl_list SYM_COMMA type declarator
{
  append_child($3, $4);
  add_list_node(&$$, $3);
}
;

function : type pointer SYM_ID func_params_decl block
{
  ast_node_t *current;

  $$ = mk_node(NODE_FUNC, @3.first_line);
  append_child($$, $1);
  append_child($$->node.children[0], $2);

  current = $$->node.children[0];
  while(current->node.num_children && current->node.children[0])
    current = current->node.children[0];
  append_child(current, mk_id_leaf($3, @3.first_line));
  append_children($$, $4);

  if($5)
    append_child($$, $5);
}
| type SYM_ID func_params_decl block
{
  $$ = mk_node(NODE_FUNC, @2.first_line);
  append_child($$, $1);
  append_child($1, mk_id_leaf($2, @2.first_line));
  append_children($$, $3);

  if($4)
    append_child($$, $4);
}
;

block : SYM_LBRACE declarations SYM_RBRACE
{
  if($2 != NULL) {
    ast_node_t *decl_node = mk_node(NODE_DECL, @2.first_line);
    append_children(decl_node, $2);
    append_child($$, decl_node);
  } else
    $$ = NULL;
}
| SYM_LBRACE declarations statement_list SYM_RBRACE
{
  $$ = mk_node(NODE_BLOCK, @1.first_line);

  if($2 != NULL) {
    ast_node_t *decl_node = mk_node(NODE_DECL, @2.first_line);
    append_children(decl_node, $2);
    append_child($$, decl_node);
  }
  append_children($$, $3);
}
;

statement_list : statement
{
  $$ = NULL;
  add_list_node(&$$, $1);
}
| statement_list statement
{
  add_list_node(&$$, $2);
}
;

statement : assignment_statement SYM_SEMI
{
  $$ = $1;
}
| SYM_FOR SYM_LPAREN assignment_statement SYM_SEMI expression SYM_SEMI
  assignment_statement SYM_RPAREN block
{
  $$ = mk_node(NODE_FOR, @1.first_line);
  append_child($$, $3);
  append_child($$, $5);
  append_child($$, $7);
  append_child($$, $9);
}
| SYM_WHILE SYM_LPAREN expression SYM_RPAREN block
{
  $$ = mk_node(NODE_WHILE, @1.first_line);
  append_child($$, $3);
  append_child($$, $5);
}
| SYM_DO block SYM_WHILE SYM_LPAREN expression SYM_RPAREN SYM_SEMI
{
  $$ = mk_node(NODE_DO, @1.first_line);
  append_child($$, $5);
  append_child($$, $2);
}
| SYM_IF SYM_LPAREN expression SYM_RPAREN block
{
  $$ = mk_node(NODE_IF, @1.first_line);
  append_child($$, $3);
  append_child($$, $5);
}
| SYM_RETURN expression SYM_SEMI
{
  $$ = mk_node(NODE_RETURN, @1.first_line);
  append_child($$, $2);
}
| atomic_block block
{
  $$ = $1;
  append_child($$, $2);
}
;

atomic_block : SYM_ATOMIC
{
  $$ = mk_node(NODE_ATOMIC, @1.first_line);
}
| SYM_ATOMIC SYM_LPAREN atomic_list SYM_RPAREN
{
  $$ = mk_node(NODE_ATOMIC, @1.first_line);
  append_children($$, $3);
}
;

atomic_list : SYM_ID
{
  $$ = NULL;
  add_list_node(&$$, mk_id_leaf($1, @1.first_line));
}
| atomic_list SYM_COMMA SYM_ID
{
  add_list_node(&$$, mk_id_leaf($3, @1.first_line));
}
;

assignment_statement : expression SYM_BECOMES expression
{
  $$ = mk_node(NODE_BECOMES, @2.first_line);
  append_child($$, $1);
  append_child($$, $3);
}
| expression
{
  $$ = $1;
}
;

expression : or_expression
{
  $$ = $1;
}
;

base_expression : identifier
{
  $$ = $1;
}
| SYM_NUM
{
  $$ = mk_num_leaf($1, @1.first_line);
}
| SYM_CHARLIT
{
  $$ = mk_char_leaf($1, @1.first_line);
}
| SYM_STRINGLIT
{
  $$ = mk_string_leaf($1, @1.first_line);
}
| SYM_LPAREN expression SYM_RPAREN
{
  $$ = $2;
}
;

postfix_expression : base_expression
{
  $$ = $1;
}
| postfix_expression SYM_LBRACKET expression SYM_RBRACKET
{
  $$ = mk_node(NODE_ARRAY, @2.first_line);
  append_child($$, $1);
  append_child($$, $3);
}
| postfix_expression SYM_LHIST expression SYM_RHIST
{
  $$ = mk_node(NODE_HISTORY, @2.first_line);
  append_child($$, $1);
  append_child($$, $3);
}
| postfix_expression SYM_LPAREN SYM_RPAREN
{
  $$ = mk_node(NODE_FUNC_CALL, @2.first_line);
  append_child($$, $1);
}
| postfix_expression SYM_LPAREN identifier_arguments SYM_RPAREN
{
  $$ = mk_node(NODE_FUNC_CALL, @2.first_line);
  append_child($$, $1);
  append_children($$, $3);
}
| postfix_expression SYM_PERIOD identifier
{
  $$ = mk_node(NODE_MEMBER, @2.first_line);
  append_child($$, $1);
  append_child($$, $3);
}
| postfix_expression SYM_ARROW identifier
{
  ast_node_t *ptr_node = mk_node(NODE_POINTER, @2.first_line);

  append_child(ptr_node, $1);

  $$ = mk_node(NODE_MEMBER, @2.first_line);
  append_child($$, ptr_node);
  append_child($$, $3);
}
;

unary_expression : postfix_expression
{
  $$ = $1;
}
| unary_op unary_expression
{
  $$ = $1;
  append_child($$, $2);
}
;

cast_expression : unary_expression
{
  $$ = $1;
}
| SYM_LBRACKET type SYM_RBRACKET unary_expression
{
  $$ = mk_node(NODE_CAST, @1.first_line);
  append_child($$, $2);
  append_child($$->node.children[0], mk_id_leaf("\0", @2.first_line));
  append_child($$, $4);
}
| SYM_LBRACKET type pointer SYM_RBRACKET unary_expression
{
  ast_node_t *current;

  $$ = mk_node(NODE_CAST, @1.first_line);

  append_child($$, $2);
  append_child($$, $5);

  append_child($$->node.children[0], $3);

  current = $$->node.children[0];
  while(current->node.num_children && current->node.children[0])
    current = current->node.children[0];

  append_child(current, mk_id_leaf("\0", @1.first_line));

}
;

mult_expression : cast_expression
{
  $$ = $1;
}
| mult_expression mulop cast_expression
{
  $$ = $2;
  append_child($$, $1);
  append_child($$, $3);
}
;

add_expression : mult_expression
{
  $$ = $1;
}
| addop mult_expression
{
  $$ = $1;
  append_child($$, $2);
}
| add_expression addop mult_expression
{
  $$ = $2;
  append_child($$, $1);
  append_child($$, $3);
}
;

relational_expression : add_expression
{
  $$ = $1;
}
| relational_expression relop add_expression
{
  $$ = $2;
  append_child($$, $1);
  append_child($$, $3);
}
;

and_expression : relational_expression
{
  $$ = $1;
}
| and_expression SYM_AND relational_expression
{
  $$ = mk_node(NODE_AND, @2.first_line);
  append_child($$, $1);
  append_child($$, $3);
}
;

or_expression : and_expression
{
  $$ = $1;
}
| or_expression SYM_OR and_expression
{
  $$ = mk_node(NODE_OR, @2.first_line);
  append_child($$, $1);
  append_child($$, $3);
}
;

unary_op : SYM_TIMES {$$ = mk_node(NODE_POINTER, @1.first_line);}
| SYM_AMPER {$$ = mk_node(NODE_ADDRESS, @1.first_line);}
;

addop : SYM_PLUS {$$ = mk_node(NODE_ADD, @1.first_line);}
| SYM_MINUS {$$ = mk_node(NODE_SUB, @1.first_line);}
;

mulop : SYM_TIMES {$$ = mk_node(NODE_MUL, @1.first_line);}
| SYM_SLASH {$$ = mk_node(NODE_DIV, @1.first_line);}
;

relop : SYM_LSS {$$ = mk_node(NODE_LSS, @1.first_line);}
| SYM_LEQ {$$ = mk_node(NODE_LEQ, @1.first_line);}
| SYM_GTR {$$ = mk_node(NODE_GTR, @1.first_line);}
| SYM_GEQ {$$ = mk_node(NODE_GEQ, @1.first_line);}
| SYM_NEQ {$$ = mk_node(NODE_NEQ, @1.first_line);}
| SYM_EQL {$$ = mk_node(NODE_EQL, @1.first_line);}
;

%%

#include <stdio.h>
#include "lex.yy.c"

void yyerror(const char *s) {
#ifdef _DEBUG_ERRORS
  fprintf(stderr, "[%s:%d] ", __FILE__, __LINE__);
#endif

  fprintf(stderr, ":%d: %s\n", yy_line_num, s);
  exit(0);
}
