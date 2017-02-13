/*
 * cflags.h
 * Ryan Mallon (2005)
 *
 * Compiler flags for command line options
 *
 */
#ifndef _CFLAGS_H_
#define _CFLAGS_H_

typedef struct {
  unsigned int flags;
  unsigned int oflags;

  int debug_level;
  char *entry_point;

  int history_simple_depth;
  int history_arg_setting;

} compiler_options_t;

/* Compiler Flags */
enum {
  CFLAG_CLEAR_ALL = 0x0,

  CFLAG_OUTPUT_IC = 0x1,
  CFLAG_OUTPUT_HLIC = 0x2,
  CFLAG_DEBUG_OUTPUT = 0x4,
  CFLAG_USE_LOCAL_HISTORY_LIB = 0x8,
  CFLAG_HISTORY_SIMPLE_STORE = 0x10,
  CFLAG_HISTORY_INLINE = 0x20,
  CFLAG_OUTPUT_STABS = 0x40,
  CFLAG_INLINE = 0x80,
  CFLAG_HISTORY_AW_ORDER_D = 0x100
};

/* History arg settings */
enum {
  HISTORY_ARG_DISABLED = 0,
  HISTORY_ARG_TEMP_COPY = 1
};

/* Optomisation Flags */
enum {
  OFLAG_REG_ASSIGN = 1
};


#endif /* _CFLAGS_H_ */
