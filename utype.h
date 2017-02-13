/*
 * utype.h
 * Ryan Mallon (2005)
 *
 * User defined types
 *
 */
#ifndef _UTYPE_H_
#define _UTYPE_H_

#include "symtable.h"

typedef struct {
  char *name;
  int offset;
  type_info_t *type_info;
} member_type_t;

typedef struct {
  char *name;
  int num_members;

#if 0
  name_record_t **members;
#endif

  member_type_t *members;
} user_type_t;

user_type_t *add_utype(char *, int members);

user_type_t *add_utype_member(user_type_t **, int, name_record_t *);
user_type_t *get_utype(int);
name_record_t *get_utype_member(name_record_t *, char *);

int utype_index(name_record_t *);
int utype_def_index(char *);

#endif /* UTYPE_H_ */
