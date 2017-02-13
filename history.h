/*
 * history.h
 *
 * Ryan Mallon (2005)
 *
 */
#ifndef _HISTORY_H_
#define _HISTORY_H_

#include "astparse.h"
#include "symtable.h"
#include "scope.h"

#define HISTORY_ATOMIC_ALL -1

typedef struct {
  int num_atomics;
  name_record_t **atomic_vars;
} atomic_list_t;

void history_add_internal_names(name_record_t *, scope_node_t *);

void history_enter_atomic(void);
void history_enter_atomic(void);
int history_in_atomic(void);
void history_add_clist_write(name_record_t *);

/* Internal name getters */
name_record_t *history_cycle_entry(name_record_t *);
name_record_t *history_current_entry(name_record_t *);
name_record_t *history_copy_entry(name_record_t *);
name_record_t *history_ptr_entry(name_record_t *);
name_record_t *history_clist_entry(name_record_t *);
name_record_t *history_clist_ptr_entry(name_record_t *);

int history_address(expression_t *);

void init_history_vars(void);

void history_store(expression_t *, expression_t *);
void history_array_store(expression_t *, expression_t *, expression_t *);

int history_load(name_record_t *, expression_t *);
//int history_array_load(name_record_t *, int, int);

#endif /* _HISTORY_H_ */
