/*
 * Type Checking
 * Ryan Mallon (2005)
 *
 */
#ifndef _TYPECHK_H_
#define _TYPECHK_H_

#include "symtable.h"

#define TYPE_MASK 0x0000ffff
#define TYPE_ARRAY_MASK 0xffff0000
#define TYPE_HISTORY_MASK 0xffff0000
#define TYPE_VALUE_MASK 0xffff0000
#define TYPE_SHIFT 16

type_info_t *create_ptr_to_type(int, int, int);
type_info_t *create_array_type(int, int, int);
type_info_t *create_type_signature(int, int, ast_node_t *);

type_info_t *result_type(type_info_t *, type_info_t *, int);
int compare_type_sigs(type_info_t *, type_info_t *);
int get_type(ast_node_t *);

type_info_t *cast_array_to_pointer(type_info_t *);

int is_array_type(type_info_t *);
int is_address_type(type_info_t *);
int is_primitive(type_info_t *);
int is_string_type(type_info_t *);
int is_struct_type(type_info_t *);
int is_history_type(type_info_t *);
int primitive_type(type_info_t *);

int history_array_type(type_info_t *);

/*
 * Array functions
 *
 * TODO: Rename these to get rid of the starting get_'s
 */
int get_num_dimensions(type_info_t *);
int get_history_depth(type_info_t *);
int get_dimension_size(type_info_t *, int);
int get_num_elements(type_info_t *);

type_check_t *type_check_node(ast_node_t *);

type_info_t *remove_type_from_sig(type_info_t **, int);
type_info_t *add_to_type_sig(type_info_t **, int);


#if 0
int sizeof_type(type_info_t *);
#endif

type_info_t *copy_sig(type_info_t *, type_info_t *);
type_info_t *type_sign_var(name_record_t *, type_info_t *);

void print_type_signature(type_info_t *);
void print_type_name(name_record_t *);

char *sprint_type_signature(type_info_t *);
char *sprint_type_name(name_record_t *);

#endif /* _TYPECHK_H_ */
