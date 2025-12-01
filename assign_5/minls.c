#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) 
{
  args_struct_t* args = Getopts(argc, argv);
  if (!args) return EXIT_FAILURE;

  minls_input_t* minls_struct = args->struct_var.minls_struct;
  fs_t fs = fs_open(minls_struct->imgfile);

  int p_index = minls_struct->part;
  
  if (p_index >= 0) 
  {
    partition_table_entry_t parts[4];

    if (read_partition_table(&fs,0, parts) != 0)
    {
      fprintf(stderr,"Error in read_primary_partition()!\n");
      exit(EXIT_FAILURE);
    }
    if (select_partition_table(p_index, &fs, parts) != 0)
    {
      fprintf(stderr,"Error in select_primary_partition()!\n");
      exit (EXIT_FAILURE);
    }
  }
  else if (minls_struct->subpart >= 0)
  {
    fprintf(stderr, "-s given without -p\n");
    exit(EXIT_FAILURE);
  }

  if (minls_struct->subpart >= 0)
  {
    partition_table_entry_t subparts [4];
    int s_index = minls_struct->subpart;
    if (read_partition_table(&fs, fs.fs_start, subparts) != 0)
    {
      fprintf(stderr,"Error in read_primary_partition()!\n");
      exit(EXIT_FAILURE);
    }
    if (select_partition_table(s_index, &fs, subparts) != 0)
    {
      fprintf(stderr,"Error in select_primary_partition()!\n");
      exit (EXIT_FAILURE);
    }
  }
  if (read_superblock(&fs) != 0)
  {
    fprintf(stderr, "Error in read_superblock() !\n");
    exit(EXIT_FAILURE);
  }

  inode_t target;
  uint32_t target_inum;

  if (fs_lookup_path(&fs, minls_struct->path, &target, &target_inum) != 0)
  {
    fprintf(stderr, "Path not found: %s\n", minls_struct->path);
    exit(EXIT_FAILURE);
  }

  if (inode_is_directory(&target))
  {
    printf("%s:\n", minls_struct->path);

    int max = target.size / DIR_ENTRY_SIZE;
    minix_dir_entry *entries = malloc(max * sizeof(*entries));
    if (!entries)
    {
      perror("malloc");
      exit(EXIT_FAILURE);
    }

    int count = fs_read_directory(&fs, &target, entries);
    if (count < 0)
    {
      fprintf(stderr, "Error in fs_read_directory\n");
      exit(EXIT_FAILURE);
    }

    for (int i = 0; i < count; i++)
    {
      inode_t child;
      if (fs_read_inode(&fs, entries[i].inode, &child) != 0)
      {
        fprintf(stderr, "Failed to read inode %u\n", entries[i].inode);
        continue;
      }

      char perm[11];
      mode_to_string(child.mode, perm);
      printf("%s %u %s\n",
             perm,
             child.size,
             entries[i].name);
    }
    free(entries);
  }
  else
  {
    char perm[11];
    mode_to_string(target.mode, perm);
    const char* name = minls_struct->path;
    while (*name == '/') name++;

    printf("%s %u %s\n",
           perm,
           target.size,
           name);
  }

  free_args(args);
  fclose(fs.img);
  return 0;
}
