/*
 * strings.c
 *
 * Ryan Mallon (2005)
 *
 * String handling. Note that strings in the string table may not always
 * be null terminated, because the string type also stores the length in
 * the descriptor.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "strings.h"
#include "icg.h"

int num_string_literals = 0;
string_literal_t **string_literals;

/*
 * Adds a string literal to the table and returns its
 * identification number
 */
int add_string_literal(char *str) {
  string_literal_t *current;

  if(!num_string_literals)
    string_literals = malloc(sizeof(string_literal_t *));
  else
    string_literals = realloc(string_literals, sizeof(string_literal_t *) *
			      (num_string_literals + 1));

  current = malloc(sizeof(string_literal_t));
  if(!num_string_literals)
    current->offset = 0;
  else {
    current->offset = string_literals[num_string_literals - 1]->offset +
      string_literals[num_string_literals - 1]->length +
      IC_SIZEOF_STRING_DESC;

    if(current->offset % IC_SIZEOF_INT)
      current->offset += IC_SIZEOF_INT - (current->offset % IC_SIZEOF_INT);
  }

  current->length = strlen(str) + 1;
  current->string = malloc(current->length);
  strcpy(current->string, str);

  debug_printf(1, "Added \"%s\" to string table, length = %d, offset = %d\n",
	       str, current->length, current->offset);

  string_literals[num_string_literals] = current;
  return num_string_literals++;

}

/*
 * Return the total size of the string table
 */
int string_table_size(void) {
  int size;

  if(!num_string_literals)
    return 0;

  size = string_literals[num_string_literals - 1]->offset +
    string_literals[num_string_literals - 1]->length +
    IC_SIZEOF_STRING_DESC;

  /* Align to wordsize boundary */
  if(size % IC_SIZEOF_INT != 0)
    size += (IC_SIZEOF_INT - (size % IC_SIZEOF_INT));

  return size;
}

/*
 * Return the total number of strings in the table
 */
int num_strings(void) {
  return num_string_literals;
}

/*
 * Return a given string literal
 */
string_literal_t *get_string_literal(int i) {
  return string_literals[i];
}

/*
 * strcat
 *
 * Concatenate str1 and str2, store the position of the new string in dest
 */
void string_strcat(int str1, int str2, int dest) {
  int offset = IC_STACK_FRAME_HEADER_BYTES + 4;

  ic_emit(IC_STO, str2, IC_SP, -offset, IC_REG, IC_REG_BASE, IC_CONST_OFFS);
  offset += 4;
  ic_emit(IC_STO, str1, IC_SP, -offset, IC_REG, IC_REG_BASE, IC_CONST_OFFS);

  ic_emit_str(IC_CAL, 0, 0, "str_strcat", IC_LABEL, IC_STRING_OP, IC_NONE);
  ic_emit(IC_MOV, IC_RV, dest, 0, IC_REG, IC_REG, IC_NONE);
}

/*
 * strcmp
 *
 * Compare two strings, return the result in register rv.
 *
 */
void string_strcmp(int str1, int str2) {
  int offset = IC_STACK_FRAME_HEADER_BYTES + 4;

  ic_emit(IC_STO, str2, IC_SP, -offset, IC_REG, IC_REG_BASE, IC_CONST_OFFS);
  offset += 4;
  ic_emit(IC_STO, str1, IC_SP, -offset, IC_REG, IC_REG_BASE, IC_CONST_OFFS);

  ic_emit_str(IC_CAL, 0, 0, "str_strcmp", IC_LABEL, IC_STRING_OP, IC_NONE);
}
