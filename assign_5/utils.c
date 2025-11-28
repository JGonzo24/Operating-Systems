#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "utils.h"



int Getopts(int argc, char *argv[]) {
  // Check what program is calling Getopts()
  char *prog = argv[0];

  if (strstr(prog, "minls"))
  {
    printf("The program being ran is: %s\n", prog);

    if (argc < 2)
    {
      fprintf(stderr, "Usage: %s [ -v ] [-p num [ -s num ] ] imagefile [ path ]\n", argv[0]);
      return EXIT_FAILURE; 
    }
    minls_input_t* minls_struct = calloc(1, sizeof(*minls_struct));
    args_struct_t* args = calloc(1, sizeof(*args));
    args->type = MINLS_TYPE;
    args->struct_var.minls_struct = minls_struct;
    int ret = allocate_struct(args, argc, argv);
    

  }
  else if (strstr(prog, "minget"))
  {
    printf("The program being ran is: %s\n", prog);
    if (argc < 3)
    {
      fprintf(stderr, "Usage: %s [ -v ] [ -p part [ -s subpart ] ] imagefile" 
                " srcpath [ dstpath ]\n", argv[0]);
      return EXIT_FAILURE;
    }
    minget_input_t* minget_struct = (minget_input_t*)calloc(1, sizeof(*minget_struct));
    args_struct_t* args = (args_struct_t*)calloc(1, sizeof(*args));
    args->type = MINGET_TYPE;
    args->struct_var.minget_struct = minget_struct;
    int ret = allocate_struct(args, argc, argv);
  }
  return 0;
}

int allocate_struct(args_struct_t* args, int argc, char *argv[])
{
  struct_type type = args->type;
  switch (type) 
  {
    case (MINGET_TYPE):
      
      break;
    case (MINLS_TYPE):
      printf("WE GOT TO PRINTF MINLS_TYPE");
      break;
  }

  return 0;
}
