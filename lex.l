/*
 * lex.l
 * Ryan Mallon (2005)
 *
 * Lexer
 *
 */
%option noyywrap

%{
#include <string.h>
#include <stdlib.h>
#include "ast.h"
#include "parser.tab.h"

  extern YYLTYPE yylloc;
  unsigned int yy_line_num = 1;

  static void set_line(void) {
    yylloc.first_line = yy_line_num;
  }

  static int strip_number(void) {
    static char num_str[32], *src_str;
    int str_len, i;

    i = 0;
    src_str = yytext;
    while(*src_str && i < 32) {
      if(*src_str >= '0' && *src_str <= '9')
        num_str[i++] = *src_str;

      src_str++;
    }
    num_str[i] = '\0';

    return atoi(num_str);
  }

%}

newline [\n]
delim [ \t]
whitespace {delim}+
letter [A-Za-z]
digit [0-9]
id ({letter}|_)({letter}|{digit}|_)*
number {digit}+
charlit '(\\.|[^\\'])'
stringlit \"(\\.|[^\\"])*\"



%x comment line_directive

%%

{whitespace} {/* Do Nothing */}
{newline} {yy_line_num++;}

"/*" BEGIN(comment);
<comment>[^*\n]*
<comment>"*"[^*/\n]*
<comment>\n yy_line_num++;
<comment>"*"+"/" BEGIN(INITIAL);

#{delim}*{number} yy_line_num = strip_number(); BEGIN(line_directive);
<line_directive>.*\n BEGIN(INITIAL);

var    {set_line(); return(SYM_VAR);}
func   {set_line(); return(SYM_FUNC);}
struct {set_line(); return(SYM_STRUCT);}
void   {set_line(); return(SYM_VOID);}
int    {set_line(); return(SYM_INT);}
char   {set_line(); return(SYM_CHAR);}
string {set_line(); return(SYM_STRING);}
while  {set_line(); return(SYM_WHILE);}
for    {set_line(); return(SYM_FOR);}
do     {set_line(); return(SYM_DO);}
if     {set_line(); return(SYM_IF);}
then   {set_line(); return(SYM_THEN);}
return {set_line(); return(SYM_RETURN);}
sizeof {set_line(); return(SYM_SIZEOF);}
inline {set_line(); return(SYM_INLINE);}

atomic {set_line(); return(SYM_ATOMIC);}


{id} {
  set_line();
  yylval.str_val = malloc(strlen(yytext) + 1);
  strcpy(yylval.str_val, yytext);
  return(SYM_ID);
}

{number} {
  set_line();
  yylval.int_val = atoi(yytext);
  return(SYM_NUM);
}

{charlit} {
  set_line();
  yylval.int_val = (int)yytext[1];
  return(SYM_CHARLIT);
}

{stringlit} {
  int length = strlen(yytext) - 1;

  set_line();
  yylval.str_val = malloc(length);
  strncpy(yylval.str_val, yytext + 1, length - 1);
  yylval.str_val[length - 1] = '\0';
  return(SYM_STRINGLIT);
}

";"   {set_line(); return(SYM_SEMI);}
":"   {set_line(); return(SYM_COLON);}
","   {set_line(); return(SYM_COMMA);}
"."   {set_line(); return(SYM_PERIOD);}
"+"   {set_line(); return(SYM_PLUS);}
"-"   {set_line(); return(SYM_MINUS);}
"/"   {set_line(); return(SYM_SLASH);}
"*"   {set_line(); return(SYM_TIMES);}
"&"   {set_line(); return(SYM_AMPER);}
"("   {set_line(); return(SYM_LPAREN);}
")"   {set_line(); return(SYM_RPAREN);}
"{"   {set_line(); return(SYM_LBRACE);}
"}"   {set_line(); return(SYM_RBRACE);}
"["   {set_line(); return(SYM_LBRACKET);}
"]"   {set_line(); return(SYM_RBRACKET);}
"<"   {set_line(); return(SYM_LSS);}
">"   {set_line(); return(SYM_GTR);}
"||"  {set_line(); return(SYM_OR);}
"&&"  {set_line(); return(SYM_AND);}
"=="  {set_line(); return(SYM_EQL);}
"<="  {set_line(); return(SYM_LEQ);}
">="  {set_line(); return(SYM_GEQ);}
"!="  {set_line(); return(SYM_NEQ);}
"->"  {set_line(); return(SYM_ARROW);}
"="   {set_line(); return(SYM_BECOMES);}
"$"   {set_line(); return(SYM_DOLLARS);}
"..." {set_line(); return(SYM_VARARGS);}

"<:"  {set_line(); return(SYM_LHIST);}
":>"  {set_line(); return(SYM_RHIST);}

%%
