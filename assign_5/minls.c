#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Handle partition setup - primary and subpartition if needed
 * Returns 0 on success, -1 on failure
 */
int setup_partitions(fs_t *fs, minls_input_t *minls_struct)
{
  partition_table_entry_t parts[4];

  /* If no partition specified, we're done */
  if (minls_struct->part < 0)
  {
    /* But wait - did they try to specify a subpartition without
     * a partition? */
    if (minls_struct->subpart >= 0)
    {
      fprintf(stderr, "-s given without -p\n");
      return -1;
    }
    return 0;
  }

  /* Read the primary partition table */
  if (read_partition_table(fs, 0, parts) != 0)
  {
    fprintf(stderr, "Error in read_primary_partition()!\n");
    return -1;
  }

  /* Show partition info if we're being verbose */
  for (int i = 0; i < 4; i++)
  {
    print_part(&parts[i], i, "minls", minls_struct->verbose);
  }

  /* Select the partition they want */
  if (select_partition_table(minls_struct->part, fs, parts) != 0)
  {
    fprintf(stderr, "Error in select_primary_partition()!\n");
    return -1;
  }

  /* Handle subpartition if they specified one */
  if (minls_struct->subpart >= 0)
  {
    partition_table_entry_t subparts[4];
    if (read_partition_table(fs, fs->fs_start, subparts) != 0)
    {
      fprintf(stderr, "Error in read_primary_partition()!\n");
      return -1;
    }
    if (select_partition_table(minls_struct->subpart, fs, subparts) != 0)
    {
      fprintf(stderr, "Error in select_primary_partition()!\n");
      return -1;
    }
  }

  return 0;
}

/*
 * Print the directory header with proper formatting
 */
void print_directory_header(const char *path)
{
  if (!path || path[0] == '\0')
  {
    printf("/:\n");
  }
  else
  {
    if (path[0] == '/')
    {
      printf("%s:\n", path);
    }
    else
    {
      printf("/%s:\n", path);
    }
  }
}

/*
 * List all entries in a directory
 * Returns 0 on success, -1 on failure
 */
int list_directory_contents(fs_t *fs, inode_t *dir_inode, const char *path)
{
  print_directory_header(path);

  /* Figure out how many entries we might have */
  int max = dir_inode->size / DIR_ENTRY_SIZE;
  minix_dir_entry *entries = safe_malloc(max * sizeof(*entries));
  if (!entries)
  {
    return -1;
  }

  /* Read all the directory entries */
  int count = fs_read_directory(fs, dir_inode, entries);
  if (count < 0)
  {
    fprintf(stderr, "Error in fs_read_directory\n");
    free(entries);
    return -1;
  }

  /* Print each entry with its permissions and size */
  for (int i = 0; i < count; i++)
  {
    inode_t child;
    if (fs_read_inode(fs, entries[i].inode, &child) != 0)
    {
      fprintf(stderr, "Failed to read inode %u\n", entries[i].inode);
      continue;
    }

    char perm[11];
    mode_to_string(child.mode, perm);
    printf("%s %u %s\n", perm, child.size, entries[i].name);
  }

  free(entries);
  return 0;
}

/*
 * Print info for a single file (not a directory)
 */
void print_file_info(inode_t *file_inode, const char *path)
{
  char perm[11];
  mode_to_string(file_inode->mode, perm);

  /* Skip leading slashes in the name */
  const char *name = path;
  while (*name == '/')
  {
    name++;
  }

  printf("%s %u %s\n", perm, file_inode->size, name);
}

/*
 * Clean up resources and close files
 */
void cleanup_resources(fs_t *fs, args_struct_t *args)
{
  if (fs && fs->img)
  {
    fclose(fs->img);
  }

  if (args)
  {
    free_args(args);
  }
}

/*
 * minls:
 *   minls [ -v ] [ -p part [ -s subpart ] ] imagefile [ path ]
 *
 * Lists the contents of a directory in the minix filesystem, or shows
 * info about a single file if the path points to a file.
 */
int main(int argc, char *argv[])
{
  /* Parse command line arguments */
  args_struct_t *args = Getopts(argc, argv);
  if (!args)
  {
    return EXIT_FAILURE;
  }

  minls_input_t *minls_struct = args->struct_var.minls_struct;

  /* Open the filesystem image */
  fs_t fs = fs_open(minls_struct->imgfile);
  if (fs.img == NULL)
  {
    cleanup_resources(NULL, args);
    return EXIT_FAILURE;
  }

  /* Handle partition setup if needed */
  if (setup_partitions(&fs, minls_struct) != 0)
  {
    cleanup_resources(&fs, args);
    return EXIT_FAILURE;
  }

  /* Read the superblock so we understand the filesystem */
  if (read_superblock(&fs) != 0)
  {
    fprintf(stderr, "Error in read_superblock() !\n");
    cleanup_resources(&fs, args);
    return EXIT_FAILURE;
  }

  /* Show superblock info if we're being verbose */
  print_superblock(&fs, "minls", minls_struct->verbose);

  /* Find the path they want to look at */
  inode_t target;
  uint32_t target_inum;
  if (fs_lookup_path(&fs, minls_struct->path, &target, &target_inum) != 0)
  {
    fprintf(stderr, "Path not found: %s\n", minls_struct->path);
    cleanup_resources(&fs, args);
    return EXIT_FAILURE;
  }

  /* Show inode info if we're being verbose */
  print_inode(&target, target_inum, "minls", minls_struct->verbose);

  /* Handle directories vs regular files differently */
  if (inode_is_directory(&target))
  {
    if (list_directory_contents(&fs, &target, minls_struct->path) != 0)
    {
      cleanup_resources(&fs, args);
      return EXIT_FAILURE;
    }
  }
  else
  {
    print_file_info(&target, minls_struct->path);
  }

  cleanup_resources(&fs, args);
  return EXIT_SUCCESS;
}
