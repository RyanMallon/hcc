/*
 * utype.c
 * Ryan Mallon (2005)
 *
 * User defined types
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"
#include "cerror.h"
#include "utype.h"

#ifndef TYPE_SHIFT
#define TYPE_SHIFT 16
#endif

int num_utypes = 0;
user_type_t *utype_table;

/*
 * Add a user defined type
 *
 * Return pointer to utype_table entry
 *
 */
user_type_t *add_utype(char *name, int members) {

  debug_printf(1, "Adding user type %s\n", name);

  if(utype_def_index(name) != -1)
    compiler_error(CERROR_ERROR, CERROR_UNKNOWN_LINE,
		   "User defined type %s already declared\n", name);

  if(!num_utypes)
    utype_table = malloc(sizeof(user_type_t));
  else
    utype_table = realloc(utype_table, sizeof(user_type_t) * (num_utypes + 1));

  utype_table[num_utypes].name = malloc(strlen(name) + 1);
  strcpy(utype_table[num_utypes].name, name);

  utype_table[num_utypes].num_members = members;
  utype_table[num_utypes].members = malloc(sizeof(member_type_t) * members);

  return &utype_table[num_utypes++];
}

/*
 * Add a member to a user defined type
 * FIXME: Not used?
 */
user_type_t *add_utype_member(user_type_t **entry, int index,
			      name_record_t *member) {
#if 0
  if(!entry) {
    compiler_error(CERROR_ERROR, CERROR_UNKNOWN_LINE,
		   "Attempt to add member to undefined utype\n");
  }

  (*entry)->members[index] = member;
#endif
  return *entry;
}


/*
 * Return the utype index for the given utype definition name
 */
int utype_def_index(char *name) {
  int i;

  for(i = 0; i < num_utypes; i++)
    if(!strcmp(utype_table[i].name, name))
      return i;

  return -1;
}

/*
 * Return the utype index for the given utype instance
 */
int utype_index(name_record_t *utype) {
  if(!is_struct_type(utype->type_info))
    return -1;

  return utype->type_info->signature[utype->type_info->length - 1]
    >> TYPE_SHIFT;
}

/*
 * Return the user type at the given index
 */
user_type_t *get_utype(int index) {
  index >>= TYPE_SHIFT;

  if(index < 0 || index > num_utypes)
    return NULL;

  return &utype_table[index];
}


/*
 * Return a member from a user defined type
 */
#if 0
name_record_t *get_utype_member(user_type_t *utype, char *name) {
  int i;
  name_record_t *member;


  for(i = 0; i < utype->num_members; i++)
    if(!strcmp(utype->members[i]->name, name))
      return utype->members[i];


  return NULL;
}
#endif

name_record_t *get_utype_member(name_record_t *utype, char *member_name) {
  int i, num_members;

  debug_printf(1, "Retrieving from %s\n", utype->name);
  debug_printf(1, "Retrieving member from struct %d\n", utype_index(utype));

  num_members = utype_table[utype_index(utype)].num_members;

  debug_printf(1, "Hunting for member %s in %d records\n", member_name,
	       num_members);

  for(i = 0; i < num_members; i++)
    if(!strcmp(utype->members[i]->name, member_name))
      return utype->members[i];

  return NULL;

}
