#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Handles partition setup,primary and subpartition if needed
 *
 * Sets up the filesystem to point to the right partition if the user
 * specified one. Reads partition tables, shows info if verbose, and
 * selects the requested partition.
 *
 * @param fs Pointer to the filesystem struct to modify
 * @param minls_struct Minls input struct containing partition info
 * @return 0 on success, -1 on failure
 */
int setup_partitions(fs_t *fs, minls_input_t *minls_struct)
{
  partition_table_entry_t parts[4];

  /* If no partition specified, done */
  if (minls_struct->part < 0)
  {
    /* Specifying subpartition without main partition */
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
    fprintf(stderr, "minls: failed to read primary partition table\n");
    return -1;
  }

  /* Show partition info if verbose flag set */
  for (int i = 0; i < 4; i++)
  {
    print_part(&parts[i], i, "minls", minls_struct->verbose);
  }

  /* Select the partition */
  if (select_partition_table(minls_struct->part, fs, parts) != 0)
  {
    fprintf(stderr, "minls: failed to select partition %d\n", 
                                            minls_struct->part);
    return -1;
  }

  /* Handle subpartition */
  if (minls_struct->subpart >= 0)
  {
    partition_table_entry_t subparts[4];
    if (read_partition_table(fs, fs->fs_start, subparts) != 0)
    {
      fprintf(stderr, "minls: failed to read subpartition table\n");
      return -1;
    }
    if (select_partition_table(minls_struct->subpart, fs, subparts) != 0)
    {
      fprintf(stderr, "minls: failed to select subpartition %d\n", 
                                              minls_struct->subpart);

      return -1;
    }
  }
  return 0;
}

/**
 * @brief Print the directory header with proper formatting
 *
 * Shows the directory name before listing its contents. If no path
 * given, then specify "/:".
 *
 * @param path The directory path to show in the header
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

/**
 * @brief List all entries in a directory
 *
 * Reads all the directory entries and prints them out with their
 * permissions, size, and name.
 *
 * @param fs Pointer to the filesystem struct
 * @param dir_inode Pointer to the directory's inode
 * @param path Directory path used for the header
 * @return 0 on success, -1 on failure
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
    fprintf(stderr, "minls: failed to read directory contents\n");
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

/**
 * @brief Print info for a single file
 *
 * Shows the file's permissions, size, and name. Used when the path
 * given points to a regular file instead of a directory.
 *
 * @param file_inode Pointer to the file's inode
 * @param path File path to display
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

/**
 * @brief Clean up resources and close files
 *
 * Makes sure we properly close the filesystem image and free any
 * memory we allocated. Called at the end of program or on error.
 *
 * @param fs Pointer to filesystem struct (or NULL)
 * @param args Pointer to arguments struct (or NULL)
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

/**
 * @brief Main function for minls
 *
 * minls [ -v ] [ -p part [ -s subpart ] ] imagefile [ path ]
 *
 * Lists the contents of a directory in the minix filesystem, or shows
 * info about a single file if the path points to a file.
 *
 * @param argc Number of command line arguments
 * @param argv Array of command line argument strings
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on failure
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

  /* Read the superblock into file system struct */
  if (read_superblock(&fs) != 0)
  {
    fprintf(stderr, "minls: failed to read superblock\n");
    cleanup_resources(&fs, args);
    return EXIT_FAILURE;
  }

  /* Show superblock info if verbose flag set */
  print_superblock(&fs, "minls", minls_struct->verbose);

  /* Find the path */
  inode_t target;
  uint32_t target_inum;
  if (fs_lookup_path(&fs, minls_struct->path, &target, &target_inum) != 0)
  {
    fprintf(stderr, "minls: path not found: %s\n", minls_struct->path);
    cleanup_resources(&fs, args);
    return EXIT_FAILURE;
  }

  /* Show inode info if verbose flag set */
  print_inode(&target, target_inum, "minls", minls_struct->verbose);

  /* Handle directories and regular files */
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
