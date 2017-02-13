/*
 * strings.h
 *
 * Ryan Mallon (2005)
 *
 * String literal handling
 *
 */
#ifndef _STRINGS_H_
#define _STRINGS_H_

typedef struct {
  char *string;
  int length;
  int offset;
} string_literal_t;

int add_string_literal(char *);
int num_strings(void);
string_literal_t *get_string_literal(int);

/* Runtime string functions */
void string_strcat(int, int, int);

#endif /* _STRINGS_H_ */
