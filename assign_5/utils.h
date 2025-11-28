#ifndef UTILS_H 
#define UTILS_H

/*
 * @brief The struct to be used for getting args
 */
typedef struct {
  bool verbose;
  int part;
  int subpart;
  char *imgfile;
  char *path;
} minls_input_t;

typedef struct {
  bool verbose;
  int part;
  int subpart;
  char *imgfile;
  char *path;
} minget_input_t;

typedef enum {
  MINLS_TYPE,
  MINGET_TYPE
} struct_type;

typedef struct {
  struct_type type;
  union {
    minls_input_t* minls_struct;
    minget_input_t* minget_struct;
  } struct_var;
} args_struct_t;


int Getopts(int argc, char *argv[]);
int allocate_struct(args_struct_t*, int argc, char *argv[]);

#endif
