/*
 * icg.c
 *
 * Ryan Mallon (2005)
 *
 * Intermediate Code Generation
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "symtable.h"
#include "icg.h"
#include "strings.h"

const ic_info_t ic_info[MAX_IC_OPS] = {
  {"ic_mov", 2},
  {"ic_add", 3},
  {"ic_sub", 3},
  {"ic_mul", 3},
  {"ic_div", 3},
  {"ic_lit", 2},
  {"ic_ldb", 2},
  {"ic_lod", 2},
  {"ic_sto", 2},
  {"ic_stb", 2},

  {"ic_cmp", 3},
  {"ic_beq", 3},
  {"ic_bne", 3},
  {"ic_blt", 3},
  {"ic_ble", 3},
  {"ic_bgt", 3},
  {"ic_bge", 3},
  {"ic_jmp", 1},
  {"ic_ret", 0},

  {"ic_inc", 1},
  {"ic_psh", 1},
  {"ic_pop", 0},
  {"ic_cal", 1},

  {"ic_vmm", 3},

  {"ic_load_word", 2},
  {"ic_load_byte", 2},
  {"ic_store_word", 2},
  {"ic_store_byte", 2},

  {"ic_push_arg", 1},

  {"ic_prologue", 2},
  {"ic_epilogue", 1}

};

static const char *reg_name[IC_NUM_SREGS] = {
  "%pc", "%sp", "%fp", "%rv"
};

static void print_reg_name(FILE *, int);
static sections_t calc_sections(void);
extern int data_seg_size(void);

static ic_list_t *list_head = NULL, *list_tail = NULL;

ic_instruction_t *instructions;
int num_instructions = 0;

static int stack_inc_line;

/*
 * Get the list head
 */
ic_list_t *ic_list_head(void) {
  return list_head;
}

/*
 * Get the list tail
 */
ic_list_t *ic_list_tail(void) {
  return list_tail;
}

/*
 * Add an instruction to the tail of the list
 */
static void ic_add_to_tail(ic_instruction_t *instr) {

  if(!list_head || !list_tail) {
    list_head = malloc(sizeof(ic_list_t));
    list_head->next = malloc(sizeof(ic_list_t));
    list_head->next->prev = list_head;
    list_head->prev = NULL;
    list_head->instruction = instr;

    list_tail = list_head;

  } else {
    list_tail = list_tail->next;

    list_tail->next = malloc(sizeof(ic_list_t));
    list_tail->next->prev = list_tail;

    list_tail->instruction = instr;

  }

  list_tail->jump = NULL;
}

/*
 * Emit an intermediate instruction
 */
ic_list_t *ic_emit(int op, int r1, int r2, int r3,
			  int t1, int t2, int t3) {
  ic_instruction_t *instruction;
  ic_list_t *current;

  instruction = malloc(sizeof(ic_instruction_t));

  instruction->op = op;
  instruction->reg[0] = r1;
  instruction->reg[1] = r2;
  instruction->reg[2] = r3;
  instruction->type[0] = t1;
  instruction->type[1] = t2;
  instruction->type[2] = t3;

  ic_add_to_tail(instruction);

  num_instructions++;

  return list_tail;
}

/*
 * Emit an IC instruction which has a string operand. The position of the
 * string operand is determined by the type arguments.
 *
 * This function is used for generating high level intermediate code, for
 * passing to a backend.
 *
 */
ic_list_t *ic_emit_str(int op, int r1, int r2, char *str,
		       int t1, int t2, int t3) {

  ic_instruction_t *instruction;
  ic_list_t *current;

  instruction = malloc(sizeof(ic_instruction_t));

  instruction->op = op;

  if(t1 == IC_STRING_OP) {
    instruction->reg[1] = r1;
    instruction->reg[2] = r2;
  } else if(t2 == IC_STRING_OP) {
    instruction->reg[0] = r1;
    instruction->reg[2] = r2;
  } else {
    instruction->reg[0] = r1;
    instruction->reg[1] = r2;
  }

  instruction->type[0] = t1;
  instruction->type[1] = t2;
  instruction->type[2] = t3;

  instruction->string_op = malloc(strlen(str) + 1);
  strcpy(instruction->string_op, str);

  ic_add_to_tail(instruction);

  num_instructions++;

  return list_tail;
}

ic_list_t *ic_current_instruction(void) {
  return list_tail;
}

void ic_set_jump(ic_list_t **instr, ic_list_t *dest) {
  (*instr)->jump = dest;
}

static void print_reg_name(FILE *fd, int reg) {
  if(reg < 0)
    fprintf(fd, "ERR");
  else if(reg < IC_NUM_SREGS)
    fprintf(fd, "%s", reg_name[reg]);
  else
    fprintf(fd, "%%t%d", reg - IC_NUM_SREGS - 1);
}

/*
 * Number the instructions in a linked list and label jump addresses
 * properly
 *
 */
void ic_number_list(void) {
  ic_list_t *current = list_head;
  int index = 0;

  /* Number the instruction in the list */
  while(current) {
    current->num = index++;
    current = current->next;
  }

  /* Label jump instructions */
  current = list_head;
  while(current) {

    if(current->instruction->op == IC_CAL) {
      name_record_t *func;

      /* Set the jump address up */
      func = get_func_entry(current->instruction->string_op);
      if(func && func->ic_offset)
	ic_set_jump(&current, func->ic_offset->next);
    }

    if(current->jump)
      switch(current->instruction->op) {
      case IC_BLT:
      case IC_BLE:
      case IC_BGT:
      case IC_BGE:
      case IC_BNE:
      case IC_BEQ:
	current->instruction->reg[2] = current->jump->num;
	break;

      case IC_CAL:
      case IC_JMP:
	current->instruction->reg[0] = current->jump->num;
	break;

      default:
	fprintf(stderr, "Illegal backpatch, op = %s, addr = %d\n",
		ic_info[current->instruction->op].name, current->num);
	break;
      }
    current = current->next;
  }
}

/*
 * Setup the sections struct
 */
static sections_t calc_sections(void) {
  sections_t sections;

  sections.rodata_base = 0;
  sections.rodata_size = string_table_size();

  sections.data_base = sections.rodata_base + sections.rodata_size;
  sections.data_size = global_scope_size();

  sections.heap_base = sections.data_base + sections.data_size;

  return sections;
}


/*
 * Print out a list version of the IC code.
 */
void print_ic(char *basename) {
  int i, j;
  char *filename;
  FILE *fd;
  ic_list_t *current = list_head;
  sections_t sections = calc_sections();

  filename = malloc(strlen(basename) + 4);
  strcpy(filename, basename);
  strcat(filename, ".ic");

  fd = fopen(filename, "w");

  /* Header */
  fprintf(fd, ".regs %d\n", tr_max_temps());
  fprintf(fd, ".stack %d\n", 1024);

  fprintf(fd, ".rodata %d %d\n", sections.rodata_base, sections.rodata_size);
  fprintf(fd, ".data %d %d\n", sections.data_base, sections.data_size);
  fprintf(fd, ".heap %d\n", sections.heap_base);
  fprintf(fd, "\n");

  /* String constants */
  for(i = 0; i < num_strings(); i++) {
    fprintf(fd, ".string %d \"%s\"\n", get_string_literal(i)->offset,
	    get_string_literal(i)->string);

  }
  if(i)
    fprintf(fd, "\n");

  while(current) {
    fprintf(fd, "%04d: %s ", current->num,
	    ic_info[current->instruction->op].name);

    for(j = 0; j < 3; j++) {

      if(j != 0 && current->instruction->type[j] != IC_NONE &&
	 current->instruction->type[j] != IC_CONST_OFFS)
	fprintf(fd, ", ");

      switch(current->instruction->type[j]) {
      case IC_STRING_OP:
	fprintf(fd, "%s", current->instruction->string_op);
	break;

      case IC_CONST:
	fprintf(fd, "%d", current->instruction->reg[j]);
	break;

      case IC_REG:
	if(current->instruction->reg[j] < 0)
	  fprintf(fd, "ERR(%d)", current->instruction->reg[j]);
	else if(current->instruction->reg[j] < IC_NUM_SREGS)
          fprintf(fd, "%s", reg_name[current->instruction->reg[j]]);
        else
          fprintf(fd, "%%s%d", current->instruction->reg[j] - IC_NUM_SREGS);
	break;

      case IC_HARD_REG:
	if(current->instruction->reg[j] < 0)
	  fprintf(fd, "ERR(%d)", current->instruction->reg[j]);
	else if(current->instruction->reg[j] < IC_NUM_SREGS)
          fprintf(fd, "%s", reg_name[current->instruction->reg[j]]);
        else
          fprintf(fd, "%%t%d", current->instruction->reg[j] - IC_NUM_SREGS);
	break;

      case IC_REG_ADDR:
	if(current->instruction->reg[j] < 0)
	  fprintf(fd, "ERR(%d)", current->instruction->reg[j]);
	else if(current->instruction->reg[j] < IC_NUM_SREGS)
          fprintf(fd, "(%s)", reg_name[current->instruction->reg[j]]);
        else
          fprintf(fd, "(%%t%d)", current->instruction->reg[j] - IC_NUM_SREGS);
	break;

      case IC_REG_BASE:
	if(current->instruction->reg[j] < 0)
	  fprintf(fd, "ERR(%d)", current->instruction->reg[j]);
	if(current->instruction->reg[j] < IC_NUM_SREGS)
          fprintf(fd, "[%s", reg_name[current->instruction->reg[j]]);
        else
          fprintf(fd, "[%%t%d", current->instruction->reg[j] - IC_NUM_SREGS);
	break;

      case IC_CONST_OFFS:
	if(current->instruction->reg[j] >= 0)
	  fprintf(fd, " + %d]", current->instruction->reg[j]);
	else
	  fprintf(fd, " - %d]", -current->instruction->reg[j]);
	break;

      case IC_LABEL:
	fprintf(fd, "<%d>", current->instruction->reg[j]);
	break;


      default:
      case IC_NONE:
	break;

      }
    }
    fprintf(fd, "\n");
    current = current->next;

  }
}
