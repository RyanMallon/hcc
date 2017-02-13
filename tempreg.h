/*
 * tempreg.h
 * Ryan Mallon (2006)
 */
#ifndef _TEMPREG_H_
#define _TEMPREG_H_

#include "symtable.h"
#include "scope.h"

void tr_dealloc_all(void);
void tr_dealloc(void);
int tr_alloc(void);
void tr_alloc_k(int);
void tr_set_lowest(int);
void tr_init(void);
void tr_init(void);
int tr_max_temps(void);

#endif /* _TEMPREG_H_ */
