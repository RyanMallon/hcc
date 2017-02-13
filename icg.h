/*
 * icg.h
 *
 * Ryan Mallon (2005)
 *
 */
#ifndef _ICG_H_
#define _ICG_H_

typedef struct ic_instruction_s {
  int op;

  int reg[3];
  char *string_op;
  int type[3];

} ic_instruction_t;

typedef struct {
  char *name;
  int num_regs;
} ic_info_t;

typedef struct ic_list_s {
  ic_instruction_t *instruction;
  int num;

  struct ic_list_s *jump;

  struct ic_list_s *prev;
  struct ic_list_s *next;
} ic_list_t;

typedef struct {
  unsigned int rodata_base;
  unsigned int rodata_size;
  unsigned int data_base;
  unsigned int data_size;
  unsigned int heap_base;
} sections_t;

typedef enum {
  IC_MOV,
  IC_ADD,
  IC_SUB,
  IC_MUL,
  IC_DIV,

  IC_LIT,
  IC_LDB,
  IC_LOD,
  IC_STO,
  IC_STB,

  IC_CMP,
  IC_BEQ,
  IC_BNE,
  IC_BLT,
  IC_BLE,
  IC_BGT,
  IC_BGE,
  IC_JMP,
  IC_RET,

  IC_INC,
  IC_PSH,
  IC_POP,
  IC_CAL,

  IC_VMM,

  /* Highlevel IC codes */
  IC_LOAD_WORD,
  IC_LOAD_BYTE,
  IC_STORE_WORD,
  IC_STORE_BYTE,

  IC_PUSH_ARG,

  IC_PROLOGUE,
  IC_EPILOGUE,

  MAX_IC_OPS
} ic_op_t;

enum {
  IC_REG,
  IC_REG_BASE,
  IC_REG_ADDR,
  IC_HARD_REG,
  IC_CONST,
  IC_CONST_OFFS,
  IC_LABEL,
  IC_STRING_OP,
  IC_NONE
};

/* Special Registers */
enum {
  IC_PC,
  IC_SP,
  IC_FP,
  IC_RV,

  IC_NUM_SREGS
};

/*
 * Size constants, sizes are in bytes
 */
#define IC_SIZEOF_CHAR 1
#define IC_SIZEOF_INT 4
#define IC_SIZEOF_POINTER 4

#define IC_SIZEOF_STRING_DESC 4

#define IC_MEM_MAX 0xffffffff
#define IC_MEM_MIN 0x0

#define IC_STACK_BASE 0xffffffff
#define IC_HEAP_BASE 0x0

#define IC_STACK_CHUNK 32
#define IC_HEAP_CHUNK 32

#define IC_STACK_FRAME_HEADER_BYTES 8

ic_list_t *ic_emit(int, int, int, int, int, int, int);
ic_list_t *ic_emit_str(int, int, int, char *, int, int, int);
int get_code_pointer(void);

ic_list_t *ic_current_instruction();
void ic_set_jump(ic_list_t **, ic_list_t *);

void backpatch_label(int, int);
void backpatch_stack_increment(int);

void ic_number_list(void);
void print_ic(char *);
void init_ic_memory(void);

ic_list_t *ic_list_head(void);
ic_list_t *ic_list_tail(void);

#endif /* _ICG_H_ */
